#ifndef BLOOD_OS2_PM_H
#define BLOOD_OS2_PM_H

#include "stdint.h"

#define BLOODPM_W 320
#define BLOODPM_H 200

#ifndef BLOODPM_SCALE
#define BLOODPM_SCALE 2
#endif
#if BLOODPM_SCALE < 1
#undef BLOODPM_SCALE
#define BLOODPM_SCALE 1
#endif

#define BLOODPM_EVENT_QUIT 1
#define BLOODPM_EVENT_KEYDOWN 2
#define BLOODPM_EVENT_MOUSEDOWN 3
#define BLOODPM_EVENT_MOUSEUP 4

#define BLOODPM_BUTTON_LEFT 1
#define BLOODPM_BUTTON_RIGHT 2

#define BLOODPM_KEY_ESC 27
#define BLOODPM_KEY_ENTER 13
#define BLOODPM_KEY_BACKSPACE 8
#define BLOODPM_KEY_F8 1008
#define BLOODPM_KEY_F9 1009
#define BLOODPM_KEY_F12 1012
#define BLOODPM_KEY_PAGEUP 1101
#define BLOODPM_KEY_PAGEDOWN 1102

typedef struct BloodPM BloodPM;

typedef struct BloodPMEvent {
 int type;
 int key;
 int button;
 int x;
 int y;
} BloodPMEvent;

int blood_pm_platform_init(void);
BloodPM *blood_pm_create(const char *title);
void blood_pm_destroy(BloodPM *pm);
void blood_pm_set_title(BloodPM *pm,const char *title);
void blood_pm_present(BloodPM *pm,const uint8_t *pixels);
int blood_pm_poll(BloodPM *pm,BloodPMEvent *event);
uint32_t blood_pm_ticks(void);
void blood_pm_sleep(uint32_t ms);
void blood_pm_get_mouse(BloodPM *pm,int *x,int *y,int *left,int *right);
void blood_pm_warp_mouse(BloodPM *pm,int x,int y);
const uint8_t *blood_pm_keys(void);
int blood_pm_queue_beep(int freq,int duration);

#endif
