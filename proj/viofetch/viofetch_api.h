#ifndef VIOFETCH_API_H
#define VIOFETCH_API_H
/* The DOS/NLS ABI stays 32-bit; only VIO crosses the C/386 far16 bridge. */
#if defined(M_I386) || defined(_WIN32)
typedef unsigned long ULONG;
#else
#include <stdint.h>
typedef uint32_t ULONG;
#endif
typedef ULONG APIRET;
typedef unsigned short USHORT;
typedef unsigned char BYTE;
#if defined(M_I386) && !defined(_WIN32)
#define VIOCALL _far16 _pascal
#define VIOPTR _far16
#define VioWrtCharStrAtt VIOWRTCHARSTRATT
#define VioGetMode VIOGETMODE
#define VioScrollUp VIOSCROLLUP
#define VioSetCurPos VIOSETCURPOS
#else
#define VIOCALL
#define VIOPTR
#endif
#ifndef __cdecl
#define __cdecl
#endif
#pragma pack(2)
typedef struct {
    USHORT cb;
    BYTE fbType, color;
    USHORT col, row, hres, vres;
} VIOMODEPREFIX;
typedef struct {
    ULONG idFileSystem, cSectorUnit, cUnit, cUnitAvail;
    USHORT cbSector;
} FSALLOCATE;
typedef struct { ULONG country, codepage; } COUNTRYCODE;
typedef struct {
    ULONG country, codepage, fsDateFmt;
    char szCurrency[5], szThousandsSeparator[2], szDecimal[2];
    char szDateSeparator[2], szTimeSeparator[2];
    BYTE fsCurrencyFmt, cDecimalPlace, fsTimeFmt;
    USHORT abReserved1[2];
    char szDataSeparator[2];
    USHORT abReserved2[5];
} COUNTRYINFO;
#pragma pack()
USHORT VIOCALL VioWrtCharStrAtt(char VIOPTR *, USHORT, USHORT, USHORT, BYTE VIOPTR *, USHORT);
USHORT VIOCALL VioGetMode(VIOMODEPREFIX VIOPTR *, USHORT);
USHORT VIOCALL VioScrollUp(USHORT, USHORT, USHORT, USHORT, USHORT, char VIOPTR *, USHORT);
USHORT VIOCALL VioSetCurPos(USHORT, USHORT, USHORT);
APIRET __cdecl DosQuerySysInfo(ULONG, ULONG, void *, ULONG);
APIRET __cdecl DosQueryCp(ULONG, ULONG *, ULONG *);
APIRET __cdecl DosQueryCtryInfo(ULONG, const COUNTRYCODE *, COUNTRYINFO *, ULONG *);
APIRET __cdecl DosQueryCurrentDisk(ULONG *, ULONG *);
APIRET __cdecl DosQueryFSInfo(ULONG, ULONG, void *, ULONG);
#endif
