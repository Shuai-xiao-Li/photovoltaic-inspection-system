/**
 * @file board_config.h
 * @brief 系统核心全局配置文件，统一定义小车线速度/角速度限幅、采样控制周期、LCD刷新频率等关键物理与控制参数。
 * @author 李帅 赵禹博 吴坨鑫
 * @date 6月12号
 * @note 本程序为研电赛沙漠光伏板检测机器人系统的一部分。
 */

#ifndef __BOARD_CONFIG_H
#define __BOARD_CONFIG_H

/*
 * board_config.h
 *
 * Central configuration file for the STM32F103ZET6 tracked chassis lower
 * controller.  Keep all pin choices, mechanical parameters, protection limits,
 * and peripheral handles here.  When the wiring changes, edit this file first.
 *
 * Important LCD version change:
 * - The original motor-only framework used PB0/PB1 as TIM3 PWM outputs.
 * - The ALIENTEK TFTLCD uses PB0 as LCD backlight control and PB1 as touch SCK.
 * - This LCD version moves motor PWM to PA6/PA7, which are TIM3_CH1/TIM3_CH2.
 */

#include "main.h"
#include "chassis_bsp.h"
#include "usart.h"
#include <stdint.h>

/* ========================= control loop timing ========================= */

/* TIM6 interrupt period.  The closed-loop speed controller runs once every
 * 10 ms.  Do not call LCD drawing functions inside this interrupt; LCD update
 * is much slower and must stay in the main loop.
 */
#define CONTROL_PERIOD_MS              10U
#define CONTROL_PERIOD_S               0.01f

/* ============================= motor PWM ============================== */

/* TIM3 PWM period.  With 72 MHz timer clock and Period=7199, Prescaler=0:
 * PWM frequency = 72 MHz / 7200 = 10 kHz.
 */
#define MOTOR_PWM_PERIOD               7199

/* Software safety limit for bench testing.
 *
 * The timer period is 7199, but the first chassis tests should NOT use full
 * duty.  3800 is about 53% duty and leaves margin for a small MG513 motor and
 * D153B/TB6612 driver while the track is off the ground or the robot is on a
 * test stand.  Raise it only after current, temperature, direction and encoder
 * signs are confirmed.
 */
#define MOTOR_PWM_LIMIT                3800

/* Minimum effective PWM and initial speed-loop feed-forward.
 *
 * Too small a PWM may only make the motor buzz or stall.  Stalling is bad for
 * both the motor and driver because current can rise while the track does not
 * move.  A non-zero open-loop PWM command is lifted to this value; in closed
 * loop, this value is added as the initial per-side drive and the PI output
 * corrects around it.  Set to 0 only when intentionally testing the dead zone.
 */
#define MOTOR_LEFT_FF_DEFAULT          1800
#define MOTOR_RIGHT_FF_DEFAULT         1800
#define MOTOR_PWM_MIN_EFFECTIVE        1800

/* Static-friction start boost for ground starts.
 *
 * Field test result:
 *   L_FF=2100 / R_FF=2000 starts and runs, but PI correction stays around
 *   -600, so the steady running PWM is closer to 1500 / 1400.  Keep the
 *   steady FF lower and only use this boost briefly when leaving zero speed.
 */
#define MOTOR_START_BOOST_MS           0U
#define MOTOR_LEFT_START_BOOST_PWM     0
#define MOTOR_RIGHT_START_BOOST_PWM    0
#define MOTOR_START_BOOST_TICKS        (MOTOR_START_BOOST_MS / CONTROL_PERIOD_MS)
#define MOTOR_START_BOOST_RAMP_STEP    300

/* PWM ramp rate.  A command step such as 0 -> 2500 is not applied immediately;
 * it changes by this amount every 10 ms.  120 means roughly 0.2 s from 0 to 2400,
 * which gives a soft start and reduces current spikes.
 */
#define MOTOR_PWM_RAMP_STEP            120

/* Optional deadband at the final motor-driver layer.  Keep 0 because the safer
 * minimum-start rule is handled in chassis.c where the mode is known.
 */
#define MOTOR_PWM_DEADBAND             0

