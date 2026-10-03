#ifndef PMPROFILE_TEST_WINDOWS_H
#define PMPROFILE_TEST_WINDOWS_H
#include <stddef.h>
#include <stdint.h>
typedef uint32_t DWORD;
typedef uint16_t WORD;
typedef unsigned char BYTE;
typedef void *HDC;
typedef int16_t SHORT;
typedef int32_t LONG;
typedef int INT;
typedef int BOOL;
typedef void *HINSTANCE;
typedef void *LPVOID;
typedef void *HWND;
typedef void *HANDLE;
typedef uintptr_t ULONG_PTR;
typedef intptr_t LPARAM;
typedef int CRITICAL_SECTION;
#define CALLBACK
#define GA_ROOT 2
#define DLL_PROCESS_ATTACH 1
#define DLL_PROCESS_DETACH 0
#define TRUE 1
#define FALSE 0
#define WINAPI
#define __cdecl
#define MAX_PATH 260
#define HEAP_ZERO_MEMORY 8
void *GetProcessHeap(void);
void *HeapAlloc(void *,DWORD,size_t);
BOOL HeapFree(void *,DWORD,void *);
DWORD GetModuleFileNameA(void *,char *,DWORD);
DWORD GetEnvironmentVariableA(const char *,char *,DWORD);
DWORD GetFullPathNameA(const char *,DWORD,char *,char **);
DWORD GetPrivateProfileStringA(const char *,const char *,const char *,char *,DWORD,const char *);
BOOL WritePrivateProfileStringA(const char *,const char *,const char *,const char *);
unsigned GetPrivateProfileIntA(const char *,const char *,INT,const char *);
void InitializeCriticalSection(CRITICAL_SECTION *);
void DeleteCriticalSection(CRITICAL_SECTION *);
void EnterCriticalSection(CRITICAL_SECTION *);
void LeaveCriticalSection(CRITICAL_SECTION *);
BOOL IsWindow(HWND);
HWND GetAncestor(HWND,unsigned);
DWORD GetCurrentProcessId(void);
DWORD GetWindowThreadProcessId(HWND,DWORD *);
BOOL ProcessIdToSessionId(DWORD,DWORD *);
HANDLE GetPropA(HWND,const char *);
BOOL SetPropA(HWND,const char *,HANDLE);
int GetWindowTextA(HWND,char *,int);
BOOL EnumWindows(BOOL (CALLBACK *)(HWND,LPARAM),LPARAM);
HANDLE RemovePropA(HWND,const char *);
HWND GetParent(HWND);
#define LF_FACESIZE 32
#define LOGPIXELSX 88
#define LOGPIXELSY 90
#define OEM_CHARSET 255
#define SYMBOL_CHARSET 2
#define SHIFTJIS_CHARSET 128
#define HANGEUL_CHARSET 129
#define GB2312_CHARSET 134
#define CHINESEBIG5_CHARSET 136
#define DEVICE_FONTTYPE 2
#define TRUETYPE_FONTTYPE 4
#define TMPF_FIXED_PITCH 1
#define FW_BOLD 700
typedef struct {
    LONG lfHeight,lfWidth,lfEscapement,lfOrientation,lfWeight;
    BYTE lfItalic,lfUnderline,lfStrikeOut,lfCharSet;
    BYTE lfOutPrecision,lfClipPrecision,lfQuality,lfPitchAndFamily;
    char lfFaceName[32];
} LOGFONTA;
typedef struct {
    LONG tmHeight,tmAscent,tmDescent,tmInternalLeading,tmExternalLeading;
    LONG tmAveCharWidth,tmMaxCharWidth,tmWeight,tmOverhang,tmDigitizedAspectX,tmDigitizedAspectY;
    BYTE tmFirstChar,tmLastChar,tmDefaultChar,tmBreakChar;
    BYTE tmItalic,tmUnderlined,tmStruckOut,tmPitchAndFamily,tmCharSet;
} TEXTMETRICA;
typedef int (CALLBACK *FONTENUMPROCA)(const LOGFONTA *,const TEXTMETRICA *,DWORD,LPARAM);
int EnumFontFamiliesA(HDC,const char *,FONTENUMPROCA,LPARAM);
int GetDeviceCaps(HDC,int);
unsigned GetACP(void);
unsigned GetOEMCP(void);
int MulDiv(int,int,int);
#endif
