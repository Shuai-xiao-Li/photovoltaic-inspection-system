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
    UI_PAGE_MAX
} UI_Page_t;

void DisplayUI_Init(void);
void DisplayUI_SetPage(UI_Page_t page);
void DisplayUI_NextPage(void);
void DisplayUI_Task(void);
UI_Page_t DisplayUI_GetPage(void);

#endif
