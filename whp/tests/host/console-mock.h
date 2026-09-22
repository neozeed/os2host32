/* Console service test double; deliberately has a nonzero viewport origin. */
typedef uint16_t WORD;typedef int16_t SHORT;typedef char CHAR;
typedef struct {SHORT X,Y;} COORD;
typedef struct {SHORT Left,Top,Right,Bottom;} SMALL_RECT;
typedef struct {union {CHAR AsciiChar;} Char;WORD Attributes;} CHAR_INFO;
typedef struct {COORD dwSize,dwCursorPosition;WORD wAttributes;SMALL_RECT srWindow;COORD dwMaximumWindowSize;} CONSOLE_SCREEN_BUFFER_INFO;
typedef struct {int bKeyDown;WORD wRepeatCount,wVirtualKeyCode,wVirtualScanCode;union {CHAR AsciiChar;} uChar;DWORD dwControlKeyState;} KEY_EVENT_RECORD;
typedef struct {WORD EventType;union {KEY_EVENT_RECORD KeyEvent;} Event;} INPUT_RECORD;
#define STD_INPUT_HANDLE 0
#define STD_OUTPUT_HANDLE 1
#define KEY_EVENT 1
#define ENABLE_LINE_INPUT 2
#define ENABLE_ECHO_INPUT 4
#define ENABLE_PROCESSED_INPUT 1
#define VK_CAPITAL 0x14
#define VK_NUMLOCK 0x90
#define VK_SCROLL 0x91
#define SHIFT_PRESSED 16
#define LEFT_CTRL_PRESSED 8
#define RIGHT_CTRL_PRESSED 4
#define LEFT_ALT_PRESSED 2
#define RIGHT_ALT_PRESSED 1
#define SCROLLLOCK_ON 64
#define NUMLOCK_ON 32
#define CAPSLOCK_ON 128
static INPUT_RECORD mock_key;
static int mock_key_pending,mock_console_available=1;
static DWORD mock_console_mode=7,mock_written,mock_fill;
static COORD mock_cursor={8,12};
static HANDLE GetStdHandle(DWORD which){return (HANDLE)(uintptr_t)(which+1);}
static int GetConsoleMode(HANDLE h,DWORD *mode){(void)h;if(!mock_console_available)return 0;*mode=mock_console_mode;return 1;}
static int SetConsoleMode(HANDLE h,DWORD mode){(void)h;mock_console_mode=mode;return 1;}
static int PeekConsoleInputA(HANDLE h,INPUT_RECORD *r,DWORD n,DWORD *got){(void)h;(void)n;*got=mock_key_pending;if(*got)*r=mock_key;return 1;}
static int ReadConsoleInputA(HANDLE h,INPUT_RECORD *r,DWORD n,DWORD *got){int ret=PeekConsoleInputA(h,r,n,got);mock_key_pending=0;return ret;}
static int FlushConsoleInputBuffer(HANDLE h){(void)h;mock_key_pending=0;return 1;}
static int WriteFile(HANDLE h,const void *p,DWORD n,DWORD *done,void *over){(void)h;(void)p;(void)over;mock_written=n;*done=n;return 1;}
static int GetConsoleScreenBufferInfo(HANDLE h,CONSOLE_SCREEN_BUFFER_INFO *i){(void)h;memset(i,0,sizeof(*i));i->srWindow=(SMALL_RECT){5,10,84,34};i->dwCursorPosition=mock_cursor;return 1;}
static int SetConsoleCursorPosition(HANDLE h,COORD p){(void)h;mock_cursor=p;return 1;}
static int FillConsoleOutputCharacterA(HANDLE h,CHAR c,DWORD n,COORD p,DWORD *done){(void)h;(void)c;assert(p.X==5&&p.Y>=10&&p.Y<=34);mock_fill+=n;*done=n;return 1;}
static int FillConsoleOutputAttribute(HANDLE h,WORD attr,DWORD n,COORD p,DWORD *done){(void)h;(void)p;assert(attr==7);*done=n;return 1;}
static int ScrollConsoleScreenBufferA(HANDLE h,const SMALL_RECT *r,const SMALL_RECT *clip,COORD p,const CHAR_INFO *fill){(void)h;(void)clip;(void)fill;assert(p.Y<r->Top);return 1;}
static void file_put16(uint8_t *p,uint16_t v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);}
