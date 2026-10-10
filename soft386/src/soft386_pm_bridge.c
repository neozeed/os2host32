/* Soft386 I386-H2F: PM personalities remain unmodified host-service DLLs.
 * This file is an ABI/marshalling bridge, not a second Presentation Manager.
 * Calls need an ABI descriptor so the guest stack can be decoded safely; the
 * descriptor is representation metadata, not permission to reimplement the
 * API. Unknown pointer graphs fail closed at this boundary. */
#include "soft386_pm_bridge.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#ifdef _WIN32
typedef int (WINAPI *Soft386GetObjectAFn)(HGDIOBJ,int,LPVOID);
typedef BOOL (WINAPI *Soft386DeleteObjectFn)(HGDIOBJ);
static void trace_native_pointer(const char *where, uint32_t guest_token, uintptr_t native)
{
    ICONINFO ii;
    BITMAP bm;
    HMODULE gdi;
    Soft386GetObjectAFn pGetObjectA;
    Soft386DeleteObjectFn pDeleteObject;
    int mw=0,mh=0,cw=0,ch=0;
    memset(&ii,0,sizeof(ii));
    if(!native){
        fprintf(stderr,"soft386: PM pointer %s guest=%08X native=00000000\n",where,guest_token);
        return;
    }
    if(!GetIconInfo((HICON)native,&ii)){
        fprintf(stderr,"soft386: PM pointer %s guest=%08X native=%08lX GetIconInfo=FAIL err=%lu\n",
                where,guest_token,(unsigned long)(DWORD)native,(unsigned long)GetLastError());
        return;
    }

    /* Trace-only GDI inspection must not add a hard GDI32 import to the
     * Soft386 vessel.  Resolve these helpers lazily through KERNEL32. */
    gdi=GetModuleHandleA("gdi32.dll");
    if(!gdi)gdi=LoadLibraryA("gdi32.dll");
    pGetObjectA=NULL;
    pDeleteObject=NULL;
    if(gdi){
        pGetObjectA=(Soft386GetObjectAFn)(void *)GetProcAddress(gdi,"GetObjectA");
        pDeleteObject=(Soft386DeleteObjectFn)(void *)GetProcAddress(gdi,"DeleteObject");
    }
    if(pGetObjectA){
        memset(&bm,0,sizeof(bm));
        if(ii.hbmMask&&pGetObjectA(ii.hbmMask,sizeof(bm),&bm)==sizeof(bm)){mw=bm.bmWidth;mh=bm.bmHeight;}
        memset(&bm,0,sizeof(bm));
        if(ii.hbmColor&&pGetObjectA(ii.hbmColor,sizeof(bm),&bm)==sizeof(bm)){cw=bm.bmWidth;ch=bm.bmHeight;}
    }
    fprintf(stderr,"soft386: PM pointer %s guest=%08X native=%08lX fIcon=%u hot=%lu,%lu mask=%dx%d color=%dx%d\n",
            where,guest_token,(unsigned long)(DWORD)native,(unsigned)ii.fIcon,
            (unsigned long)ii.xHotspot,(unsigned long)ii.yHotspot,mw,mh,cw,ch);
    if(pDeleteObject){
        if(ii.hbmMask)pDeleteObject(ii.hbmMask);
        if(ii.hbmColor)pDeleteObject(ii.hbmColor);
    }
}
static void trace_native_static(const char *where, uintptr_t hwnd)
{
    RECT wr,cr;
    LRESULT image=0;
    int wx=0,wy=0,ww=0,wh=0,cw=0,ch=0;
    if(!hwnd)return;
    memset(&wr,0,sizeof(wr));memset(&cr,0,sizeof(cr));
    if(GetWindowRect((HWND)hwnd,&wr)){wx=wr.left;wy=wr.top;ww=wr.right-wr.left;wh=wr.bottom-wr.top;}
    if(GetClientRect((HWND)hwnd,&cr)){cw=cr.right-cr.left;ch=cr.bottom-cr.top;}
    image=SendMessageA((HWND)hwnd,STM_GETICON,0,0);
    fprintf(stderr,"soft386: PM static %s hwnd=%08lX win=%d,%d %dx%d client=%dx%d STM_GETICON=%08lX visible=%d\n",
            where,(unsigned long)(DWORD)hwnd,wx,wy,ww,wh,cw,ch,
            (unsigned long)(DWORD)image,IsWindowVisible((HWND)hwnd)?1:0);
}
#endif
#ifndef __cdecl
#define __cdecl
#endif
#define PM_MAX_BUFFER 0x01000000u
#define PM_MAX_STRING 65536u
#define PM_MAX_ARGS 13u
enum PmAbiFlags {
    PM_ABI_NONE = 0u,
    PM_ABI_CUSTOM = 1u << 0,
    PM_ABI_CALLBACK = 1u << 1,
    PM_ABI_SCHEDULER = 1u << 2
};
struct PmSig { unsigned module,ordinal; const char *name,*sig; unsigned result; unsigned flags; };
/* W/A/Q/P/D/B/R/K/X/E are typed handles. s is bounded ASCIIZ.
 * r/o/v: RECTL in/out/inout, t/u: POINTL in/out, l: ULONG inout.
 * '?' selects an explicitly marshalled variable/nested structure below. */
