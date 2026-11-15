#include "sys.h"
#include "usart.h"	  
#define PA 1
#define PB 2
#define PC 3
#define PD 4
#define PE 5
#define PF 6
#define PG 7
#define AIN			1
#define IN_FLOATING	2
#define IPD			3
#define IPU			4
#define Out_OD		5
#define Out_PP		6
#define AF_OD		7
#define AF_PP		8
#define TR_USART1 1
#define TR_USART2 2
#define TR_USART3 3
#define TR_UART4  4
#define TR_UART5  5 	 

#if SYSTEM_SUPPORT_OS
#include "includes.h"				  
#endif
  
#if 1
#pragma import(__use_no_semihosting)             
               
struct __FILE 
{ 
	int handle; 

}; 

FILE __stdout;       
  
void _sys_exit(int x) 
{ 
	x = x; 
} 


#endif 

 
 
#if EN_USART1_RX  
  	
u8 USART_RX_BUF[USART_REC_LEN];     

u16 USART_RX_STA=0;        

void USART1_Putc(unsigned char c)
{
    USART_SendData(USART1, c);
    while(USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET );
}  
void uart_init(u32 bound){
 
  GPIO_InitTypeDef GPIO_InitStructure;
	USART_InitTypeDef USART_InitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;
	 
	RCC_APB2PeriphClockCmd(RCC_APB1Periph_USART3|RCC_APB2Periph_GPIOB, ENABLE);
  

  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10; //PA.9
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;	
  GPIO_Init(GPIOB, &GPIO_InitStructure);
   
 
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
  GPIO_Init(GPIOB, &GPIO_InitStructure);  

  NVIC_InitStructure.NVIC_IRQChannel = USART3_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority=3 ;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 3;		
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;			
	NVIC_Init(&NVIC_InitStructure);
  


	USART_InitStructure.USART_BaudRate = bound;
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = USART_Parity_No;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;	

  USART_Init(USART3, &USART_InitStructure); 
  USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);
  USART_Cmd(USART3, ENABLE);                   

}

void USART1_IRQHandler(void)                
	{
	u8 Res;
#if SYSTEM_SUPPORT_OS 		
	OSIntEnter();    
#endif
	if(USART_GetITStatus(USART1, USART_IT_RXNE) != RESET) 
		{
		Res =USART_ReceiveData(USART1);	
		
		if((USART_RX_STA&0x8000)==0)
			{
			if(USART_RX_STA&0x4000)
				{
				if(Res!=0x0a)USART_RX_STA=0;
				else USART_RX_STA|=0x8000;	
				}
			else 
				{	
				if(Res==0x0d)USART_RX_STA|=0x4000;
				else
					{
					USART_RX_BUF[USART_RX_STA&0X3FFF]=Res ;
					USART_RX_STA++;
					if(USART_RX_STA>(USART_REC_LEN-1))USART_RX_STA=0;
					}		 
				}
			}   		 
     } 
#if SYSTEM_SUPPORT_OS 	
	OSIntExit();  											 
#endif
} 
#endif	


