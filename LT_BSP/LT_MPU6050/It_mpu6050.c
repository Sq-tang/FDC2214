#include "It_mpu6050.h"
#include "delay.h"

#define MPU6050_REG_ACCEL_XOUT_H  0x3BU
#define MPU_SCL                  GPIO_Pin_10
#define MPU_SDA                  GPIO_Pin_11
#define I2C_BUS_ERROR            5U

void MPU_6050_IIC_Init(void)
{
    GPIO_InitTypeDef pins;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    GPIO_SetBits(GPIOB, MPU_SCL | MPU_SDA);
    pins.GPIO_Pin = MPU_SCL | MPU_SDA;
    pins.GPIO_Mode = GPIO_Mode_Out_OD;
    pins.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOB, &pins);
}

/* Open drain: setting a pin releases it to the external pull-up. */
static uint8_t IIC_RaiseClock(void)
{
    unsigned int tries;

    GPIO_SetBits(GPIOB, MPU_SCL);
    for (tries = 0; tries < 100U; ++tries)
    {
        if (GPIO_ReadInputDataBit(GPIOB, MPU_SCL))
        {
            delay_us(5);
            return 0;
        }
        delay_us(1);
    }
    return I2C_BUS_ERROR;
}

static uint8_t IIC_Start(void)
{
    GPIO_SetBits(GPIOB, MPU_SDA);
    delay_us(5);
    if (IIC_RaiseClock() != 0 || !GPIO_ReadInputDataBit(GPIOB, MPU_SDA))
        return I2C_BUS_ERROR;

    GPIO_ResetBits(GPIOB, MPU_SDA);
    delay_us(5);
    GPIO_ResetBits(GPIOB, MPU_SCL);
    return 0;
}

static uint8_t IIC_Stop(void)
{
    uint8_t status;

    GPIO_ResetBits(GPIOB, MPU_SCL | MPU_SDA);
    delay_us(5);
    status = IIC_RaiseClock();
    GPIO_SetBits(GPIOB, MPU_SDA);
    delay_us(5);
    if (!GPIO_ReadInputDataBit(GPIOB, MPU_SDA))
        return I2C_BUS_ERROR;
    return status;
}

static uint8_t IIC_Bit(uint8_t release_sda, uint8_t *input)
{
    if (release_sda)
        GPIO_SetBits(GPIOB, MPU_SDA);
    else
        GPIO_ResetBits(GPIOB, MPU_SDA);

    delay_us(5);
    if (IIC_RaiseClock() != 0)
        return I2C_BUS_ERROR;
    *input = GPIO_ReadInputDataBit(GPIOB, MPU_SDA);
    GPIO_ResetBits(GPIOB, MPU_SCL);
    return 0;
}

static uint8_t IIC_SendByte(uint8_t value, uint8_t nack_status)
{
    uint8_t bit;
    uint8_t input;

    for (bit = 0; bit < 8U; ++bit)
    {
        if (IIC_Bit(value & 0x80U, &input) != 0)
            return I2C_BUS_ERROR;
        value <<= 1;
    }

    if (IIC_Bit(1, &input) != 0)
        return I2C_BUS_ERROR;
    return input ? nack_status : 0;
}

static uint8_t IIC_ReadByte(uint8_t nack, uint8_t *value)
{
    uint8_t bit;
    uint8_t input;
    uint8_t data = 0;

    for (bit = 0; bit < 8U; ++bit)
    {
        if (IIC_Bit(1, &input) != 0)
            return I2C_BUS_ERROR;
        data = (uint8_t)((data << 1) | input);
    }

    if (IIC_Bit(nack, &input) != 0)
        return I2C_BUS_ERROR;
    *value = data;
    return 0;
}

static uint8_t MPU6050_SelectReg(uint8_t reg)
{
    uint8_t status = IIC_Start();

    if (status == 0)
        status = IIC_SendByte((uint8_t)(MPU6050_ADDR << 1), 1);
    if (status == 0)
        status = IIC_SendByte(reg, 2);
    return status;
}

static uint8_t IIC_Finish(uint8_t status)
{
    uint8_t stop_status = IIC_Stop();

    return status != 0 ? status : stop_status;
}

uint8_t MPU_6050_WriteReg(uint8_t reg, uint8_t value)
{
    uint8_t status = MPU6050_SelectReg(reg);

    if (status == 0)
        status = IIC_SendByte(value, 3);
    return IIC_Finish(status);
}

uint8_t MPU_6050_ReadReg(uint8_t reg, uint8_t *value)
{
    return MPU_6050_ReadRegs(reg, value, 1);
}

uint8_t MPU_6050_ReadRegs(uint8_t reg, uint8_t *buf, uint8_t len)
{
    uint8_t index;
    uint8_t status;

    if (buf == 0 || len == 0)
        return 4;

    status = MPU6050_SelectReg(reg);
    if (status == 0)
        status = IIC_Start(); /* Repeated START. */
    if (status == 0)
        status = IIC_SendByte((uint8_t)((MPU6050_ADDR << 1) | 1U), 3);

    for (index = 0; status == 0 && index < len; ++index)
        status = IIC_ReadByte(index + 1U == len, &buf[index]);

    return IIC_Finish(status);
}


static int16_t MPU6050_DecodeRaw(const uint8_t *bytes)
{
    uint16_t word = (uint16_t)(((uint16_t)bytes[0] << 8) | bytes[1]);

    if (word >= 0x8000U)
        return (int16_t)((int32_t)word - 65536L);
    return (int16_t)word;
}

uint8_t MPU6050_GetRaw(int16_t accel[3], int16_t gyro[3], int16_t *temperature)
{
    uint8_t raw[14];
    uint8_t status;
    uint8_t axis;

    if (accel == 0 || gyro == 0 || temperature == 0)
        return 4;

    status = MPU_6050_ReadRegs(MPU6050_REG_ACCEL_XOUT_H, raw, sizeof(raw));
    if (status != 0)
        return status;

    /* Register order: accel X/Y/Z, temperature, gyro X/Y/Z. */
    for (axis = 0; axis < 3; ++axis)
    {
        accel[axis] = MPU6050_DecodeRaw(&raw[2U * axis]);
        gyro[axis] = MPU6050_DecodeRaw(&raw[8U + 2U * axis]);
    }
    *temperature = MPU6050_DecodeRaw(&raw[6]);
    return 0;
}