static const struct PmSig signatures[]={
    {0,776,"WinLoadAccelTable","A..",9,PM_ABI_NONE},
    {0,798,"WinQueryAccelTable","AW",9,PM_ABI_NONE},
    {0,850,"WinSetAccelTable","AXW",0,PM_ABI_NONE},
    {0,779,"WinLoadMessage","A...?",0,PM_ABI_NONE},
    {0,812,"WinQueryCursorInfo","W?",0,PM_ABI_NONE},
    {0,890,"WinTrackRect","WP?",0,PM_ABI_NONE},
    {0,707,"WinCloseClipbrd","A",0,PM_ABI_NONE},
    {0,733,"WinEmptyClipbrd","A",0,PM_ABI_NONE},
    {0,793,"WinOpenClipbrd","A",0,PM_ABI_NONE},
    {0,807,"WinQueryClipbrdFmtInfo","A.l",0,PM_ABI_NONE},
    {1,586,"GpiQueryFonts","P.sl.?",0,PM_ABI_NONE},
    {0,701,"WinAlarm","W.",0,PM_ABI_NONE},
    {0,702,"WinBeginEnumWindows","W",10,PM_ABI_NONE},
    {0,703,"WinBeginPaint","WPo",4,PM_ABI_NONE},
    {0,710,"WinCopyRect","Aor",0,PM_ABI_NONE},
    {0,715,"WinCreateCursor","W.....r",0,PM_ABI_NONE},
    {0,716,"WinCreateMsgQueue","A.",3,PM_ABI_SCHEDULER},
    {0,725,"WinDestroyCursor","W",0,PM_ABI_NONE},
    {0,726,"WinDestroyMsgQueue","Q",0,PM_ABI_SCHEDULER},
    {0,727,"WinDestroyPointer","K",0,PM_ABI_NONE},
    {0,728,"WinDestroyWindow","W",0,PM_ABI_NONE},
    {0,729,"WinDismissDlg","W.",0,PM_ABI_NONE},
    {0,730,"WinDrawBitmap","PBr?...",0,PM_ABI_NONE},
    {0,735,"WinEnableWindow","W.",0,PM_ABI_NONE},
    {0,736,"WinEnableWindowUpdate","W.",0,PM_ABI_NONE},
    {0,737,"WinEndEnumWindows","E",0,PM_ABI_NONE},
    {0,738,"WinEndPaint","P",0,PM_ABI_NONE},
    {0,743,"WinFillRect","Pr.",0,PM_ABI_NONE},
    {0,746,"WinFocusChange","WW.",0,PM_ABI_NONE},
    {0,751,"WinGetErrorInfo","A",0,PM_ABI_CUSTOM},
    {0,752,"WinGetKeyState","W.",0,PM_ABI_NONE},
    {0,753,"WinGetLastError","A",0,PM_ABI_NONE},
    {0,756,"WinGetNextWindow","E",1,PM_ABI_NONE},
    {0,757,"WinGetPS","W",4,PM_ABI_NONE},
    {0,763,"WinInitialize",".",2,PM_ABI_NONE},
    {0,764,"WinIntersectRect","Aorr",0,PM_ABI_NONE},
    {0,765,"WinInvalidateRect","Wr.",0,PM_ABI_NONE},
    {0,766,"WinInvalidateRegion","WR.",0,PM_ABI_NONE},
    {0,767,"WinInvertRect","Pr",0,PM_ABI_NONE},
    {0,773,"WinIsWindowEnabled","W",0,PM_ABI_NONE},
    {0,775,"WinIsWindowVisible","W",0,PM_ABI_NONE},
    {0,778,"WinLoadMenu","W..",1,PM_ABI_NONE},
    {0,780,"WinLoadPointer","W..",8,PM_ABI_NONE},
    {0,781,"WinLoadString","A...?",0,PM_ABI_NONE},
    {0,788,"WinMapWindowPoints","WW?.",0,PM_ABI_NONE},
    {0,789,"WinMessageBox","WWss..",0,PM_ABI_NONE},
    {0,794,"WinOpenWindowDC","W",5,PM_ABI_NONE},
    {0,797,"WinPtInRect","Art",0,PM_ABI_NONE},
    {0,804,"WinQueryCapture","W",1,PM_ABI_NONE},
    {0,814,"WinQueryDlgItemShort","W.?.",0,PM_ABI_NONE},
    {0,815,"WinQueryDlgItemText","W..?",0,PM_ABI_NONE},
    {0,817,"WinQueryFocus","W",1,PM_ABI_NONE},
    {0,821,"WinQueryPointer","W",8,PM_ABI_NONE},
    {0,822,"WinQueryPointerInfo","K?",0,PM_ABI_NONE},
    {0,823,"WinQueryPointerPos","Wu",0,PM_ABI_NONE},
    {0,828,"WinQuerySysPointer","W..",8,PM_ABI_NONE},
    {0,829,"WinQuerySysValue","W.",0,PM_ABI_NONE},
    {0,832,"WinQueryUpdateRegion","WR",0,PM_ABI_NONE},
    {0,833,"WinQueryVersion","A",0,PM_ABI_NONE},
    {0,834,"WinQueryWindow","W.",1,PM_ABI_NONE},
    {0,837,"WinQueryWindowPos","W?",0,PM_ABI_NONE},
    {0,838,"WinQueryWindowProcess","W??",0,PM_ABI_NONE},
    {0,840,"WinQueryWindowRect","Wo",0,PM_ABI_NONE},
    {0,841,"WinQueryWindowText","W.?",0,PM_ABI_NONE},
    {0,842,"WinQueryWindowTextLength","W",0,PM_ABI_NONE},
    {0,843,"WinQueryWindowULong","W.",0,PM_ABI_NONE},
    {0,844,"WinQueryWindowUShort","W.",0,PM_ABI_NONE},
    {0,848,"WinReleasePS","P",0,PM_ABI_NONE},
    {0,849,"WinScrollWindow","W..rrRo.",0,PM_ABI_NONE},
    {0,852,"WinSetCapture","WW",0,PM_ABI_NONE},
    {0,858,"WinSetDlgItemShort","W...",0,PM_ABI_NONE},
    {0,859,"WinSetDlgItemText","W.s",0,PM_ABI_NONE},
    {0,860,"WinSetFocus","WW",0,PM_ABI_NONE},
    {0,865,"WinSetParent","WW.",0,PM_ABI_NONE},
    {0,866,"WinSetPointer","WK",0,PM_ABI_NONE},
    {0,868,"WinSetRect","Ao....",0,PM_ABI_NONE},
    {0,872,"WinSetSysModalWindow","WW",0,PM_ABI_NONE},
    {0,875,"WinSetWindowPos","WW.....",0,PM_ABI_NONE},
    {0,877,"WinSetWindowText","Ws",0,PM_ABI_NONE},
    {0,878,"WinSetWindowULong","W..",0,PM_ABI_NONE},
    {0,880,"WinShowCursor","W.",0,PM_ABI_NONE},
    {0,867,"WinSetPointerPos","W..",0,PM_ABI_NONE},
    {0,881,"WinShowPointer","W.",0,PM_ABI_NONE},
    {0,883,"WinShowWindow","W.",0,PM_ABI_NONE},
    {0,884,"WinStartTimer","AW..",0,PM_ABI_NONE},
    {0,885,"WinStopTimer","AW.",0,PM_ABI_NONE},
    {0,888,"WinTerminate","A",0,PM_ABI_SCHEDULER},
    {0,891,"WinUnionRect","Aorr",0,PM_ABI_NONE},
    {0,892,"WinUpdateWindow","W",0,PM_ABI_NONE},
    {0,895,"WinValidateRect","Wr.",0,PM_ABI_NONE},
    {0,896,"WinValidateRegion","WR.",0,PM_ABI_NONE},
    {0,899,"WinWindowFromID","W.",1,PM_ABI_NONE},
    {0,902,"WinPostQueueMsg","Q...",0,PM_ABI_NONE},
    {0,903,"WinSendDlgItemMsg","W....",0,PM_ABI_CUSTOM},
    {0,908,"WinCreateStdWindow","W.lss...?",1,PM_ABI_NONE},
    {0,909,"WinCreateWindow","Wss.....WW.??",1,PM_ABI_NONE},
    {0,910,"WinDefDlgProc","W...",0,PM_ABI_NONE},
    {0,911,"WinDefWindowProc","W...",0,PM_ABI_NONE},
    {0,912,"WinDispatchMsg","A?",0,PM_ABI_CUSTOM|PM_ABI_SCHEDULER},
    {0,913,"WinDrawText","P.?v...",0,PM_ABI_NONE},
    {0,915,"WinGetMsg","A?W..",0,PM_ABI_CUSTOM|PM_ABI_SCHEDULER},
    {0,918,"WinPeekMsg","A?W...",0,PM_ABI_CUSTOM|PM_ABI_SCHEDULER},
    {0,919,"WinPostMsg","W...",0,PM_ABI_NONE},
    {0,920,"WinSendMsg","W...",0,PM_ABI_CUSTOM},
    {0,923,"WinDlgBox","WW?..?",0,PM_ABI_CUSTOM|PM_ABI_CALLBACK},
    {0,924,"WinLoadDlg","WW?..?",1,PM_ABI_CUSTOM|PM_ABI_CALLBACK},
    {0,926,"WinRegisterClass","As?..",0,PM_ABI_CALLBACK},
    {0,929,"WinSubclassWindow","W?",0,PM_ABI_CUSTOM|PM_ABI_CALLBACK},
    {1,351,"GpiAssociate","PD",0,PM_ABI_NONE},
    {1,354,"GpiBeginPath","P.",0,PM_ABI_NONE},
    {1,355,"GpiBitBlt","PP.?..",0,PM_ABI_NONE},
    {1,356,"GpiBox","P.t..",0,PM_ABI_NONE},
    {1,359,"GpiCharStringAt","Pt.?",0,PM_ABI_NONE},
    {1,362,"GpiCombineRegion","PRRR.",0,PM_ABI_NONE},
    {1,364,"GpiConvert","P...?",0,PM_ABI_NONE},
    {1,368,"GpiCreateLogFont","Ps.?",0,PM_ABI_NONE},
    {1,369,"GpiCreatePS","ADt.",4,PM_ABI_NONE},
    {1,370,"GpiCreateRegion","P.?",7,PM_ABI_NONE},
    {1,371,"GpiDeleteBitmap","B",0,PM_ABI_NONE},
    {1,378,"GpiDeleteSetId","P.",0,PM_ABI_NONE},
    {1,379,"GpiDestroyPS","P",0,PM_ABI_NONE},
    {1,387,"GpiEndPath","P",0,PM_ABI_NONE},
    {1,398,"GpiLine","Pt",0,PM_ABI_NONE},
    {1,399,"GpiLoadBitmap","P....",6,PM_ABI_NONE},
    {1,404,"GpiMove","Pt",0,PM_ABI_NONE},
    {1,409,"GpiPaintRegion","PR",0,PM_ABI_NONE},
    {1,417,"GpiPolySpline","P.?",0,PM_ABI_NONE},
    {1,443,"GpiQueryDefaultViewMatrix","P.?",0,PM_ABI_NONE},
    {1,453,"GpiQueryFontMetrics","P.?",0,PM_ABI_NONE},
    {1,476,"GpiQueryPel","Pt",0,PM_ABI_NONE},
    {1,481,"GpiQueryRegionBox","PRo",0,PM_ABI_NONE},
    {1,489,"GpiQueryTextBox","P.?.?",0,PM_ABI_NONE},
    {1,492,"GpiQueryWidthTable","P..?",0,PM_ABI_NONE},
    {1,503,"GpiSetAttrMode","P.",0,PM_ABI_NONE},
    {1,504,"GpiSetBackColor","P.",0,PM_ABI_NONE},
    {1,505,"GpiSetBackMix","P.",0,PM_ABI_NONE},
    {1,506,"GpiSetBitmap","PB",6,PM_ABI_NONE},
    {1,513,"GpiSetCharSet","P.",0,PM_ABI_NONE},
    {1,515,"GpiSetClipPath","P..",0,PM_ABI_NONE},
    {1,516,"GpiSetClipRegion","PR?",0,PM_ABI_NONE},
    {1,517,"GpiSetColor","P.",0,PM_ABI_NONE},
    {1,519,"GpiSetCurrentPosition","Pt",0,PM_ABI_NONE},
    {1,520,"GpiSetDefaultViewMatrix","P.?.",0,PM_ABI_NONE},
    {1,530,"GpiSetLineType","P.",0,PM_ABI_NONE},
    {1,544,"GpiSetPel","Pt",0,PM_ABI_NONE},
    {1,546,"GpiSetRegion","PR.?",0,PM_ABI_NONE},
    {1,588,"GpiSetAttrs","P...?",0,PM_ABI_NONE},
    {1,592,"GpiCreateLogColorTable","P....?",0,PM_ABI_NONE},
    {1,598,"GpiCreateBitmap","P?.??",6,PM_ABI_NONE},
    {1,599,"GpiQueryBitmapBits","P..??",0,PM_ABI_NONE},
    {1,601,"GpiQueryBitmapInfoHeader","B?",0,PM_ABI_NONE},
    {1,602,"GpiSetBitmapBits","P..??",0,PM_ABI_NONE},
    {1,604,"DevCloseDC","D",0,PM_ABI_NONE},
    {1,610,"DevOpenDC","A.s.?D",5,PM_ABI_NONE},
    {1,611,"GpiDestroyRegion","PR",0,PM_ABI_NONE},
    {2,4,"WinFileDlg","WW?",1,PM_ABI_NONE},
    {3,4,"DosInsertMessage","?.?.?.?",0,PM_ABI_NONE},
    {3,6,"DosTrueGetMessage","??.?..s?",0,PM_ABI_NONE},
    {4,123,"WinChangeSwitchEntry","S?",0,PM_ABI_NONE},
    {4,124,"WinQuerySwitchEntry","S?",0,PM_ABI_NONE},
    {4,125,"WinQuerySwitchHandle","W.",12,PM_ABI_NONE},
    {0,854,"WinSetClipbrdData","A?..",0,PM_ABI_NONE},
    {4,101,"PrfQueryProfileSize","Iss?",0,PM_ABI_NONE},
    {4,102,"PrfOpenProfile","As",11,PM_ABI_NONE},
    {4,103,"PrfCloseProfile","I",0,PM_ABI_NONE},
    {4,114,"PrfQueryProfileInt","Iss.",0,PM_ABI_NONE},
    {4,116,"PrfWriteProfileString","Isss",0,PM_ABI_NONE},
    {4,117,"PrfQueryProfileData","Iss??",0,PM_ABI_NONE},
    {4,118,"PrfWriteProfileData","Iss?.",0,PM_ABI_NONE},
    {4,120,"WinAddSwitchEntry","?",12,PM_ABI_NONE},
    {4,129,"WinRemoveSwitchEntry","S",0,PM_ABI_NONE},
    {5,51,"WinCreateHelpInstance","A?",13,PM_ABI_NONE},
    {5,52,"WinDestroyHelpInstance","H",0,PM_ABI_NONE},
    {5,54,"WinAssociateHelpInstance","HW",0,PM_ABI_NONE},
};
static const char *module_names[]={"PMWIN.dll","PMGPI.dll","PMCTLS.dll","MSG.dll","PMSHAPI.dll","HELPMGR.dll"};
static uint16_t get16(const void *p){const uint8_t *b=p;return b[0]|((uint16_t)b[1]<<8);}
static uint32_t get32(const void *p){const uint8_t *b=p;return b[0]|((uint32_t)b[1]<<8)|((uint32_t)b[2]<<16)|((uint32_t)b[3]<<24);}
static void put16(void *p,uint16_t v){uint8_t *b=p;b[0]=(uint8_t)v;b[1]=(uint8_t)(v>>8);}
static void put32(void *p,uint32_t v){uint8_t *b=p;b[0]=(uint8_t)v;b[1]=(uint8_t)(v>>8);b[2]=(uint8_t)(v>>16);b[3]=(uint8_t)(v>>24);}
static const struct PmSig *signature(unsigned m,uint32_t ord){unsigned i;for(i=0;i<sizeof(signatures)/sizeof(*signatures);i++)if(signatures[i].module==m&&signatures[i].ordinal==ord)return &signatures[i];return NULL;}
static int abi_has_guest_memory(const struct PmSig *s){const char *p;if(!s)return 0;for(p=s->sig;*p;p++)if(*p=='s'||*p=='r'||*p=='o'||*p=='v'||*p=='t'||*p=='u'||*p=='l'||*p=='?')return 1;return 0;}
static const char *abi_class(const struct PmSig *s){if(!s)return "missing";if(s->flags&PM_ABI_CALLBACK)return "callback";if(s->flags&PM_ABI_SCHEDULER)return "scheduler";return abi_has_guest_memory(s)?"marshal":"scalar/handle";}
const char *soft386_pm_api_name(unsigned m,uint32_t ord){const struct PmSig *s=signature(m,ord);return s?s->name:NULL;}
uint32_t soft386_pm_named_ordinal(unsigned m,const char *name){unsigned i;for(i=0;i<sizeof(signatures)/sizeof(*signatures);i++)if(signatures[i].module==m&&!strcmp(signatures[i].name,name))return signatures[i].ordinal;return 0;}
static uint32_t tid(struct Soft386PmBridge *b){return b->tid?b->tid(b->opaque):1;}
static struct Soft386PmHandle *handle(struct Soft386PmBridge *b,uint32_t token,unsigned kind){unsigned i;for(i=0;i<S386_PM_HANDLES;i++)if(b->handles[i].token==token&&b->handles[i].kind==kind)return &b->handles[i];return NULL;}
static int native_handle(struct Soft386PmBridge *b,uint32_t token,unsigned kind,uintptr_t *out){struct Soft386PmHandle *h;if(!token||(kind==S386_HWND&&token<=4)||(kind==S386_HINI&&(token==0xffffffffu||token==0xfffffffeu))){*out=token;return 1;}h=handle(b,token,kind);if(!h)return 0;*out=h->native;return 1;}
static uint32_t guest_handle(struct Soft386PmBridge *b,uint32_t native,unsigned kind){unsigned i;struct Soft386PmHandle *free_h=NULL;if(!native||native==UINT32_MAX||(kind==S386_HWND&&native<=4))return native;for(i=0;i<S386_PM_HANDLES;i++){struct Soft386PmHandle *h=&b->handles[i];if(h->token&&h->native==native&&h->kind==kind&&(kind!=S386_HAB||h->tid==tid(b)))return h->token;if(!h->token&&!free_h)free_h=h;}if(!free_h||b->serial>=0x00ffffffu)return 0;memset(free_h,0,sizeof(*free_h));free_h->native=native;free_h->kind=kind;free_h->tid=tid(b);free_h->token=0x6d000000u|++b->serial;return free_h->token;}
static unsigned kind(char c){switch(c){case 'W':return S386_HWND;case 'A':return S386_HAB;case 'Q':return S386_HMQ;case 'P':return S386_HPS;case 'D':return S386_HDC;case 'B':return S386_HBITMAP;case 'R':return S386_HRGN;case 'K':return S386_HPOINTER;case 'X':return S386_HACCEL;case 'E':return S386_HENUM;case 'I':return S386_HINI;case 'S':return S386_HSWITCH;case 'H':return S386_HHELP;default:return 0;}}
static void *proc(struct Soft386PmBridge *b,unsigned m,uint32_t ord){struct Soft386NativeModule *n=&b->module[m];return n->loaded&&n->get_proc?n->get_proc(n->opaque,ord):NULL;}
/* Native 32-bit personalities use cdecl with 32-bit stack slots. uintptr_t
 * also lets the exact marshal code be tested with 64-bit provider shims. */
