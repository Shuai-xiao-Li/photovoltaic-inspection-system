/**
 * @file chassis_bsp.h
 * @brief 底盘板级物理驱动头文件，定义引脚宏与初始化声明。
 * @author 李帅 赵禹博 吴坨鑫
 * @date 6月12号
 * @note 本程序为研电赛沙漠光伏板检测机器人系统的一部分。
 */

#ifndef __CHASSIS_BSP_H
#define __CHASSIS_BSP_H

#include "main.h"

/*
 * chassis_bsp.h
 *
 * This file supplies the peripheral handles and initialization functions that
 * are missing in the ALIENTEK TFTLCD example, because the ALIENTEK examples do
 * not use the CubeMX-style tim.c/adc.c/usart.c files.
 *
 * Add chassis_bsp.c to Keil and call ChassisBSP_Init() after delay_init(72).
 */

extern TIM_HandleTypeDef htim2;   /* right encoder: PA0/PA1 */
extern TIM_HandleTypeDef htim3;   /* motor PWM: PA6/PA7 */
extern TIM_HandleTypeDef htim6;   /* 10 ms control interrupt */
extern TIM_HandleTypeDef htim8;   /* left encoder: PC6/PC7 */
extern ADC_HandleTypeDef hadc1;   /* battery voltage: PA5/ADC1_IN5 */
extern TIM_HandleTypeDef htim1;   /* servo 1 PWM: PA8 (TIM1_CH1) */
extern TIM_HandleTypeDef htim4;   /* servo 2 PWM: PB9 (TIM4_CH4) */

void ChassisBSP_Init(void);

#endif
