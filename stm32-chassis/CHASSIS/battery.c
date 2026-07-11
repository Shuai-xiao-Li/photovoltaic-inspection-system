/**
 * @file battery.c
 * @brief 驱动板电池电压监测与滤波计算模块，实现低电量阈值警报以及防反灌保护状态采样。
 * @author 李帅 赵禹博 吴坨鑫
 * @date 6月12号
 * @note 本程序为研电赛沙漠光伏板检测机器人系统的一部分。
 */

#include "battery.h"
#include "board_config.h"

float Battery_ReadVoltage(void)
{
    uint32_t adc_value = 0;

    if (HAL_ADC_Start(BATTERY_ADC_HANDLE) != HAL_OK)
    {
        return 0.0f;
    }

    if (HAL_ADC_PollForConversion(BATTERY_ADC_HANDLE, 2) == HAL_OK)
    {
        adc_value = HAL_ADC_GetValue(BATTERY_ADC_HANDLE);
    }

    HAL_ADC_Stop(BATTERY_ADC_HANDLE);

    /* D153B ADC output is Vin / 11.  ADC reference is 3.3 V. */
    return ((float)adc_value) * 3.3f * BATTERY_ADC_DIV_RATIO / 4095.0f;
}
