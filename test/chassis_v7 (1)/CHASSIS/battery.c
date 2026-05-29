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
