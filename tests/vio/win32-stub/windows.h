#ifndef TEST_WINDOWS_H
#define TEST_WINDOWS_H

#include <stddef.h>

typedef unsigned int DWORD;
typedef unsigned short WORD;
typedef short SHORT;
typedef int BOOL;
typedef void *HANDLE;

typedef struct _COORD {
    SHORT X;
    SHORT Y;
} COORD;

typedef struct _SMALL_RECT {
    SHORT Left;
    SHORT Top;
    SHORT Right;
    SHORT Bottom;
} SMALL_RECT;

typedef union _CHAR_INFO_CHAR {
    char AsciiChar;
    unsigned short UnicodeChar;
} CHAR_INFO_CHAR;

typedef struct _CHAR_INFO {
    CHAR_INFO_CHAR Char;
    WORD Attributes;
} CHAR_INFO;

typedef struct _CONSOLE_SCREEN_BUFFER_INFO {
    COORD dwSize;
    COORD dwCursorPosition;
    WORD wAttributes;
    SMALL_RECT srWindow;
    COORD dwMaximumWindowSize;
} CONSOLE_SCREEN_BUFFER_INFO;

typedef struct _CONSOLE_CURSOR_INFO {
    DWORD dwSize;
    BOOL bVisible;
} CONSOLE_CURSOR_INFO;

#define TRUE 1
#define FALSE 0
#define STD_OUTPUT_HANDLE ((DWORD)-11)
#define INVALID_HANDLE_VALUE ((HANDLE)(-1L))
#define ERROR_INVALID_HANDLE 6U
#define ERROR_INVALID_PARAMETER 87U

DWORD GetLastError(void);
HANDLE GetStdHandle(DWORD which);
BOOL GetConsoleScreenBufferInfo(HANDLE handle,
                                CONSOLE_SCREEN_BUFFER_INFO *info);
BOOL GetConsoleCursorInfo(HANDLE handle, CONSOLE_CURSOR_INFO *info);
BOOL SetConsoleCursorInfo(HANDLE handle, const CONSOLE_CURSOR_INFO *info);
BOOL SetConsoleCursorPosition(HANDLE handle, COORD position);
BOOL ReadConsoleOutputCharacterA(HANDLE handle, char *text, DWORD count,
                                 COORD position, DWORD *done);
BOOL ReadConsoleOutputAttribute(HANDLE handle, WORD *attribute, DWORD count,
                                COORD position, DWORD *done);
BOOL WriteConsoleOutputCharacterA(HANDLE handle, const char *text,
                                  DWORD count, COORD position, DWORD *done);
BOOL FillConsoleOutputAttribute(HANDLE handle, WORD attribute, DWORD count,
                                COORD position, DWORD *done);
BOOL FillConsoleOutputCharacterA(HANDLE handle, char character, DWORD count,
                                 COORD position, DWORD *done);
BOOL ScrollConsoleScreenBufferA(HANDLE handle, const SMALL_RECT *source,
                                const SMALL_RECT *clip, COORD destination,
                                const CHAR_INFO *fill);
BOOL WriteFile(HANDLE handle, const void *buffer, DWORD count, DWORD *done,
               void *overlapped);

#endif
