#ifndef IT_MPU6050_H
#define IT_MPU6050_H

#include <stdint.h>

#define MPU6050_ADDR          0x68U
#define MPU6050_REG_WHO_AM_I   0x75U
#define MPU6050_REG_WHO_AN_I   MPU6050_REG_WHO_AM_I

/* Call after delay_init(); PB10/PB11 require pull-ups to 3.3 V. */
void MPU_6050_IIC_Init(void);
uint8_t MPU_6050_Init(void);

/* 0: success; 1: write address NACK; 2: register NACK;
 * 3: data/read address NACK; 4: invalid argument; 5: bus error.
 * ReadRegs requires len bytes of buffer space. Discard output on failure.
 */
uint8_t MPU_6050_WriteReg(uint8_t reg, uint8_t value);
uint8_t MPU_6050_ReadReg(uint8_t reg, uint8_t *value);
uint8_t MPU_6050_ReadRegs(uint8_t reg, uint8_t *buf, uint8_t len);

/* accel and gyro each hold three raw values in X, Y, Z order. */
uint8_t MPU6050_GetRaw(int16_t accel[3], int16_t gyro[3], int16_t *temperature);

#endif
