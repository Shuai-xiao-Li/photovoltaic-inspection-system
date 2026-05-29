#include "motor_driver.h"
#include "board_config.h"

/*
 * motor_driver.c - D153B/TB6612 safe output layer
 *
 * Why this file is stricter than a normal motor driver:
 * If the STM32 is powered but the D153B is not powered, any STM32 GPIO high
 * level on PWMA/PWMB/AIN/BIN can back-feed the D153B through the driver input
 * protection network.  The visible symptom is: the D153B power LED glows dimly
 * even though the D153B power switch is OFF.
 *
 * To stop this at the software level, we support two output states:
 *   1. Enabled state: PA6/PA7 are TIM3 PWM outputs and PE0/PE1/PE2/PE6 are
 *      push-pull direction outputs.  This is used only when D153B power is ON.
 *   2. Hi-Z state: PA6/PA7/PE0/PE1/PE2/PE6 are analog/high-impedance inputs.
 *      This is used when D153B power is OFF, so STM32 cannot feed current into
 *      the unpowered driver.
 *
 * Hardware is still recommended: add 1k~4.7k series resistors on PWMA/PWMB and
 * AIN/BIN lines for extra current limiting during debugging.
 */

static uint8_t g_outputs_enabled = 0U;

/* Limit PWM command to a safe range before writing TIM compare registers. */
static int16_t limit_pwm(int32_t pwm)
{
    if (pwm > MOTOR_PWM_LIMIT) return MOTOR_PWM_LIMIT;
    if (pwm < -MOTOR_PWM_LIMIT) return -MOTOR_PWM_LIMIT;
    return (int16_t)pwm;
}

static void motor_direction_gpio_output_init(void)
{
    GPIO_InitTypeDef GPIO_Initure;

    __HAL_RCC_GPIOE_CLK_ENABLE();

    GPIO_Initure.Pin = MOTOR_LEFT_IN1_PIN | MOTOR_LEFT_IN2_PIN |
                       MOTOR_RIGHT_IN1_PIN | MOTOR_RIGHT_IN2_PIN;
    GPIO_Initure.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_Initure.Pull = GPIO_NOPULL;
    GPIO_Initure.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOE, &GPIO_Initure);

    HAL_GPIO_WritePin(GPIOE,
                      MOTOR_LEFT_IN1_PIN | MOTOR_LEFT_IN2_PIN |
                      MOTOR_RIGHT_IN1_PIN | MOTOR_RIGHT_IN2_PIN,
                      GPIO_PIN_RESET);
}

static void motor_pwm_gpio_af_init(void)
{
    GPIO_InitTypeDef GPIO_Initure;

    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_Initure.Pin = GPIO_PIN_6 | GPIO_PIN_7;
    GPIO_Initure.Mode = GPIO_MODE_AF_PP;
    GPIO_Initure.Pull = GPIO_NOPULL;
    GPIO_Initure.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_Initure);
}

static void motor_all_control_pins_analog_hiz(void)
{
    GPIO_InitTypeDef GPIO_Initure;

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();

    /* PA6/PA7: PWM pins.  Analog mode disables the digital output driver. */
    GPIO_Initure.Pin = GPIO_PIN_6 | GPIO_PIN_7;
    GPIO_Initure.Mode = GPIO_MODE_ANALOG;
    GPIO_Initure.Pull = GPIO_NOPULL;
    GPIO_Initure.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_Initure);

    /* PE0/PE1/PE2/PE6: direction pins. */
    GPIO_Initure.Pin = MOTOR_LEFT_IN1_PIN | MOTOR_LEFT_IN2_PIN |
                       MOTOR_RIGHT_IN1_PIN | MOTOR_RIGHT_IN2_PIN;
    GPIO_Initure.Mode = GPIO_MODE_ANALOG;
    GPIO_Initure.Pull = GPIO_NOPULL;
    GPIO_Initure.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOE, &GPIO_Initure);
}

void Motor_EnableOutputs(void)
{
    if (g_outputs_enabled) return;

    motor_direction_gpio_output_init();
    motor_pwm_gpio_af_init();

    __HAL_TIM_SET_COMPARE(MOTOR_LEFT_PWM_TIM, MOTOR_LEFT_PWM_CHANNEL, 0);
    __HAL_TIM_SET_COMPARE(MOTOR_RIGHT_PWM_TIM, MOTOR_RIGHT_PWM_CHANNEL, 0);

    HAL_TIM_PWM_Start(MOTOR_LEFT_PWM_TIM, MOTOR_LEFT_PWM_CHANNEL);
    HAL_TIM_PWM_Start(MOTOR_RIGHT_PWM_TIM, MOTOR_RIGHT_PWM_CHANNEL);

    g_outputs_enabled = 1U;
}

void Motor_DisableOutputsHiZ(void)
{
    /* Stop timer outputs first, then make pins high-impedance. */
    __HAL_TIM_SET_COMPARE(MOTOR_LEFT_PWM_TIM, MOTOR_LEFT_PWM_CHANNEL, 0);
    __HAL_TIM_SET_COMPARE(MOTOR_RIGHT_PWM_TIM, MOTOR_RIGHT_PWM_CHANNEL, 0);

    HAL_TIM_PWM_Stop(MOTOR_LEFT_PWM_TIM, MOTOR_LEFT_PWM_CHANNEL);
    HAL_TIM_PWM_Stop(MOTOR_RIGHT_PWM_TIM, MOTOR_RIGHT_PWM_CHANNEL);

    motor_all_control_pins_analog_hiz();
    g_outputs_enabled = 0U;
}

