#include "delay.h"
#include "sys.h"
#include "usart.h"
#include "lt_led.h"
#include "lt_key.h"
#include "lt_beep.h"
#include "lt_relay.h"
#include "lt_ir_switch.h"
#include "lt_ir_man.h"
#include "lt_dht11.h"
#include "lt_adc.h"
#include "lt_pwm.h"
#include "lt_ds18b20.h"
#include "lt_oled.h"
#include "lt_matrix_key.h"
#include "lt_rc522.h"
#include "lt_ds1302.h"
/*
    由于加入了MAX30102驱动，导致RAM占用约为11K，大于了C6T6的最大值
    所以在使用到MAX30102模块时再加入相应的驱动代码，否则不添加
*/
//#include "lt_max30102.h"

/************************************************
	零涂STM32 毕设开发板
    STM32芯片：>= STM32F103C8T6的F1系列
    OLED模块：
                VCC: 3.3V
                GND: GND
                SCL: A5
                SDA: A7
    DS18B20:
                VCC: 5V
                DQ:  B11
                GND: GND
    按键: B12   低电平触发
    蜂鸣器：
                VCC: 5V
                IO:  B13
                GND: GND
    
************************************************/

// ADC1转换的电压值通过MDA方式传到SRAM
//extern __IO uint16_t ADC_ConvertedValue[NOFCHANEL];
//extern u8 USART_RX_BUF[USART_REC_LEN];

int main(void)
{		
    CALENDAR calendar={0x2022,0x02,0x21,0x19,0x34,0x00,0x00};
    u8 hour = 0;
	u8 min = 0;
	u8 sec = 0;
    
    int32_t n_heart_rate = 0;
    int32_t n_sp02 = 0;
    char buf[100] = {0};
    float temperature = 0;
    int i = 0;
    
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB |  RCC_APB2Periph_AFIO, ENABLE);//使能PORTB口时钟、复用引脚PB3、PB4引脚
    GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);  //关闭JTAG，SWD可以调试的

	delay_init();	    	 //延时函数初始化	  
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2); //设置NVIC中断分组2:2位抢占优先级，2位响应优先级
	uart_init(115200);	 //串口初始化为115200
    
//  RC522_GPIO_Config();
//	LED_GPIO_Config();
//	KEY_GPIO_Config();
//	BEEP_GPIO_Config();
   
    
//	RELAY_GPIO_Config();
//	IR_SW_GPIO_Config();
//	IR_MAN_GPIO_Config();
//	ADCx_Init();
    OLED_Init();
    OLED_Clear();
    DS1302_Init();
//  max30102_init();
//  Matrix_KEY_GPIO_Config();
//	PWM_Init();
//	while(!DHT11_Init()); 
//  while(DS18B20_GPIO_Config());	//DS18B20初始化
    
    // 显示心率监测系统
//    for(i = 0; i < 6; i++)
//    {
//        OLED_ShowCHinese(16+i*16,0, i);
//    }
//    
//    // 显示心率
//    OLED_ShowCHinese(0, 2, 6);
//    OLED_ShowCHinese(16,2, 7);
//    
//    // 显示血氧
//    OLED_ShowCHinese(64,   2, 8);
//    OLED_ShowCHinese(64+16,2, 9);
//    
//    // 显示温度
//    OLED_ShowCHinese(0, 4, 10);
//    OLED_ShowCHinese(16,4, 11);
//    
//    // 显示状态
//    OLED_ShowCHinese(64,   4, 12);
//    OLED_ShowCHinese(64+16,4, 13);

//    Set_Calendar(&calendar);   //校准时间函数
    
    while(1)
    {
        Get_Calendar(&calendar);
		
		hour = BCD2DEC(calendar.hour);
		min  = BCD2DEC(calendar.minute);
		sec = BCD2DEC(calendar.second);
		
		OLED_ShowString(0,2,"               ", 16);
		sprintf(buf, "%x-%x %2d:%2d:%2d", calendar.month , calendar.date, hour, min, sec);
		OLED_ShowString(0,2,buf, 16);
        delay_ms(10);
    }        
 }

