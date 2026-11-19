#include "It_mpu6050.h"
#include "i2c.h"
#include "delay.h"

#define MPU6050_REG_SMPLRT_DIV    0x19U
#define MPU6050_REG_ACCEL_XOUT_H  0x3BU
#define MPU6050_REG_PWR_MGMT_1    0x6BU
#define MPU6050_DEVICE_RESET      0x80U
#define MPU6050_CLOCK_PLL_XGYRO   0x01U

/* 总线层返回有符号错误码；此处集中适配原有 MPU6050 接口的返回值。 */
static uint8_t MPU6050_Status(int bus_status)
{
    switch (bus_status)
    {
    case MY_I2C_OK:                return MPU6050_OK;
    case MY_I2C_NACK_ADDRESS:      return MPU6050_ERR_ADDRESS;
    case MY_I2C_NACK_REGISTER:     return MPU6050_ERR_REGISTER;
    case MY_I2C_NACK_READ_ADDRESS:
    case MY_I2C_NACK_DATA:         return MPU6050_ERR_DATA;
    case MY_I2C_INVALID_ARGUMENT:  return MPU6050_ERR_ARGUMENT;
    default:                       return MPU6050_ERR_BUS;
    }
}

void MPU_6050_IIC_Init(void)
{
    My_I2C2_Init();
}

uint8_t MPU_6050_WriteReg(uint8_t reg, uint8_t value)
{
    return MPU6050_Status(My_I2C_RegWriteBytes(I2C2, MPU6050_ADDR, reg, &value, 1U));
}

uint8_t MPU_6050_ReadReg(uint8_t reg, uint8_t *value)
{
    return MPU_6050_ReadRegs(reg, value, 1U);
}

uint8_t MPU_6050_ReadRegs(uint8_t reg, uint8_t *buf, uint8_t len)
{
    return MPU6050_Status(My_I2C_RegReadBytes(I2C2, MPU6050_ADDR, reg, buf, len));
}

uint8_t MPU_6050_Init(void)
{
    /* 从 0x19 连写四个相邻寄存器。DLPF=3 时陀螺数据率为 1 kHz。 */
    static const uint8_t sample_config[] = {
        9U,     /* SMPLRT_DIV: 1000 / (1 + 9) = 100 Hz */
        3U,     /* CONFIG: 陀螺带宽 42 Hz，加速度带宽 44 Hz */
        0U,     /* GYRO_CONFIG: ±250 °/s */
        0U      /* ACCEL_CONFIG: ±2 g */
    };
    uint8_t id;
    uint8_t status;

    MPU_6050_IIC_Init();
    delay_ms(100);  /* MCU 与传感器同时上电时，等待传感器就绪。 */
    status = MPU_6050_ReadReg(MPU6050_REG_WHO_AM_I, &id);
    if (status != MPU6050_OK)
        return status;
    if (id != MPU6050_WHO_AM_I_VALUE)
        return MPU6050_ERR_IDENTITY;

    status = MPU_6050_WriteReg(MPU6050_REG_PWR_MGMT_1, MPU6050_DEVICE_RESET);
    if (status != MPU6050_OK)
        return status;
    delay_ms(100);

    /* 退出睡眠，以 X 轴陀螺仪的 PLL 作为时钟。 */
    status = MPU_6050_WriteReg(MPU6050_REG_PWR_MGMT_1, MPU6050_CLOCK_PLL_XGYRO);
    if (status != MPU6050_OK)
        return status;
    status = MPU6050_Status(My_I2C_RegWriteBytes(I2C2, MPU6050_ADDR,
                            MPU6050_REG_SMPLRT_DIV, sample_config, sizeof(sample_config)));
    if (status != MPU6050_OK)
        return status;
    delay_ms(50);   /* 陀螺仪启动并取得首批滤波样本后，再让调用方读取。 */
    return MPU6050_OK;
}

/* 原始值高字节在前，合并后按 16 位有符号补码解释。 */
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
        return MPU6050_ERR_ARGUMENT;

    status = MPU_6050_ReadRegs(MPU6050_REG_ACCEL_XOUT_H, raw, sizeof(raw));
    if (status != MPU6050_OK)
        return status;

    /* 14 字节依次是加速度 X/Y/Z、温度、陀螺仪 X/Y/Z，每项占 2 字节。 */
    for (axis = 0; axis < 3; ++axis)
    {
        accel[axis] = MPU6050_DecodeRaw(&raw[2U * axis]);
        gyro[axis] = MPU6050_DecodeRaw(&raw[8U + 2U * axis]);
    }
    *temperature = MPU6050_DecodeRaw(&raw[6]);
    return MPU6050_OK;
}