/* Default speed-loop PI after raised and ground bring-up tests. */
#define SPEED_PID_DEFAULT_KP           60.0f
#define SPEED_PID_DEFAULT_KI           3.0f

/* Azimuth (Turn) Loop PID Defaults
 * These tune the outer MPU6050 heading loop. 
 */
#define TURN_PID_DEFAULT_KP            0.8f
#define TURN_PID_DEFAULT_KI            0.05f
#define TURN_PID_DEFAULT_KD            0.2f

/* LCD-safe PWM pins:
 *   PA6 -> TIM3_CH1 -> D153B PWMA -> left track PWM
 *   PA7 -> TIM3_CH2 -> D153B PWMB -> right track PWM
 */
#define MOTOR_LEFT_PWM_TIM             (&htim3)
#define MOTOR_LEFT_PWM_CHANNEL         TIM_CHANNEL_1
#define MOTOR_RIGHT_PWM_TIM            (&htim3)
#define MOTOR_RIGHT_PWM_CHANNEL        TIM_CHANNEL_2

/* ============================ motor GPIO ============================== */

/* D153B/TB6612 direction pins.  These pins were selected because they do not
 * conflict with the TFTLCD FSMC interface or the LCD backlight pin.
 */
#define MOTOR_LEFT_IN1_PORT            GPIOE
#define MOTOR_LEFT_IN1_PIN             GPIO_PIN_0
#define MOTOR_LEFT_IN2_PORT            GPIOE
#define MOTOR_LEFT_IN2_PIN             GPIO_PIN_1
#define MOTOR_RIGHT_IN1_PORT           GPIOE
#define MOTOR_RIGHT_IN1_PIN            GPIO_PIN_2
#define MOTOR_RIGHT_IN2_PORT           GPIOE
#define MOTOR_RIGHT_IN2_PIN            GPIO_PIN_6

/* Direction correction switches.
 * If a positive command makes one track move backward, change the matching
 * value from 0 to 1 instead of rewiring immediately.
 */
#define MOTOR_LEFT_INVERT              1
#define MOTOR_RIGHT_INVERT             0

/* ========================== encoder timers ============================ */

/* Encoder wiring:
 *   left  motor encoder E1A/E1B -> PC6/PC7 -> TIM8_CH1/TIM8_CH2
 *   right motor encoder E2A/E2B -> PA0/PA1 -> TIM2_CH1/TIM2_CH2
 *
 * The D153B STM32F103C8 demo uses PB6/PB7 for E1A/E1B, but the ALIENTEK
 * Elite board connects those pins to its 24C02 EEPROM.  PC6/PC7 are free when
 * the camera connector is unused, so this project deliberately wires E1 here.
 * PA1 is shared with STM_ADC/TPAD on the Elite board; remove jumper P7 before
 * connecting E2B to PA1.
 */
#define ENCODER_LEFT_TIM               (&htim8)
#define ENCODER_RIGHT_TIM              (&htim2)

/* Encoder sign correction.  During a forward motion test, both encoder deltas
 * should be positive.  If one side is negative, change its sign to -1.
 */
#define ENCODER_LEFT_SIGN             -1
#define ENCODER_RIGHT_SIGN             1

/* ======================= tracked chassis geometry ===================== */

/* Track center distance.  221 mm is inferred from your chassis drawing and
 * should be replaced by your real measured center-to-center distance.
 */
#define TRACK_WIDTH_MM                 221.0f

/* Effective drive sprocket diameter measured by you.  For a tracked chassis,
 * this is the effective diameter that converts motor output rotation to track
 * linear travel.  It can be slightly different from the visible wheel diameter.
 */
#define DRIVE_SPROCKET_DIAMETER_MM     45.0f
#define PI_F                           3.1415926f

/* MG513 encoder and gearbox parameters from your motor information.
 * ENCODER_MULTIPLE is set to 4 for STM32 timer encoder x4 mode.  Verify it by
 * rotating the output shaft once and reading the accumulated encoder count.
 */
