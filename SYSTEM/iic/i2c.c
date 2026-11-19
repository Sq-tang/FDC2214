/**
 * @file    i2c.c
 * @author  Sq_tang
 * @brief   STM32F1 硬件 I2C 主机收发与寄存器读写。
 */
#include "i2c.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_i2c.h"
#include "stm32f10x_rcc.h"

/* 每个硬件事件的轮询上限，等待时间随 CPU 主频和编译优化变化。 */
#define I2C_POLL_LIMIT    100000U
/* BERR：总线时序错误，ARLO：仲裁丢失，OVR：数据溢出。AF 单独处理以报告 NACK 阶段。 */
#define I2C_BUS_FAULTS    (I2C_SR1_BERR | I2C_SR1_ARLO | I2C_SR1_OVR)
#define I2C_CLEAR_ERRORS  (I2C_FLAG_BERR | I2C_FLAG_ARLO | I2C_FLAG_OVR | I2C_FLAG_AF)

/* PB10/PB11 设为复用开漏，I2C2 外设以 100 kHz 生成总线时序。 */
void My_I2C2_Init(void)
{
    GPIO_InitTypeDef pins;
    I2C_InitTypeDef config;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    pins.GPIO_Pin = GPIO_Pin_10 | GPIO_Pin_11;
    pins.GPIO_Mode = GPIO_Mode_AF_OD;
    pins.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOB, &pins);

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C2, ENABLE);
    I2C_DeInit(I2C2);
    I2C_StructInit(&config);
    config.I2C_ClockSpeed = 100000U;
    config.I2C_Ack = I2C_Ack_Enable;
    I2C_Init(I2C2, &config);
}

/* 仅在总线空闲后开始事务。若其它主机占用总线，本函数超时返回。 */
static int prepare(I2C_TypeDef *bus, uint8_t addr, const void *data, uint16_t size)
{
    uint32_t polls;

    if ((bus != I2C1 && bus != I2C2) || addr > 0x7FU || data == 0 || size == 0U)
        return MY_I2C_INVALID_ARGUMENT;
    if ((bus->CR1 & I2C_CR1_PE) == 0U)
        return MY_I2C_BUS_ERROR;

    for (polls = 0; polls < I2C_POLL_LIMIT; ++polls)
    {
        if ((bus->SR2 & I2C_SR2_BUSY) == 0U)
        {
            I2C_ClearFlag(bus, I2C_CLEAR_ERRORS);
            return MY_I2C_OK;
        }
    }
    return MY_I2C_BUS_ERROR;
}

/* 优先检查错误和 NACK，再检查目标事件。轮询次数用完后返回总线错误。 */
static int wait_sr1(I2C_TypeDef *bus, uint16_t flag, int nack_status)
{
    uint32_t polls;

    /* 不能顺便读 SR2：在 ADDR 置位时，这会提前启动下一阶段。 */
    for (polls = 0; polls < I2C_POLL_LIMIT; ++polls)
    {
        uint16_t sr1 = bus->SR1;
        if ((sr1 & I2C_BUS_FAULTS) != 0U)
            return MY_I2C_BUS_ERROR;
        if ((sr1 & I2C_SR1_AF) != 0U)
            return nack_status;
        if ((sr1 & flag) != 0U)
            return MY_I2C_OK;
    }
    return MY_I2C_BUS_ERROR;
}

/* STM32F1 要按 SR1 → SR2 的顺序清除 ADDR，接收流程必须选对调用时机。 */
static void clear_addr(I2C_TypeDef *bus)
{
    (void)bus->SR1;
    (void)bus->SR2;
}

static int send_address(I2C_TypeDef *bus, uint8_t addr, uint8_t direction, int nack_status)
{
    int status;

    I2C_GenerateSTART(bus, ENABLE);
    status = wait_sr1(bus, I2C_SR1_SB, MY_I2C_BUS_ERROR);
    if (status != MY_I2C_OK)
        return status;

    /* 调用方传 7 位地址，库函数设置地址字节最低位的读写方向。 */
    I2C_Send7bitAddress(bus, (uint8_t)(addr << 1), direction);
    return wait_sr1(bus, I2C_SR1_ADDR, nack_status);
}

