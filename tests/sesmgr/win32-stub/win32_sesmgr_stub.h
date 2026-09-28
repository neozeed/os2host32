#ifndef WIN32_SESMGR_STUB_H
#define WIN32_SESMGR_STUB_H
#include "windows.h"
extern DWORD stub_last_creation_flags;
extern STARTUPINFOA stub_last_startup;
extern char stub_last_command[4096];
extern char stub_last_environment[512];
extern int stub_set_title_calls;
extern char stub_last_title[260];
void stub_sesmgr_reset(void);
#endif
