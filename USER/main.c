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
#include "lt_uln2003.h"
#include "lt_fdc2214_iic.h"
#include "lt_fdc2214.h"

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
                SCL: B10
                SDA: B11
    ESP8266：
                RX:  A9
                TX:  A10
                CHECH_IO: A12
                VCC: 3.3V
                GND:GND
    MQ7：
                VCC:   5V
                GND:   GND
                DO:    NC
                AO:    A5
    MQ2：
                VCC:   5V
                GND:   GND
                DO:    NC
                AO:    A4
    MQ135：
                VCC:   5V
                GND:   GND
                DO:    NC
                AO:    A6
    蜂鸣器模块：
                VCC:   3.3V
                IO:    B15
                GND:   GND
    按键模块：  B7  B8  B9  低电平触发
    
    DS1302模块：
                VCC:   3.3V
                GND:    GND
                SCLK:   B12
                DATA:   B13
                RST:    B14
    
************************************************/

// ADC1转换的电压值通过MDA方式传到SRAM
extern __IO uint16_t ADC_ConvertedValue[NOFCHANEL];
//extern u8 USART_RX_BUF[USART_REC_LEN];

typedef enum {
	IDLE,
	SET_H,
	SET_M,
    SET_S,
}KEY_STA_;
KEY_STA_ KEY_STA =  IDLE;
int set_h = 0;
int set_m = 0;
int set_s = 0;
u8  hour = 0;
u8  min = 0;
u8  sec = 0;
CALENDAR calendar={0x2022,0x03,0x10,0x16,0x16,0x00,0x00};

void handle_key_event(void)
{
	if(KEY3 == 0)
	{
		delay_ms(10);
		if(KEY3 == 0)
		{
			switch(KEY_STA)
			{
				case SET_H:
				{
					set_h++;
					if(set_h == 24)
						set_h = 0;
				}
				break;
				case SET_M:
				{
					set_m++;
					if(set_m == 60)
						set_m = 0;
				}
				break;
                case SET_S:
				{
					set_s++;
					if(set_s == 60)
						set_s = 0;
				}
				break;
				defalut:
				break;
			}
		}
		//while(!KEY0);
	}
	if(KEY2 == 0)
	{
		delay_ms(10);
		if(KEY2 == 0)
		{
			switch(KEY_STA)
			{
				case IDLE:
				{
                    set_h = hour;
                    set_m = min;
                    set_s = sec;
					KEY_STA = SET_H;
					OLED_ShowCHinese(112,  4, 14);
				}
				break;
				case SET_H:
				{
					KEY_STA = SET_M;
					OLED_ShowCHinese(112,  4, 15);
				}
				break;
				case SET_M:
				{
					KEY_STA = SET_S;
					OLED_ShowCHinese(112,  4, 16);
				}
				break;
                case SET_S:
				{
					KEY_STA = IDLE;
                    calendar.hour = DEC2BCD(set_h);
                    calendar.minute = DEC2BCD(set_m);
                    calendar.second = DEC2BCD(set_s);
                    Set_Calendar(&calendar);   //校准时间函数
					OLED_ShowCHinese(112,  4, 17);
				}
				break;
			}
		}
		while(!KEY2);
	}
	if(KEY1 == 0)
	{
		delay_ms(10);
		if(KEY1 == 0)
		{
			switch(KEY_STA)
			{
				case SET_H:
				{
					set_h--;
					if(set_h < 0)
						set_h = 23;
				}
				break;
				case SET_M:
				{
					set_m--;
					if(set_m < 0)
						set_m = 59;
				}
				break;
                case SET_S:
				{
					set_s--;
					if(set_s < 0)
						set_s = 0;
				}
				break;
				default:
				break;
			}
		}
		//while(!KEY2);
	}
    
    if(KEY_STA != IDLE)
    {
        OLED_ShowNum(40, 6, set_h, 2, 16);
        OLED_ShowNum(64, 6, set_m, 2, 16);
        OLED_ShowNum(88, 6, set_s, 2, 16);
    }
}

int main(void)
{	   
    double cap[4] = {0};
    int i = 0;

//    u8 USART_RX_BUF[15] = {0x12, 0x14, 0xAA, 0xA5, 0x09, 0x01, 0x01, 0x34, 0x01, 0x69, 0x01, 0x29, 0xD3, 0x55, 0x12};
//    uint16_t RecIndex = 15;
    
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB |  RCC_APB2Periph_AFIO, ENABLE);//使能PORTB口时钟、复用引脚PB3、PB4引脚
    GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);  //关闭JTAG，SWD可以调试的

	delay_init();	    	 //延时函数初始化	  
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2); //设置NVIC中断分组2:2位抢占优先级，2位响应优先级
	uart_init(115200);	 //串口初始化为115200
//  uart2_init(9600);
    
