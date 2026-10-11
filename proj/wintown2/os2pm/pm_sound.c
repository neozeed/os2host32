/* C89 DosBeep worker. Queue data contains integers, NEVER struct pointers. */
#include "pm_sound.h"
#ifndef MP_PORT_TEST
#define INCL_32
#define INCL_DOSPROCESS
#define INCL_DOSQUEUES
#define INCL_DOSMISC
#include <os2.h>
#include <stdio.h>

#define NOTE_FREQ 1UL
#define NOTE_TIME 2UL
#define NOTE_STOP 3UL
static HQUEUE soundQueue;
static TID soundThread;
static volatile int soundEnabled;
static int soundReady;

static void sound_worker(void)
{
    REQUESTDATA request;
    ULONG size, rc, frequency;
    PVOID value;
    BYTE priority;
    int haveFrequency;
    frequency = 0; haveFrequency = 0;
    for (;;) {
        value = 0; size = 0; priority = 0;
        rc = DosReadQueue(soundQueue, &request, &size, &value,
                          0UL, DCWW_WAIT, &priority, 0UL);
        if (rc || request.ulData == NOTE_STOP) break;
        if (request.ulData == NOTE_FREQ) {
            frequency = (ULONG)value;
            haveFrequency = 1;
        } else if (request.ulData == NOTE_TIME) {
            if (haveFrequency && soundEnabled && frequency >= 37UL &&
                frequency <= 32767UL && (ULONG)value > 0UL &&
                (ULONG)value <= 120UL)
                DosBeep(frequency, (ULONG)value);
            haveFrequency = 0;
        }
    }
    DosExit(EXIT_THREAD, 0UL);
}

void pm_sound_init(void)
{
    char name[64];
    ULONG rc;
    int slot;
    if (soundReady) return;
    /* Claim a free name atomically; allow several WinTown processes. */
    for (slot = 0; slot < 64; ++slot) {
        sprintf(name, "\\QUEUES\\WINTOWN_BEEP_%02d", slot);
        rc = DosCreateQueue(&soundQueue, QUE_FIFO, name);
        if (!rc) break;
    }
    if (slot == 64) {
        printf("WINTOWN: SOUND-R23 queue unavailable rc=%lu\n", rc);
        return;
    }
    rc = DosCreateThread(&soundThread, sound_worker, 0UL, 0UL, 8192UL);
    if (rc) {
        DosCloseQueue(soundQueue);
        printf("WINTOWN: SOUND-R23 worker failed rc=%lu\n", rc);
        return;
    }
    soundReady = 1; soundEnabled = 1;
    printf("WINTOWN: SOUND-R23 DosBeep worker ready (%s)\n", name);
    fflush(stdout);
}
int pm_sound_enabled(void) { return soundReady && soundEnabled; }
void pm_sound_enable(int enabled)
{
    soundEnabled = soundReady && enabled;
    if (soundReady && !soundEnabled) DosPurgeQueue(soundQueue);
}
static void sound_note(ULONG frequency, ULONG duration)
{
    ULONG count;
    if (!pm_sound_enabled()) return;
    if (DosQueryQueue(soundQueue, &count) || count >= 8UL) return;
    if (DosWriteQueue(soundQueue, NOTE_FREQ, sizeof(ULONG),
                      (PVOID)frequency, 0UL)) return;
    /* A missing duration is safe: the next tagged frequency replaces it. */
    DosWriteQueue(soundQueue, NOTE_TIME, sizeof(ULONG), (PVOID)duration, 0UL);
}
void pm_sound_play(int effect)
{
    if (effect == PM_SOUND_BUILD) sound_note(880UL, 25UL);
    else if (effect == PM_SOUND_ERROR) sound_note(180UL, 90UL);
    else if (effect == PM_SOUND_ALERT) {
        sound_note(660UL, 70UL); sound_note(990UL, 90UL);
    }
}
void pm_sound_stop(void)
{
    int closed;
    if (!soundReady) return;
    soundEnabled = 0;
    DosPurgeQueue(soundQueue);
    closed = DosWriteQueue(soundQueue, NOTE_STOP, 0UL, (PVOID)0, 0UL) != 0;
    if (closed) DosCloseQueue(soundQueue);
    DosWaitThread(&soundThread, DCWW_WAIT);
    if (!closed) DosCloseQueue(soundQueue);
    soundReady = 0;
}
#else
void pm_sound_init(void) { }
void pm_sound_stop(void) { }
int pm_sound_enabled(void) { return 0; }
void pm_sound_enable(int enabled) { (void)enabled; }
void pm_sound_play(int effect) { (void)effect; }
#endif