void FDC_GPIO_Init(unsigned char GPIOx,unsigned short GPIOx_x,unsigned char Type)  //?????? "GPIO.h"
{			  
	GPIO_InitTypeDef   GPIO_InitStructure;	  //????
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;		//??????,??50M
  GPIO_InitStructure.GPIO_Pin = GPIOx_x;			//????
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE); //????	
  switch(Type)
	{
	  case 1:
		GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;		//?????????
		break;
		case 2:
		GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;		//?????????
		break;
		case 3:
		GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPD;		//?????????
		break;
		case 4:
		GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;		//?????????
		break;
		case 5:
		GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;		//?????????
		break;
		case 6:
		GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;		//?????????
		break;
		case 7:
		GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_OD;		//???????????
		break;
		case 8:
		GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;		//???????????
		break;
	}
	switch(GPIOx)
	{
		case 1:	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);	 //??GPIOA??
		GPIO_Init(GPIOA, &GPIO_InitStructure);				//?????????
		break;
		case 2:	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);	 //??GPIOB??
		GPIO_Init(GPIOB, &GPIO_InitStructure);				//?????????
		break;
		case 3:	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);	 //??GPIOC??
		GPIO_Init(GPIOC, &GPIO_InitStructure);				//?????????
		break;
		case 4:	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOD, ENABLE);	 //??GPIOD??
		GPIO_Init(GPIOD, &GPIO_InitStructure);				//?????????
		break;
		case 5:	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOE, ENABLE);	 //??GPIOE??
		GPIO_Init(GPIOE, &GPIO_InitStructure);				//?????????
		break;
		case 6:	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOF, ENABLE);	 //??GPIOF??
		GPIO_Init(GPIOF, &GPIO_InitStructure);				//?????????
		break;
		case 7:	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOG, ENABLE);	 //??GPIOG??
		GPIO_Init(GPIOG, &GPIO_InitStructure);				//?????????
		break;
	}
}
void FDC_USART_Init(unsigned long Baud, unsigned char USARTx)
{
	USART_InitTypeDef    USART_InitStructure;
	USART_InitStructure.USART_BaudRate = Baud; //bps,?9600  19200
#ifndef _USART_Use
#define _USART_Use
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;	//8???
	USART_InitStructure.USART_StopBits = USART_StopBits_1;	  //1????
	USART_InitStructure.USART_Parity = USART_Parity_No;	//	?????
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;	   //??????
	USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;	 //??????
#endif
	if(USARTx == 1)
	{
		RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1 , ENABLE);
		FDC_GPIO_Init(PA, GPIO_Pin_9, AF_PP);	//??????  TX
		FDC_GPIO_Init(PA, GPIO_Pin_10, IN_FLOATING);	//  RX
		USART_Init(USART1, &USART_InitStructure);
		USART_ClearITPendingBit(USART1, USART_IT_RXNE); //?????
		USART_Cmd(USART1, ENABLE);
		/* CPU????:?????,????Send,??1????????*/
		USART_ClearFlag(USART1, USART_FLAG_TC);     /* ?Transmission Complete flag */
	}
	if(USARTx == 2)
	{
		RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2 , ENABLE);
		FDC_GPIO_Init(PA, GPIO_Pin_2, AF_PP);	//??????  TX
		FDC_GPIO_Init(PA, GPIO_Pin_3, IN_FLOATING);	//  RX
		// 		FDC_GPIO_Init(PE,GPIO_Pin_3,Out_PP);      //EN485-1????
		USART_Init(USART2, &USART_InitStructure);
		USART_ClearITPendingBit(USART2, USART_IT_RXNE); //?????
		USART_Cmd(USART2, ENABLE);
		/* CPU????:?????,????Send,??1????????*/
		USART_ClearFlag(USART2, USART_FLAG_TC);     /* ?Transmission Complete flag */
	}
	if(USARTx == 3)
	{
		RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3 , ENABLE);
		//GPIO_PinRemapConfig(GPIO_PartialRemap_USART3,ENABLE);  //???????
		FDC_GPIO_Init(PB, GPIO_Pin_10, AF_PP);	//??????  TX
		FDC_GPIO_Init(PB, GPIO_Pin_11, IN_FLOATING);	//  RX   IPU
		//  		FDC_GPIO_Init(PB,GPIO_Pin_1,Out_PP);      //EN485-2????
		USART_Init(USART3, &USART_InitStructure);
		USART_ClearITPendingBit(USART3, USART_IT_RXNE); //?????
		USART_Cmd(USART3, ENABLE);
		/* CPU????:?????,????Send,??1????????*/
		USART_ClearFlag(USART3, USART_FLAG_TC);     /* ?Transmission Complete flag */
	}
	if(USARTx == 4)
	{
		RCC_APB1PeriphClockCmd(RCC_APB1Periph_UART4 , ENABLE);
		//GPIO_PinRemapConfig(GPIO_PartialRemap_UART4,ENABLE);  //???????
		FDC_GPIO_Init(PC, GPIO_Pin_10, AF_PP);	//??????  TX
		FDC_GPIO_Init(PC, GPIO_Pin_11, IN_FLOATING);	//  RX
		USART_Init(UART4, &USART_InitStructure);
		USART_ClearITPendingBit(UART4, USART_IT_RXNE); //?????
		USART_Cmd(UART4, ENABLE);
		/* CPU????:?????,????Send,??1????????*/
		USART_ClearFlag(UART4, USART_FLAG_TC);     /* ?Transmission Complete flag */
	}
	if(USARTx == 5)
	{
		RCC_APB1PeriphClockCmd(RCC_APB1Periph_UART5 , ENABLE);
		//GPIO_PinRemapConfig(GPIO_PartialRemap_UART4,ENABLE);  //???????
		FDC_GPIO_Init(PC, GPIO_Pin_12, AF_PP);	//??????  TX
		FDC_GPIO_Init(PD, GPIO_Pin_2, IN_FLOATING);	//  RX
		USART_Init(UART5, &USART_InitStructure);
		USART_ClearITPendingBit(UART5, USART_IT_RXNE); //?????
		USART_Cmd(UART5, ENABLE);
		/* CPU????:?????,????Send,??1????????*/
		USART_ClearFlag(UART5, USART_FLAG_TC);     /* ?Transmission Complete flag */
	}

}
void Init_USART(void)
{
	unsigned int baund=115200;
	uart2_init(baund);
	FDC_GPIO_Init(PE,GPIO_Pin_15,Out_PP ); 
	switch(baund)
	{
	case 115200:
		FDC_USART_Init(115200,TR_USART1);   //?????3
		break;
	case 9600:
		FDC_USART_Init(9600,TR_USART1);   //?????3
		break;
	case 4800:
		FDC_USART_Init(4800,TR_USART1);   //?????3
		break;
	case 2400:
		FDC_USART_Init(2400,TR_USART1);   //?????3
		break;
	case 1200:
		FDC_USART_Init(1200,TR_USART1);   //?????3
		break;
	default:
		FDC_USART_Init(9600,TR_USART1);  //?????3
		break;
	}

}








