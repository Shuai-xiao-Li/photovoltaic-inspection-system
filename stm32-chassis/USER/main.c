/**
 * @file main.c
 * @brief 主程序入口，负责系统外设配置、定时器使能、任务时间片划分与核心控制环调度。
 * @author 李帅 赵禹博 吴坨鑫
 * @date 6月12号
 * @note 本程序为研电赛沙漠光伏板检测机器人系统的一部分。
 */

#include "sys.h"
#include "delay.h"
#include "usart.h"
#include "led.h"
#include "lcd.h"

#include "chassis_bsp.h"
#include "chassis.h"
#include "debug_protocol.h"
#include "display_ui.h"
#include "coordinated_task.h"
#include "key_control.h"
#include "ioi2c.h"
#include "mpu6050.h"
#include "servo.h"

/*
 * USER/main.c
 *
 * 本工程是在正点原子“实验13 TFTLCD显示实验”的工程体系下直接改出来的。
 * 也就是说：LCD、FSMC、delay、usart、sys 等底层文件仍然沿用正点原子原例程，
 * 我只是在此基础上新增了 BSP/ 和 CHASSIS/ 两个目录，用于履带底盘下位机控制。
 *
 * 重要设计原则：
 * 1. TIM6 每 10ms 进入一次中断，只做底盘闭环控制，不在中断里刷屏。
 * 2. LCD 显示放在 while(1) 中低频刷新，避免影响电机控制实时性。
 * 3. 电机 PWM 使用 PA6/PA7，而不是 PB0/PB1，因为 PB0 是 LCD 背光脚，PB1 属于触摸相关脚。
 * 4. 当前版本不使用触摸。优先使用 KEY0/KEY1 做本地安全测试，串口仍保留为可选调试接口。
 */

int main(void)
{
    HAL_Init();                         /* HAL库初始化，配置SysTick等基础功能 */
    Stm32_Clock_Init(RCC_PLL_MUL9);      /* 正点原子常用配置：外部8MHz晶振倍频到72MHz */
    delay_init(72);                      /* delay模块初始化，参数对应72MHz系统时钟 */

    uart_init(115200);                   /* USART1调试串口：PA9_TX / PA10_RX，115200-8-N-1 */
    LED_Init();                          /* 保留正点原子LED初始化，方便后续扩展运行指示 */

    /* 初始化底盘专用外设：
     * - PE0/PE1/PE2/PE6：TB6612方向控制
     * - PA6/PA7：TIM3_CH1/CH2 PWM输出
     * - PC6/PC7：TIM8编码器输入（避开精英板 PB6/PB7 上的 24C02）
     * - PA0/PA1：TIM2编码器输入（右路 E2B 使用 PA1 前需拔掉精英板 P7）
     * - PA5：ADC1_IN5电池电压采样
     * - TIM6：10ms控制周期中断
     */
    ChassisBSP_Init();

    DisplayUI_Init();                    /* 优先初始化2.8寸TFTLCD，使其通电即亮 */
    POINT_COLOR = WHITE;
    BACK_COLOR = BLACK;
    LCD_ShowString(4, 100, 312, 16, 16, (uint8_t *)"MPU6050 DMP Initializing...");
    LCD_ShowString(4, 120, 312, 16, 16, (uint8_t *)"Please keep the vehicle still.");

    IIC_Init();                          /* 初始化软件I2C (PB10/PB11) */
    MPU6050_initialize();                /* 初始化陀螺仪 */
    DMP_Init();                          /* 初始化姿态解算(DMP)，成功后可读Yaw角 */

    Servo_Init();                        /* 初始化舵机PWM */
    Chassis_Init();                      /* 初始化电机、编码器、PID和底盘状态机 */
    KeyControl_Init();                   /* 初始化KEY0/KEY1本地测试控制，不使用KEY_UP */
    DebugProtocol_Init();                /* 串口命令解析模块保留为可选调试接口 */

    HAL_TIM_Base_Start_IT(&htim6);        /* 启动TIM6中断，之后每10ms执行一次闭环控制 */

    while (1)
    {
        Servo_ControlLoop();              /* 更新舵机PWM输出及限位检查 */
        // CoordinatedTask_Run();            /* 控制权移交给飞腾派 */
        KeyControl_Task();                /* KEY0选择测试项目，KEY1执行；长按KEY1急停 */
        DebugProtocol_Task();             /* 串口仍可选用，不接串口也不影响按键测试 */
        DisplayUI_Task();                 /* 低频、缓存式刷新LCD，减少屏幕闪烁 */
    }
}

/*
 * HAL定时器周期中断回调。
 * TIM6_IRQHandler 在 BSP/chassis_bsp.c 中，HAL_TIM_IRQHandler 会进一步调用本函数。
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM6)
    {
        Chassis_ControlLoop10ms();        /* 底盘10ms控制核心：编码器读取 + 安全检查 + PID + PWM输出 */
    }
}

