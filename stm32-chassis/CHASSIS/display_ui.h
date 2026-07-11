/**
 * @file display_ui.h
 * @brief LCD界面显示头文件，声明界面显示与页面切换接口。
 * @author 李帅 赵禹博 吴坨鑫
 * @date 6月12号
 * @note 本程序为研电赛沙漠光伏板检测机器人系统的一部分。
 */

#ifndef __DISPLAY_UI_H
#define __DISPLAY_UI_H

#include <stdint.h>

typedef enum
{
    UI_PAGE_MAIN = 0,
    UI_PAGE_ENCODER,
    UI_PAGE_SYSTEM,
    UI_PAGE_TUNE,
    UI_PAGE_TURN_TUNE,
    UI_PAGE_SERVO_TUNE,
    UI_PAGE_MAX
} UI_Page_t;

void DisplayUI_Init(void);
void DisplayUI_SetPage(UI_Page_t page);
void DisplayUI_NextPage(void);
void DisplayUI_Task(void);
UI_Page_t DisplayUI_GetPage(void);

#endif
