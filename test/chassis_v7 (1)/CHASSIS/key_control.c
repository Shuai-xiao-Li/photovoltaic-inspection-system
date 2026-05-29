#include "key_control.h"
#include "board_config.h"
#include "chassis.h"
#include "display_ui.h"
#include "speed_tune.h"
#include "stm32f1xx_hal.h"
#include <stdio.h>
#include <string.h>

/*
 * key_control.c
 *
 * Safe two-key local control.
 *
 * Operation:
 *   KEY0 short press : select next test item
 *   KEY1 short press : execute selected item
 *   KEY0 long press  : switch LCD page quickly
 *   KEY1 long press  : emergency stop; if already faulted, clear fault
 *
 * This is deliberately conservative.  It only exposes the safe test commands
 * that already have speed/PWM limits and soft ramps in chassis.c.
 */

#define KEY_ACTIVE_LEVEL              GPIO_PIN_RESET
#define KEY_DEBOUNCE_MS               30U
#define KEY_LONG_PRESS_MS             900U

#define KEY0_PORT                     GPIOE
#define KEY0_PIN                      GPIO_PIN_4
#define KEY1_PORT                     GPIOE
#define KEY1_PIN                      GPIO_PIN_3

typedef struct
{
    GPIO_TypeDef *port;
    uint16_t pin;

    GPIO_PinState raw_last;
    GPIO_PinState stable;
    uint32_t raw_change_tick;
    uint32_t press_start_tick;

    uint8_t short_event;
    uint8_t long_event;
} KeyDebounce_t;

static KeyDebounce_t key0;
static KeyDebounce_t key1;
static KeyAction_t selected_action;
static char last_event[48];

static void set_event_text(const char *text)
{
    strncpy(last_event, text, sizeof(last_event) - 1U);
    last_event[sizeof(last_event) - 1U] = '\0';
}

static void key_init_one(KeyDebounce_t *key, GPIO_TypeDef *port, uint16_t pin)
{
    key->port = port;
    key->pin = pin;
    key->raw_last = HAL_GPIO_ReadPin(port, pin);
    key->stable = key->raw_last;
    key->raw_change_tick = HAL_GetTick();
    key->press_start_tick = 0U;
    key->short_event = 0U;
    key->long_event = 0U;
}

static uint8_t is_pressed(GPIO_PinState state)
{
    return (state == KEY_ACTIVE_LEVEL) ? 1U : 0U;
}

static void key_update_one(KeyDebounce_t *key)
{
    GPIO_PinState raw = HAL_GPIO_ReadPin(key->port, key->pin);
    uint32_t now = HAL_GetTick();

    key->short_event = 0U;
    key->long_event = 0U;

    if (raw != key->raw_last)
    {
        key->raw_last = raw;
        key->raw_change_tick = now;
    }

    if ((now - key->raw_change_tick) < KEY_DEBOUNCE_MS)
    {
        return;
    }

    if (raw != key->stable)
    {
        key->stable = raw;

        if (is_pressed(key->stable))
        {
            key->press_start_tick = now;
        }
        else
        {
            uint32_t press_time = now - key->press_start_tick;
            if (press_time >= KEY_LONG_PRESS_MS)
            {
                key->long_event = 1U;
            }
            else
            {
                key->short_event = 1U;
            }
        }
    }
}

static const char *action_name(KeyAction_t action)
{
    switch (action)
    {
        case KEY_ACTION_STOP:         return "STOP";
        case KEY_ACTION_TEST_LOW:     return "TEST LOW";
        case KEY_ACTION_TEST_MID:     return "TEST MID";
        case KEY_ACTION_TURN_LEFT:    return "TURN LEFT";
        case KEY_ACTION_TURN_RIGHT:   return "TURN RIGHT";
        case KEY_ACTION_PAGE_MAIN:    return "PAGE MAIN";
        case KEY_ACTION_PAGE_ENCODER: return "PAGE ENC";
        case KEY_ACTION_PAGE_SYSTEM:  return "PAGE SYS";
        case KEY_ACTION_PAGE_TUNE:    return "PAGE TUNE";
        case KEY_ACTION_ESTOP_CLEAR:  return "ESTOP/CLEAR";
        default:                      return "UNKNOWN";
    }
}

static uint8_t driver_required_action(KeyAction_t action)
{
    return (action == KEY_ACTION_TEST_LOW ||
            action == KEY_ACTION_TEST_MID ||
            action == KEY_ACTION_TURN_LEFT ||
            action == KEY_ACTION_TURN_RIGHT) ? 1U : 0U;
}

