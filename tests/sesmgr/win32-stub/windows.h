#ifndef WINDOWS_H_STUB_SESMGR
#define WINDOWS_H_STUB_SESMGR

#include <stddef.h>
#include <stdint.h>

typedef void *HANDLE;
typedef void *HINSTANCE;
typedef void *LPVOID;
typedef char *LPSTR;
typedef const char *LPCSTR;
typedef const void *LPCVOID;
typedef uint32_t DWORD;
typedef uintptr_t ULONG_PTR;
typedef size_t SIZE_T;
typedef int BOOL;
typedef unsigned short WORD;

typedef struct { int dummy; } CRITICAL_SECTION;
typedef struct { DWORD dwLowDateTime; DWORD dwHighDateTime; } FILETIME;
typedef struct {
    DWORD cb;
    LPSTR lpReserved;
    LPSTR lpDesktop;
    LPSTR lpTitle;
    DWORD dwX, dwY, dwXSize, dwYSize;
    DWORD dwXCountChars, dwYCountChars, dwFillAttribute, dwFlags;
    WORD wShowWindow, cbReserved2;
    unsigned char *lpReserved2;
    HANDLE hStdInput, hStdOutput, hStdError;
} STARTUPINFOA;
typedef struct {
    HANDLE hProcess;
    HANDLE hThread;
    DWORD dwProcessId;
    DWORD dwThreadId;
} PROCESS_INFORMATION;

#define TRUE 1
#define FALSE 0
#define MAX_PATH 260
#define HEAP_ZERO_MEMORY 0x00000008UL
#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)
#define PAGE_READWRITE 0x04UL
#define FILE_MAP_ALL_ACCESS 0x000f001fUL
#define INFINITE 0xffffffffUL
#define WAIT_OBJECT_0 0UL
#define WAIT_ABANDONED 0x00000080UL
#define ERROR_INVALID_PARAMETER 87UL
#define PROCESS_QUERY_INFORMATION 0x0400UL
#define SYNCHRONIZE 0x00100000UL
#define STILL_ACTIVE 259UL
#define STARTF_USESHOWWINDOW 0x00000001UL
#define STARTF_USESIZE 0x00000002UL
#define STARTF_USEPOSITION 0x00000004UL
#define SW_HIDE 0
#define SW_SHOWNOACTIVATE 4
#define SW_SHOWMINIMIZED 2
#define SW_SHOWMAXIMIZED 3
#define CREATE_SUSPENDED 0x00000004UL
#define DETACHED_PROCESS 0x00000008UL
#define CREATE_NEW_CONSOLE 0x00000010UL
#define CREATE_NEW_PROCESS_GROUP 0x00000200UL

DWORD GetEnvironmentVariableA(LPCSTR, LPSTR, DWORD);
DWORD GetModuleFileNameA(HINSTANCE, LPSTR, DWORD);
HANDLE GetProcessHeap(void);
LPVOID HeapAlloc(HANDLE, DWORD, SIZE_T);
BOOL HeapFree(HANDLE, DWORD, LPVOID);
int lstrcmpiA(LPCSTR, LPCSTR);
void InitializeCriticalSection(CRITICAL_SECTION *);
void DeleteCriticalSection(CRITICAL_SECTION *);
void EnterCriticalSection(CRITICAL_SECTION *);
void LeaveCriticalSection(CRITICAL_SECTION *);
HANDLE CreateMutexA(LPVOID, BOOL, LPCSTR);
HANDLE CreateFileMappingA(HANDLE, LPVOID, DWORD, DWORD, DWORD, LPCSTR);
LPVOID MapViewOfFile(HANDLE, DWORD, DWORD, DWORD, SIZE_T);
BOOL UnmapViewOfFile(LPCVOID);
DWORD WaitForSingleObject(HANDLE, DWORD);
BOOL ReleaseMutex(HANDLE);
HANDLE GetCurrentProcess(void);
DWORD GetCurrentProcessId(void);
BOOL GetProcessTimes(HANDLE, FILETIME *, FILETIME *, FILETIME *, FILETIME *);
HANDLE OpenProcess(DWORD, BOOL, DWORD);
BOOL GetExitCodeProcess(HANDLE, DWORD *);
DWORD GetProcessId(HANDLE);
BOOL CloseHandle(HANDLE);
BOOL CreateProcessA(LPCSTR, LPSTR, LPVOID, LPVOID, BOOL, DWORD, LPVOID, LPCSTR,
                    STARTUPINFOA *, PROCESS_INFORMATION *);
HANDLE CreateJobObjectA(LPVOID, LPCSTR);
BOOL AssignProcessToJobObject(HANDLE, HANDLE);
DWORD ResumeThread(HANDLE);
BOOL TerminateJobObject(HANDLE, unsigned int);
BOOL TerminateProcess(HANDLE, unsigned int);
DWORD GetLastError(void);
BOOL SetConsoleTitleA(LPCSTR);

#endif