static uint32_t call(void *fn,unsigned n,uintptr_t *a)
{
    if(!fn)return 0;
    switch(n){
    case 0:return ((uint32_t (__cdecl *)(void))fn)();
    case 1:return ((uint32_t (__cdecl *)(uintptr_t))fn)(a[0]);
    case 2:return ((uint32_t (__cdecl *)(uintptr_t,uintptr_t))fn)(a[0],a[1]);
    case 3:return ((uint32_t (__cdecl *)(uintptr_t,uintptr_t,uintptr_t))fn)(a[0],a[1],a[2]);
    case 4:return ((uint32_t (__cdecl *)(uintptr_t,uintptr_t,uintptr_t,uintptr_t))fn)(a[0],a[1],a[2],a[3]);
    case 5:return ((uint32_t (__cdecl *)(uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t))fn)(a[0],a[1],a[2],a[3],a[4]);
    case 6:return ((uint32_t (__cdecl *)(uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t))fn)(a[0],a[1],a[2],a[3],a[4],a[5]);
    case 7:return ((uint32_t (__cdecl *)(uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t))fn)(a[0],a[1],a[2],a[3],a[4],a[5],a[6]);
    case 8:return ((uint32_t (__cdecl *)(uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t))fn)(a[0],a[1],a[2],a[3],a[4],a[5],a[6],a[7]);
    case 9:return ((uint32_t (__cdecl *)(uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t))fn)(a[0],a[1],a[2],a[3],a[4],a[5],a[6],a[7],a[8]);
    case 10:return ((uint32_t (__cdecl *)(uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t))fn)(a[0],a[1],a[2],a[3],a[4],a[5],a[6],a[7],a[8],a[9]);
    case 11:return ((uint32_t (__cdecl *)(uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t))fn)(a[0],a[1],a[2],a[3],a[4],a[5],a[6],a[7],a[8],a[9],a[10]);
    case 12:return ((uint32_t (__cdecl *)(uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t))fn)(a[0],a[1],a[2],a[3],a[4],a[5],a[6],a[7],a[8],a[9],a[10],a[11]);
    case 13:return ((uint32_t (__cdecl *)(uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t))fn)(a[0],a[1],a[2],a[3],a[4],a[5],a[6],a[7],a[8],a[9],a[10],a[11],a[12]);
    default:return 0;
    }
}
#ifdef _WIN32
static void *win_proc(void *h,uint32_t ordinal){return (void *)(uintptr_t)GetProcAddress((HMODULE)h,(LPCSTR)(uintptr_t)ordinal);}
static int __cdecl pm_scheduler_idle(void *opaque){struct Soft386PmBridge *b=(struct Soft386PmBridge *)opaque;if(!b||!b->scheduler_yield||b->closing)return 0;return b->scheduler_yield(b->opaque);}
static void install_scheduler_hook(struct Soft386PmBridge *b,int enable){typedef void (__cdecl *SetHook)(int (__cdecl *)(void *),void *);SetHook set;if(!b||!b->module[S386_PMWIN].loaded||b->module[S386_PMWIN].get_proc!=win_proc)return;set=(SetHook)(uintptr_t)GetProcAddress((HMODULE)b->module[S386_PMWIN].opaque,"OS2PM_SetSchedulerHook");if(set)set(enable?pm_scheduler_idle:NULL,enable?b:NULL);}
static void win_close(void *h){FreeLibrary((HMODULE)h);}
#endif
static int register_resources(struct Soft386PmBridge *b){unsigned i;
#ifdef _WIN32
    typedef int (__cdecl *Register)(uint32_t,uint16_t,uint16_t,const void *,uint32_t);
    Register reg=(Register)(uintptr_t)GetProcAddress((HMODULE)b->module[0].opaque,"OS2PM_RegisterResource");
    if(!reg)return b->nresources==0;
    for(i=0;i<b->nresources;i++){struct Soft386PmResource *r=&b->resources[i];if(!r->registered){
        if(b->trace&&r->type==1){const unsigned char *p=(const unsigned char *)r->data;uint32_t h=2166136261u,j;for(j=0;j<r->size;j++){h^=p[j];h*=16777619u;}fprintf(stderr,"soft386: PM resource module=%08X type=%u id=%u size=%u fnv=%08X head=%02X%02X%02X%02X\n",r->module,(unsigned)r->type,(unsigned)r->id,(unsigned)r->size,h,r->size>0?p[0]:0,r->size>1?p[1]:0,r->size>2?p[2]:0,r->size>3?p[3]:0);}
        if(!reg(r->module,r->type,r->id,r->data,r->size))return 0;r->registered=1;}}
#else
    (void)b;(void)i;
#endif
    return 1;
}
void soft386_pm_init(struct Soft386PmBridge *b,void *opaque,uint32_t (*get_tid)(void *),int (*invoke)(void *,uint32_t,uint32_t,const uint32_t *,uint32_t,uint32_t *)){memset(b,0,sizeof(*b));b->opaque=opaque;b->tid=get_tid;b->invoke=invoke;b->pid=1;}
int soft386_pm_open(struct Soft386PmBridge *b,unsigned m){struct Soft386NativeModule *n;if(m>=S386_PM_COUNT)return 0;n=&b->module[m];if(n->loaded)return 1;if(b->disabled)return 0;
    if((m==S386_PMGPI||m==S386_PMCTLS)&&!soft386_pm_open(b,S386_PMWIN))return 0;
#ifdef _WIN32
    n->opaque=LoadLibraryA(b->paths[m]&&b->paths[m][0]?b->paths[m]:module_names[m]);
    if(n->opaque){n->get_proc=win_proc;n->close=win_close;n->loaded=1;n->label=module_names[m];if(m==S386_PMWIN&&!register_resources(b))return 0;if(m==S386_PMWIN)install_scheduler_hook(b,1);if(b->trace)fprintf(stderr,"soft386: native %s loaded [PM bridge]\n",module_names[m]);return 1;}
#endif
    if(b->trace)fprintf(stderr,"soft386: native %s unavailable\n",module_names[m]);return 0;
}
int soft386_pm_export(struct Soft386PmBridge *b,unsigned m,uint32_t ordinal)
{
    if(m>=S386_PM_COUNT||!soft386_pm_open(b,m))return 0;
    return proc(b,m,ordinal)!=NULL;
}
void soft386_pm_set_provider(struct Soft386PmBridge *b,unsigned m,void *opaque,void *(*get_proc)(void *,uint32_t)){if(m>=S386_PM_COUNT)return;b->module[m].opaque=opaque;b->module[m].get_proc=get_proc;b->module[m].loaded=get_proc!=NULL;}
void soft386_pm_set_scheduler_yield(struct Soft386PmBridge *b,int (*yield)(void *)){if(!b)return;b->scheduler_yield=yield;
#ifdef _WIN32
if(b->module[S386_PMWIN].loaded)install_scheduler_hook(b,yield!=NULL);
#endif
}
void soft386_pm_set_thread_hostcall(struct Soft386PmBridge *b,int (*fn)(void *,uint32_t)){if(b)b->thread_hostcall=fn;}
int soft386_pm_add_resource(struct Soft386PmBridge *b,uint32_t mod,uint16_t type,uint16_t id,const void *data,uint32_t size){struct Soft386PmResource *r;if(b->nresources==S386_PM_RESOURCES||!size||size>PM_MAX_BUFFER)return 0;r=&b->resources[b->nresources];r->data=malloc(size);if(!r->data)return 0;memcpy(r->data,data,size);r->module=mod;r->type=type;r->id=id;r->size=size;b->nresources++;return !b->module[0].loaded||register_resources(b);}

static void retire_window(struct Soft386PmBridge *b,uint32_t token)
{
    unsigned i;struct Soft386PmHandle *h=handle(b,token,S386_HWND);
    if(!h)return;
    h->token=0;
    for(i=0;i<S386_PM_HANDLES;i++)if(b->handles[i].token&&b->handles[i].kind==S386_HWND&&b->handles[i].parent==token)
        retire_window(b,b->handles[i].token);
}