/* 寄存器号发送完成后才进入数据阶段，以区分两种 NACK。 */
static int write_prefix(I2C_TypeDef *bus, uint8_t addr, const uint8_t *reg)
{
    int status = send_address(bus, addr, I2C_Direction_Transmitter, MY_I2C_NACK_ADDRESS);
    if (status != MY_I2C_OK)
        return status;

    clear_addr(bus);
    if (reg != 0)
    {
        I2C_SendData(bus, *reg);
        return wait_sr1(bus, I2C_SR1_BTF, MY_I2C_NACK_REGISTER);
    }
    return MY_I2C_OK;
}

/* 超时可能留下挂起的 START 或尚未取出的数据，复位后恢复该实例的原配置。 */
static void reset_controller(I2C_TypeDef *bus)
{
    uint16_t cr1 = bus->CR1 & (uint16_t)~(I2C_CR1_START | I2C_CR1_STOP | I2C_CR1_POS);
    uint16_t cr2 = bus->CR2;
    uint16_t ccr = bus->CCR;
    uint16_t trise = bus->TRISE;
    uint16_t oar1 = bus->OAR1;
    uint16_t oar2 = bus->OAR2;

    I2C_SoftwareResetCmd(bus, ENABLE);
    I2C_SoftwareResetCmd(bus, DISABLE);
    bus->CR2 = cr2;
    bus->CCR = ccr;
    bus->TRISE = trise;
    bus->OAR1 = oar1;
    bus->OAR2 = oar2;
    bus->CR1 = cr1 | I2C_CR1_ACK;
}

/* 统一收尾：出错时终止本次事务，再等待外设退出主机状态。 */
static int finish(I2C_TypeDef *bus, int status)
{
    uint32_t polls;

    if (status != MY_I2C_OK)
    {
        /* 仲裁丢失后外设已释放总线，此时不能向其它主机发 STOP。 */
        if ((bus->SR2 & I2C_SR2_MSL) != 0U && (bus->CR1 & I2C_CR1_STOP) == 0U)
            I2C_GenerateSTOP(bus, ENABLE);
        I2C_ClearFlag(bus, I2C_CLEAR_ERRORS);
    }

    for (polls = 0; polls < I2C_POLL_LIMIT; ++polls)
    {
        if ((bus->CR1 & (I2C_CR1_START | I2C_CR1_STOP)) == 0U &&
            (bus->SR2 & I2C_SR2_MSL) == 0U)
            break;
    }
    if (polls == I2C_POLL_LIMIT || status == MY_I2C_BUS_ERROR)
    {
        reset_controller(bus);
        return MY_I2C_BUS_ERROR;
    }

    /* STOP 完成后才能再改 CR1。恢复 ACK 和 POS 供下次接收。 */
    I2C_NACKPositionConfig(bus, I2C_NACKPosition_Current);
    I2C_AcknowledgeConfig(bus, ENABLE);
    return status;
}

static int write_bytes(I2C_TypeDef *bus, uint8_t addr, const uint8_t *reg,
                       const uint8_t *data, uint16_t size)
{
    int status = prepare(bus, addr, data, size);
    if (status != MY_I2C_OK)
        return status;

    status = write_prefix(bus, addr, reg);
    while (status == MY_I2C_OK && size > 0U)
    {
        /* TXE 表示可以再写入一个字节，BTF 才表示最后一字节已经发完。 */
        status = wait_sr1(bus, I2C_SR1_TXE, MY_I2C_NACK_DATA);
        if (status == MY_I2C_OK)
        {
            I2C_SendData(bus, *data++);
            --size;
        }
    }
    if (status == MY_I2C_OK)
        status = wait_sr1(bus, I2C_SR1_BTF, MY_I2C_NACK_DATA);
    if (status == MY_I2C_OK)
        I2C_GenerateSTOP(bus, ENABLE);
    return finish(bus, status);
}

/* 已收到读地址应答，ADDR 仍置位。STM32F1 的清 ADDR / NACK / STOP
 * 顺序取决于接收长度。短临界区结束时恢复调用前的中断状态。 */
