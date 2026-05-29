#include "encoder_driver.h"
#include "board_config.h"

uint8_t g_enc_l_status = 99;
uint8_t g_enc_r_status = 99;

void Encoder_Init(void)
{
    g_enc_l_status = (uint8_t)HAL_TIM_Encoder_Start(ENCODER_LEFT_TIM, TIM_CHANNEL_ALL);
    g_enc_r_status = (uint8_t)HAL_TIM_Encoder_Start(ENCODER_RIGHT_TIM, TIM_CHANNEL_ALL);
    Encoder_Reset();
}

void Encoder_Reset(void)
{
    __HAL_TIM_SET_COUNTER(ENCODER_LEFT_TIM, 0);
    __HAL_TIM_SET_COUNTER(ENCODER_RIGHT_TIM, 0);
}

int16_t Encoder_ReadLeftDelta(void)
{
    int16_t cnt = (int16_t)__HAL_TIM_GET_COUNTER(ENCODER_LEFT_TIM);
    __HAL_TIM_SET_COUNTER(ENCODER_LEFT_TIM, 0);
    return (int16_t)(cnt * ENCODER_LEFT_SIGN);
}

int16_t Encoder_ReadRightDelta(void)
{
    int16_t cnt = (int16_t)__HAL_TIM_GET_COUNTER(ENCODER_RIGHT_TIM);
    __HAL_TIM_SET_COUNTER(ENCODER_RIGHT_TIM, 0);
    return (int16_t)(cnt * ENCODER_RIGHT_SIGN);
}
