#ifndef OS2HOST_FONT_QUERY_H
#define OS2HOST_FONT_QUERY_H
#include <stddef.h>
/* Flat OS/2 FONTMETRICS: 208-byte base plus atoms and PANOSE = 228 bytes.
 * TELNETPM requests all 228 bytes; older callers may request only the base. */
typedef struct O2FONTMETRICS {
    char szFamilyname[32];
    char szFacename[32];
    WORD idRegistry;
    WORD usCodePage;
    LONG lEmHeight;
    LONG lXHeight;
    LONG lMaxAscender;
    LONG lMaxDescender;
    LONG lLowerCaseAscent;
    LONG lLowerCaseDescent;
    LONG lInternalLeading;
    LONG lExternalLeading;
    LONG lAveCharWidth;
    LONG lMaxCharInc;
    LONG lEmInc;
    LONG lMaxBaselineExt;
    short sCharSlope;
    short sInlineDir;
    short sCharRot;
    WORD usWeightClass;
    WORD usWidthClass;
    short sXDeviceRes;
    short sYDeviceRes;
    short sFirstChar;
    short sLastChar;
    short sDefaultChar;
    short sBreakChar;
    short sNominalPointSize;
    short sMinimumPointSize;
    short sMaximumPointSize;
    WORD fsType;
    WORD fsDefn;
    WORD fsSelection;
    WORD fsCapabilities;
    LONG lSubscriptXSize;
    LONG lSubscriptYSize;
    LONG lSubscriptXOffset;
    LONG lSubscriptYOffset;
    LONG lSuperscriptXSize;
    LONG lSuperscriptYSize;
    LONG lSuperscriptXOffset;
    LONG lSuperscriptYOffset;
    LONG lUnderscoreSize;
    LONG lUnderscorePosition;
    LONG lStrikeoutSize;
    LONG lStrikeoutPosition;
    short sKerningPairs;
    short sFamilyClass;
    LONG lMatch;
    LONG FamilyNameAtom,FaceNameAtom;
    BYTE panose[12];
} O2FONTMETRICS;
typedef char PMFontSizeCheck[sizeof(O2FONTMETRICS)==228?1:-1];
typedef char PMFontTypeCheck[offsetof(O2FONTMETRICS,fsType)==144?1:-1];
typedef char PMFontSelectionCheck[offsetof(O2FONTMETRICS,fsSelection)==148?1:-1];
LONG pm_query_fonts(HDC,DWORD,const char *,LONG *,LONG,void *);
#endif
