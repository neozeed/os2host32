#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../dlls/pmgpi/font_query.c"
unsigned GetACP(void) { return 1252; }
unsigned GetOEMCP(void) { return 437; }
int GetDeviceCaps(HDC dc,int what) { (void)what; return dc==(HDC)1?96:0; }
int MulDiv(int a,int b,int c) { return (a*b+c/2)/c; }
int EnumFontFamiliesA(HDC dc,const char *face,FONTENUMPROCA callback,LPARAM context)
{
    LOGFONTA lf; TEXTMETRICA tm; int i;
    (void)dc;
    for(i=0;i<4;++i) {
        memset(&lf,0,sizeof(lf)); memset(&tm,0,sizeof(tm));
        strcpy(lf.lfFaceName,i<2?"Fixed fixture":i==2?"Variable fixture":"Device fixture");
        if(face && strcmp(face,lf.lfFaceName)) continue;
        if(!face && i==1) continue;
        tm.tmHeight=12; tm.tmAscent=9; tm.tmDescent=3;
        tm.tmAveCharWidth=tm.tmMaxCharWidth=8;
        tm.tmWeight=i==1?700:400; tm.tmItalic=(BYTE)(i==1);
        tm.tmFirstChar=32;tm.tmLastChar=255;tm.tmDefaultChar='?';tm.tmBreakChar=' ';
        tm.tmPitchAndFamily=(BYTE)(i==2?TMPF_FIXED_PITCH:0);tm.tmCharSet=OEM_CHARSET;
        if(!callback(&lf,&tm,i==3?DEVICE_FONTTYPE:TRUETYPE_FONTTYPE,context)) return 0;
    }
    return 1;
}
#define CHECK(c) do { ++checks; if(!(c)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#c); return 1; } } while(0)
int main(void)
{
    struct { DWORD before; O2FONTMETRICS fonts[4]; DWORD after; } out;
    LONG count,remaining; int checks=0; unsigned char small[48],extended[252];
    CHECK(sizeof(O2FONTMETRICS)==228 && offsetof(O2FONTMETRICS,lAveCharWidth)==100);
    count=0;CHECK(pm_query_fonts((HDC)1,1,NULL,&count,0,NULL)==4 && count==0);
    count=2;out.before=0x12345678;out.after=0xabcdef01;
    remaining=pm_query_fonts((HDC)1,1,NULL,&count,228,out.fonts);
    CHECK(remaining==2 && count==2 && out.before==0x12345678 && out.after==0xabcdef01);
    CHECK(!strcmp(out.fonts[0].szFacename,"Fixed fixture") && out.fonts[0].fsType==1);
    CHECK(out.fonts[0].lMaxBaselineExt==12 && out.fonts[0].lAveCharWidth==8 && out.fonts[0].usCodePage==437);
    CHECK(out.fonts[0].sLastChar==255 && out.fonts[0].usWeightClass==4 && out.fonts[0].fsSelection==0);
    CHECK(out.fonts[1].fsSelection==0x21 && out.fonts[1].usWeightClass==7);
    count=4;CHECK(!pm_query_fonts((HDC)1,1,NULL,&count,228,out.fonts) && count==4);
    CHECK(out.fonts[2].fsType==0 && !(out.fonts[3].fsDefn&0x8000));
    count=4;CHECK(!pm_query_fonts((HDC)1,1,"Fixed fixture",&count,228,out.fonts) && count==2);
    count=0;CHECK(pm_query_fonts((HDC)1,9,NULL,&count,0,NULL)==3 && count==0);
    count=0;CHECK(pm_query_fonts((HDC)1,5,NULL,&count,0,NULL)==1);
    count=0;CHECK(!pm_query_fonts((HDC)1,13,NULL,&count,0,NULL));
    count=4;CHECK(!pm_query_fonts((HDC)1,2,NULL,&count,228,out.fonts) && count==0);
    count=4;CHECK(!pm_query_fonts((HDC)1,1,"absent",&count,228,out.fonts) && count==0);
    memset(small,0xaa,sizeof(small));count=2;
    CHECK(pm_query_fonts((HDC)1,1,NULL,&count,16,small)==2 && count==2 && small[32]==0xaa);
    memset(extended,0xaa,sizeof(extended));count=1;
    CHECK(pm_query_fonts((HDC)1,1,NULL,&count,248,extended)==3 && count==1 && extended[247]==0 && extended[248]==0xaa);
    count=-1;CHECK(pm_query_fonts((HDC)1,1,NULL,&count,228,out.fonts)==-1);
    count=1;CHECK(pm_query_fonts((HDC)1,1,NULL,&count,0,out.fonts)==-1);
    CHECK(pm_query_fonts((HDC)1,1,NULL,&count,228,NULL)==-1);
    CHECK(pm_query_fonts((HDC)1,0,NULL,&count,228,out.fonts)==-1);
    CHECK(pm_query_fonts((HDC)1,16,NULL,&count,228,out.fonts)==-1);
    CHECK(pm_query_fonts((HDC)2,1,NULL,&count,228,out.fonts)==-1);
    printf("font-query-host-check: %d checks PASS (production enumerator, controlled GDI fixtures)\n",checks);return 0;
}