struct PmProc {struct Soft386PmBridge *bridge;uint32_t entry,tid;};
static struct PmProc callbacks[S386_PM_PROCS];
static uint32_t callback(unsigned slot,uint32_t hwnd,uint32_t msg,uint32_t mp1,uint32_t mp2){struct PmProc *p=&callbacks[slot];struct Soft386PmBridge *b=p->bridge;struct Soft386PmHandle *h;uint32_t args[4],result=0;if(!b||b->closing||!b->invoke)return 0;args[0]=guest_handle(b,hwnd,S386_HWND);h=handle(b,args[0],S386_HWND);if(!h)return 0;h->proc=p->entry;h->tid=p->tid;h->owned=1;if(msg==0x3b)b->dialog_window=args[0];args[1]=msg;args[2]=mp1;args[3]=msg==0x3b?b->dialog_param:mp2;
    /* This is the exact scalar callback surface emitted by the native PMWIN.
     * Pointer-bearing messages need a dedicated reverse marshal, not a cast. */
    switch(msg){case 1:case 2:case 7:case 0x20:case 0x21:case 0x23:case 0x24:case 0x29:case 0x30:case 0x3b:case 0x4f:case 0x70:case 0x71:case 0x72:case 0x73:case 0x7a:break;
    case 0x101:break; /* SM_QUERYHANDLE */
    case 0x100: /* SM_SETHANDLE: native icon -> guest token */
        args[2]=guest_handle(b,mp1,S386_HPOINTER);if(mp1&&!args[2])return 0;break;
    case 0x31:case 0x32:if(mp1)args[2]=guest_handle(b,mp1,S386_HWND);break;
    default:if(b->trace)fprintf(stderr,"soft386: untyped native PM message %04X rejected\n",msg);return 0;}
    if(!b->invoke(b->opaque,p->tid,p->entry,args,4,&result)&&b->trace)fprintf(stderr,"soft386: guest callback failed TID=%u entry=%08X\n",p->tid,p->entry);
    if(msg==0x100||msg==0x101){uintptr_t native=0;
        if(!native_handle(b,result,S386_HPOINTER,&native))return 0;
        result=(uint32_t)native;
    }
    if(msg==2)retire_window(b,args[0]);
    return result;
}
static uint32_t __cdecl callback0(uint32_t w,uint32_t m,uint32_t a,uint32_t c){return callback(0,w,m,a,c);}
static uint32_t __cdecl callback1(uint32_t w,uint32_t m,uint32_t a,uint32_t c){return callback(1,w,m,a,c);}
static uint32_t __cdecl callback2(uint32_t w,uint32_t m,uint32_t a,uint32_t c){return callback(2,w,m,a,c);}
static uint32_t __cdecl callback3(uint32_t w,uint32_t m,uint32_t a,uint32_t c){return callback(3,w,m,a,c);}
static uint32_t __cdecl callback4(uint32_t w,uint32_t m,uint32_t a,uint32_t c){return callback(4,w,m,a,c);}
static uint32_t __cdecl callback5(uint32_t w,uint32_t m,uint32_t a,uint32_t c){return callback(5,w,m,a,c);}
static uint32_t __cdecl callback6(uint32_t w,uint32_t m,uint32_t a,uint32_t c){return callback(6,w,m,a,c);}
static uint32_t __cdecl callback7(uint32_t w,uint32_t m,uint32_t a,uint32_t c){return callback(7,w,m,a,c);}
static uint32_t __cdecl callback8(uint32_t w,uint32_t m,uint32_t a,uint32_t c){return callback(8,w,m,a,c);}
static uint32_t __cdecl callback9(uint32_t w,uint32_t m,uint32_t a,uint32_t c){return callback(9,w,m,a,c);}
static uint32_t __cdecl callback10(uint32_t w,uint32_t m,uint32_t a,uint32_t c){return callback(10,w,m,a,c);}
static uint32_t __cdecl callback11(uint32_t w,uint32_t m,uint32_t a,uint32_t c){return callback(11,w,m,a,c);}
static uint32_t __cdecl callback12(uint32_t w,uint32_t m,uint32_t a,uint32_t c){return callback(12,w,m,a,c);}
static uint32_t __cdecl callback13(uint32_t w,uint32_t m,uint32_t a,uint32_t c){return callback(13,w,m,a,c);}
static uint32_t __cdecl callback14(uint32_t w,uint32_t m,uint32_t a,uint32_t c){return callback(14,w,m,a,c);}
static uint32_t __cdecl callback15(uint32_t w,uint32_t m,uint32_t a,uint32_t c){return callback(15,w,m,a,c);}
static uint32_t __cdecl callback16(uint32_t w,uint32_t m,uint32_t a,uint32_t c){return callback(16,w,m,a,c);}
static uint32_t __cdecl callback17(uint32_t w,uint32_t m,uint32_t a,uint32_t c){return callback(17,w,m,a,c);}
static uint32_t __cdecl callback18(uint32_t w,uint32_t m,uint32_t a,uint32_t c){return callback(18,w,m,a,c);}
static uint32_t __cdecl callback19(uint32_t w,uint32_t m,uint32_t a,uint32_t c){return callback(19,w,m,a,c);}
static uint32_t __cdecl callback20(uint32_t w,uint32_t m,uint32_t a,uint32_t c){return callback(20,w,m,a,c);}
static uint32_t __cdecl callback21(uint32_t w,uint32_t m,uint32_t a,uint32_t c){return callback(21,w,m,a,c);}
static uint32_t __cdecl callback22(uint32_t w,uint32_t m,uint32_t a,uint32_t c){return callback(22,w,m,a,c);}
static uint32_t __cdecl callback23(uint32_t w,uint32_t m,uint32_t a,uint32_t c){return callback(23,w,m,a,c);}
static uint32_t __cdecl callback24(uint32_t w,uint32_t m,uint32_t a,uint32_t c){return callback(24,w,m,a,c);}
static uint32_t __cdecl callback25(uint32_t w,uint32_t m,uint32_t a,uint32_t c){return callback(25,w,m,a,c);}
static uint32_t __cdecl callback26(uint32_t w,uint32_t m,uint32_t a,uint32_t c){return callback(26,w,m,a,c);}
static uint32_t __cdecl callback27(uint32_t w,uint32_t m,uint32_t a,uint32_t c){return callback(27,w,m,a,c);}
static uint32_t __cdecl callback28(uint32_t w,uint32_t m,uint32_t a,uint32_t c){return callback(28,w,m,a,c);}
static uint32_t __cdecl callback29(uint32_t w,uint32_t m,uint32_t a,uint32_t c){return callback(29,w,m,a,c);}
static uint32_t __cdecl callback30(uint32_t w,uint32_t m,uint32_t a,uint32_t c){return callback(30,w,m,a,c);}
static uint32_t __cdecl callback31(uint32_t w,uint32_t m,uint32_t a,uint32_t c){return callback(31,w,m,a,c);}
static uint32_t __cdecl callback32(uint32_t w,uint32_t m,uint32_t a,uint32_t c){return callback(32,w,m,a,c);}
static uint32_t __cdecl callback33(uint32_t w,uint32_t m,uint32_t a,uint32_t c){return callback(33,w,m,a,c);}
static uint32_t __cdecl callback34(uint32_t w,uint32_t m,uint32_t a,uint32_t c){return callback(34,w,m,a,c);}
static uint32_t __cdecl callback35(uint32_t w,uint32_t m,uint32_t a,uint32_t c){return callback(35,w,m,a,c);}
static uint32_t __cdecl callback36(uint32_t w,uint32_t m,uint32_t a,uint32_t c){return callback(36,w,m,a,c);}
static uint32_t __cdecl callback37(uint32_t w,uint32_t m,uint32_t a,uint32_t c){return callback(37,w,m,a,c);}
static uint32_t __cdecl callback38(uint32_t w,uint32_t m,uint32_t a,uint32_t c){return callback(38,w,m,a,c);}
static uint32_t __cdecl callback39(uint32_t w,uint32_t m,uint32_t a,uint32_t c){return callback(39,w,m,a,c);}
static uint32_t __cdecl callback40(uint32_t w,uint32_t m,uint32_t a,uint32_t c){return callback(40,w,m,a,c);}
static uint32_t __cdecl callback41(uint32_t w,uint32_t m,uint32_t a,uint32_t c){return callback(41,w,m,a,c);}
static uint32_t __cdecl callback42(uint32_t w,uint32_t m,uint32_t a,uint32_t c){return callback(42,w,m,a,c);}
static uint32_t __cdecl callback43(uint32_t w,uint32_t m,uint32_t a,uint32_t c){return callback(43,w,m,a,c);}
static uint32_t __cdecl callback44(uint32_t w,uint32_t m,uint32_t a,uint32_t c){return callback(44,w,m,a,c);}
static uint32_t __cdecl callback45(uint32_t w,uint32_t m,uint32_t a,uint32_t c){return callback(45,w,m,a,c);}
static uint32_t __cdecl callback46(uint32_t w,uint32_t m,uint32_t a,uint32_t c){return callback(46,w,m,a,c);}
static uint32_t __cdecl callback47(uint32_t w,uint32_t m,uint32_t a,uint32_t c){return callback(47,w,m,a,c);}
static uint32_t __cdecl callback48(uint32_t w,uint32_t m,uint32_t a,uint32_t c){return callback(48,w,m,a,c);}
static uint32_t __cdecl callback49(uint32_t w,uint32_t m,uint32_t a,uint32_t c){return callback(49,w,m,a,c);}
static uint32_t __cdecl callback50(uint32_t w,uint32_t m,uint32_t a,uint32_t c){return callback(50,w,m,a,c);}
static uint32_t __cdecl callback51(uint32_t w,uint32_t m,uint32_t a,uint32_t c){return callback(51,w,m,a,c);}
static uint32_t __cdecl callback52(uint32_t w,uint32_t m,uint32_t a,uint32_t c){return callback(52,w,m,a,c);}
static uint32_t __cdecl callback53(uint32_t w,uint32_t m,uint32_t a,uint32_t c){return callback(53,w,m,a,c);}
static uint32_t __cdecl callback54(uint32_t w,uint32_t m,uint32_t a,uint32_t c){return callback(54,w,m,a,c);}
static uint32_t __cdecl callback55(uint32_t w,uint32_t m,uint32_t a,uint32_t c){return callback(55,w,m,a,c);}
static uint32_t __cdecl callback56(uint32_t w,uint32_t m,uint32_t a,uint32_t c){return callback(56,w,m,a,c);}
static uint32_t __cdecl callback57(uint32_t w,uint32_t m,uint32_t a,uint32_t c){return callback(57,w,m,a,c);}
static uint32_t __cdecl callback58(uint32_t w,uint32_t m,uint32_t a,uint32_t c){return callback(58,w,m,a,c);}
static uint32_t __cdecl callback59(uint32_t w,uint32_t m,uint32_t a,uint32_t c){return callback(59,w,m,a,c);}
static uint32_t __cdecl callback60(uint32_t w,uint32_t m,uint32_t a,uint32_t c){return callback(60,w,m,a,c);}
static uint32_t __cdecl callback61(uint32_t w,uint32_t m,uint32_t a,uint32_t c){return callback(61,w,m,a,c);}
static uint32_t __cdecl callback62(uint32_t w,uint32_t m,uint32_t a,uint32_t c){return callback(62,w,m,a,c);}
static uint32_t __cdecl callback63(uint32_t w,uint32_t m,uint32_t a,uint32_t c){return callback(63,w,m,a,c);}
static uint32_t (__cdecl *const callback_functions[])(uint32_t,uint32_t,uint32_t,uint32_t)={
    callback0,
    callback1,
    callback2,
    callback3,
    callback4,
    callback5,
    callback6,
    callback7,
    callback8,
    callback9,
    callback10,
    callback11,
    callback12,
    callback13,
    callback14,
    callback15,
    callback16,
    callback17,
    callback18,
    callback19,
    callback20,
    callback21,
    callback22,
    callback23,
    callback24,
    callback25,
    callback26,
    callback27,
    callback28,
    callback29,
    callback30,
    callback31,
    callback32,
    callback33,
    callback34,
    callback35,
    callback36,
    callback37,
    callback38,
    callback39,
    callback40,
    callback41,
    callback42,
    callback43,
    callback44,
    callback45,
    callback46,
    callback47,
    callback48,
    callback49,
    callback50,
    callback51,
    callback52,
    callback53,
    callback54,
    callback55,
    callback56,
    callback57,
    callback58,
    callback59,
    callback60,
    callback61,
    callback62,
    callback63
};
static void *bind_callback(struct Soft386PmBridge *b,uint32_t entry){unsigned i;int slot=-1;if(!entry||!b->invoke||(b->code_address&&!b->code_address(b->opaque,entry)))return NULL;for(i=0;i<S386_PM_PROCS;i++){if(callbacks[i].bridge==b&&callbacks[i].entry==entry&&callbacks[i].tid==tid(b))return (void *)(uintptr_t)callback_functions[i];if(!callbacks[i].bridge&&slot<0)slot=(int)i;}if(slot<0)return NULL;callbacks[slot].bridge=b;callbacks[slot].entry=entry;callbacks[slot].tid=tid(b);return (void *)(uintptr_t)callback_functions[slot];}
struct Marshal {const struct Soft386GuestMemoryOps *m;uint32_t guest[PM_MAX_ARGS],size[PM_MAX_ARGS];void *buffer[PM_MAX_ARGS];int output[PM_MAX_ARGS],checked[PM_MAX_ARGS];};
static int buffer(struct Marshal *c,unsigned i,uintptr_t *a,uint32_t size,int in,int out,int optional){uint32_t g=c->guest[i];c->checked[i]=1;if(!g){a[i]=0;return optional||!size;}if(size>PM_MAX_BUFFER||!c->m->valid(c->m->opaque,g,size,out))return 0;c->buffer[i]=calloc(1,size?size:1);if(!c->buffer[i])return 0;c->size[i]=size;c->output[i]=out;a[i]=(uintptr_t)c->buffer[i];return !in||c->m->read(c->m->opaque,g,c->buffer[i],size);}
static int string(struct Marshal *c,unsigned i,uintptr_t *a,int optional){c->checked[i]=1;if(!c->guest[i]){a[i]=0;return optional;}c->buffer[i]=malloc(PM_MAX_STRING);if(!c->buffer[i])return 0;if(!c->m->read_cstr(c->m->opaque,c->guest[i],c->buffer[i],PM_MAX_STRING))return 0;a[i]=(uintptr_t)c->buffer[i];return 1;}
static int array(struct Marshal *c,unsigned i,uintptr_t *a,uint32_t count,uint32_t stride,int in,int out,int optional){return stride&&count<=PM_MAX_BUFFER/stride&&buffer(c,i,a,count*stride,in,out,optional);}
static void cleanup(struct Marshal *c){unsigned i;for(i=0;i<PM_MAX_ARGS;i++)free(c->buffer[i]);}
static int copyout(struct Marshal *c){unsigned i;for(i=0;i<PM_MAX_ARGS;i++)if(c->output[i]&&c->buffer[i]&&!c->m->write(c->m->opaque,c->guest[i],c->buffer[i],c->size[i]))return 0;return 1;}
static int write32(const struct Soft386GuestMemoryOps *m,uint32_t g,uint32_t v){uint8_t buf[4];put32(buf,v);return g&&m->valid(m->opaque,g,4,1)&&m->write(m->opaque,g,buf,4);}

/* Guest PM threads are multiplexed on one host thread.  The primary guest
 * queue owns the real USER32 queue; additional guest queues are scheduler
 * contexts only.  This is execution-context representation, not PM policy. */
