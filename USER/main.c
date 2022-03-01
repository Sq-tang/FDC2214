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
#include "lt_timer.h"
#include "lt_voice.h"
/*
    由于加入了MAX30102驱动，导致RAM占用约为11K，大于了C6T6的最大值
    所以在使用到MAX30102模块时再加入相应的驱动代码，否则不添加
*/
//#include "lt_max30102.h"

#define MAX_TIME_NUM                3

/************************************************
	零涂STM32 毕设开发板
    STM32芯片：>= STM32F103C6T6的F1系列
    OLED模块：
                VCC: 3.3V
                GND: GND
                SCL: A5
                SDA: A7
    按键: B7    低电平触发
    语音模块：
                RX:  A2
                TX:  A3
                VCC: 5V
                GND:GND
    超声波测距模块：
                GND:   GND
                TXD:   A10
                RXD:   A9
                VCC:   5V
    
************************************************/

// ADC1转换的电压值通过MDA方式传到SRAM
//extern __IO uint16_t ADC_ConvertedValue[NOFCHANEL];
//extern u8 USART_RX_BUF[USART_REC_LEN];

char TriMeasCmd[1] = {0x55};

extern u8 USART_RX_BUF[USART_REC_LEN];      // AA A5 09 01 01 4B 01 6D 00 EA AE 55
extern uint16_t RecIndex;
extern uint8_t RecFlag;

void LT_Analyze_IR_Temp_Data(u8* USART_RX_BUF, uint16_t RecIndex, float *MAN_Temp, float *ENV_Temp)
{
    uint16_t i = 0;
    uint16_t man_temper_hex = 0;
    uint16_t env_temper_hex = 0;
    
    for(i = 0; i < RecIndex; i++)
    {
        if((USART_RX_BUF[i] == 0xAA) && (USART_RX_BUF[i+1] == 0xA5) && (USART_RX_BUF[i+2] == 0x09) 
           && (USART_RX_BUF[i+3] == 0x01) && (USART_RX_BUF[i+11] == 0x55))
        {
            man_temper_hex = USART_RX_BUF[i+6]<<8;
            man_temper_hex += USART_RX_BUF[i+7];
            *MAN_Temp = man_temper_hex/10.0;
            
            env_temper_hex = USART_RX_BUF[i+8]<<8;
            env_temper_hex += USART_RX_BUF[i+9];
            *ENV_Temp = env_temper_hex/10.0;
            return;
        }
    }
}

void LT_Analyze_Distance_Data(u8* USART_RX_BUF, uint16_t RecIndex, uint32_t *distance)
{
    uint16_t i = 0; 
    *distance = 0;
    
    for(i = 0; i < RecIndex; i++)
    {
        if(USART_RX_BUF[i] == 0xFF)
        {
            *distance =  USART_RX_BUF[i+1]<<8;
            *distance += USART_RX_BUF[i+2];
            return;
        }
    }
}

int main(void)
{	
    int i = 0;
    
    // CALENDAR calendar={0x2022,0x02,0x21,0x19,0x34,0x00,0x00};
//    u8 USART_RX_BUF[15] = {0x12, 0x14, 0xAA, 0xA5, 0x09, 0x01, 0x01, 0x34, 0x01, 0x69, 0x01, 0x29, 0xD3, 0x55, 0x12};
//    uint16_t RecIndex = 15;
    uint32_t distance = 0;
    u8 buf[10] = {0};
    
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB |  RCC_APB2Periph_AFIO, ENABLE);//使能PORTB口时钟、复用引脚PB3、PB4引脚
    GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);  //关闭JTAG，SWD可以调试的

	delay_init();	    	 //延时函数初始化	  
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2); //设置NVIC中断分组2:2位抢占优先级，2位响应优先级
	uart_init(9600);	 //串口初始化为115200
    uart2_init(9600);
    
//  RC522_GPIO_Config();
	LED_GPIO_Config();
	KEY_GPIO_Config();
	BEEP_GPIO_Config();
   
    
//	RELAY_GPIO_Config();
//	IR_SW_GPIO_Config();
//	IR_MAN_GPIO_Config();
//	ADCx_Init();
    OLED_Init();
    OLED_Clear();
//  DS1302_Init();
//  Set_Calendar(&calendar);   //校准时间函数
//  max30102_init();
//  Matrix_KEY_GPIO_Config();
//	PWM_Init();
//	while(!DHT11_Init()); 
//  while(DS18B20_GPIO_Config());	//DS18B20初始化
    /*超(0) 声(1) 测(2) 距(3) 系(4) 统(5) 测(6) 量(7) 距(8) 离(9) 毫(10) 米(11)*/
    // 显示超声测距系统
    for(i = 0; i < 6; i++)
    {
        OLED_ShowCHinese(16+i*16,0, i);
    }
    // 显示测量距离
    for(i = 0; i < 4; i++)
    {
        OLED_ShowCHinese(0+i*16,2, i+6);
    }
    OLED_ShowChar(64, 2, ':',16);
    OLED_ShowString(104,2,"MM",16);
   
    while(1)
    {
       if(KEY1 == 0)
       {
            delay_ms(10);
            if(KEY1 == 0)
            {
                uart1_send_buff(TriMeasCmd, 1);
                while(!KEY1);
            }
       }
       
       if(RecFlag)
       {
           LT_Analyze_Distance_Data(USART_RX_BUF, RecIndex, &distance);
           OLED_ShowNum(72, 2, distance,4,16);
           play_distance(distance);
           RecFlag = 0;
           RecIndex = 0;
       }
    }        
 }
