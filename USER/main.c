/**
 * 单通道电容测量主程序：物理 CH0 对应界面及串口中的通道 1。
 * FDC2214 连续转换，主循环读取新数据并更新滑动滤波结果。
 * 正常采集不插入固定延时；串口和 OLED 写入仍会占用 CPU 时间。
 */
#include "delay.h"
#include "fdc2214.h"
#include "fdc2214_iic.h"
#include "lt_led.h"
#include "lt_oled.h"
#include "string.h"
#include "sys.h"
#include "usart.h"

/* 板级固定零点补偿，单位 pF；实际数值需按接线及空载测量校准。 */
#define CAP_OFFSET_PF 9.0
/* 每 25 个有效滤波结果刷新一次屏幕；这是样本计数，不是毫秒定时。 */
#define OLED_UPDATE_SAMPLES 25U

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

int main(void)
{
    double capacitance; /* 电容值，单位 pF；读取成功后才可使用。 */
    /* 从阈值前一位开始计数，使首个有效结果立即显示。 */
    unsigned int display_count = OLED_UPDATE_SAMPLES - 1U;
    char display[32]; /* 16 字符显示行及格式化缓冲空间。 */

    /* 释放 JTAG 引脚，同时保留 SWD 下载调试接口。 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO, ENABLE);
    GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);
    delay_init();
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    Init_USART(); /* USART1：115200 bit/s，8N1，PA9/PA10。 */
    LED_GPIO_Config();
    /* 不初始化按键：原按键定义的 PB6/PB7 与 FDC2214 总线重叠。 */
    /* OLED 使用 PB10/PB11，标题和单位只在启动时绘制。 */
    OLED_Init();
    OLED_Clear();
    OLED_ShowString(0, 0, (u8 *)"CH1 Capacitance", 16);
    OLED_ShowString(0, 6, (u8 *)"Unit: pF", 16);
    FDC2214_IIC_Init(); /* 软件 I2C：PB6 为 SCL，PB7 为 SDA。 */
    /* 初始化返回 0 表示成功；失败时每隔 100 ms 重试。 */
    while (FDC2214_Init())
    {
        OLED_ShowString(0, 2, (u8 *)"Sensor error    ", 16);
        delay_ms(100);
    }

    // FDC2214_SetSleepMode(1);
    OLED_ShowString(0, 2, (u8 *)"Warming up...   ", 16);
    printf("FDC2214 Init OK!\r\n");

    /* 初始化后需收满 20 个有效样本；预热阶段不输出电容。 */
    while (1)
    {
        /* 返回 0 表示窗口未填满或读取失败，不发送未更新的数据。 */
        if (!FDC2214_ReadCapacitance(&capacitance))
        {
            printf("ERROR\r\n");
            continue;
        }

        capacitance -= CAP_OFFSET_PF; /* 在驱动已扣除 33 pF 的基础上补偿。 */
        printf("%5.3f\r\n", capacitance);

        /* Send every valid result on USART2; keep the existing text format. */
        if (capacitance > 999999.999 || capacitance < -99999.999)
            sprintf(display, "Out of range    ");
        else
            sprintf(display, "%16.3f", capacitance);
        uart2_send_buff((u8 *)display, strlen(display));
        uart2_send_buff((u8 *)"\r\n", 2);

        /* Only OLED updates are decimated; serial output is independent. */
        if (++display_count >= OLED_UPDATE_SAMPLES)
        {
            display_count = 0;
            OLED_ShowString(0, 2, (u8 *)display, 16);
            LED1_TOGGLE;
        }
    }
}