static struct Soft386PmHandle *thread_handle(struct Soft386PmBridge *b,unsigned k,uint32_t t)
{
    unsigned i;
    for(i=0;i<S386_PM_HANDLES;i++){
        struct Soft386PmHandle *h=&b->handles[i];
        if(h->token&&h->kind==k&&h->tid==t)return h;
    }
    return NULL;
}
static int thread_has_queue(struct Soft386PmBridge *b,uint32_t t){return thread_handle(b,S386_HMQ,t)!=NULL;}
static int thread_owns_handle(struct Soft386PmBridge *b,uint32_t token,unsigned k,uint32_t t)
{
    struct Soft386PmHandle *h=handle(b,token,k);return h&&h->tid==t;
}
static uint32_t synthetic_queue(struct Soft386PmBridge *b,uint32_t t)
{
    uint32_t native=0xf1000000u|(t&0x00ffffffu);
    return guest_handle(b,native,S386_HMQ);
}

/* Only the native public-control adapter returned by reviewed PMWIN.929
 * can enter this table. A guest gets a generated OUT/RET veneer, never fn. */
static uint32_t old_proc_entry(struct Soft386PmBridge *b,uintptr_t fn,uint32_t hwnd,uint32_t owner)
{
    unsigned i;struct Soft386PmOldProc *p;
    if(!b->veneer)return 0;
    for(i=0;i<S386_PM_OLDPROCS;i++){p=&b->oldprocs[i];
        if(p->native==fn&&p->hwnd==hwnd&&p->tid==owner)return p->entry;}
    for(i=0;i<S386_PM_OLDPROCS;i++)if(!b->oldprocs[i].native)break;
    if(i==S386_PM_OLDPROCS)return 0;p=&b->oldprocs[i];
    p->entry=b->veneer(b->opaque,i+1);if(!p->entry)return 0;
    p->native=fn;p->hwnd=hwnd;p->tid=owner;return p->entry;
}
/* Message parameters are their own ABI boundary.  This table describes only
 * representation: scalar MPARAMs can pass unchanged; handle-bearing MPARAMs
 * are translated.  It intentionally does not implement button/listbox/slider
 * semantics -- PMWIN/PMCTLS own those meanings. */
enum PmMsgAbi {
    PM_MSG_SCALAR=0,
    PM_MSG_HPOINTER_MP1=1,
    PM_MSG_HPOINTER_RESULT=2,
    PM_MSG_MENUITEM_OUT=3
};
struct PmMsgSig { uint16_t msg; uint8_t abi; };
static const struct PmMsgSig message_abi[]={
    {0x101,PM_MSG_HPOINTER_RESULT}, /* SM_QUERYHANDLE */
    {0x100,PM_MSG_HPOINTER_MP1},   /* SM_SETHANDLE */
    {0x124,PM_MSG_SCALAR},         /* BM_QUERYCHECK */
    {0x143,PM_MSG_SCALAR},         /* EM_SETTEXTLIMIT */
    {0x160,PM_MSG_SCALAR},         /* LM_QUERYITEMCOUNT */
    {0x164,PM_MSG_SCALAR},         /* LM_SELECTITEM */
    {0x165,PM_MSG_SCALAR},         /* LM_QUERYSELECTION */
    {0x16e,PM_MSG_SCALAR},         /* LM_DELETEALL */
    {0x182,PM_MSG_MENUITEM_OUT},   /* MM_QUERYITEM: mp2 -> MENUITEM */
    {0x192,PM_MSG_SCALAR},         /* MM_SETITEMATTR */
    {0x1a0,PM_MSG_SCALAR},{0x1a1,PM_MSG_SCALAR},{0x1a6,PM_MSG_SCALAR},
    {0x36c,PM_MSG_SCALAR},{0x371,PM_MSG_SCALAR},{0x372,PM_MSG_SCALAR}
};
static const struct PmMsgSig *message_signature(uint32_t msg){unsigned i;for(i=0;i<sizeof(message_abi)/sizeof(message_abi[0]);i++)if(message_abi[i].msg==(uint16_t)msg)return &message_abi[i];return NULL;}
static int marshal_message(struct Soft386PmBridge *b,struct Marshal *c,uintptr_t *a,unsigned msg,unsigned mp1,unsigned mp2)
{
    const struct PmMsgSig *s=message_signature(c->guest[msg]);(void)mp2;
    if(!s)return 0;
    if(s->abi==PM_MSG_HPOINTER_MP1)return native_handle(b,c->guest[mp1],S386_HPOINTER,&a[mp1]);
    if(s->abi==PM_MSG_MENUITEM_OUT)return buffer(c,mp2,a,16,0,1,0);
    return 1;
}
uint32_t soft386_pm_call_proc(struct Soft386PmBridge *b,const struct Soft386GuestMemoryOps *m,uint32_t slot,uint32_t esp)
{
    struct Soft386PmOldProc *p;struct Soft386PmHandle *h;uintptr_t a[4];uint32_t g[4],result;unsigned i;
    if(b->closing||!esp||!slot||slot>S386_PM_OLDPROCS||esp>UINT32_MAX-20||!m->valid(m->opaque,esp+4,16,0))return 0;
    p=&b->oldprocs[slot-1];h=handle(b,p->hwnd,S386_HWND);
    if(!p->native||!h||p->tid!=tid(b))return 0;
    for(i=0;i<4;i++)a[i]=g[i]=m->read_u32(m->opaque,esp+4+4*i);
    if(g[0]!=p->hwnd)return 0;a[0]=h->native;
    if(b->trace)fprintf(stderr,"soft386: PM oldproc slot=%u hwnd=%08X msg=%04X mp1=%08X mp2=%08X\n",slot,g[0],g[1],g[2],g[3]);
    switch(g[1]){
    case 2:case 7:case 0x23:case 0x24:break;
    case 0x101:break;
    case 0x100:if(!native_handle(b,g[2],S386_HPOINTER,&a[2]))return 0;break;
    default:return 0;
    }
#ifdef _WIN32
    if(b->trace&&g[1]==0x100){trace_native_pointer("oldproc-set",g[2],a[2]);trace_native_static("before-set",a[0]);}
    if(b->trace&&g[1]==0x23)trace_native_static("before-paint",a[0]);
#endif
    result=call((void *)p->native,4,a);
#ifdef _WIN32
    if(b->trace&&g[1]==0x100)trace_native_static("after-set",a[0]);
    if(b->trace&&g[1]==0x23)trace_native_static("after-paint",a[0]);
#endif
    if(g[1]==0x100||g[1]==0x101)result=guest_handle(b,result,S386_HPOINTER);
    if(b->trace)fprintf(stderr,"soft386: PM oldproc result=%08X\n",result);
    return result;
}

static uint16_t new_message_token(struct Soft386PmBridge *b){unsigned tries,i;for(tries=0;tries<65535;tries++){if(!++b->message_serial)++b->message_serial;for(i=0;i<S386_PM_MESSAGES;i++)if(b->messages[i].used&&b->messages[i].token==b->message_serial)break;if(i==S386_PM_MESSAGES)return b->message_serial;}return 0;}
static struct Soft386PmMessage *new_message(struct Soft386PmBridge *b){unsigned i;for(i=0;i<S386_PM_MESSAGES;i++)if(!b->messages[i].used){memset(&b->messages[i],0,sizeof(b->messages[i]));b->messages[i].used=1;return &b->messages[i];}return NULL;}
static uint32_t post_message_to_tid(struct Soft386PmBridge *b,uint32_t target,uint32_t hwnd,uint32_t msg,uint32_t mp1,uint32_t mp2){struct Soft386PmHandle *h=handle(b,hwnd,S386_HWND);struct Soft386PmMessage *q;if(!target||!thread_has_queue(b,target)||(!h&&hwnd&&msg!=0x2a)||(h&&!h->proc))return 0;q=new_message(b);if(!q)return 0;q->posted=1;q->order=++b->enqueue_serial;q->proc=h?h->proc:0;q->tid=target;put32(q->wire,hwnd);put16(q->wire+4,(uint16_t)msg);put32(q->wire+8,mp1);put32(q->wire+12,mp2);return 1;}
static uint32_t post_message(struct Soft386PmBridge *b,uint32_t hwnd,uint32_t msg,uint32_t mp1,uint32_t mp2){struct Soft386PmHandle *h=handle(b,hwnd,S386_HWND);return post_message_to_tid(b,h?h->tid:tid(b),hwnd,msg,mp1,mp2);}
static uint32_t post_queue_message(struct Soft386PmBridge *b,uint32_t hmq,uint32_t msg,uint32_t mp1,uint32_t mp2){struct Soft386PmHandle *h=handle(b,hmq,S386_HMQ);return h?post_message_to_tid(b,h->tid,0,msg,mp1,mp2):0;}
static void show_pending(struct Soft386PmBridge *b){unsigned i;uintptr_t a[2];for(i=0;i<S386_PM_HANDLES;i++){struct Soft386PmHandle *h=&b->handles[i];if(h->token&&h->pending_show){h->pending_show=0;a[0]=h->native;a[1]=1;call(proc(b,0,883),2,a);}}}
static uint32_t get_message(struct Soft386PmBridge *b,const struct Soft386GuestMemoryOps *m,uintptr_t *a,uint32_t dest,int peek){unsigned i;struct Soft386PmMessage *q=NULL;uint8_t raw[28];uintptr_t pa[6];int remove=!peek||(a[5]&1u);uint32_t msg,native;
    if(!thread_has_queue(b,tid(b))||!dest||!m->valid(m->opaque,dest,28,1))return 0;
    show_pending(b);b->waiting=0;
    for(i=0;i<S386_PM_MESSAGES;i++)if(b->messages[i].used&&b->messages[i].posted&&!b->messages[i].removed){struct Soft386PmMessage *p=&b->messages[i];if(p->tid!=tid(b))continue;uintptr_t filter=0;uint32_t w=get32(p->wire),message=get16(p->wire+4);native_handle(b,w,S386_HWND,&filter);if(a[2]&&a[2]!=filter)continue;if(a[3]||a[4]){if(message<a[3]||message>a[4])continue;}if(!q||p->order<q->order)q=p;}
    if(!q){
        /* Only the primary guest PM thread owns the real USER32 queue.
         * Secondary guest queues are fed by jar-posted queue/window messages. */
        if(tid(b)!=b->queue_tid){if(!peek)b->waiting=1;return 0;}
        /* Reserve before removing a host message: exhaustion cannot drop it. */
        q=new_message(b);if(!q)return 0;
        memset(raw,0,sizeof(raw));memcpy(pa,a,5*sizeof(*a));pa[1]=(uintptr_t)raw;pa[5]=remove?1:0;
        if(!call(proc(b,0,918),6,pa)){q->used=0;if(!peek)b->waiting=1;return 0;}
        for(i=0;i<S386_PM_MESSAGES;i++)if(&b->messages[i]!=q&&b->messages[i].used&&!b->messages[i].posted&&!b->messages[i].removed&&!memcmp(raw,b->messages[i].native,28)){q->used=0;q=&b->messages[i];break;}
        memcpy(q->native,raw,28);memcpy(q->wire,raw,28);native=get32(raw);put32(q->wire,guest_handle(b,native,S386_HWND));
        msg=get16(raw+4);put32(q->wire+8,0);put32(q->wire+12,0);
        /* Raw MSG pointer fields stay in this record. Dispatch uses the saved
         * native copy only after validating the complete public QMSG. */
        switch(msg){case 0x12:msg=0x2a;break;case 0x0f:msg=0x23;break;case 0x10:msg=0x29;break;case 0x113:msg=0x24;put32(q->wire+8,get32(raw+8));break;case 5:msg=7;put32(q->wire+12,get32(raw+12));break;default:break;}
        put16(q->wire+4,(uint16_t)msg);
    }
    if(!q->token)q->token=new_message_token(b);put16(q->wire+6,q->token);q->removed=remove;
    if(!m->write(m->opaque,dest,q->wire,28))return 0;
    msg=get16(q->wire+4);if(msg==0x2a&&remove)q->used=0;
    return msg==0x2a&&!peek?0:1;
}
static uint32_t dispatch_message(struct Soft386PmBridge *b,const struct Soft386GuestMemoryOps *m,uintptr_t hab,uint32_t ptr){uint8_t wire[28];unsigned i;uint32_t result=0,args[4];struct Soft386PmMessage q;uintptr_t a[2];if(!ptr||!m->read(m->opaque,ptr,wire,28))return 0;for(i=0;i<S386_PM_MESSAGES;i++)if(b->messages[i].used&&b->messages[i].token==get16(wire+6)&&!memcmp(b->messages[i].wire,wire,28))break;if(i==S386_PM_MESSAGES)return 0;q=b->messages[i];b->messages[i].used=0;
    if(q.posted){struct Soft386PmHandle *h;if(!get32(wire))return 0;h=handle(b,get32(wire),S386_HWND);if(!h||h->proc!=q.proc||h->tid!=q.tid)return 0;args[0]=get32(wire);args[1]=get16(wire+4);args[2]=get32(wire+8);args[3]=get32(wire+12);if(b->invoke)b->invoke(b->opaque,q.tid,q.proc,args,4,&result);return result;}
    a[0]=hab;a[1]=(uintptr_t)q.native;return call(proc(b,0,912),2,a);
}
static int bitmap_geometry(const uint8_t *p,uint32_t *w,uint32_t *h,uint32_t *bits,uint32_t *stride){uint32_t cb=get32(p),planes;uint64_t bytes;
    if(cb==12){*w=get16(p+4);*h=get16(p+6);planes=get16(p+8);*bits=get16(p+10);}else if(cb==64){*w=get32(p+4);*h=get32(p+8);planes=get16(p+12);*bits=get16(p+14);}else return 0;
    if(!*w||!*h||*w>32768||*h>32768||planes!=1||(*bits!=1&&*bits!=4&&*bits!=8&&*bits!=24&&*bits!=32))return 0;
    *stride=((*w**bits+31u)/32u)*4u;bytes=(uint64_t)*stride**h;return bytes<=PM_MAX_BUFFER;
}
static int bitmap_info(struct Marshal *c,unsigned i,uintptr_t *a,uint32_t bits,int out){uint8_t head[4];uint32_t cb,colors=bits<=8?1u<<bits:0;if(!c->guest[i]){a[i]=0;c->checked[i]=1;return 1;}if(!c->m->read(c->m->opaque,c->guest[i],head,4))return 0;cb=get32(head);if(cb!=12&&cb!=64)return 0;return buffer(c,i,a,cb+colors*(cb==12?3u:4u),1,out,0);}
static int native_bitmap_geometry(struct Soft386PmBridge *b,struct Soft386PmHandle *h){uint8_t header[64];uintptr_t a[2];memset(header,0,sizeof(header));put32(header,64);a[0]=h->native;a[1]=(uintptr_t)header;if(!call(proc(b,1,601),2,a))return 0;return bitmap_geometry(header,&h->width,&h->height,&h->bits,&h->stride);}

