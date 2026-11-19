/**
 * @file    i2c.h
 * @author  Sq_tang
 * @brief   STM32F1 硬件 I2C 主机轮询接口。
 */
#ifndef MY_I2C_H
#define MY_I2C_H

#include "stm32f10x.h"

enum
{
    MY_I2C_OK                =  0,
    MY_I2C_NACK_ADDRESS      = -1, /* 首次寻址未应答 */
    MY_I2C_NACK_DATA         = -2, /* 从机拒收数据 */
    MY_I2C_NACK_REGISTER     = -3, /* 从机拒收寄存器号 */
    MY_I2C_INVALID_ARGUMENT  = -4,
    MY_I2C_BUS_ERROR         = -5, /* 超时、总线错误、仲裁丢失或溢出 */
    MY_I2C_NACK_READ_ADDRESS = -6  /* 重复 START 后的读地址未应答 */
};

/* 本板的 I2C2：PB10=SCL，PB11=SDA，100 kHz，两线需上拉至 3.3 V。 */
void My_I2C2_Init(void);

/* I2Cx 须已初始化。ADDR 为未左移的 7 位地址，例如 0x68。
 * Size 为 1..65535，缓冲区至少有 Size 个字节。接口同步阻塞，同一总线串行调用。
 * 读失败时丢弃本次输出。寄存器接口使用单字节寄存器号，寄存器读通过重复 START 切换方向。
 */
/* 直接向从机发送字节流。 */
int My_I2C_SendBytes(I2C_TypeDef *I2Cx, uint8_t ADDR, const uint8_t *pData, uint16_t Size);
/* 直接从从机当前读位置接收字节流。 */
int My_I2C_ReceiveBytes(I2C_TypeDef *I2Cx, uint8_t ADDR, uint8_t *pBuffer, uint16_t Size);
/* 先发送寄存器地址，再用重复 START 切换到读方向。 */
int My_I2C_RegReadBytes(I2C_TypeDef *I2Cx, uint8_t ADDR, uint8_t Reg, uint8_t *pBuffer, uint16_t Size);
/* 在同一事务内先写寄存器地址，再写入数据。 */
int My_I2C_RegWriteBytes(I2C_TypeDef *I2Cx, uint8_t ADDR, uint8_t Reg, const uint8_t *pData, uint16_t Size);

#endif
