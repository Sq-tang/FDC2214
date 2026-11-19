#ifndef IT_MPU6050_H
#define IT_MPU6050_H

#include <stdint.h>

/* 本板 AD0 接地，地址为 0x68。WHO_AM_I 的读值不随 AD0 改变。 */
#define MPU6050_ADDR           0x68U
#define MPU6050_REG_WHO_AM_I   0x75U
#define MPU6050_REG_WHO_AN_I   MPU6050_REG_WHO_AM_I  /* 旧名称 */
#define MPU6050_WHO_AM_I_VALUE 0x68U

enum {
    MPU6050_OK = 0,
    MPU6050_ERR_ADDRESS = 1,
    MPU6050_ERR_REGISTER = 2,
    MPU6050_ERR_DATA = 3,       /* 数据或重复 START 后的读地址未应答 */
    MPU6050_ERR_ARGUMENT = 4,
    MPU6050_ERR_BUS = 5,        /* 总线错误或超时 */
    MPU6050_ERR_IDENTITY = 6    /* 器件应答，但 WHO_AM_I 不匹配 */
};

/* 原有总线初始化接口。需要正常采样时直接调用下方 MPU_6050_Init。 */
void MPU_6050_IIC_Init(void);

/* delay_init() 后调用。初始化总线，检查身份，复位并配置 100 Hz、
 * 加速度 ±2 g、角速度 ±250 °/s。返回 MPU6050_OK 后即可读取数据。 */
uint8_t MPU_6050_Init(void);

uint8_t MPU_6050_WriteReg(uint8_t reg, uint8_t value);
uint8_t MPU_6050_ReadReg(uint8_t reg, uint8_t *value);
/* buf 至少有 len 字节，len 大于 0。错误时应丢弃本次 buf 内容。 */
uint8_t MPU_6050_ReadRegs(uint8_t reg, uint8_t *buf, uint8_t len);

/* 单次突发读取，同一组样本。accel、gyro 按 X/Y/Z 排列，temperature 是
 * 温度原始计数。默认量程下分别为 16384 LSB/g、131 LSB/(°/s)，
 * 温度换算为 temperature / 340.0 + 36.53 °C。失败时保留输出原值。 */
uint8_t MPU6050_GetRaw(int16_t accel[3], int16_t gyro[3], int16_t *temperature);

#endif