static int dev_open_data(struct Marshal *c,unsigned index,uintptr_t *a,uint32_t count,char **nested)
{
    uintptr_t *table;uint32_t i;
    if(count>9)return 0;
    c->checked[index]=1;
    if(!count){a[index]=0;return c->guest[index]==0;}
    if(!c->guest[index]||!c->m->valid(c->m->opaque,c->guest[index],count*4u,0))return 0;
    table=calloc(count,sizeof(*table));if(!table)return 0;
    c->buffer[index]=table;a[index]=(uintptr_t)table;
    for(i=0;i<count;i++){
        uint32_t gp=c->m->read_u32(c->m->opaque,c->guest[index]+4u*i);
        if(!gp){table[i]=0;continue;}
        if(i==2){
            uint32_t cb;
            if(!c->m->valid(c->m->opaque,gp,4,0))return 0;
            cb=c->m->read_u32(c->m->opaque,gp);
            if(cb<40u||cb>PM_MAX_BUFFER||!c->m->valid(c->m->opaque,gp,cb,0))return 0;
            nested[i]=malloc(cb);if(!nested[i]||!c->m->read(c->m->opaque,gp,nested[i],cb))return 0;
        }else{
            nested[i]=malloc(PM_MAX_STRING);if(!nested[i]||!c->m->read_cstr(c->m->opaque,gp,nested[i],PM_MAX_STRING))return 0;
        }
        table[i]=(uintptr_t)nested[i];
    }
    return 1;
}
static int table_strings(struct Marshal *c,unsigned index,uintptr_t *a,uint32_t count,char **strings){uint32_t i;uintptr_t *table;if(count>9)return 0;c->checked[index]=1;if(!count){a[index]=0;return 1;}if(!c->guest[index]||!c->m->valid(c->m->opaque,c->guest[index],count*4u,0))return 0;table=calloc(count,sizeof(*table));if(!table)return 0;c->buffer[index]=table;a[index]=(uintptr_t)table;for(i=0;i<count;i++){uint32_t ptr=c->m->read_u32(c->m->opaque,c->guest[index]+4*i);strings[i]=malloc(PM_MAX_STRING);if(!strings[i]||!c->m->read_cstr(c->m->opaque,ptr,strings[i],PM_MAX_STRING))return 0;table[i]=(uintptr_t)strings[i];}return 1;}