static int receive_data(I2C_TypeDef *bus, uint8_t *data, uint16_t size)
{
    int status;
    uint32_t primask;

    /* 单字节接收：先关 ACK，再清 ADDR 并请求 STOP，最后等 RXNE 读取。 */
    if (size == 1U)
    {
        I2C_AcknowledgeConfig(bus, DISABLE);
        primask = __get_PRIMASK();
        __disable_irq();
        clear_addr(bus);
        I2C_GenerateSTOP(bus, ENABLE);
        __set_PRIMASK(primask);
        status = wait_sr1(bus, I2C_SR1_RXNE, MY_I2C_BUS_ERROR);
        if (status == MY_I2C_OK)
            *data = I2C_ReceiveData(bus);
        return status;
    }

    if (size == 2U)
    {
        /* POS 已选中下一字节。清 ADDR 后立即关 ACK，让第二字节收到 NACK。 */
        primask = __get_PRIMASK();
        __disable_irq();
        clear_addr(bus);
        I2C_AcknowledgeConfig(bus, DISABLE);
        __set_PRIMASK(primask);
    }
    else
    {
        clear_addr(bus);
        while (size > 3U)
        {
            status = wait_sr1(bus, I2C_SR1_BTF, MY_I2C_BUS_ERROR);
            if (status != MY_I2C_OK)
                return status;
            *data++ = I2C_ReceiveData(bus);
            --size;
        }

        /* 留下最后 3 字节。BTF 置位时先关闭 ACK，再取出倒数第三字节。 */
        status = wait_sr1(bus, I2C_SR1_BTF, MY_I2C_BUS_ERROR);
        if (status != MY_I2C_OK)
            return status;
        I2C_AcknowledgeConfig(bus, DISABLE);
        *data++ = I2C_ReceiveData(bus);
    }

    status = wait_sr1(bus, I2C_SR1_BTF, MY_I2C_BUS_ERROR);
    if (status != MY_I2C_OK)
        return status;

    /* 剩两字节时，先请求 STOP，再读取 DR 中的第一个字节。 */
    primask = __get_PRIMASK();
    __disable_irq();
    I2C_GenerateSTOP(bus, ENABLE);
    data[0] = I2C_ReceiveData(bus);
    __set_PRIMASK(primask);
    data[1] = I2C_ReceiveData(bus);
    return MY_I2C_OK;
}

static int read_bytes(I2C_TypeDef *bus, uint8_t addr, const uint8_t *reg,
                      uint8_t *data, uint16_t size)
{
    int status = prepare(bus, addr, data, size);
    int nack_status = MY_I2C_NACK_ADDRESS;
    if (status != MY_I2C_OK)
        return status;

    if (reg != 0)
    {
        status = write_prefix(bus, addr, reg);
        if (status != MY_I2C_OK)
            return finish(bus, status);
        /* 不发 STOP，下一次 send_address 产生重复 START。 */
        nack_status = MY_I2C_NACK_READ_ADDRESS;
    }
    I2C_AcknowledgeConfig(bus, ENABLE);
    I2C_NACKPositionConfig(bus, size == 2U ? I2C_NACKPosition_Next : I2C_NACKPosition_Current);
    status = send_address(bus, addr, I2C_Direction_Receiver, nack_status);
    if (status == MY_I2C_OK)
        status = receive_data(bus, data, size);
    return finish(bus, status);
}

/* 纯写接口为弱符号，项目可提供同名强符号来替换默认实现。 */
#if defined(__CC_ARM)
__weak
#elif defined(__GNUC__)
__attribute__((weak))
#endif
int My_I2C_SendBytes(I2C_TypeDef *bus, uint8_t addr, const uint8_t *data, uint16_t size)
{
    return write_bytes(bus, addr, 0, data, size);
}

int My_I2C_ReceiveBytes(I2C_TypeDef *bus, uint8_t addr, uint8_t *data, uint16_t size)
{
    return read_bytes(bus, addr, 0, data, size);
}

int My_I2C_RegWriteBytes(I2C_TypeDef *bus, uint8_t addr, uint8_t reg, const uint8_t *data, uint16_t size)
{
    return write_bytes(bus, addr, &reg, data, size);
}

int My_I2C_RegReadBytes(I2C_TypeDef *bus, uint8_t addr, uint8_t reg, uint8_t *data, uint16_t size)
{
    return read_bytes(bus, addr, &reg, data, size);
}