//  RC522_GPIO_Config();
//	LED_GPIO_Config();
//	KEY_GPIO_Config();
//	BEEP_GPIO_Config();
//  ULN2003_GPIO_Config();
   
    
//	RELAY_GPIO_Config();
//	IR_SW_GPIO_Config();
//	IR_MAN_GPIO_Config();
//	ADCx_Init();
//    OLED_Init();
//    OLED_Clear();
//    DS1302_Init();
    FDC2214_IIC_Init();
    while(FDC2214_Init());
//  Set_Calendar(&calendar);   //校准时间函数
//  max30102_init();
//  Matrix_KEY_GPIO_Config();
//	PWM_Init();
//	while(!DHT11_Init()); 
//  while(DS18B20_GPIO_Config());	//DS18B20初始化

     /* 危(0) 险(1) 气(2) 体(3) 系(4) 统(5) 可(6) 燃(7) 烟(8) 
        雾(9) 设(10) 置(11) 时(12) 间(13) 时(14) 分(15) 秒(16) 无(17)*/
    /* 第一行危险气体系统 */
//    OLED_ShowCHinese(16, 0, 0);
//    OLED_ShowCHinese(32, 0, 1);
//    OLED_ShowCHinese(48, 0, 2);
//    OLED_ShowCHinese(64, 0, 3);
//    OLED_ShowCHinese(80, 0, 4);
//    OLED_ShowCHinese(96, 0, 5);
//    
//    /* 第二行显示CO：   可燃：  */
//    OLED_ShowChar(0,  2,'C', 16);
//    OLED_ShowChar(16, 2,'O',16);
//    OLED_ShowChar(32, 2, ':', 16);
//    
//    OLED_ShowCHinese(64, 2, 6);
//    OLED_ShowCHinese(80, 2, 7);
//    OLED_ShowChar(96, 2, ':', 16);
//    
//    
//    /* 第二行显示烟雾：   设置：  */
//    OLED_ShowCHinese(0,  4, 8);
//    OLED_ShowCHinese(16, 4, 9);
//    OLED_ShowChar(32, 4, ':', 16);
//    
//    OLED_ShowCHinese(64, 4, 10);
//    OLED_ShowCHinese(80, 4, 11);
//    OLED_ShowChar(96, 4, ':', 16);
//    OLED_ShowCHinese(112,  4, 17);
//    
//    /* 第三行时间: */
//    OLED_ShowCHinese(0,  6, 12);
//    OLED_ShowCHinese(16, 6, 13);
//    OLED_ShowChar(32, 6, ':', 16);
//    OLED_ShowChar(56, 6, ':', 16);
//    OLED_ShowChar(80, 6, ':', 16);
    
//    OLED_ShowCHinese(64,  6, 12);
//    OLED_ShowCHinese(80, 6, 13);
//    OLED_ShowChar(96, 6, ':', 16); 
//    OLED_ShowCHinese(112,  6, 14);
//    while(ESP);
//    Set_Calendar(&calendar);   //校准时间函数
    while(1)
    {
        for(i = 0; i < 4; i++)
        {
            cap[i]=getcap(i,18,33);//采集数据
            printf("cap[%d]=%lfpf,", i, cap[i]);
        }
        printf("\r\n");
        delay_ms(500);
    }
    #if 0
    while(1)
    {
        ADC_ConvertedValueLocal[0] =(float) ADC_ConvertedValue[0]/4096*3.3;
        ADC_ConvertedValueLocal[1] =(float) ADC_ConvertedValue[1]/4096*3.3;
        ADC_ConvertedValueLocal[2] =(float) ADC_ConvertedValue[2]/4096*3.3;
        MQ2   = ADC_ConvertedValueLocal[0]*30;
        MQ7   = ADC_ConvertedValueLocal[1]*30;
        MQ135 = ADC_ConvertedValueLocal[2]*30;
        
        handle_key_event();
        
        if(KEY_STA == IDLE)
        {
            Get_Calendar(&calendar);
            hour = BCD2DEC(calendar.hour);
            min  = BCD2DEC(calendar.minute);
            sec = BCD2DEC(calendar.second);
            if((min == 0) && (sec >= 0) && (sec <= 3))
            {
                BEEP_ON;
            }
            else
            {
                BEEP_OFF;
            }
            OLED_ShowNum(40, 6, hour, 2, 16);
            OLED_ShowNum(64, 6, min, 2, 16);
            OLED_ShowNum(88, 6, sec, 2, 16);
        }
        
        if(count%100 == 0)
        {
            OLED_ShowNum(48,2,MQ7,2,16);
            OLED_ShowNum(112,2,MQ2,2,16);
            OLED_ShowNum(48,4,MQ135,2,16);
            printf("{\"MQ7\":%d,\"MQ2\":%d,\"MQ135\":%d}\r\n", MQ7, MQ2, MQ135);
        }
        
        delay_ms(10);
        count++;
    }
    #endif    
 }