uint8_t Motor_OutputsEnabled(void)
{
    return g_outputs_enabled;
}

/*
 * Convert signed left motor command into TB6612 direction pins.
 * Positive command means "left track forward" by project convention.
 * If the actual direction is reversed, set MOTOR_LEFT_INVERT to 1 in
 * board_config.h instead of changing every control formula.
 */
static void set_left_direction(int16_t *pwm)
{
    int16_t value = *pwm;

#if MOTOR_LEFT_INVERT
    value = -value;
#endif

    if (value >= 0)
    {
        HAL_GPIO_WritePin(MOTOR_LEFT_IN1_PORT, MOTOR_LEFT_IN1_PIN, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(MOTOR_LEFT_IN2_PORT, MOTOR_LEFT_IN2_PIN, GPIO_PIN_SET);
    }
    else
    {
        HAL_GPIO_WritePin(MOTOR_LEFT_IN1_PORT, MOTOR_LEFT_IN1_PIN, GPIO_PIN_SET);
        HAL_GPIO_WritePin(MOTOR_LEFT_IN2_PORT, MOTOR_LEFT_IN2_PIN, GPIO_PIN_RESET);
        value = -value;
    }

    if (value < MOTOR_PWM_DEADBAND) value = 0;
    *pwm = value;
}

/* Same conversion for the right motor.  The positive direction may be different
 * from the left side because the two motors are mounted mirrored on the chassis.
 */
static void set_right_direction(int16_t *pwm)
{
    int16_t value = *pwm;

#if MOTOR_RIGHT_INVERT
    value = -value;
#endif

    if (value >= 0)
    {
        HAL_GPIO_WritePin(MOTOR_RIGHT_IN1_PORT, MOTOR_RIGHT_IN1_PIN, GPIO_PIN_SET);
        HAL_GPIO_WritePin(MOTOR_RIGHT_IN2_PORT, MOTOR_RIGHT_IN2_PIN, GPIO_PIN_RESET);
    }
    else
    {
        HAL_GPIO_WritePin(MOTOR_RIGHT_IN1_PORT, MOTOR_RIGHT_IN1_PIN, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(MOTOR_RIGHT_IN2_PORT, MOTOR_RIGHT_IN2_PIN, GPIO_PIN_SET);
        value = -value;
    }

    if (value < MOTOR_PWM_DEADBAND) value = 0;
    *pwm = value;
}

void Motor_Init(void)
{
    /* Do not enable PWM/direction outputs at boot.  The chassis layer will call
     * Motor_EnableOutputs() only after it detects that D153B power is present.
     */
    Motor_DisableOutputsHiZ();
}

void Motor_SetPWM(int16_t left_pwm, int16_t right_pwm)
{
    Motor_EnableOutputs();

    left_pwm = limit_pwm(left_pwm);
    right_pwm = limit_pwm(right_pwm);

    set_left_direction(&left_pwm);
    set_right_direction(&right_pwm);

    __HAL_TIM_SET_COMPARE(MOTOR_LEFT_PWM_TIM, MOTOR_LEFT_PWM_CHANNEL, (uint32_t)left_pwm);
    __HAL_TIM_SET_COMPARE(MOTOR_RIGHT_PWM_TIM, MOTOR_RIGHT_PWM_CHANNEL, (uint32_t)right_pwm);
}

void Motor_Stop(void)
{
    /* Normal stop when D153B is powered: drive all control signals low.  This is
     * safer than Hi-Z while the driver is powered because TB6612 inputs will not
     * float.  When D153B is unpowered, chassis.c calls Motor_DisableOutputsHiZ().
     */
    Motor_EnableOutputs();

    __HAL_TIM_SET_COMPARE(MOTOR_LEFT_PWM_TIM, MOTOR_LEFT_PWM_CHANNEL, 0);
    __HAL_TIM_SET_COMPARE(MOTOR_RIGHT_PWM_TIM, MOTOR_RIGHT_PWM_CHANNEL, 0);

    HAL_GPIO_WritePin(MOTOR_LEFT_IN1_PORT, MOTOR_LEFT_IN1_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(MOTOR_LEFT_IN2_PORT, MOTOR_LEFT_IN2_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(MOTOR_RIGHT_IN1_PORT, MOTOR_RIGHT_IN1_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(MOTOR_RIGHT_IN2_PORT, MOTOR_RIGHT_IN2_PIN, GPIO_PIN_RESET);
}

void Motor_Brake(void)
{
    Motor_EnableOutputs();

    __HAL_TIM_SET_COMPARE(MOTOR_LEFT_PWM_TIM, MOTOR_LEFT_PWM_CHANNEL, 0);
    __HAL_TIM_SET_COMPARE(MOTOR_RIGHT_PWM_TIM, MOTOR_RIGHT_PWM_CHANNEL, 0);

    HAL_GPIO_WritePin(MOTOR_LEFT_IN1_PORT, MOTOR_LEFT_IN1_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(MOTOR_LEFT_IN2_PORT, MOTOR_LEFT_IN2_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(MOTOR_RIGHT_IN1_PORT, MOTOR_RIGHT_IN1_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(MOTOR_RIGHT_IN2_PORT, MOTOR_RIGHT_IN2_PIN, GPIO_PIN_SET);
}