uint32_t soft386_pm_dispatch(struct Soft386PmBridge *b,const struct Soft386GuestMemoryOps *m,unsigned module,uint32_t ordinal,uint32_t esp,int *handled)
{
    const struct PmSig *s=signature(module,ordinal);struct Marshal c;uintptr_t a[PM_MAX_ARGS];
    unsigned i,n,k;void *fn;uint32_t result=0,raw_result=0,w=0,h=0,bits=0,stride=0,saved_param=b->dialog_param,saved_dialog=b->dialog_window;
    struct Soft386PmHandle *object=NULL;char *nested[9]={0};int waiting=0;
    uint32_t failure=(module==S386_PMGPI&&ordinal==586)?UINT32_MAX:(module==S386_MSG||(module==S386_PMSHAPI&&(ordinal==123||ordinal==124||ordinal==129)))?87u:0u;
    if(handled)*handled=0;b->waiting=0;
    if(b->closing||module>=S386_PM_COUNT)return module==S386_MSG?1u:0u;
    /* Export existence is service-provider policy.  ABI metadata only decides
     * whether this guest/host representation crossing is safe. */
    if(!soft386_pm_export(b,module,ordinal) && !(module==0&&(ordinal==915||ordinal==919||ordinal==902||ordinal==920))){
        if(b->trace)fprintf(stderr,"soft386: service %s.%u native export missing\n",module_names[module],ordinal);
        if(handled)*handled=1;
        return module==S386_MSG?1u:0u;
    }
    if(!s){
        if(b->trace)fprintf(stderr,"soft386: service %s.%u export resolved; ABI descriptor missing; call not attempted\n",module_names[module],ordinal);
        if(handled)*handled=1;
        return module==S386_MSG?1u:0u;
    }
    if(handled)*handled=1;
    memset(&c,0,sizeof(c));c.m=m;n=(unsigned)strlen(s->sig);
    if(n>PM_MAX_ARGS||!esp||esp>UINT32_MAX-4u-n*4u||!m->valid(m->opaque,esp+4,n*4,0))return failure;
    for(i=0;i<n;i++){c.guest[i]=m->read_u32(m->opaque,esp+4+4*i);a[i]=c.guest[i];}
    fn=proc(b,module,ordinal);
    /* GetMsg and secondary guest-thread posts are jar queue operations.
     * Primary/native HWND posts must reach PMWIN so native modal loops can
     * dispatch them while the owning guest thread is parked in a hostcall. */
    if(!fn&&!(module==0&&(ordinal==915||ordinal==919||ordinal==902||ordinal==920)))return failure;
    if(b->trace){
        fprintf(stderr,"soft386: PM enter %s.%u %s class=%s argc=%u args=",module_names[module],ordinal,s->name,abi_class(s),n);
        for(i=0;i<n;i++)fprintf(stderr,"%s%08X",i?",":"",c.guest[i]);
        fputc('\n',stderr);
    }
    for(i=0;i<n;i++){
        k=kind(s->sig[i]);if(k){if(!native_handle(b,c.guest[i],k,&a[i]))goto bad;continue;}
        switch(s->sig[i]){
        case 's':if(module==0&&ordinal==909&&i==1&&(c.guest[i]&0xffff0000u)==0xffff0000u)break;
            if(!string(&c,i,a,1))goto bad;break;
        case 'r':case 'o':case 'v':if(!buffer(&c,i,a,16,s->sig[i]!='o',s->sig[i]!='r',1))goto bad;break;
        case 't':case 'u':if(!buffer(&c,i,a,8,s->sig[i]=='t',s->sig[i]=='u',1))goto bad;break;
        case 'l':if(!buffer(&c,i,a,4,1,1,1))goto bad;break;
        default:break;
        }
    }
    if(module==0){switch(ordinal){
    case 716:{struct Soft386PmHandle *habh=handle(b,c.guest[0],S386_HAB);if(!habh||habh->tid!=tid(b)||thread_has_queue(b,tid(b)))goto bad;if(b->queue_tid&&b->queue_tid!=tid(b)){result=synthetic_queue(b,tid(b));if(!result)goto bad;if(b->trace)fprintf(stderr,"soft386: PM scheduler secondary HMQ guest_tid=%u token=%08X\n",tid(b),result);goto done;}break;}
    case 726:if(!thread_owns_handle(b,c.guest[0],S386_HMQ,tid(b)))goto bad;if(tid(b)!=b->queue_tid){object=handle(b,c.guest[0],S386_HMQ);if(object)object->token=0;result=1;goto done;}break;
    case 888:if(!thread_owns_handle(b,c.guest[0],S386_HAB,tid(b)))goto bad;if(tid(b)!=b->queue_tid){result=1;goto done;}break;
    case 926:if(!thread_has_queue(b,tid(b))||!a[1]||!c.guest[2])goto bad;a[2]=(uintptr_t)bind_callback(b,c.guest[2]);c.checked[2]=1;if(!a[2])goto bad;break;
    case 908:if(!thread_has_queue(b,tid(b))||!a[3]||!buffer(&c,8,a,4,0,1,1))goto bad;break;
    case 909:if(!thread_has_queue(b,tid(b))||c.guest[11]||c.guest[12])goto bad;a[11]=a[12]=0;c.checked[11]=c.checked[12]=1;break;
    case 923:case 924:if(!thread_has_queue(b,tid(b)))goto bad;a[2]=(uintptr_t)bind_callback(b,c.guest[2]);c.checked[2]=c.checked[5]=1;if(!a[2])goto bad;b->dialog_param=c.guest[5];b->dialog_window=0;a[5]=0;break;
    case 915:case 918:result=get_message(b,m,a,c.guest[1],ordinal==918);waiting=b->waiting;goto done;
    case 912:if(!thread_has_queue(b,tid(b)))goto bad;result=dispatch_message(b,m,a[0],c.guest[1]);goto done;
    case 902:result=post_queue_message(b,c.guest[0],c.guest[1],c.guest[2],c.guest[3]);goto done;
    case 919:
        object=handle(b,c.guest[0],S386_HWND);
        /* H2K: ordinary guest posts stay in the jar queue, including private
         * application messages.  Only bypass the jar when the target guest
         * thread is currently parked inside a native hostcall/modal loop; in
         * that state it cannot poll the jar queue, while PMWIN's native loop
         * can consume its HOST_WM_OS2_POST wrapper immediately. */
        if(object&&fn&&b->thread_hostcall&&b->thread_hostcall(b->opaque,object->tid)){
            if(b->trace)fprintf(stderr,"soft386: PM modal native post target=TID %u hwnd=%08X msg=%04X\n",object->tid,c.guest[0],c.guest[1]);
            result=call(fn,4,a);goto done;
        }
        result=post_message(b,c.guest[0],c.guest[1],c.guest[2],c.guest[3]);goto done;
    case 920:object=handle(b,c.guest[0],S386_HWND);
        if(object&&object->proc){uint32_t args[4];if(b->trace)fprintf(stderr,"soft386: PM guest send hwnd=%08X msg=%04X mp1=%08X mp2=%08X\n",c.guest[0],c.guest[1],c.guest[2],c.guest[3]);memcpy(args,c.guest,16);if(b->invoke)b->invoke(b->opaque,object->tid,object->proc,args,4,&result);goto done;}
        /* Scalar public-control messages only. No generic pointer MPARAMs. */
        if(!marshal_message(b,&c,a,1,2,3))goto bad;break;
    case 929:{uintptr_t old,newproc=0;struct Soft386PmOldProc *saved=NULL;
        object=handle(b,c.guest[0],S386_HWND);
        if(!object||object->tid!=tid(b)||!thread_has_queue(b,tid(b))||!b->veneer)goto bad;
        for(i=0;i<S386_PM_OLDPROCS;i++)if(!b->oldprocs[i].native)break;
        if(i==S386_PM_OLDPROCS)goto bad;
        for(i=0;i<S386_PM_OLDPROCS;i++)if(b->oldprocs[i].entry==c.guest[1]&&b->oldprocs[i].native){saved=&b->oldprocs[i];break;}
        if(saved){if(saved->hwnd!=object->token||saved->tid!=tid(b))goto bad;newproc=saved->native;}
        else newproc=(uintptr_t)bind_callback(b,c.guest[1]);
        if(!newproc)goto bad;
        /* PFNWP is pointer-sized in the provider ABI; never truncate it through
         * the scalar call helper on a 64-bit test host. */
        old=((uintptr_t (__cdecl *)(uintptr_t,uintptr_t))fn)(a[0],newproc);
        if(!old)goto done;
        for(i=0;i<S386_PM_PROCS;i++)if(callbacks[i].bridge==b&&old==(uintptr_t)callback_functions[i])break;
        if(i<S386_PM_PROCS){
            /* Frozen PMWIN deliberately keeps its collapsed frame/client
             * procedure for registered windows. Honor that backend contract. */
            result=callbacks[i].entry;
        }else{
            result=old_proc_entry(b,old,object->token,object->tid);
            if(result){object->proc=saved?0:c.guest[1];object->owned=1;}
        }
        goto done;}
    case 730:if(!buffer(&c,3,a,(c.guest[6]&4u)?16:8,1,0,0))goto bad;break;
    case 814:if(!buffer(&c,2,a,2,1,1,0))goto bad;break;
    case 822:if(!buffer(&c,1,a,28,0,1,0))goto bad;break;
    case 838:if(!buffer(&c,1,a,4,0,1,1)||!buffer(&c,2,a,4,0,1,1))goto bad;break;
    case 903:if(!marshal_message(b,&c,a,2,3,4))goto bad;break;
    case 854:if(c.guest[2]!=2||!native_handle(b,c.guest[1],S386_HBITMAP,&a[1]))goto bad;c.checked[1]=1;break;
    case 812:if(!buffer(&c,1,a,40,0,1,0))goto bad;break;
    case 890:if(!buffer(&c,2,a,76,1,1,0))goto bad;break;
    case 779:case 781:if(!buffer(&c,4,a,c.guest[3],0,1,0))goto bad;break;
    case 788:if(!array(&c,2,a,c.guest[3],8,1,1,0))goto bad;break;
    case 815:if(!buffer(&c,3,a,c.guest[2],0,1,0))goto bad;break;
    case 837:if(!buffer(&c,1,a,36,0,1,0))goto bad;break;
    case 841:if(!buffer(&c,2,a,c.guest[1],0,1,0))goto bad;break;
    case 913:if((int32_t)c.guest[1]<0){if(!string(&c,2,a,0))goto bad;}else if(!buffer(&c,2,a,c.guest[1],1,0,0))goto bad;break;
    default:break;
    }}else if(module==1){switch(ordinal){
    case 355:
        /* GpiBitBlt uses 3-4 POINTLs when a source HPS is supplied, but the
         * historical NULL-source forms (ROP_ONE/ROP_ZERO etc.) use only the
         * two destination-corner POINTLs.  NULL HPS itself is a valid typed
         * handle value; validate point count according to source presence. */
        if((c.guest[1] ? (c.guest[2]<3||c.guest[2]>4) : c.guest[2]!=2) ||
           !array(&c,3,a,c.guest[2],8,1,0,0))goto bad;
        break;
    case 359:if(!buffer(&c,3,a,c.guest[2],1,0,0))goto bad;break;
    case 364:if(!array(&c,4,a,c.guest[3],8,1,1,0))goto bad;break;
    case 368:if(!buffer(&c,3,a,56,1,0,0))goto bad;break;
    case 370:if(!array(&c,2,a,c.guest[1],16,1,0,1))goto bad;break;
    case 417:if(!array(&c,2,a,c.guest[1],8,1,0,0))goto bad;break;
    case 443:if(!array(&c,2,a,c.guest[1],4,0,1,0))goto bad;break;
    case 453:if(!buffer(&c,2,a,c.guest[1],0,1,0))goto bad;break;
    case 489:if(c.guest[3]<5||!buffer(&c,2,a,c.guest[1],1,0,0)||!array(&c,4,a,c.guest[3],8,0,1,0))goto bad;break;
    case 492:if(!array(&c,3,a,c.guest[2],4,0,1,0))goto bad;break;
    case 516:if(!buffer(&c,2,a,4,0,1,1))goto bad;break;
    case 520:if(c.guest[1]<9||!array(&c,2,a,c.guest[1],4,1,0,0))goto bad;break;
    case 546:if(!array(&c,3,a,c.guest[2],16,1,0,1))goto bad;break;
    case 586:{uint32_t count;
        if(!c.buffer[3])goto bad;count=get32(c.buffer[3]);
        if((int32_t)count<0)goto bad;
        /* A zero-capacity query returns the font count without touching
         * metrics; OS/2 allows zero stride and NULL metrics in this form. */
        if(!count){a[5]=0;c.checked[5]=1;}
        else if((int32_t)c.guest[4]<=0||!array(&c,5,a,count,c.guest[4],0,1,0))goto bad;
        break;}
    case 588:if(c.guest[1]!=2||(c.guest[2]&~1u)||!buffer(&c,4,a,4,1,0,0))goto bad;break;
    case 592:if(c.guest[3]>255||c.guest[4]>256-c.guest[3]||!array(&c,5,a,c.guest[4],4,1,0,0))goto bad;break;
    case 598:{uint32_t cb;
        if(!c.guest[1]||!m->valid(m->opaque,c.guest[1],4,0))goto bad;cb=m->read_u32(m->opaque,c.guest[1]);
        if((cb!=12&&cb!=64)||!buffer(&c,1,a,cb,1,0,0)||!bitmap_geometry(c.buffer[1],&w,&h,&bits,&stride)||
           !buffer(&c,3,a,stride*h,1,0,1)||!bitmap_info(&c,4,a,bits,0))goto bad;
        break;}
    case 599:case 602:{struct Soft386PmHandle *ps=handle(b,c.guest[0],S386_HPS);uint32_t rows=c.guest[2];
        object=ps?handle(b,ps->aux,S386_HBITMAP):NULL;
        if(!object||!object->stride||(int32_t)c.guest[2]<0||c.guest[1]>=object->height)goto bad;
        if(rows>object->height-c.guest[1])rows=object->height-c.guest[1];a[2]=rows;
        if(!array(&c,3,a,rows,object->stride,ordinal==602,ordinal==599,0)||!bitmap_info(&c,4,a,object->bits,ordinal==599))goto bad;break;}
    case 601:if(!buffer(&c,1,a,64,0,1,0))goto bad;break;
    case 610:if(!dev_open_data(&c,4,a,c.guest[3],nested))goto bad;break;
    default:break;
    }}else if(module==S386_MSG){
        if(ordinal==4){if(!table_strings(&c,0,a,c.guest[1],nested)||!buffer(&c,2,a,c.guest[3],1,0,0)||!buffer(&c,4,a,c.guest[5],0,1,0)||!buffer(&c,6,a,4,0,1,0))goto bad;}
        else{uint8_t head[18];uint32_t off;
            c.checked[0]=1;a[0]=0;
            if(c.guest[0]){if(!m->read(m->opaque,c.guest[0],head,18)||memcmp(head,"\xffMSGSEG32\0",10))goto bad;off=get32(head+14);if(off<18||off>PM_MAX_BUFFER-4||!buffer(&c,0,a,off+4,1,0,0))goto bad;}
            if(!table_strings(&c,1,a,c.guest[2],nested)||!buffer(&c,3,a,c.guest[4],0,1,0)||!buffer(&c,7,a,4,0,1,0))goto bad;
        }
    }else if(module==S386_PMSHAPI){
        switch(ordinal){
        case 101:if(!buffer(&c,3,a,4,0,1,0))goto bad;break;
        case 117:{uint32_t pcb_in;
            if(!buffer(&c,4,a,4,1,1,0))goto bad;
            pcb_in=get32(c.buffer[4]);
            if(b->trace)fprintf(stderr,
                "soft386: PMSHAPI.117 request guest_hini=%08X native_hini=%08lX app=\"%s\" key=\"%s\" buffer=%08X pcb=%08X pcb_in=%u\n",
                c.guest[0],(unsigned long)a[0],
                c.buffer[1]?(const char *)c.buffer[1]:"",
                c.buffer[2]?(const char *)c.buffer[2]:"",
                c.guest[3],c.guest[4],pcb_in);
            if(!buffer(&c,3,a,pcb_in,1,1,1))goto bad;
            break;}
        case 118:if(!buffer(&c,3,a,c.guest[4],1,0,c.guest[4]==0))goto bad;break;
        case 125:if(c.guest[1]&&c.guest[1]!=(b->pid?b->pid:1))goto bad;a[1]=0;break;
        case 124:if(!buffer(&c,1,a,96,0,1,0))goto bad;break;
        case 120:case 123:{uint8_t *p;uintptr_t hwnd,icon;unsigned ix=ordinal==120?0:1;
            if(!buffer(&c,ix,a,96,1,0,0))goto bad;p=c.buffer[ix];
            if(get32(p+8)||!native_handle(b,get32(p),S386_HWND,&hwnd)||
               !native_handle(b,get32(p+4),S386_HWND,&icon))goto bad;
            if(get32(p+12)&&get32(p+12)!=(b->pid?b->pid:1))goto bad;
            put32(p+12,0);put32(p,(uint32_t)hwnd);put32(p+4,(uint32_t)icon);p[91]=0;
            break;}
        default:break;
        }
    }else if(module==S386_HELPMGR&&ordinal==51){
        /* Pointer widths differ in the 64-bit test provider. Construct the
         * native ABI separately and copy back only ulReturnCode, not pointers.
         * The existing HELPMGR implements help-instance lifetime, not IPF. */
        struct NativeHelpInit {uint32_t cb,rc;char *tutorial;void *table;
            uint32_t table_module,accel_module,accel,action;char *title;
            uint32_t show;char *library;} init;
        uint8_t wire[44];unsigned fields[]={8,32,40};uint32_t table;
        if(!c.guest[1]||!m->read(m->opaque,c.guest[1],wire,44)||get32(wire)!=44||
           !m->valid(m->opaque,c.guest[1]+4,4,1))goto bad;
        table=get32(wire+12);if(table&&(table&0xffff0000u)!=0xffff0000u)goto bad;
        memset(&init,0,sizeof(init));init.cb=sizeof(init);init.rc=get32(wire+4);
        for(i=0;i<3;i++){uint32_t p=get32(wire+fields[i]);if(p){nested[i]=malloc(PM_MAX_STRING);
            if(!nested[i]||!m->read_cstr(m->opaque,p,nested[i],PM_MAX_STRING))goto bad;}}
        init.tutorial=nested[0];init.title=nested[1];init.library=nested[2];
        init.table=(void *)(uintptr_t)table;init.table_module=get32(wire+16);init.accel_module=get32(wire+20);
        init.accel=get32(wire+24);init.action=get32(wire+28);init.show=get32(wire+36);
        a[1]=(uintptr_t)&init;raw_result=call(fn,n,a);
        if(!m->write(m->opaque,c.guest[1]+4,&init.rc,4))goto bad;
        result=guest_handle(b,raw_result,S386_HHELP);goto done;
    }else if(module==S386_PMCTLS){
#ifdef _WIN32
        /* The native FILEDLG contract is explicitly 32-bit. Rebase only the
         * supported strings; custom procedures/templates/type arrays remain
         * rejected by this milestone. Copy back outputs, never native pointers. */
        uint8_t *f;unsigned fields[]={20,24,40};uint32_t original[3];
        if(!buffer(&c,2,a,328,1,0,0))goto bad;f=c.buffer[2];
        if(get32(f)!=328||get32(f+28)||get32(f+32)||get32(f+36)||get32(f+44)||get32(f+48)||!memchr(f+52,0,260))goto bad;
        if(!m->valid(m->opaque,c.guest[2],328,1))goto bad;
        for(i=0;i<3;i++){original[i]=get32(f+fields[i]);if(original[i]){nested[i]=malloc(PM_MAX_STRING);if(!nested[i]||!m->read_cstr(m->opaque,original[i],nested[i],PM_MAX_STRING))goto bad;put32(f+fields[i],(uint32_t)(uintptr_t)nested[i]);}}
        result=call(fn,n,a);
        for(i=0;i<3;i++)put32(f+fields[i],original[i]);
        put32(f+312,0);if(!m->write(m->opaque,c.guest[2],f,328))goto bad;
        result=result?1:0;goto done;
#else
        goto bad;
#endif
    }
    for(i=0;i<n;i++)if(s->sig[i]=='?'&&!c.checked[i])goto bad;
    raw_result=call(fn,n,a);
    if(b->trace&&module==S386_PMSHAPI&&ordinal==117){
        uint32_t pcb_out=c.buffer[4]?get32(c.buffer[4]):0;
        unsigned dump=0,j;
        if(c.buffer[3]){dump=pcb_out;if(dump>c.size[3])dump=c.size[3];if(dump>16)dump=16;}
        fprintf(stderr,"soft386: PMSHAPI.117 response result=%08lX pcb_out=%u data=",
                (unsigned long)raw_result,pcb_out);
        for(j=0;j<dump;j++)fprintf(stderr,"%02X",((const uint8_t *)c.buffer[3])[j]);
        if(!dump)fputs("-",stderr);
        fputc('\n',stderr);
    }
#ifdef _WIN32
    if(b->trace&&module==S386_PMWIN&&ordinal==780)trace_native_pointer("WinLoadPointer",0,raw_result);
#endif
    result=s->result?guest_handle(b,raw_result,s->result):raw_result;
    /* H2R: WinLoadString/WinLoadMessage are bounded string outputs.  The
     * provider writes only the returned characters plus a terminating NUL;
     * the remainder of the caller-supplied capacity is not API output.
     * Shrink copyout to the bytes actually defined by the call so the
     * zero-filled host staging tail cannot overwrite adjacent guest globals. */
    if(module==S386_PMWIN&&(ordinal==779||ordinal==781)&&c.buffer[4]){
        uint32_t used=0;
        if((int32_t)raw_result>=0&&c.size[4]){
            used=raw_result<c.size[4]?raw_result+1u:c.size[4];
        }
        c.size[4]=used;
    }
#ifdef _WIN32
    if(b->trace&&module==S386_PMWIN&&ordinal==780&&result)trace_native_pointer("WinLoadPointer-token",result,raw_result);
#endif
    if(module==0){switch(ordinal){
    case 751:
        /* The current PMWIN provider intentionally returns NULL.  A future
         * non-NULL ERRINFO is a pointer graph and must be copied into guest
         * storage rather than exposing a native pointer. */
        if(raw_result){if(b->trace)fprintf(stderr,"soft386: PM ABI reject PMWIN.dll.751 WinGetErrorInfo native ERRINFO pointer requires guest-copy allocator\n");result=0;}
        break;
    case 752:result=(uint32_t)(int32_t)(int16_t)raw_result;break;
    case 844:result=(uint16_t)raw_result;break;
    case 923:result=(uint16_t)raw_result;retire_window(b,b->dialog_window);break;
    case 903:case 920:{const struct PmMsgSig *ms=message_signature(c.guest[ordinal==903?2:1]);
        if(ms&&ms->abi==PM_MSG_HPOINTER_RESULT)result=guest_handle(b,raw_result,S386_HPOINTER);
        else if(ms&&ms->abi==PM_MSG_MENUITEM_OUT){unsigned ix=ordinal==903?4:3;uint8_t *mi=c.buffer[ix];if(result&&mi)put32(mi+8,guest_handle(b,get32(mi+8),S386_HWND));}
        break;}
    case 822:if(result&&c.buffer[1])for(i=12;i<28;i+=4){uint8_t *p=c.buffer[1];put32(p+i,guest_handle(b,get32(p+i),S386_HBITMAP));}break;
    case 838:
        object=handle(b,c.guest[0],S386_HWND);
        if(result&&object&&object->owned){if(c.buffer[1])put32(c.buffer[1],b->pid);if(c.buffer[2])put32(c.buffer[2],object->tid);}break;
    case 909:case 924:object=handle(b,result,S386_HWND);if(object){object->owned=1;object->parent=c.guest[0];}break;
    case 899:object=handle(b,result,S386_HWND);if(object&&result!=c.guest[0]){
        struct Soft386PmHandle *parent=handle(b,c.guest[0],S386_HWND);
        object->parent=c.guest[0];if(parent){object->owned=parent->owned;object->tid=parent->tid;}}break;
    case 716:if(result&&!b->queue_tid)b->owner_tid=b->queue_tid=tid(b);break;
    case 726:if(result){object=handle(b,c.guest[0],S386_HMQ);if(object)object->token=0;if(tid(b)==b->queue_tid)b->queue_tid=0;}break;
    case 888:if(result&&tid(b)==b->owner_tid)b->owner_tid=0;break;
    case 703:object=handle(b,result,S386_HPS);if(object&&!c.guest[1])object->lifetime=1;break;
    case 757:object=handle(b,result,S386_HPS);if(object)object->lifetime=2;break;
    case 908:
        if(c.buffer[8])put32(c.buffer[8],guest_handle(b,get32(c.buffer[8]),S386_HWND));
        object=handle(b,result,S386_HWND);if(object){object->pending_show=(c.guest[1]&0x80000000u)!=0;object->owned=1;}break;
    case 812:if(c.buffer[1])put32(c.buffer[1],guest_handle(b,get32(c.buffer[1]),S386_HWND));break;
    case 837:if(c.buffer[1]){uint8_t *p=c.buffer[1];put32(p+20,guest_handle(b,get32(p+20),S386_HWND));put32(p+24,guest_handle(b,get32(p+24),S386_HWND));}break;
    case 728:if(result)retire_window(b,c.guest[0]);break;
    case 738:case 848:object=handle(b,c.guest[0],S386_HPS);if(result&&object&&object->lifetime==(ordinal==738?1:2))object->token=0;break;
    default:break;
    }}else if(module==1){switch(ordinal){
    case 506:object=handle(b,c.guest[0],S386_HPS);if(raw_result!=UINT32_MAX&&object)object->aux=c.guest[1];break;
    case 598:object=handle(b,result,S386_HBITMAP);if(object){object->width=w;object->height=h;object->bits=bits;object->stride=stride;}break;
    case 399:object=handle(b,result,S386_HBITMAP);if(object)native_bitmap_geometry(b,object);break;
    case 516:if(c.buffer[2])put32(c.buffer[2],guest_handle(b,get32(c.buffer[2]),S386_HRGN));break;
    case 371:case 379:case 604:object=handle(b,c.guest[0],ordinal==371?S386_HBITMAP:(ordinal==379?S386_HPS:S386_HDC));if(result&&object)object->token=0;break;
    case 611:object=handle(b,c.guest[1],S386_HRGN);if(result&&object)object->token=0;break;
    default:break;
    }}else if(module==S386_PMSHAPI){
        if(ordinal==124&&!result&&c.buffer[1]){uint8_t *p=c.buffer[1];put32(p,guest_handle(b,get32(p),S386_HWND));put32(p+4,guest_handle(b,get32(p+4),S386_HWND));put32(p+8,0);put32(p+12,b->pid?b->pid:1);}
        if(ordinal==114)result=(uint32_t)(int32_t)(int16_t)raw_result;
        if((ordinal==103&&result)||(ordinal==129&&!result)){
            object=handle(b,c.guest[0],ordinal==103?S386_HINI:S386_HSWITCH);if(object)object->token=0;
        }
    }else if(module==S386_HELPMGR&&ordinal==52&&result){
        object=handle(b,c.guest[0],S386_HHELP);if(object)object->token=0;
    }
    if(!copyout(&c))result=0;
    goto done;
bad:
    if(b->trace){fprintf(stderr,"soft386: PM ABI reject %s.%u %s class=%s args=",module_names[module],ordinal,s->name,abi_class(s));
        for(i=0;i<n;i++)fprintf(stderr,"%s%08X",i?",":"",c.guest[i]);fputc('\n',stderr);}
    result=failure;
done:
    if(b->trace){
        fprintf(stderr,"soft386: PM leave %s.%u %s result=%08X",module_names[module],ordinal,s->name,result);
        if(waiting)fprintf(stderr," wait=guest-thread");
        if(module==0&&ordinal==823&&result&&c.buffer[1])fprintf(stderr," point=%d,%d",(int32_t)get32(c.buffer[1]),(int32_t)get32((uint8_t *)c.buffer[1]+4));
        fputc('\n',stderr);
    }
    b->waiting=waiting;b->dialog_param=saved_param;b->dialog_window=saved_dialog;for(i=0;i<9;i++)free(nested[i]);cleanup(&c);return result;
}
void soft386_pm_quiesce_process(struct Soft386PmBridge *b)
{
    unsigned i;
    if(!b)return;
    b->closing=1;
#ifdef _WIN32
    install_scheduler_hook(b,0);
#endif
    /* A process vessel is ending: sever every route back into guest code, but
     * do not synchronously destroy native HWNDs or unload providers here.
     * Those are process-owned objects and Win32 process teardown reclaims them
     * atomically.  Deep cleanup remains available through soft386_pm_close()
     * for tests and reusable/in-process bridge users. */
    for(i=0;i<S386_PM_PROCS;i++)if(callbacks[i].bridge==b)memset(&callbacks[i],0,sizeof(callbacks[i]));
}

