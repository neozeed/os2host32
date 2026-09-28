#ifndef OS2_NLS_WIN32_H
#define OS2_NLS_WIN32_H

struct Os2NlsState;

/* Initialize one OS/2 NLS session using Win32 locale/OEM values only as
 * bootstrap hints.  Guest-visible state remains owned by common NLS. */
void os2_nls_win32_init_session(struct Os2NlsState *state);

#endif
