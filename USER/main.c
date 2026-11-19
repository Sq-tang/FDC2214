/**
 * 双传感器采集主程序。
 * MPU6050 通过 USART1 发送六轴 CSV；FDC2214 通过 USART2 发送 CH0 的电容值。
 * 主循环先检查运动采样时间，再处理电容采集，两路共用 CPU。
 */
#include "It_mpu6050.h"
#include "delay.h"
#include "fdc2214.h"
#include "fdc2214_iic.h"
#include "lt_led.h"
#include "string.h"
#include "sys.h"
#include "usart.h"

/* 每 25 个有效滤波结果翻转一次 LED；这是样本计数，不是毫秒定时。 */
#define LED_UPDATE_SAMPLES 25U

#ifdef __GNUC__
#define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
#else
#define PUTCHAR_PROTOTYPE int fputc(int ch, FILE *f)
#endif

/* 将字符输出送到 USART1；逐字符等待发送寄存器空，因此仍为阻塞发送。 */
PUTCHAR_PROTOTYPE
{
    USART_SendData(USART1, (uint8_t)ch);
    while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET)
    {
    }
    return ch;
}

/* 量程由 MPU_6050_Init 配置：加速度 ±2 g，角速度 ±250 °/s。 */
#define MPU_ACCEL_LSB_PER_G    16384.0
#define MPU_GYRO_LSB_PER_DPS   131.0
#define MPU_SAMPLE_HZ         100U  /* 目标每 10 ms 读取一帧。 */

/* SysTick 已用于 delay_ms/us；用 Cortex-M3 周期计数器安排采样。 */
#define DWT_CONTROL_REG (*(volatile uint32_t *)0xE0001000UL)
#define DWT_CYCLE_COUNT  (*(volatile uint32_t *)0xE0001004UL)

/* 读取同一组原始样本，换算为物理量后发送一行 CSV。 */
static void MPU6050_SendMotion(void)
{
    int16_t accel_raw[3], gyro_raw[3], temperature_raw;
    double accel_g[3], gyro_dps[3];
    unsigned int axis;

    /* 驱动会同时读出温度；本串口帧只发送加速度和角速度。 */
    if (MPU6050_GetRaw(accel_raw, gyro_raw, &temperature_raw) != MPU6050_OK)
        return;

    /* 原始计数除以每单位的计数，分别得到 g 和 °/s。 */
    for (axis = 0; axis < 3U; ++axis)
    {
        accel_g[axis] = accel_raw[axis] / MPU_ACCEL_LSB_PER_G;
        gyro_dps[axis] = gyro_raw[axis] / MPU_GYRO_LSB_PER_DPS;
    }

    /* 列顺序与启动时的 CSV 表头一致。 */
    printf("%.4f,%.4f,%.4f,%.3f,%.3f,%.3f\r\n",
           accel_g[0], accel_g[1], accel_g[2],
           gyro_dps[0], gyro_dps[1], gyro_dps[2]);
}

int main(void)
{
    double capacitance; /* 电容值，单位 pF；读取成功后才可使用。 */
    /* 从阈值前一位开始计数，使首个有效结果立即翻转 LED。 */
    unsigned int led_count = LED_UPDATE_SAMPLES - 1U;
    uint8_t mpu_status;
    uint8_t fdc_ready = 0;
    uint32_t last_sample, last_fdc_attempt, now;
    /* 周期计数器每个 CPU 时钟加一，下面两个间隔均以周期数表示。 */
    const uint32_t sample_period = SystemCoreClock / MPU_SAMPLE_HZ;
    const uint32_t fdc_retry_period = SystemCoreClock / 10U;
    char display[32]; /* USART2 电容文本的格式化缓冲区。 */

    /* 释放 JTAG 引脚，同时保留 SWD 下载调试接口。 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO, ENABLE);
    GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);
    delay_init();
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    Init_USART(); /* USART1：115200 bit/s，8N1，PA9/PA10。 */
    LED_GPIO_Config();
    /* 不初始化按键：原按键定义的 PB6/PB7 与 FDC2214 总线重叠。 */
    mpu_status = MPU_6050_Init(); /* 硬件 I2C2，PB10 为 SCL，PB11 为 SDA。 */
    if (mpu_status == MPU6050_OK)
        printf("ax_g,ay_g,az_g,gx_dps,gy_dps,gz_dps\r\n");
    else
        printf("MPU6050 init error: %u\r\n", (unsigned int)mpu_status);

    FDC2214_IIC_Init(); /* 软件 I2C，PB6 为 SCL，PB7 为 SDA。 */

    /* 先打开跟踪模块，再启动 DWT 周期计数器。 */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT_CONTROL_REG |= 1U;
    now = DWT_CYCLE_COUNT;
    /* 把初始基准回拨一个周期，使主循环首次进入就可以采样和初始化。 */
    last_sample = now - sample_period;
    last_fdc_attempt = now - fdc_retry_period;

    while (1)
    {
        /* 用无符号差值判断间隔，计数器回绕时同样适用。 */
        now = DWT_CYCLE_COUNT;
        /* 先处理运动采样。下方电容预热或读取失败时，下一轮仍会检查 MPU6050。 */
        if (mpu_status == MPU6050_OK && now - last_sample >= sample_period)
        {
            /* 从本次采样重新计时，避免延迟后连续补发多帧。 */
            last_sample = now;
            MPU6050_SendMotion();
        }

        /* 初始化失败后约 100 ms 再试，等待期间主循环可以继续检查运动采样。 */
        if (!fdc_ready)
        {
            if (now - last_fdc_attempt >= fdc_retry_period)
            {
                fdc_ready = (FDC2214_Init() == 0);
                last_fdc_attempt = DWT_CYCLE_COUNT;
            }
            continue;
        }

        /* 滤波窗口尚未填满或采集失败时，本轮没有有效电容值可发送。 */
        if (!FDC2214_ReadCapacitance(&capacitance))
            continue;

        /* USART2 保留原有的 16 字符电容格式，超出显示范围时发送提示。 */
        if (capacitance > 999999.999 || capacitance < -99999.999)
            sprintf(display, "Out of range    ");
        else
            sprintf(display, "%16.3f", capacitance);
        uart2_send_buff((u8 *)display, strlen(display));
        uart2_send_buff((u8 *)"\r\n", 2);

        /* LED 以有效电容样本计数翻转。 */
        if (++led_count >= LED_UPDATE_SAMPLES)
        {
            led_count = 0;
            LED1_TOGGLE;
        }
    }
}