void soft386_pm_close(struct Soft386PmBridge *b)
{
    unsigned i;uintptr_t a[1];b->closing=1;
#ifdef _WIN32
    install_scheduler_hook(b,0);
#endif
    for(i=0;i<S386_PM_HANDLES;i++)if(b->handles[i].token){
        struct Soft386PmHandle *h=&b->handles[i];a[0]=h->native;
        if(h->kind==S386_HINI){call(proc(b,S386_PMSHAPI,103),1,a);h->token=0;}
        else if(h->kind==S386_HSWITCH){call(proc(b,S386_PMSHAPI,129),1,a);h->token=0;}
        else if(h->kind==S386_HHELP){call(proc(b,S386_HELPMGR,52),1,a);h->token=0;}
    }
    /* Destroy peers while the DLLs and callback bindings are still alive. No
     * guest code is entered after process termination has started. */
    for(i=0;i<S386_PM_HANDLES;i++)if(b->handles[i].token&&b->handles[i].kind==S386_HWND){a[0]=b->handles[i].native;call(proc(b,0,728),1,a);b->handles[i].token=0;}
    for(i=0;i<S386_PM_PROCS;i++)if(callbacks[i].bridge==b)memset(&callbacks[i],0,sizeof(callbacks[i]));
#ifdef _WIN32
    if(b->module[0].loaded&&b->module[0].close==win_close){void (__cdecl *reset)(void)=(void (__cdecl *)(void))(uintptr_t)GetProcAddress((HMODULE)b->module[0].opaque,"OS2PM_ResetResources");if(reset)reset();}
#endif
    for(i=S386_PM_COUNT;i>0;i--){struct Soft386NativeModule *n=&b->module[i-1];if(n->close&&n->opaque)n->close(n->opaque);memset(n,0,sizeof(*n));}
    for(i=0;i<b->nresources;i++)free(b->resources[i].data);b->nresources=0;
}
