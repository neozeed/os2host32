#ifndef QUEUE_TEST_WINDOWS_H
#define QUEUE_TEST_WINDOWS_H

#include <stddef.h>

typedef unsigned long DWORD;
typedef int BOOL;
typedef void *HANDLE;
typedef void *HINSTANCE;
typedef void *LPVOID;

typedef struct _CRITICAL_SECTION { int held; } CRITICAL_SECTION;

#define WINAPI
#define TRUE 1
#define FALSE 0
#define DLL_PROCESS_DETACH 0UL
#define DLL_PROCESS_ATTACH 1UL
#define INFINITE 0xffffffffUL
#define WAIT_OBJECT_0 0UL
#define WAIT_TIMEOUT 258UL
#define WAIT_FAILED 0xffffffffUL

void InitializeCriticalSection(CRITICAL_SECTION *cs);
void DeleteCriticalSection(CRITICAL_SECTION *cs);
void EnterCriticalSection(CRITICAL_SECTION *cs);
void LeaveCriticalSection(CRITICAL_SECTION *cs);
HANDLE CreateEventA(void *attributes, BOOL manual_reset, BOOL initial_state,
                    const char *name);
BOOL CloseHandle(HANDLE handle);
BOOL SetEvent(HANDLE handle);
BOOL ResetEvent(HANDLE handle);
DWORD WaitForSingleObject(HANDLE handle, DWORD timeout);
DWORD GetCurrentProcessId(void);

#endif
