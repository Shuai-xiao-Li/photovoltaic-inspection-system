/**
 * @file usart.c
 * @brief 串口底层驱动，实现中断驱动的异步串口接收发送机制。
 * @author 李帅 赵禹博 吴坨鑫
 * @date 6月12号
 * @note 本程序为研电赛沙漠光伏板检测机器人系统的一部分。
 */

#include "sys.h"

#include "usart.h"	



#if SYSTEM_SUPPORT_OS

#include "includes.h"					// 濡傛灉浣跨敤ucos, 鍒欏寘鍚鍏跺ご鏂囦欢

#endif



// 閲嶅畾鍚慺putc 鍒 USART1

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

int fputc(int ch, FILE *f)

{      

	while((USART1->SR&0X40)==0); // 绛夊緟鍙戦佺粨鏉

    USART1->DR = (u8) ch;      

	return ch;

}

#endif 



#if EN_USART1_RX   // 濡傛灉浣胯兘浜嗘帴鏀

u8 USART_RX_BUF[USART_REC_LEN];     // 鍏煎逛繚鐣

u16 USART_RX_STA=0;       // 鍏煎逛繚鐣



u8 aRxBuffer[RXBUFFERSIZE]; // HAL搴撲娇鐢ㄧ殑涓插彛鎺ユ敹缂撳啿

UART_HandleTypeDef UART1_Handler; // UART鍙ユ焺



// --- 鐜褰㈢紦鍐插尯瀹炵幇 ---

#define RING_BUF_SIZE 2048

static u8 ring_buf[RING_BUF_SIZE];

static volatile u16 ring_head = 0;

static volatile u16 ring_tail = 0;



static void ring_push(u8 val)

{

    u16 next = (ring_head + 1) % RING_BUF_SIZE;

    if (next != ring_tail)

    {

        ring_buf[ring_head] = val;

        ring_head = next;

    }

}



static u8 ring_pop(u8 *val)

{

    if (ring_head == ring_tail) return 0;

    *val = ring_buf[ring_tail];

    ring_tail = (ring_tail + 1) % RING_BUF_SIZE;

    return 1;

}



u8 USART_Get_Char(u8 *ch)

{

    return ring_pop(ch);

}

  

void uart_init(u32 bound)

{	

	UART1_Handler.Instance=USART1;					    // USART1

	UART1_Handler.Init.BaudRate=bound;				    // 娉㈢壒鐜

	UART1_Handler.Init.WordLength=UART_WORDLENGTH_8B;   // 8浣嶆暟鎹鏍煎紡

	UART1_Handler.Init.StopBits=UART_STOPBITS_1;	    // 涓涓鍋滄浣

	UART1_Handler.Init.Parity=UART_PARITY_NONE;		    // 鏃犲囧伓鏍￠獙

	UART1_Handler.Init.HwFlowCtl=UART_HWCONTROL_NONE;   // 鏃犵‖浠舵祦鎺

	UART1_Handler.Init.Mode=UART_MODE_TX_RX;		    // 鏀跺彂妯″紡

	HAL_UART_Init(&UART1_Handler);					    // 鍒濆嬪寲涓插彛1

	

	__HAL_UART_ENABLE_IT(&UART1_Handler, UART_IT_RXNE); // 鐩存帴浣胯兘鎺ユ敹涓鏂

}



void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
	GPIO_InitTypeDef GPIO_Initure;
	
	if(huart->Instance==USART1)
	{
		__HAL_RCC_GPIOA_CLK_ENABLE();			// 使能GPIOA时钟
		__HAL_RCC_USART1_CLK_ENABLE();			// 使能USART1时钟
		__HAL_RCC_AFIO_CLK_ENABLE();
	
		GPIO_Initure.Pin=GPIO_PIN_9;			// PA9 (TX)
		GPIO_Initure.Mode=GPIO_MODE_AF_PP;		// 复用推挽输出
		GPIO_Initure.Pull=GPIO_PULLUP;
		GPIO_Initure.Speed=GPIO_SPEED_FREQ_HIGH;
		HAL_GPIO_Init(GPIOA,&GPIO_Initure);

		GPIO_Initure.Pin=GPIO_PIN_10;			// PA10 (RX)
		GPIO_Initure.Mode=GPIO_MODE_AF_INPUT;	// 复用输入
		HAL_GPIO_Init(GPIOA,&GPIO_Initure);
		
#if EN_USART1_RX
		HAL_NVIC_EnableIRQ(USART1_IRQn);		// 使能USART1中断
		HAL_NVIC_SetPriority(USART1_IRQn,0,0);	// 优先级设置 最高，防止MPU6050阻塞丢字
#endif	
	}
}

// 兼容占位
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    (void)huart;
}

void USART1_IRQHandler(void)                	

{ 

	u8 Res;

#if SYSTEM_SUPPORT_OS

	OSIntEnter();    

#endif

	if((__HAL_UART_GET_FLAG(&UART1_Handler,UART_FLAG_RXNE)!=RESET))  // 鎺ユ敹涓鏂

	{

		Res=USART1->DR; 

		ring_push(Res); // 灏嗘帴鏀跺埌鐨勫瓧绗︾洿鎺ュ炲叆鐜褰㈢紦鍐插尯锛屼笉闃诲炪佷笉涓㈠純

	}

	HAL_UART_IRQHandler(&UART1_Handler); // 浠嶆棫璋冪敤HAL涓鏂鍏鐢ㄥ嚱鏁板勭悊TX鍜岄敊璇娓呴櫎

#if SYSTEM_SUPPORT_OS

	OSIntExit();  											 

#endif

} 

#endif