static void execute_action(KeyAction_t action)
{
    const ChassisState_t *s;

    if (driver_required_action(action) && !Chassis_IsDriverPowered())
    {
        /* Do not let a key press produce any high level on PWMA/PWMB/AIN/BIN
         * while D153B is off.  The chassis layer has already forced Hi-Z.
         */
        Chassis_Stop();
        set_event_text("DRV OFF: check VM+ADC");
        return;
    }

    switch (action)
    {
        case KEY_ACTION_STOP:
            Chassis_Stop();
            set_event_text("RUN: stop");
            break;

        case KEY_ACTION_TEST_LOW:
            /* First bring-up uses open-loop PWM.  It does not depend on encoder
             * feedback or PID, so it is much better for proving wiring.
             */
            Chassis_SetOpenLoopPWM(KEY_TEST_PWM_LOW, KEY_TEST_PWM_LOW);
            set_event_text("RUN: open pwm low");
            break;

        case KEY_ACTION_TEST_MID:
            Chassis_SetOpenLoopPWM(KEY_TEST_PWM_MID, KEY_TEST_PWM_MID);
            set_event_text("RUN: open pwm mid");
            break;

        case KEY_ACTION_TURN_LEFT:
            Chassis_SetOpenLoopPWM(-KEY_TEST_PWM_TURN, KEY_TEST_PWM_TURN);
            set_event_text("RUN: open turn left");
            break;

        case KEY_ACTION_TURN_RIGHT:
            Chassis_SetOpenLoopPWM(KEY_TEST_PWM_TURN, -KEY_TEST_PWM_TURN);
            set_event_text("RUN: open turn right");
            break;

        case KEY_ACTION_PAGE_MAIN:
            /* Page switching must never leave a motor command running. */
            Chassis_Stop();
            DisplayUI_SetPage(UI_PAGE_MAIN);
            set_event_text("LCD: main page, motor stop");
            break;

        case KEY_ACTION_PAGE_ENCODER:
            Chassis_Stop();
            DisplayUI_SetPage(UI_PAGE_ENCODER);
            set_event_text("LCD: encoder page, motor stop");
            break;

        case KEY_ACTION_PAGE_SYSTEM:
            Chassis_Stop();
            DisplayUI_SetPage(UI_PAGE_SYSTEM);
            set_event_text("LCD: system page, motor stop");
            break;

        case KEY_ACTION_PAGE_TUNE:
            /* Always enter tuning from a known stopped state. */
            Chassis_Stop();
            SpeedTune_Init();
            DisplayUI_SetPage(UI_PAGE_TUNE);
            set_event_text("LCD: tune page, motor stop");
            break;

        case KEY_ACTION_ESTOP_CLEAR:
            s = Chassis_GetState();
            if (s->fault_code == CHASSIS_FAULT_NONE)
            {
                Chassis_EStop();
                set_event_text("RUN: emergency stop");
            }
            else
            {
                Chassis_ClearFault();
                set_event_text("RUN: fault cleared");
            }
            break;

        default:
            break;
    }
}

void KeyControl_Init(void)
{
    selected_action = KEY_ACTION_TEST_LOW;
    set_event_text("KEY0 select, KEY1 run");

    key_init_one(&key0, KEY0_PORT, KEY0_PIN);
    key_init_one(&key1, KEY1_PORT, KEY1_PIN);
}

void KeyControl_Task(void)
{
    const ChassisState_t *s;

    key_update_one(&key0);
    key_update_one(&key1);

    /* ---- TUNE page: override key behavior for parameter tuning ---- */
    if (DisplayUI_GetPage() == UI_PAGE_TUNE)
    {
        if (key0.short_event)
        {
            SpeedTune_NextParam();
        }

        if (key1.short_event)
        {
            SpeedTune_Increase();
        }

        if (key0.long_event)
        {
            /* KEY0 long = exit to next page, same as all other pages. */
            SpeedTune_ForceStop();
            DisplayUI_NextPage();
            set_event_text("LCD: next page");
        }

        if (key1.long_event)
        {
            if (SpeedTune_GetState()->running)
            {
                /* Motor running: emergency stop (safety first). */
                SpeedTune_ForceStop();
                Chassis_EStop();
                set_event_text("LONG: emergency stop");
            }
            else
            {
                /* Motor stopped: decrease current parameter value. */
                SpeedTune_Decrease();
            }
        }

        return;   /* Skip normal key handling when on TUNE page. */
    }

    /* ---- Normal pages: original key behavior ---- */
    if (key0.short_event)
    {
        selected_action = (KeyAction_t)((selected_action + 1U) % KEY_ACTION_MAX);
        snprintf(last_event, sizeof(last_event), "SEL: %s", action_name(selected_action));
    }

    if (key1.short_event)
    {
        execute_action(selected_action);
    }

    if (key0.long_event)
    {
        DisplayUI_NextPage();
        set_event_text("LCD: next page");
    }

    if (key1.long_event)
    {
        s = Chassis_GetState();
        if (s->fault_code == CHASSIS_FAULT_NONE)
        {
            Chassis_EStop();
            set_event_text("LONG: emergency stop");
        }
        else
        {
            Chassis_ClearFault();
            set_event_text("LONG: fault cleared");
        }
    }
}

KeyAction_t KeyControl_GetAction(void)
{
    return selected_action;
}

const char *KeyControl_GetActionName(void)
{
    return action_name(selected_action);
}

const char *KeyControl_GetLastEvent(void)
{
    return last_event;
}
