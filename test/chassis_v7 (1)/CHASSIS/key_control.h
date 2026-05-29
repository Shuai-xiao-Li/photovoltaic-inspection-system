#ifndef __KEY_CONTROL_H
#define __KEY_CONTROL_H

#include <stdint.h>

/*
 * key_control.h
 *
 * Two-button bench-test controller for the ALIENTEK STM32F103ZET6 Elite board.
 * Touch is intentionally not used.  This module lets you test the chassis with
 * KEY0/KEY1 before the Phytium Pi upper computer or serial control is added.
 *
 * Board keys used:
 *   KEY0 -> PE4, active low: select next item
 *   KEY1 -> PE3, active low: execute selected item
 *
 * KEY_UP is NOT used because PA0 is already assigned to TIM2_CH1 encoder input.
 */

typedef enum
{
    KEY_ACTION_STOP = 0,
    KEY_ACTION_TEST_LOW,
    KEY_ACTION_TEST_MID,
    KEY_ACTION_TURN_LEFT,
    KEY_ACTION_TURN_RIGHT,
    KEY_ACTION_PAGE_MAIN,
    KEY_ACTION_PAGE_ENCODER,
    KEY_ACTION_PAGE_SYSTEM,
    KEY_ACTION_PAGE_TUNE,
    KEY_ACTION_ESTOP_CLEAR,
    KEY_ACTION_MAX
} KeyAction_t;

void KeyControl_Init(void);
void KeyControl_Task(void);

KeyAction_t KeyControl_GetAction(void);
const char *KeyControl_GetActionName(void);
const char *KeyControl_GetLastEvent(void);

#endif