#define ENCODER_PPR_MOTOR              13.0f
#define GEAR_RATIO                     30.0f
#define ENCODER_MULTIPLE               4.0f
#define ENCODER_CPR_OUTPUT             (ENCODER_PPR_MOTOR * GEAR_RATIO * ENCODER_MULTIPLE)
#define SPROCKET_CIRCUMFERENCE_MM      (PI_F * DRIVE_SPROCKET_DIAMETER_MM)
#define MM_PER_ENCODER_COUNT           (SPROCKET_CIRCUMFERENCE_MM / ENCODER_CPR_OUTPUT)

/* =========================== safety limits ============================ */

/* Speed command limits for early bench tests.
 *
 * These are intentionally conservative.  You can still tune speed through
 * serial commands, but values outside this range are clamped before they reach
 * PID or PWM.  Start with the chassis lifted, then test on the ground.
 */
#define CHASSIS_MAX_COUNT_PER_10MS     25
#define CHASSIS_MIN_COUNT_PER_10MS     3
#define CHASSIS_COUNT_RAMP_STEP        1

#define CHASSIS_MAX_V_MM_S             180.0f
#define CHASSIS_MAX_W_MRAD_S           800.0f

/* Ready-made safe test speeds.  The protocol command `test low/mid/high/turn`
 * uses these values so you can test without guessing numbers.
 */
#define TEST_SPEED_LOW_MM_S            60.0f
#define TEST_SPEED_MID_MM_S            100.0f
#define TEST_SPEED_HIGH_MM_S           150.0f
#define TEST_TURN_MRAD_S               350.0f

/* Key-panel first motor tests now use OPEN-LOOP PWM instead of closed-loop
 * velocity.  This is intentional: before encoder direction and PID are
 * verified, a tiny PI output may be too small to start the geared motor.
 * These PWM values are conservative and still pass through the software ramp.
 */
#define KEY_TEST_PWM_LOW               2000
#define KEY_TEST_PWM_MID               2800
#define KEY_TEST_PWM_TURN              2400

/* 0 disables command timeout during bench testing.  After the Phytium Pi
 * upper computer is connected, set this to e.g. 12000 ms so the chassis stops
 * automatically if communication is lost.
 */
#define CHASSIS_CMD_TIMEOUT_MS         12000U

/* D153B voltage sample is divided by 11 before ADC. */
#define BATTERY_ADC_DIV_RATIO          11.0f
#define BATTERY_LOW_VOLTAGE            9.5f
#define BATTERY_ENABLE_PROTECTION      0U

/* ===================== D153B power-backfeed protection =====================
 *
 * The D153B must NOT receive control high-levels while its motor power is off.
 * Otherwise STM32 GPIOs can back-feed the driver through PWMA/PWMB/AIN/BIN and
 * make the D153B power LED glow dimly.
 *
 * Therefore this version uses the D153B ADC output on PA5 to judge whether the
 * driver board is powered.  Wire:
 *
 *      D153B ADC  -> STM32 PA5
 *      D153B GND  -> STM32 GND
 *
 * If this check is enabled and PA5 does not see enough voltage, all motor
 * control pins are changed to analog/high-impedance and TEST commands are
 * refused.  If you temporarily do not connect ADC, set the enable macro to 0,
 * but then you lose the back-feed protection.
 */
#define DRIVER_POWER_CHECK_ENABLE       1U
#define DRIVER_POWER_ON_VOLTAGE         4.5f
#define DRIVER_POWER_OFF_VOLTAGE        3.0f

/* ============================ peripherals ============================= */

#define BATTERY_ADC_HANDLE             (&hadc1)
#define DEBUG_UART_HANDLE              (&UART1_Handler)

/* =============================== LCD UI =============================== */

/* 2.8-inch ALIENTEK TFTLCD is normally 240x320.  The UI uses landscape mode
 * to get a 320x240 dashboard.  Touch is not used.
 *
 * Refresh is intentionally slow and value strings are cached in display_ui.c.
 * This greatly reduces visible flicker on the 2.8-inch parallel TFTLCD.
 */
#define LCD_UI_ENABLE                  1U
#define LCD_UI_REFRESH_MS              300U
#define LCD_UI_AUTO_PAGE_MS            0U

#endif
