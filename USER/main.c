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

#include "lt_max30102.h"

#define MAX_ERROR_NUM           3
#define PW_NUM                  6
#define ID_NUM                  3
#define MAN_TIME                5     // 单位:S 

enum SYS_STATUS {
    NORMAL,
    NIGHT,
    LOCK
}SYS_STA;


/************************************************
	零涂STM32 毕设开发板
    矩阵键盘：行：B11、B10、B2、B1
              列: B0、A7、A6、A5
    RC522   ：VCC: 	3.3V
              RST: 	B6
              GND: 	GND
              IRQ: 	NC
              MISO:	B14
              MOSI: B15
              SCK:	B13
              SDA:  B5
    BEEP   : VCC:5V    IO:A15    GND:GND
    红外   : VCC:5V    IO:A1    GND:GND
    照明LED: C13  低电平
    ESP32  ：VCC:5V RX:A9 TX:A10 CHECK:A11 GND
    电磁锁 ：VCC：5V   IO:A12  GND
    
************************************************/

// ADC1转换的电压值通过MDA方式传到SRAM
extern __IO uint16_t ADC_ConvertedValue[NOFCHANEL];
extern u8 USART_RX_BUF[USART_REC_LEN];


int main(void)
{		
    int32_t n_heart_rate = 0;
    int32_t n_sp02 = 0;  
//    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB |  RCC_APB2Periph_AFIO, ENABLE);//使能PORTB口时钟、复用引脚PB3、PB4引脚
//    GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);  //关闭JTAG，SWD可以调试的

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
//    OLED_Init();
//    OLED_Clear();
//  Matrix_KEY_GPIO_Config();
//	PWM_Init();
//	while(!DHT11_Init()); 
//  while(DS18B20_GPIO_Config());	//DS18B20初始化

    max30102_init();
    
    while(1)
    {
        max30102_Read_Data(&n_heart_rate, &n_sp02);
        printf("heart:%d, sp02:%d\r\n", n_heart_rate, n_sp02);
    }        
 }

