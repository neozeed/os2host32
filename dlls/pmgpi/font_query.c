/* GDI font enumeration -> original 32-bit OS/2 FONTMETRICS.
 * Count-only queries enumerate the same fonts as copying queries. The return
 * is the number NOT copied; *requested is the number actually copied. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string.h>
#include <limits.h>
#include "font_query.h"

struct FontQuery {
    HDC dc; DWORD flags; LONG capacity,stride,total,copied;
    int dpi_x,dpi_y,failed; unsigned char *out;
};
static WORD font_codepage(BYTE charset)
{
    switch(charset) {
    case OEM_CHARSET: return (WORD)GetOEMCP();
    case SYMBOL_CHARSET: return 0;
    case SHIFTJIS_CHARSET: return 932;
    case HANGEUL_CHARSET: return 949;
    case GB2312_CHARSET: return 936;
    case CHINESEBIG5_CHARSET: return 950;
    default: return (WORD)GetACP();
    }
}
static int CALLBACK font_record(const LOGFONTA *lf,const TEXTMETRICA *tm,
                                DWORD type,LPARAM context)
{
    struct FontQuery *q=(struct FontQuery *)context;
    O2FONTMETRICS fm; LONG bytes,em,points;
    int device=(type&DEVICE_FONTTYPE)!=0;
    if((device && (q->flags&8)) || (!device && (q->flags&4))) return 1;
    if(q->total==0x7fffffffL) { q->failed=1; return 0; }
    ++q->total;
    if(q->copied>=q->capacity) return 1;
    memset(&fm,0,sizeof(fm));
    memcpy(fm.szFamilyname,lf->lfFaceName,31); memcpy(fm.szFacename,lf->lfFaceName,31);
    fm.usCodePage=font_codepage(tm->tmCharSet);
    em=tm->tmHeight-tm->tmInternalLeading; if(em<1) em=tm->tmHeight;
    fm.lEmHeight=em; fm.lXHeight=tm->tmAscent/2;
    fm.lMaxAscender=tm->tmAscent; fm.lMaxDescender=tm->tmDescent;
    fm.lLowerCaseAscent=tm->tmAscent; fm.lLowerCaseDescent=tm->tmDescent;
    fm.lInternalLeading=tm->tmInternalLeading; fm.lExternalLeading=tm->tmExternalLeading;
    fm.lAveCharWidth=tm->tmAveCharWidth; fm.lMaxCharInc=tm->tmMaxCharWidth;
    fm.lEmInc=tm->tmAveCharWidth; fm.lMaxBaselineExt=tm->tmHeight;
    fm.usWeightClass=(WORD)((tm->tmWeight+50)/100);
    if(fm.usWeightClass<1) fm.usWeightClass=1;
    if(fm.usWeightClass>9) fm.usWeightClass=9;
    fm.usWidthClass=5;
    fm.sXDeviceRes=(short)q->dpi_x; fm.sYDeviceRes=(short)q->dpi_y;
    fm.sFirstChar=(short)(unsigned char)tm->tmFirstChar;
    fm.sLastChar=(short)(unsigned char)tm->tmLastChar;
    fm.sDefaultChar=(short)(unsigned char)tm->tmDefaultChar;
    fm.sBreakChar=(short)(unsigned char)tm->tmBreakChar;
    points=MulDiv(em,720,q->dpi_y); if(points>32767) points=32767;
    fm.sNominalPointSize=(short)points;
    fm.sMinimumPointSize=fm.sMaximumPointSize=fm.sNominalPointSize;
    /* GDI's TMPF_FIXED_PITCH bit is set for VARIABLE pitch fonts. */
    fm.fsType=(WORD)((tm->tmPitchAndFamily&TMPF_FIXED_PITCH)?0:1);
    fm.fsDefn=(WORD)(((type&TRUETYPE_FONTTYPE)?1:0) | (device?0:0x8000));
    fm.fsSelection=(WORD)((tm->tmItalic?1:0) | (tm->tmUnderlined?2:0) |
        (tm->tmStruckOut?0x10:0) | (tm->tmWeight>=FW_BOLD?0x20:0));
    bytes=q->stride; if(bytes>(LONG)sizeof(fm)) bytes=sizeof(fm);
    memset(q->out+(size_t)q->copied*q->stride,0,(size_t)q->stride);
    memcpy(q->out+(size_t)q->copied*q->stride,&fm,(size_t)bytes);
    ++q->copied; return 1;
}
static int CALLBACK font_family(const LOGFONTA *lf,const TEXTMETRICA *tm,
                                DWORD type,LPARAM context)
{
    struct FontQuery *q=(struct FontQuery *)context;
    (void)tm; (void)type;
    /* First pass visits families; the named pass includes their styles and
     * raster sizes instead of reporting only one representative per family. */
    EnumFontFamiliesA(q->dc,lf->lfFaceName,font_record,context);
    return !q->failed;
}
LONG pm_query_fonts(HDC dc,DWORD flags,const char *face,LONG *requested,
                     LONG stride,void *out)
{
    struct FontQuery q;
    if(!dc || !requested || *requested<0 || (flags&~15UL) || !(flags&3)) return -1;
    if(*requested && (!out || stride<=0 || (DWORD)*requested>0x7fffffffUL/(DWORD)stride)) return -1;
    memset(&q,0,sizeof(q)); q.dc=dc; q.flags=flags; q.capacity=*requested;
    q.stride=stride; q.out=(unsigned char *)out;
    q.dpi_x=GetDeviceCaps(dc,LOGPIXELSX); q.dpi_y=GetDeviceCaps(dc,LOGPIXELSY);
    if(q.dpi_x<=0 || q.dpi_y<=0) return -1;
    /* This personality has no private-font loader. Host-installed fonts are
     * its public set; QF_PRIVATE alone therefore has an empty result. */
    if(flags&1) EnumFontFamiliesA(dc,face,face?font_record:font_family,(LPARAM)&q);
    if(q.failed) return -1;
    *requested=q.copied; return q.total-q.copied;
}
