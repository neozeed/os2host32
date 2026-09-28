#ifndef KBD_TEST_WINDOWS_H
#define KBD_TEST_WINDOWS_H

#include <stddef.h>
#include <string.h>

typedef unsigned long DWORD;
typedef unsigned short WORD;
typedef long LONG;
typedef int BOOL;
typedef void *HANDLE;
typedef BOOL (*PHANDLER_ROUTINE)(DWORD);

#define WINAPI
#define TRUE 1
#define FALSE 0
#define NULL_HANDLE ((HANDLE)0)
#define INVALID_HANDLE_VALUE ((HANDLE)(long)-1)

#define STD_INPUT_HANDLE ((DWORD)-10)
#define STD_OUTPUT_HANDLE ((DWORD)-11)
#define FILE_TYPE_CHAR 2UL
#define FILE_TYPE_PIPE 3UL
#define KEY_EVENT 1U
#define CTRL_C_EVENT 0UL
#define CTRL_BREAK_EVENT 1UL
#define ERROR_INVALID_HANDLE 6UL
#define ERROR_INVALID_PARAMETER 87UL

#define RIGHT_ALT_PRESSED  0x0001UL
#define LEFT_ALT_PRESSED   0x0002UL
#define RIGHT_CTRL_PRESSED 0x0004UL
#define LEFT_CTRL_PRESSED  0x0008UL
#define SHIFT_PRESSED      0x0010UL
#define NUMLOCK_ON         0x0020UL
#define SCROLLLOCK_ON      0x0040UL
#define CAPSLOCK_ON        0x0080UL

#define ZeroMemory(p,n) memset((p),0,(n))

typedef struct _KEY_EVENT_RECORD {
    BOOL bKeyDown;
    WORD wRepeatCount;
    WORD wVirtualKeyCode;
    WORD wVirtualScanCode;
    union { char AsciiChar; } uChar;
    DWORD dwControlKeyState;
} KEY_EVENT_RECORD;

typedef struct _INPUT_RECORD {
    WORD EventType;
    union { KEY_EVENT_RECORD KeyEvent; } Event;
} INPUT_RECORD;

HANDLE GetStdHandle(DWORD which);
DWORD GetFileType(HANDLE handle);
BOOL SetConsoleCtrlHandler(PHANDLER_ROUTINE routine, BOOL add);
LONG InterlockedCompareExchange(volatile LONG *target, LONG exchange, LONG comparand);
LONG InterlockedExchange(volatile LONG *target, LONG value);
BOOL WriteConsoleInputA(HANDLE handle, const INPUT_RECORD *records,
                        DWORD count, DWORD *written);
BOOL GetNumberOfConsoleInputEvents(HANDLE handle, DWORD *count);
BOOL ReadConsoleInputA(HANDLE handle, INPUT_RECORD *records,
                       DWORD count, DWORD *read);
BOOL PeekConsoleInputA(HANDLE handle, INPUT_RECORD *records,
                       DWORD count, DWORD *read);
BOOL PeekNamedPipe(HANDLE handle, void *buffer, DWORD buffer_size,
                   DWORD *bytes_read, DWORD *total_avail, DWORD *left);
BOOL ReadFile(HANDLE handle, void *buffer, DWORD count, DWORD *read,
              void *overlapped);
BOOL FlushConsoleInputBuffer(HANDLE handle);
BOOL WriteConsoleA(HANDLE handle, const void *buffer, DWORD count,
                   DWORD *written, void *reserved);
DWORD GetTickCount(void);
DWORD GetLastError(void);

/* Test-only controls exported by the stub. */
void kbd_stub_reset(void);
void kbd_stub_push_key(unsigned char ch, unsigned char scan, DWORD state);
void kbd_stub_push_nonkey(void);
void kbd_stub_set_input_type(DWORD type);
void kbd_stub_set_pipe_bytes(const char *text);
const char *kbd_stub_echo(void);
unsigned int kbd_stub_echo_len(void);
unsigned int kbd_stub_flush_count(void);

#endif
