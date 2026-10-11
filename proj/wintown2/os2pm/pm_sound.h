#ifndef PM_SOUND_H
#define PM_SOUND_H
#define PM_SOUND_BUILD 1
#define PM_SOUND_ERROR 2
#define PM_SOUND_ALERT 3
void pm_sound_init(void);
void pm_sound_stop(void);
int pm_sound_enabled(void);
void pm_sound_enable(int enabled);
void pm_sound_play(int effect);
#endif