#if EN_USART1_RX   //如果使能了接收
//串口1中断服务程序
//注意,读取USARTx->SR能避免莫名其妙的错误
u8 USART_RX_BUF[USART_REC_LEN];     //接收缓冲,最大USART_REC_LEN个字节.
//接收状态
//bit15，	接收完成标志
//bit14，	接收到0x0d
//bit13~0，	接收到的有效字节数目

uint16_t RecIndex = 0;
uint8_t RecFlag  = 0;


/**
* USART2发送len个字节.
* buf:发送区首地址
* len:发送的字节数(为了和本代码的接收匹配,这里建议不要超过64个字节)
**/
void uart1_send_buff(u8 *buf,u16 len)
{
    u16 t;
    for(t=0; t<len; t++)      //循环发送数据
    {
        while(USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET);
        USART_SendData(USART1,buf[t]);
    }
    while(USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET);
}
void uart2_init(u32 bound)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;

    /* GPIOA 在 APB2 总线，USART2 在 APB1 总线。 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);

    /* 先填入默认值，避免结构体字段未初始化。 */
    GPIO_StructInit(&GPIO_InitStructure);
    USART_StructInit(&USART_InitStructure);

    /* PA2：发送引脚，由 USART 外设控制输出。 */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* PA3：接收引脚。 */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* 串口参数：指定波特率，8 位数据，1 位停止，无校验。 */
    USART_InitStructure.USART_BaudRate = bound;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl =   USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;

    USART_Init(USART2, &USART_InitStructure);

    /* 先验证发送，不开启接收中断，无需配置 NVIC。 */
    USART_ITConfig(USART2, USART_IT_RXNE, DISABLE);
    USART_Cmd(USART2, ENABLE);
}

/**
* USART2发送len个字节.
* buf:发送区首地址
* len:发送的字节数(为了和本代码的接收匹配,这里建议不要超过64个字节)
**/
void uart2_send_buff(u8 *buf,u16 len)
{
    u16 t;
      for(t=0;t<len;t++)        //循环发送数据
    {
        while(USART_GetFlagStatus(USART2, USART_FLAG_TC) == RESET);
        USART_SendData(USART2,buf[t]);
    }
    while(USART_GetFlagStatus(USART2, USART_FLAG_TC) == RESET);
}



//void USART1_IRQHandler(void)                	//串口1中断服务程序
//{
//    u8 Res;
//#if SYSTEM_SUPPORT_OS 		//如果SYSTEM_SUPPORT_OS为真，则需要支持OS.
//    OSIntEnter();
//#endif

//    if(USART_GetITStatus(USART1, USART_IT_RXNE) != RESET)  //接收中断(接收到的数据必须是0x0d 0x0a结尾)
//    {
//        // 打开为定时器方式判断结束
//        Res =USART_ReceiveData(USART1);	//读取接收到的数据
//        USART_RX_BUF[RecIndex] = Res;
//        RecIndex++;
//        TIM3->CNT=0; //重新计数
//        if(RecIndex >= USART_REC_LEN)
//        {
//            RecIndex = 0;
//        }
//    }
//#if SYSTEM_SUPPORT_OS 	//如果SYSTEM_SUPPORT_OS为真，则需要支持OS.
//    OSIntExit();
//#endif
//}
#endif

