#ifndef OS2_KBD_H
#define OS2_KBD_H

/*
 * Backend-neutral OS/2 base keyboard state and semantics.
 *
 * KBDINFO process/session state, OS/2 ABI result construction, KbdStringIn
 * line semantics and API validation live here.  Host console/input mechanics
 * are supplied by a backend.  This is intentionally only the six-call KBD
 * surface already implemented by os2host32; it is not a full logical-keyboard
 * subsystem yet.
 */

#include <stdint.h>
#include "os2_kbd_backend.h"

#ifndef __cdecl
#define __cdecl
#endif

#define OS2_KBD_NO_ERROR                 0U
#define OS2_KBD_ERROR_INVALID_FUNCTION   1U
#define OS2_KBD_ERROR_INVALID_HANDLE     6U
#define OS2_KBD_ERROR_INVALID_PARAMETER 87U
#define OS2_KBD_ERROR_INVALID_LENGTH   376U

#define OS2_KBD_IO_WAIT                  0U
#define OS2_KBD_IO_NOWAIT                1U
#define OS2_KBD_STATUS_CHAR_IN           0x40U

#pragma pack(push, 2)
struct Os2KbdKeyInfo {
    unsigned char chChar;
    unsigned char chScan;
    unsigned char fbStatus;
    unsigned char bNlsShift;
    unsigned short fsState;
    uint32_t time;
};

struct Os2KbdStringInBuf {
    unsigned short cb;
    unsigned short cchIn;
};

struct Os2KbdInfo {
    unsigned short cb;
    unsigned short fsMask;
    unsigned short chTurnAround;
    unsigned short fsInterim;
    unsigned short fsState;
};
#pragma pack(pop)

typedef char Os2KbdKeyInfo_must_be_10_bytes[(sizeof(struct Os2KbdKeyInfo) == 10) ? 1 : -1];
typedef char Os2KbdStringInBuf_must_be_4_bytes[(sizeof(struct Os2KbdStringInBuf) == 4) ? 1 : -1];
typedef char Os2KbdInfo_must_be_10_bytes[(sizeof(struct Os2KbdInfo) == 10) ? 1 : -1];

/* A host-normalized key event.  No Win32 types cross this boundary. */
struct Os2KbdHostEvent {
    unsigned char ch_char;
    unsigned char ch_scan;
    unsigned short fs_state;
    uint32_t time;
    int echoable;
};

struct Os2KbdSession {
    void *backend_opaque;
    const struct Os2KbdBackendOps *backend;
    struct Os2KbdInfo status;
};

void os2_kbd_session_init(struct Os2KbdSession *session,
                          void *backend_opaque,
                          const struct Os2KbdBackendOps *backend);

Os2KbdApiRet os2_kbd_KbdCharIn(struct Os2KbdSession *session,
                                struct Os2KbdKeyInfo *info,
                                unsigned short wait,
                                unsigned short hkbd);
Os2KbdApiRet os2_kbd_KbdStringIn(struct Os2KbdSession *session,
                                  char *buffer,
                                  struct Os2KbdStringInBuf *length,
                                  unsigned short wait,
                                  unsigned short hkbd);
Os2KbdApiRet os2_kbd_KbdGetStatus(struct Os2KbdSession *session,
                                   struct Os2KbdInfo *info,
                                   unsigned short hkbd);
Os2KbdApiRet os2_kbd_KbdSetStatus(struct Os2KbdSession *session,
                                   const struct Os2KbdInfo *info,
                                   unsigned short hkbd);
Os2KbdApiRet os2_kbd_KbdFlushBuffer(struct Os2KbdSession *session,
                                     unsigned short hkbd);
Os2KbdApiRet os2_kbd_KbdPeek(struct Os2KbdSession *session,
                              struct Os2KbdKeyInfo *info,
                              unsigned short hkbd);

/* Scheduler-friendly nonblocking semantic primitive for future WHP/OS2SS. */
Os2KbdApiRet os2_kbd_try_char(struct Os2KbdSession *session,
                               struct Os2KbdKeyInfo *info,
                               int *present);

#endif
