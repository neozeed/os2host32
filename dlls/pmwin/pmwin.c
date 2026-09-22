/*
 * pmwin.c - small Win32-backed OS/2 Presentation Manager personality.
 *
 * The original implementation was deliberately limited to the subset used by
 * the Sarien C/386 PM specimen.  Milestone 31A extends that architecture for
 * the Microsoft/IBM OS/2 2.0 Beta 2 WMCHAR SDK sample.  It remains a compact
 * compatibility layer, but PMWIN-created presentation spaces now share a
 * private representation with PMGPI so ordinary WinBeginPaint(NULL) and
 * WinGetPS callers work alongside the Sarien GpiCreatePS path.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pmcompat.h"

#ifndef __cdecl
#define __cdecl
#endif

typedef DWORD O2ULONG;
typedef LONG O2LONG;
typedef WORD O2USHORT;
typedef DWORD O2HWND;
typedef DWORD O2HAB;
typedef DWORD O2HMQ;
typedef DWORD O2HPS;
typedef DWORD O2HBITMAP;
typedef DWORD O2HENUM;
typedef DWORD O2MRESULT;
typedef DWORD O2MPARAM;
typedef O2MRESULT (__cdecl *O2PFNWP)(O2HWND, O2USHORT, O2MPARAM, O2MPARAM);

typedef struct O2RECTL {
    O2LONG xLeft;
    O2LONG yBottom;
    O2LONG xRight;
    O2LONG yTop;
} O2RECTL;

typedef struct O2POINTL {
    O2LONG x;
    O2LONG y;
} O2POINTL;

typedef struct O2QMSG {
    O2HWND hwnd;
    O2USHORT msg;
    O2USHORT reserved;
    O2MPARAM mp1;
    O2MPARAM mp2;
    O2ULONG time;
    O2POINTL ptl;
} O2QMSG;

typedef struct O2SWP {
    O2ULONG fl;
    O2LONG cy;
    O2LONG cx;
    O2LONG y;
    O2LONG x;
    O2HWND hwndInsertBehind;
    O2HWND hwnd;
    O2ULONG ulReserved1;
    O2ULONG ulReserved2;
} O2SWP;

typedef struct O2POINTERINFO {
    O2ULONG fPointer;
    O2LONG xHotSpot;
    O2LONG yHotSpot;
    O2HBITMAP hbmPointer;
    O2HBITMAP hbmColor;
    O2HBITMAP hbmMiniPointer;
    O2HBITMAP hbmMiniColor;
} O2POINTERINFO;

#define PMCOMPAT_ENUM_MAGIC 0x4d554e45UL /* ENUM */
#define PMCOMPAT_ENUM_MAX_WINDOWS 256
struct PMCompatEnum {
    DWORD magic;
    DWORD count;
    DWORD index;
    HWND windows[PMCOMPAT_ENUM_MAX_WINDOWS];
};

typedef struct O2MENUITEM {
    short iPosition;
    O2USHORT afStyle;
    O2USHORT afAttribute;
    O2USHORT id;
    O2HWND hwndSubMenu;
    O2ULONG hItem;
} O2MENUITEM;

#define O2_HWND_DESKTOP        1UL
#define O2_HWND_OBJECT         2UL
#define O2_HWND_BOTTOM         3UL
#define O2_HWND_TOP            4UL

/* Predefined OS/2 PM public classes are encoded as 0xffff000N. */
#define O2_WC_FRAME            1U
#define O2_WC_COMBOBOX         2U
#define O2_WC_BUTTON           3U
#define O2_WC_MENU             4U
#define O2_WC_STATIC           5U
#define O2_WC_ENTRYFIELD       6U
#define O2_WC_LISTBOX          7U
#define O2_WC_SCROLLBAR        8U
#define O2_WC_TITLEBAR         9U
/* OS/2 2.x extended public class used by NEKO dialog resources. */
#define O2_WC_SLIDER          38U

#define O2_WM_CREATE           0x0001U
#define O2_WM_DESTROY          0x0002U
#define O2_WM_SIZE             0x0007U
#define O2_WM_COMMAND          0x0020U
#define O2_WM_SYSCOMMAND       0x0021U
#define O2_WM_CLOSE            0x0029U
#define O2_WM_QUIT             0x002aU
#define O2_WM_VSCROLL          0x0031U
#define O2_WM_HSCROLL          0x0032U
#define O2_WM_PAINT            0x0023U
#define O2_WM_TIMER            0x0024U
#define O2_WM_INITDLG          0x003bU
#define O2_WM_USER             0x1000U
#define O2_WM_ERASEBACKGROUND  0x004fU
#define O2_WM_MOUSEMOVE        0x0070U
#define O2_WM_BUTTON1DOWN      0x0071U
#define O2_WM_BUTTON1UP        0x0072U
#define O2_WM_BUTTON1DBLCLK    0x0073U
#define O2_WM_CHAR             0x007aU

/* WC_STATIC control messages used by OS/2 2.x. */
#define O2_SM_SETHANDLE         0x0100U
#define O2_SM_QUERYHANDLE       0x0101U

/* OS/2 PM class styles (Microsoft OS/2 2.0 Beta 2 pmwin.h). */
#define O2_CS_SIZEREDRAW       0x00000004UL
#define O2_CS_CLIPCHILDREN     0x20000000UL

/* Microsoft OS/2 2.0 Beta 2 pmwin.h. */
#define O2_KC_CHAR             0x0001U
#define O2_KC_VIRTUALKEY       0x0002U
#define O2_KC_SCANCODE         0x0004U
#define O2_KC_SHIFT            0x0008U
#define O2_KC_CTRL             0x0010U
#define O2_KC_ALT              0x0020U
#define O2_KC_KEYUP            0x0040U
#define O2_KC_PREVDOWN         0x0080U

#define O2_VK_BREAK            0x0004U
#define O2_VK_BACKSPACE        0x0005U
#define O2_VK_TAB              0x0006U
#define O2_VK_BACKTAB          0x0007U
#define O2_VK_NEWLINE          0x0008U
#define O2_VK_SHIFT            0x0009U
#define O2_VK_CTRL             0x000aU
#define O2_VK_ALT              0x000bU
#define O2_VK_PAUSE            0x000dU
#define O2_VK_CAPSLOCK         0x000eU
#define O2_VK_ESC              0x000fU
#define O2_VK_SPACE            0x0010U
#define O2_VK_PAGEUP           0x0011U
#define O2_VK_PAGEDOWN         0x0012U
#define O2_VK_END              0x0013U
#define O2_VK_HOME             0x0014U
#define O2_VK_LEFT             0x0015U
#define O2_VK_UP               0x0016U
#define O2_VK_RIGHT            0x0017U
#define O2_VK_DOWN             0x0018U
#define O2_VK_PRINTSCRN        0x0019U
#define O2_VK_INSERT           0x001aU
#define O2_VK_DELETE           0x001bU
#define O2_VK_SCRLLOCK         0x001cU
#define O2_VK_NUMLOCK          0x001dU
#define O2_VK_ENTER            0x001eU
#define O2_VK_SYSRQ            0x001fU
#define O2_VK_F1               0x0020U
#define O2_VK_F24              0x0037U

/* Sarien's engine consumes IBM-PC-style 16-bit function-key values. */
#define SARIEN_KEY_F1          0x3b00U
#define SARIEN_KEY_F2          0x3c00U
#define SARIEN_KEY_F3          0x3d00U
#define SARIEN_KEY_F4          0x3e00U
#define SARIEN_KEY_F5          0x3f00U
#define SARIEN_KEY_F6          0x4000U
#define SARIEN_KEY_F7          0x4100U
#define SARIEN_KEY_F8          0x4200U
#define SARIEN_KEY_F9          0x4300U
#define SARIEN_KEY_F10         0x4400U
#define SARIEN_KEY_F11         0xd900U
#define SARIEN_KEY_F12         0xda00U

#define O2_SV_CXSIZEBORDER     4L
#define O2_SV_CYSIZEBORDER     5L
#define O2_SV_CXSCREEN         20L
#define O2_SV_CYSCREEN         21L
#define O2_SV_CXBORDER         26L
#define O2_SV_CYBORDER         27L
#define O2_SV_CYTITLEBAR       30L
#define O2_SV_CYMENU           35L
#define O2_SV_CXFULLSCREEN     36L
#define O2_SV_CYFULLSCREEN     37L
#define O2_SV_CXBYTEALIGN      49L
#define O2_SV_CYBYTEALIGN      50L

#define O2_SWP_SIZE            0x0001UL
#define O2_SWP_MOVE            0x0002UL
#define O2_SWP_ZORDER          0x0004UL
#define O2_SWP_SHOW            0x0008UL
#define O2_SWP_HIDE            0x0010UL
#define O2_SWP_NOREDRAW        0x0020UL
#define O2_SWP_NOADJUST        0x0040UL
#define O2_SWP_ACTIVATE        0x0080UL
#define O2_SWP_DEACTIVATE      0x0100UL
#define O2_SWP_EXTSTATECHANGE  0x0200UL
#define O2_SWP_MINIMIZE        0x0400UL
#define O2_SWP_MAXIMIZE        0x0800UL
#define O2_SWP_RESTORE         0x1000UL

#define O2_WS_VISIBLE          0x80000000UL

#define O2_FCF_TITLEBAR        0x00000001UL
#define O2_FCF_SYSMENU         0x00000002UL
#define O2_FCF_MENU            0x00000004UL
#define O2_FCF_SIZEBORDER      0x00000008UL
#define O2_FCF_MINBUTTON       0x00000010UL
#define O2_FCF_MAXBUTTON       0x00000020UL
#define O2_FCF_VERTSCROLL      0x00000040UL
#define O2_FCF_HORZSCROLL      0x00000080UL
#define O2_FCF_ICON            0x00004000UL
#define O2_FCF_ACCELTABLE      0x00008000UL

#define O2_QW_NEXT             0
#define O2_QW_PREV             1
#define O2_QW_TOP              2
#define O2_QW_BOTTOM           3
#define O2_QW_OWNER            4
#define O2_QW_PARENT           5

/* OS/2 PM window-word offsets. */
#define O2_QWS_ID            (-16L)

#define O2_FID_TITLEBAR        0x8003U
#define O2_FID_MENU            0x8005U
#define O2_FID_VERTSCROLL      0x8006U
#define O2_FID_HORZSCROLL      0x8007U
#define O2_FID_CLIENT          0x8008U

#define O2_MM_QUERYITEM        0x0182U
#define O2_MM_SETITEMATTR      0x0192U
#define O2_MIA_CHECKED         0x2000U
#define O2_MIA_DISABLED        0x4000U

#define O2_RT_POINTER          1U
#define O2_RT_BITMAP           2U
#define O2_RT_MENU             3U
#define O2_RT_DIALOG           4U
#define O2_RT_STRING           5U
#define O2_RT_ACCELTABLE       8U
#define O2_MIS_TEXT            0x0001U
#define O2_MIS_BITMAP          0x0002U
#define O2_MIS_SEPARATOR       0x0004U
#define O2_MIS_SUBMENU         0x0010U
#define O2_MIS_SYSCOMMAND      0x0040U
#define O2_SC_MINIMIZE         0x8002U
#define O2_SC_MAXIMIZE         0x8003U
#define O2_SC_CLOSE            0x8004U
#define O2_SC_RESTORE          0x8008U

#define O2_BFT_ICON            0x4349U
#define O2_BFT_COLORICON       0x4943U
#define O2_BFT_POINTER         0x5450U
#define O2_BFT_COLORPOINTER    0x5043U
#define O2_BFT_BITMAPARRAY     0x4142U
#define O2_DT_QUERYEXTENT      0x0002UL
#define O2_EM_SETTEXTLIMIT     0x0143U
#define O2_BM_QUERYCHECK       0x0124U
#define O2_LM_QUERYITEMCOUNT    0x0160U
#define O2_LM_INSERTITEM        0x0161U
#define O2_LM_SELECTITEM        0x0164U
#define O2_LM_QUERYSELECTION    0x0165U
#define O2_LM_QUERYITEMTEXT     0x0168U
#define O2_LM_DELETEALL         0x016eU
#define O2_LIT_NONE             ((short)-1)
#define O2_LIT_END              ((short)-1)
#define O2_LIT_SORTASCENDING    ((short)-2)
#define O2_LIT_SORTDESCENDING   ((short)-3)
#define O2_LN_SELECT            1U
#define O2_LN_ENTER             5U
#define O2_SBM_SETSCROLLBAR    0x01a0U
#define O2_SBM_SETPOS          0x01a1U
#define O2_SBM_SETTHUMBSIZE    0x01a6U

/* OS/2 linear-slider messages/attributes used by 2.0 GA applications. */
#define O2_SLM_QUERYSLIDERINFO  0x036cU
#define O2_SLM_SETSLIDERINFO    0x0371U
#define O2_SLM_SETTICKSIZE      0x0372U
#define O2_SMA_SLIDERARMDIMENSIONS 2U
#define O2_SMA_SLIDERARMPOSITION  3U
#define O2_SMA_RANGEVALUE          0U
#define O2_SMA_INCREMENTVALUE      1U
#define O2_SMA_SETALLTICKS         0xffffU
#define O2_SLN_CHANGE              1U
#define O2_SLN_SLIDERTRACK         2U

/* Avoid a hard build dependency on commctrl.h/comctl32 import libraries. */
#define PMCOMPAT_TRACKBAR_CLASS    "msctls_trackbar32"
#define PMCOMPAT_TBS_AUTOTICKS     0x0001UL
#define PMCOMPAT_TBS_VERT          0x0002UL
#define PMCOMPAT_TBS_FIXEDLENGTH   0x0040UL
#define PMCOMPAT_TBM_GETPOS        (WM_USER)
#define PMCOMPAT_TBM_GETRANGEMIN   (WM_USER + 1)
#define PMCOMPAT_TBM_GETRANGEMAX   (WM_USER + 2)
#define PMCOMPAT_TBM_SETTIC        (WM_USER + 4)
#define PMCOMPAT_TBM_SETPOS        (WM_USER + 5)
#define PMCOMPAT_TBM_SETRANGE      (WM_USER + 6)
#define PMCOMPAT_TBM_SETTICFREQ    (WM_USER + 20)
#define PMCOMPAT_TBM_SETLINESIZE   (WM_USER + 23)
#define PMCOMPAT_TBM_SETPAGESIZE   (WM_USER + 21)
#define PMCOMPAT_TBM_SETTHUMBLENGTH (WM_USER + 27)
#define PMCOMPAT_TB_THUMBTRACK     5U
#define O2_DID_OK              1U
#define O2_DID_CANCEL          2U
#define O2_MB_ICONEXCLAMATION  0x0020UL
#define O2_WM_CONTROL           0x0030U
#define O2_PM_REMOVE            0x0001U
#define O2_PM_NOREMOVE          0x0000U
#define HOST_WM_OS2_POST       (WM_APP + 0x2A1)

#define O2_SYSCLR_WINDOWSTATICTEXT (-26L)
#define O2_SYSCLR_WINDOW           (-20L)
#define O2_SYSCLR_WINDOWTEXT       (-17L)

static HINSTANCE g_instance;
static O2PFNWP g_guest_proc;
static char g_guest_class[128];
static int g_class_registered;
static MSG g_last_msg;
static HWND g_frame_hwnd;
static int g_native_create_depth;
static int g_native_show_depth;
static int g_show_pending;
static PVOID g_trace_veh;

#define PMCOMPAT_MAX_RESOURCES 256
#define PMCOMPAT_MAX_SYSCOMMANDS 64
#define PMCOMPAT_MAX_OWNED_POINTERS 256
struct PMCompatResource {
    O2ULONG module;
    O2USHORT type;
    O2USHORT id;
    const unsigned char *data;
    O2ULONG size;
};
static struct PMCompatResource g_resources[PMCOMPAT_MAX_RESOURCES];
static unsigned g_resource_count;
static O2USHORT g_syscommands[PMCOMPAT_MAX_SYSCOMMANDS];
static unsigned g_syscommand_count;
static HICON g_frame_icon;
static O2ULONG g_owned_pointers[PMCOMPAT_MAX_OWNED_POINTERS];
static HWND g_sys_modal_hwnd;

static int pm_remember_owned_pointer(O2ULONG hptr)
{
    unsigned i;
    if (!hptr)
        return 0;
    for (i = 0; i < PMCOMPAT_MAX_OWNED_POINTERS; ++i) {
        if (g_owned_pointers[i] == hptr)
            return 1;
        if (g_owned_pointers[i] == 0) {
            g_owned_pointers[i] = hptr;
            return 1;
        }
    }
    return 0;
}

static int pm_forget_owned_pointer(O2ULONG hptr)
{
    unsigned i;
    if (!hptr)
        return 0;
    for (i = 0; i < PMCOMPAT_MAX_OWNED_POINTERS; ++i) {
        if (g_owned_pointers[i] == hptr) {
            g_owned_pointers[i] = 0;
            return 1;
        }
    }
    return 0;
}

static int pm_is_owned_pointer(O2ULONG hptr)
{
    unsigned i;
    if (!hptr)
        return 0;
    for (i = 0; i < PMCOMPAT_MAX_OWNED_POINTERS; ++i)
        if (g_owned_pointers[i] == hptr)
            return 1;
    return 0;
}

struct PMCompatPostedMsg {
    O2USHORT msg;
    O2MPARAM mp1;
    O2MPARAM mp2;
};

struct PMCompatDialog {
    O2PFNWP guest_proc;
    O2USHORT result;
    int done;
    int heap_owned;
    HICON control_icon;
    O2ULONG user_ulong0;
};


#define PMCOMPAT_MAX_CLASSES 32
struct PMCompatClass {
    char name[128];
    O2PFNWP proc;
    O2ULONG style;
};
struct PMCompatWindow {
    O2PFNWP proc;
    char class_name[128];
    O2ULONG user_ulong0;
    /* Non-NULL only when a native Win32 public control was subclassed by
       WinSubclassWindow.  pm_wndproc is the bridge; native_proc remains the
       control class's original host procedure. */
    WNDPROC native_proc;
};

#define PMCOMPAT_SCROLL_MAGIC 0x4353324fUL /* O2SC */
#define PMCOMPAT_MAX_SCROLL_PROXIES 32
struct PMCompatScrollProxy {
    DWORD magic;
    HWND parent;
    int bar;
};
static struct PMCompatScrollProxy g_scroll_proxies[PMCOMPAT_MAX_SCROLL_PROXIES];
static unsigned g_scroll_proxy_count;
static const char g_object_parent_prop[] = "OS2HOST32_PM_OBJECT_PARENT";

static struct PMCompatScrollProxy *pm_scroll_proxy_from_handle(O2HWND h)
{
    struct PMCompatScrollProxy *sp=(struct PMCompatScrollProxy *)(DWORD)h;
    unsigned i;
    if (!sp) return NULL;
    for (i=0; i<g_scroll_proxy_count; ++i)
        if (sp == &g_scroll_proxies[i] && sp->magic == PMCOMPAT_SCROLL_MAGIC)
            return sp;
    return NULL;
}

static O2HWND pm_scroll_proxy_handle(HWND parent, int bar)
{
    unsigned i;
    struct PMCompatScrollProxy *sp;
    for (i=0; i<g_scroll_proxy_count; ++i) {
        sp=&g_scroll_proxies[i];
        if (sp->magic==PMCOMPAT_SCROLL_MAGIC && sp->parent==parent && sp->bar==bar)
            return (O2HWND)(DWORD)sp;
    }
    if (g_scroll_proxy_count >= PMCOMPAT_MAX_SCROLL_PROXIES) return 0;
    sp=&g_scroll_proxies[g_scroll_proxy_count++];
    sp->magic=PMCOMPAT_SCROLL_MAGIC; sp->parent=parent; sp->bar=bar;
    return (O2HWND)(DWORD)sp;
}
static struct PMCompatClass g_classes[PMCOMPAT_MAX_CLASSES];
static unsigned g_class_count;

static const char g_dialog_class[] = "OS2HOST32_PM_DIALOG";
static const char g_update_disabled_prop[] = "OS2HOST32_PM_UPDATE_DISABLED";
static int g_dialog_class_registered;

#define PMCOMPAT_MAX_ACCELS 64
struct PMCompatAccel {
    O2USHORT fs;
    O2USHORT key;
    O2USHORT cmd;
};
static struct PMCompatAccel g_accels[PMCOMPAT_MAX_ACCELS];
static unsigned g_accel_count;

static WORD pm_rd16(const unsigned char *p)
{
    return (WORD)((WORD)p[0] | ((WORD)p[1] << 8));
}

static DWORD pm_rd32(const unsigned char *p)
{
    return (DWORD)p[0] | ((DWORD)p[1] << 8) | ((DWORD)p[2] << 16) |
           ((DWORD)p[3] << 24);
}

void __cdecl OS2PM_ResetResources(void)
{
    g_resource_count = 0;
    g_syscommand_count = 0;
    g_accel_count = 0;
}

void __cdecl OS2PM_UnregisterModuleResources(O2ULONG module)
{
    unsigned i, out;
    if (module == 1UL)
        module = 0UL;
    out = 0;
    for (i = 0; i < g_resource_count; ++i) {
        if (g_resources[i].module == module)
            continue;
        if (out != i)
            g_resources[out] = g_resources[i];
        ++out;
    }
    g_resource_count = out;
}

int __cdecl OS2PM_RegisterResource(O2ULONG module, O2USHORT type,
                                    O2USHORT id, const void *data,
                                    O2ULONG size)
{
    struct PMCompatResource *r;
    if (!data || size == 0 || g_resource_count >= PMCOMPAT_MAX_RESOURCES)
        return 0;
    r = &g_resources[g_resource_count++];
    r->module = module;
    r->type = type;
    r->id = id;
    r->data = (const unsigned char *)data;
    r->size = size;
    return 1;
}

const void *__cdecl OS2PM_QueryResource(O2ULONG module, O2USHORT type,
                                        O2USHORT id, O2ULONG *sizeOut)
{
    unsigned i;
    if (sizeOut)
        *sizeOut = 0;
    if (module == 1UL)
        module = 0UL;
    for (i = 0; i < g_resource_count; ++i) {
        if (g_resources[i].module == module &&
            g_resources[i].type == type && g_resources[i].id == id) {
            if (sizeOut)
                *sizeOut = g_resources[i].size;
            return g_resources[i].data;
        }
    }
    return NULL;
}

static const struct PMCompatResource *pm_find_resource(O2ULONG module,
                                                        O2USHORT type,
                                                        O2USHORT id)
{
    unsigned i;
    /* OS/2 convention: hmod==0 is the executable.  The host also uses
     * synthetic module handle 1 for the main guest, so treat both as main. */
    if (module == 1UL)
        module = 0UL;
    for (i = 0; i < g_resource_count; ++i) {
        if (g_resources[i].module == module &&
            g_resources[i].type == type && g_resources[i].id == id)
            return &g_resources[i];
    }
    return NULL;
}

static void pm_load_accel_resource(const struct PMCompatResource *rr)
{
    WORD count;
    WORD i;
    DWORD pos;
    g_accel_count = 0;
    if (!rr || rr->size < 4UL)
        return;
    count = pm_rd16(rr->data);
    pos = 4UL;
    for (i = 0; i < count && g_accel_count < PMCOMPAT_MAX_ACCELS; ++i) {
        if (pos + 6UL > rr->size)
            break;
        g_accels[g_accel_count].fs = pm_rd16(rr->data + pos + 0);
        g_accels[g_accel_count].key = pm_rd16(rr->data + pos + 2);
        g_accels[g_accel_count].cmd = pm_rd16(rr->data + pos + 4);
        ++g_accel_count;
        pos += 6UL;
    }
}

static O2USHORT pm_match_accel(UINT native_msg, WPARAM wParam)
{
    unsigned i;
    O2USHORT ch;
    int alt, ctrl, shift;
    alt = (GetKeyState(VK_MENU) & 0x8000) != 0 || native_msg == WM_SYSCHAR;
    ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    ch = (O2USHORT)((DWORD)wParam & 0xffffUL);
    if (ctrl && ch >= 1U && ch <= 26U)
        ch = (O2USHORT)('a' + ch - 1U);
    for (i = 0; i < g_accel_count; ++i) {
        O2USHORT fs = g_accels[i].fs;
        O2USHORT key = g_accels[i].key;
        if ((fs & 0x0001U) == 0) /* AF_CHAR only for now */
            continue;
        if (((fs & 0x0020U) != 0) != alt) continue;
        if (((fs & 0x0010U) != 0) != ctrl) continue;
        if (((fs & 0x0008U) != 0) != shift) continue;
        if (key >= 'A' && key <= 'Z') key = (O2USHORT)(key - 'A' + 'a');
        if (ch >= 'A' && ch <= 'Z') ch = (O2USHORT)(ch - 'A' + 'a');
        if (key == ch)
            return g_accels[i].cmd;
    }
    return 0;
}

static void pm_remember_syscommand(O2USHORT id)
{
    unsigned i;
    for (i = 0; i < g_syscommand_count; ++i)
        if (g_syscommands[i] == id)
            return;
    if (g_syscommand_count < PMCOMPAT_MAX_SYSCOMMANDS)
        g_syscommands[g_syscommand_count++] = id;
}

static int pm_is_syscommand(O2USHORT id)
{
    unsigned i;
    for (i = 0; i < g_syscommand_count; ++i)
        if (g_syscommands[i] == id)
            return 1;
    return 0;
}

static char *pm_menu_text(const unsigned char *src, DWORD len)
{
    char *dst;
    DWORD i, j;
    dst = (char *)HeapAlloc(GetProcessHeap(), 0, (SIZE_T)(len * 2UL + 1UL));
    if (!dst)
        return NULL;
    j = 0;
    for (i = 0; i < len; ++i) {
        if (src[i] == '~')
            dst[j++] = '&';
        else if (src[i] == '&') {
            dst[j++] = '&';
            dst[j++] = '&';
        } else
            dst[j++] = (char)src[i];
    }
    dst[j] = 0;
    return dst;
}

static HMENU pm_parse_menu_template(const unsigned char *data, DWORD avail,
                                    DWORD *used, int top_level)
{
    DWORD total, pos, slen, child_used;
    WORD count, i, style, attr, id;
    HMENU menu, sub;
    UINT flags;
    char *text;

    if (used)
        *used = 0;
    if (!data || avail < 10)
        return NULL;
    total = pm_rd32(data + 0);
    if (total < 10 || total > avail)
        return NULL;
    count = pm_rd16(data + 8);
    pos = 10;
    menu = top_level ? CreateMenu() : CreatePopupMenu();
    if (!menu)
        return NULL;

    for (i = 0; i < count; ++i) {
        if (pos + 6UL > total)
            goto bad;
        style = pm_rd16(data + pos + 0);
        attr  = pm_rd16(data + pos + 2);
        id    = pm_rd16(data + pos + 4);
        pos += 6;

        if (style & O2_MIS_SEPARATOR) {
            if (!AppendMenuA(menu, MF_SEPARATOR, 0, NULL))
                goto bad;
            continue;
        }

        slen = 0;
        while (pos + slen < total && data[pos + slen] != 0)
            ++slen;
        if (pos + slen >= total)
            goto bad;
        text = pm_menu_text(data + pos, slen);
        if (!text)
            goto bad;
        pos += slen + 1UL;

        flags = MF_STRING;
        if (attr & O2_MIA_CHECKED)
            flags |= MF_CHECKED;
        if (attr & O2_MIA_DISABLED)
            flags |= MF_GRAYED;

        if (style & O2_MIS_SUBMENU) {
            child_used = 0;
            sub = pm_parse_menu_template(data + pos, total - pos,
                                         &child_used, 0);
            if (!sub || child_used == 0) {
                HeapFree(GetProcessHeap(), 0, text);
                goto bad;
            }
            if (!AppendMenuA(menu, flags | MF_POPUP,
                             (UINT_PTR)sub, text)) {
                DestroyMenu(sub);
                HeapFree(GetProcessHeap(), 0, text);
                goto bad;
            }
            pos += child_used;
        } else {
            if (!AppendMenuA(menu, flags, (UINT_PTR)id, text)) {
                HeapFree(GetProcessHeap(), 0, text);
                goto bad;
            }
            if (style & O2_MIS_SYSCOMMAND)
                pm_remember_syscommand(id);
        }
        HeapFree(GetProcessHeap(), 0, text);
    }

    if (pos > total)
        goto bad;
    if (used)
        *used = total;
    return menu;

bad:
    DestroyMenu(menu);
    return NULL;
}

static HMENU pm_menu_for_command(HMENU menu, UINT id)
{
    int i, count;
    UINT item_id;
    HMENU sub, found;
    if (!menu)
        return NULL;
    count = GetMenuItemCount(menu);
    for (i = 0; i < count; ++i) {
        item_id = GetMenuItemID(menu, i);
        if (item_id == id)
            return menu;
        sub = GetSubMenu(menu, i);
        if (sub) {
            found = pm_menu_for_command(sub, id);
            if (found)
                return found;
        }
    }
    return NULL;
}

static HICON pm_create_os2_pointer_icon(const unsigned char *data, DWORD size,
                                        int *is_pointer_out)
{
    DWORD first, second, first_header, first_colors, first_used;
    DWORD mask_off, color_off, width, height, mask_height, color_bpp;
    DWORD mask_stride, color_stride, mask_bytes, color_bytes, colors;
    DWORD second_header, second_used;
    WORD first_type, second_type;
    int is_pointer;
    BITMAPINFO *cbi;
    struct {
        BITMAPINFOHEADER h;
        RGBQUAD colors[2];
    } mbi;
    HDC dc;
    HBITMAP hmask, hcolor;
    ICONINFO ii;
    HICON icon;

    if (is_pointer_out)
        *is_pointer_out = 0;
    if (!data || size < 100 || pm_rd16(data) != O2_BFT_BITMAPARRAY)
        return NULL;
    first = 14UL;
    first_type = pm_rd16(data + first);
    if (first_type != O2_BFT_COLORICON && first_type != O2_BFT_ICON &&
        first_type != O2_BFT_COLORPOINTER && first_type != O2_BFT_POINTER)
        return NULL;
    is_pointer = (first_type == O2_BFT_COLORPOINTER ||
                  first_type == O2_BFT_POINTER);
    first_header = pm_rd32(data + first + 2);
    if (first_header < 78UL || first + first_header > size)
        return NULL;
    if (pm_rd32(data + first + 14) != 64UL)
        return NULL; /* OS/2 2.x BITMAPINFOHEADER2 */
    width = pm_rd32(data + first + 18);
    mask_height = pm_rd32(data + first + 22);
    if (width == 0 || mask_height < 2 || pm_rd16(data + first + 28) != 1)
        return NULL;
    first_used = pm_rd32(data + first + 46);
    first_colors = first_used ? first_used : 2UL;
    second = first + first_header + first_colors * 4UL;
    if (second + 78UL > size)
        return NULL;
    second_type = pm_rd16(data + second);
    if ((!is_pointer && second_type != O2_BFT_COLORICON) ||
        (is_pointer && second_type != O2_BFT_COLORPOINTER))
        return NULL;
    second_header = pm_rd32(data + second + 2);
    if (second_header < 78UL || second + second_header > size ||
        pm_rd32(data + second + 14) != 64UL)
        return NULL;
    if (pm_rd32(data + second + 18) != width)
        return NULL;
    height = pm_rd32(data + second + 22);
    color_bpp = pm_rd16(data + second + 28);
    if (height == 0 || mask_height != height * 2UL ||
        (color_bpp != 1UL && color_bpp != 4UL && color_bpp != 8UL))
        return NULL;
    second_used = pm_rd32(data + second + 46);
    colors = second_used ? second_used : (1UL << color_bpp);
    if (colors > 256UL || second + second_header + colors * 4UL > size)
        return NULL;

    mask_off = pm_rd32(data + first + 10);
    color_off = pm_rd32(data + second + 10);
    mask_stride = ((width + 31UL) / 32UL) * 4UL;
    color_stride = ((width * color_bpp + 31UL) / 32UL) * 4UL;
    mask_bytes = mask_stride * height;
    color_bytes = color_stride * height;
    if (mask_off > size || mask_bytes * 2UL > size - mask_off ||
        color_off > size || color_bytes > size - color_off)
        return NULL;

    cbi = (BITMAPINFO *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
          sizeof(BITMAPINFOHEADER) + (SIZE_T)colors * sizeof(RGBQUAD));
    if (!cbi)
        return NULL;
    cbi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    cbi->bmiHeader.biWidth = (LONG)width;
    cbi->bmiHeader.biHeight = (LONG)height;
    cbi->bmiHeader.biPlanes = 1;
    cbi->bmiHeader.biBitCount = (WORD)color_bpp;
    cbi->bmiHeader.biCompression = BI_RGB;
    cbi->bmiHeader.biSizeImage = color_bytes;
    memcpy(cbi->bmiColors, data + second + second_header,
           (size_t)colors * sizeof(RGBQUAD));

    memset(&mbi, 0, sizeof(mbi));
    mbi.h.biSize = sizeof(BITMAPINFOHEADER);
    mbi.h.biWidth = (LONG)width;
    mbi.h.biHeight = (LONG)height;
    mbi.h.biPlanes = 1;
    mbi.h.biBitCount = 1;
    mbi.h.biCompression = BI_RGB;
    mbi.h.biSizeImage = mask_bytes;
    mbi.colors[0].rgbRed = mbi.colors[0].rgbGreen = mbi.colors[0].rgbBlue = 0;
    mbi.colors[1].rgbRed = mbi.colors[1].rgbGreen = mbi.colors[1].rgbBlue = 255;

    dc = GetDC(NULL);
    if (!dc) {
        HeapFree(GetProcessHeap(), 0, cbi);
        return NULL;
    }
    /* The 2*height 1-bpp OS/2 ANDXOR bitmap stores the monochrome XOR
     * fallback first and the AND transparency mask second.  Win32 needs the
     * AND half plus the separate colour bitmap for either icon or cursor. */
    hmask = CreateDIBitmap(dc, &mbi.h, CBM_INIT,
                           data + mask_off + mask_bytes,
                           (BITMAPINFO *)&mbi, DIB_RGB_COLORS);
    hcolor = CreateDIBitmap(dc, &cbi->bmiHeader, CBM_INIT,
                            data + color_off, cbi, DIB_RGB_COLORS);
    ReleaseDC(NULL, dc);
    HeapFree(GetProcessHeap(), 0, cbi);
    if (!hmask || !hcolor) {
        if (hmask) DeleteObject(hmask);
        if (hcolor) DeleteObject(hcolor);
        return NULL;
    }

    memset(&ii, 0, sizeof(ii));
    ii.fIcon = is_pointer ? FALSE : TRUE;
    if (is_pointer) {
        ii.xHotspot = (DWORD)(WORD)pm_rd16(data + first + 6);
        ii.yHotspot = (DWORD)(WORD)pm_rd16(data + first + 8);
    }
    ii.hbmMask = hmask;
    ii.hbmColor = hcolor;
    icon = CreateIconIndirect(&ii);
    DeleteObject(hmask);
    DeleteObject(hcolor);
    if (icon && is_pointer_out)
        *is_pointer_out = is_pointer;
    return icon;
}

static HICON pm_create_os2_color_icon(const unsigned char *data, DWORD size)
{
    int is_pointer;
    HICON icon;
    icon = pm_create_os2_pointer_icon(data, size, &is_pointer);
    if (icon && is_pointer) {
        DestroyIcon(icon);
        return NULL;
    }
    return icon;
}

static LONG WINAPI pm_trace_veh(PEXCEPTION_POINTERS ep)
{
    if (ep && ep->ExceptionRecord && ep->ContextRecord) {
        unsigned long fault = 0UL;
        unsigned long access = 0UL;
        if (ep->ExceptionRecord->NumberParameters >= 2) {
            access = (unsigned long)ep->ExceptionRecord->ExceptionInformation[0];
            fault = (unsigned long)ep->ExceptionRecord->ExceptionInformation[1];
        }
        fprintf(stderr,
                "PMWIN: EXCEPTION code=%08lX address=%08lX "
                "EIP=%08lX ESP=%08lX EBP=%08lX EAX=%08lX "
                "ECX=%08lX EDX=%08lX ESI=%08lX EDI=%08lX "
                "EFLAGS=%08lX access=%lu fault=%08lX\n",
                (unsigned long)ep->ExceptionRecord->ExceptionCode,
                (unsigned long)(DWORD)ep->ExceptionRecord->ExceptionAddress,
                (unsigned long)ep->ContextRecord->Eip,
                (unsigned long)ep->ContextRecord->Esp,
                (unsigned long)ep->ContextRecord->Ebp,
                (unsigned long)ep->ContextRecord->Eax,
                (unsigned long)ep->ContextRecord->Ecx,
                (unsigned long)ep->ContextRecord->Edx,
                (unsigned long)ep->ContextRecord->Esi,
                (unsigned long)ep->ContextRecord->Edi,
                (unsigned long)ep->ContextRecord->EFlags,
                access, fault);

        /* M31A R10: when a guest C frame is still readable, dump its first
         * four arguments and the 8-byte record pointed to by argument 3.
         * For WMCHAR DrawMessage this is:
         *   [ebp+08] hps, [ebp+0c] iMessage, [ebp+10] CHMSG *, [ebp+14] cy.
         * This is trace-only diagnostics; no guest state is modified. */
        {
            DWORD ebp = (DWORD)ep->ContextRecord->Ebp;
            DWORD pcm = 0;
            if (ebp != 0 &&
                !IsBadReadPtr((const void *)(DWORD)(ebp + 8), 16)) {
                DWORD *a = (DWORD *)(DWORD)(ebp + 8);
                pcm = a[2];
                fprintf(stderr,
                        "PMWIN: EXC frame args hps=%08lX msgno=%08lX "
                        "pcm=%08lX cy=%08lX\n",
                        (unsigned long)a[0], (unsigned long)a[1],
                        (unsigned long)a[2], (unsigned long)a[3]);
            }
            if (pcm != 0 && !IsBadReadPtr((const void *)(DWORD)pcm, 8)) {
                const unsigned char *m = (const unsigned char *)(DWORD)pcm;
                fprintf(stderr,
                        "PMWIN: EXC pcm bytes %02X %02X %02X %02X "
                        "%02X %02X %02X %02X  "
                        "usKC=%04X rep=%02X scan=%02X ch=%04X usVK=%04X\n",
                        m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7],
                        (unsigned)(m[0] | ((unsigned)m[1] << 8)),
                        (unsigned)m[2], (unsigned)m[3],
                        (unsigned)(m[4] | ((unsigned)m[5] << 8)),
                        (unsigned)(m[6] | ((unsigned)m[7] << 8)));
            }
            if (ebp > 0x18UL &&
                !IsBadReadPtr((const void *)(DWORD)(ebp - 0x18UL), 16)) {
                const unsigned char *z =
                    (const unsigned char *)(DWORD)(ebp - 0x18UL);
                fprintf(stderr,
                        "PMWIN: EXC DrawMessage sz bytes "
                        "%02X %02X %02X %02X %02X %02X %02X %02X "
                        "%02X %02X %02X %02X %02X %02X %02X %02X\n",
                        z[0], z[1], z[2], z[3], z[4], z[5], z[6], z[7],
                        z[8], z[9], z[10], z[11], z[12], z[13], z[14], z[15]);
            }
        }
        fflush(stderr);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

static int pm_trace_enabled(void)
{
    static int initialized;
    static int enabled;
    char value[8];
    DWORD n;
    if (!initialized) {
        n = GetEnvironmentVariableA("OS2_PM_TRACE", value, sizeof(value));
        enabled = (n != 0 && n < sizeof(value) && value[0] != '0');
        initialized = 1;
    }
    return enabled;
}

static void pm_trace(const char *what, unsigned long a,
                     unsigned long b, unsigned long c)
{
    if (!pm_trace_enabled())
        return;
    fprintf(stderr, "PMWIN: %-22s %08lX %08lX %08lX\n", what, a, b, c);
    fflush(stderr);
}

static HWND native_hwnd(O2HWND hwnd)
{
    if (hwnd == O2_HWND_DESKTOP)
        return GetDesktopWindow();
    if (hwnd == O2_HWND_OBJECT)
        return NULL;
    return (HWND)(DWORD)hwnd;
}

static O2HWND guest_hwnd(HWND hwnd)
{
    if (hwnd == GetDesktopWindow())
        return O2_HWND_DESKTOP;
    return (O2HWND)(DWORD)hwnd;
}

/* A V1 guest must never acquire authority over unrelated host desktop windows. */
static int pm_is_own_process_window(HWND hwnd)
{
    DWORD pid;
    if (!hwnd || !IsWindow(hwnd))
        return 0;
    pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    return pid == GetCurrentProcessId();
}

static int pm_is_trackbar(HWND hwnd)
{
    char cls[64];
    if (!hwnd)
        return 0;
    cls[0] = 0;
    if (!GetClassNameA(hwnd, cls, sizeof(cls)))
        return 0;
    return _stricmp(cls, PMCOMPAT_TRACKBAR_CLASS) == 0;
}

static int pm_ensure_trackbar_class(void)
{
    static HMODULE common_controls;
    typedef void (WINAPI *PFNINITCOMMONCONTROLS)(void);
    PFNINITCOMMONCONTROLS initfn;

    if (common_controls)
        return 1;
    common_controls = LoadLibraryA("comctl32.dll");
    if (!common_controls)
        return 0;
    initfn = (PFNINITCOMMONCONTROLS)GetProcAddress(common_controls,
                                                   "InitCommonControls");
    if (initfn)
        initfn();
    return 1;
}

static O2MRESULT pm_send_slider(HWND wh, O2USHORT msg,
                                O2MPARAM mp1, O2MPARAM mp2, int *handled)
{
    O2USHORT info;
    O2USHORT format;
    O2USHORT tick;
    O2USHORT tick_size;
    LONG pos;
    LONG minpos;
    LONG maxpos;

    *handled = 0;
    if (!pm_is_trackbar(wh))
        return 0;

    if (msg == O2_SLM_SETTICKSIZE) {
        tick = LOWORD((DWORD)mp1);
        tick_size = HIWORD((DWORD)mp1);
        if (tick == O2_SMA_SETALLTICKS) {
            /* Win32 trackbars use a uniform tick size; preserve the scale
               density rather than pretending tick pixel lengths are exact. */
            SendMessageA(wh, PMCOMPAT_TBM_SETTICFREQ, 10, 0);
        } else {
            SendMessageA(wh, PMCOMPAT_TBM_SETTIC, 0, (LPARAM)(LONG)tick);
        }
        pm_trace("SLM_SETTICKSIZE", (unsigned long)(DWORD)wh,
                 (unsigned long)tick, (unsigned long)tick_size);
        *handled = 1;
        return 1;
    }

    if (msg == O2_SLM_SETSLIDERINFO) {
        info = LOWORD((DWORD)mp1);
        format = HIWORD((DWORD)mp1);
        if (info == O2_SMA_SLIDERARMPOSITION) {
            pos = (LONG)(short)LOWORD((DWORD)mp2);
            minpos = (LONG)SendMessageA(wh, PMCOMPAT_TBM_GETRANGEMIN, 0, 0);
            maxpos = (LONG)SendMessageA(wh, PMCOMPAT_TBM_GETRANGEMAX, 0, 0);
            if (pos < minpos) pos = minpos;
            if (pos > maxpos) pos = maxpos;
            SendMessageA(wh, PMCOMPAT_TBM_SETPOS, TRUE, (LPARAM)pos);
        } else if (info == O2_SMA_SLIDERARMDIMENSIONS) {
            O2USHORT arm_length;
            arm_length = LOWORD((DWORD)mp2);
            if (arm_length != 0)
                SendMessageA(wh, PMCOMPAT_TBM_SETTHUMBLENGTH,
                             (WPARAM)arm_length, 0);
        }
        pm_trace("SLM_SETSLIDERINFO", (unsigned long)(DWORD)wh,
                 (unsigned long)info, (unsigned long)format);
        *handled = 1;
        return 1;
    }

    if (msg == O2_SLM_QUERYSLIDERINFO) {
        info = LOWORD((DWORD)mp1);
        format = HIWORD((DWORD)mp1);
        if (info == O2_SMA_SLIDERARMPOSITION) {
            pos = (LONG)SendMessageA(wh, PMCOMPAT_TBM_GETPOS, 0, 0);
            if (format == O2_SMA_INCREMENTVALUE) {
                *handled = 1;
                return (O2MRESULT)(O2USHORT)pos;
            }
            minpos = (LONG)SendMessageA(wh, PMCOMPAT_TBM_GETRANGEMIN, 0, 0);
            maxpos = (LONG)SendMessageA(wh, PMCOMPAT_TBM_GETRANGEMAX, 0, 0);
            *handled = 1;
            return (O2MRESULT)((DWORD)(O2USHORT)(pos - minpos) |
                    ((DWORD)(O2USHORT)(maxpos - minpos) << 16));
        }
        *handled = 1;
        return 0;
    }

    return 0;
}

static struct CompatPS *ps_from(O2HPS hps)
{
    struct CompatPS *p;
    p = (struct CompatPS *)(DWORD)hps;
    if (!p || p->magic != PMCOMPAT_PS_MAGIC)
        return NULL;
    return p;
}

static struct CompatPS *alloc_ps(HWND hwnd, HDC dc, DWORD flags)
{
    struct CompatPS *p;
    p = (struct CompatPS *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
                                     sizeof(*p));
    if (!p)
        return NULL;
    p->magic = PMCOMPAT_PS_MAGIC;
    p->dc = dc;
    p->color = -1; /* OS/2 GPI default foreground is CLR_BLACK */
    p->default_view[0] = 65536L;
    p->default_view[4] = 65536L;
    p->default_view[8] = 1L;
    p->hwnd = hwnd;
    p->flags = flags;
    p->saved_dc = dc ? SaveDC(dc) : 0;
    return p;
}

static void client_rect_os2(HWND hwnd, O2RECTL *out)
{
    RECT rc;
    if (!out)
        return;
    memset(&rc, 0, sizeof(rc));
    if (hwnd)
        GetClientRect(hwnd, &rc);
    out->xLeft = 0;
    out->yBottom = 0;
    out->xRight = rc.right - rc.left;
    out->yTop = rc.bottom - rc.top;
}

static void rect_os2_to_win(HWND hwnd, const O2RECTL *src, RECT *dst)
{
    RECT client;
    LONG h;
    memset(&client, 0, sizeof(client));
    if (hwnd)
        GetClientRect(hwnd, &client);
    h = client.bottom - client.top;
    dst->left = src->xLeft;
    dst->right = src->xRight;
    dst->top = h - src->yTop;
    dst->bottom = h - src->yBottom;
}

static void rect_win_to_os2(HWND hwnd, const RECT *src, O2RECTL *dst)
{
    RECT client;
    LONG h;
    memset(&client, 0, sizeof(client));
    if (hwnd)
        GetClientRect(hwnd, &client);
    h = client.bottom - client.top;
    dst->xLeft = src->left;
    dst->xRight = src->right;
    dst->yBottom = h - src->bottom;
    dst->yTop = h - src->top;
}

static COLORREF os2_color(O2LONG color)
{
    /*
     * OS/2 has two colour namespaces that reach PMWIN drawing APIs:
     * negative SYSCLR_* values and the ordinary CLR_* logical palette.
     * PMGPI already translates the latter; WinFillRect/WinDrawText must do
     * the same or CLR_WHITE (-2) becomes an invalid Win32 COLORREF.
     */
    switch (color) {
    case O2_SYSCLR_WINDOW:           return GetSysColor(COLOR_WINDOW);
    case O2_SYSCLR_WINDOWTEXT:       return GetSysColor(COLOR_WINDOWTEXT);
    case O2_SYSCLR_WINDOWSTATICTEXT: return GetSysColor(COLOR_WINDOWTEXT);
    case -2L: return RGB(255,255,255); /* CLR_WHITE */
    case -1L: return RGB(0,0,0);       /* CLR_BLACK */
    case 0L:  return RGB(192,192,192); /* CLR_BACKGROUND */
    case 1L:  return RGB(0,0,255);
    case 2L:  return RGB(255,0,0);
    case 3L:  return RGB(255,0,255);
    case 4L:  return RGB(0,255,0);
    case 5L:  return RGB(0,255,255);
    case 6L:  return RGB(255,255,0);
    case 7L:  return RGB(192,192,192);
    case 8L:  return RGB(128,128,128);
    case 9L:  return RGB(0,0,128);
    case 10L: return RGB(128,0,0);
    case 11L: return RGB(128,0,128);
    case 12L: return RGB(0,128,0);
    case 13L: return RGB(0,128,128);
    case 14L: return RGB(128,128,0);
    case 15L: return RGB(224,224,224); /* CLR_PALEGRAY */
    default:  return (COLORREF)(DWORD)color;
    }
}

static O2PFNWP pm_find_class_proc(const char *name)
{
    unsigned i;
    if (!name) return NULL;
    for (i=0; i<g_class_count; ++i)
        if (strcmp(g_classes[i].name,name)==0) return g_classes[i].proc;
    return NULL;
}

static O2ULONG pm_find_class_style(const char *name)
{
    unsigned i;
    if (!name) return 0;
    for (i=0; i<g_class_count; ++i)
        if (strcmp(g_classes[i].name,name)==0) return g_classes[i].style;
    return 0;
}

static struct PMCompatWindow *pm_window_state(HWND hwnd)
{
    if (!hwnd) return NULL;
    return (struct PMCompatWindow *)(DWORD)GetWindowLongA(hwnd,GWL_USERDATA);
}

static O2MRESULT call_guest(HWND hwnd, O2USHORT msg,
                            O2MPARAM mp1, O2MPARAM mp2)
{
    O2MRESULT r;
    O2PFNWP proc;
    struct PMCompatWindow *st;
    st = pm_window_state(hwnd);
    proc = st && st->proc ? st->proc : g_guest_proc;
    if (!proc)
        return 0;
    pm_trace("call_guest enter", (unsigned long)msg,
             (unsigned long)(DWORD)proc,
             (unsigned long)guest_hwnd(hwnd));
    r = proc(guest_hwnd(hwnd), msg, mp1, mp2);
    pm_trace("call_guest leave", (unsigned long)msg,
             (unsigned long)r, 0);
    return r;
}

static O2MRESULT call_dialog_guest(HWND hwnd, struct PMCompatDialog *dlg,
                                   O2USHORT msg, O2MPARAM mp1,
                                   O2MPARAM mp2)
{
    O2MRESULT r;
    if (!dlg || !dlg->guest_proc)
        return 0;
    pm_trace("dialog guest enter", (unsigned long)msg,
             (unsigned long)(DWORD)dlg->guest_proc,
             (unsigned long)(DWORD)hwnd);
    r = dlg->guest_proc(guest_hwnd(hwnd), msg, mp1, mp2);
    pm_trace("dialog guest leave", (unsigned long)msg,
             (unsigned long)r, 0);
    return r;
}

static O2USHORT pm_scroll_code(UINT code)
{
    switch (code) {
    case SB_LINEUP: return 1;
    case SB_LINEDOWN: return 2;
    case SB_PAGEUP: return 3;
    case SB_PAGEDOWN: return 4;
    case SB_THUMBTRACK: return 5;
    case SB_THUMBPOSITION: return 6;
    default: return 0;
    }
}

static O2MPARAM pm_mouse_point(HWND hwnd, LPARAM lParam)
{
    RECT cr;
    O2USHORT ox, oy;
    GetClientRect(hwnd, &cr);
    ox = (O2USHORT)(short)LOWORD(lParam);
    oy = (O2USHORT)(short)(cr.bottom - (short)HIWORD(lParam));
    return (O2MPARAM)((DWORD)ox | ((DWORD)oy << 16));
}

static LRESULT CALLBACK pm_dialog_wndproc(HWND hwnd, UINT msg,
                                           WPARAM wParam, LPARAM lParam)
{
    struct PMCompatDialog *dlg;
    dlg = (struct PMCompatDialog *)(DWORD)GetWindowLongA(hwnd, GWL_USERDATA);

    if (msg == WM_NCCREATE) {
        CREATESTRUCTA *cs = (CREATESTRUCTA *)lParam;
        dlg = (struct PMCompatDialog *)cs->lpCreateParams;
        SetWindowLongA(hwnd, GWL_USERDATA, (LONG)(DWORD)dlg);
    }

    switch (msg) {
    case WM_COMMAND:
        if (dlg) {
            UINT code;
            UINT id;
            O2USHORT notify;
            code = HIWORD(wParam);
            id = LOWORD(wParam);

            /*
             * Win32 multiplexes menu/button commands and child-control
             * notifications through WM_COMMAND.  OS/2 PM does not: child
             * controls notify their owner with WM_CONTROL, while command
             * sources such as pushbuttons arrive as WM_COMMAND.
             *
             * Treating every Win32 WM_COMMAND as an OS/2 WM_COMMAND is not
             * merely cosmetic.  JIGSAW's cheap Open dialog dismisses itself
             * on any WM_COMMAND; focusing/configuring its entry field during
             * WM_INITDLG causes Win32 EDIT notifications and used to dismiss
             * the dialog with control id 258 before the user could open a
             * file.
             */
            if (lParam != 0) {
                char cls[32];
                cls[0] = 0;
                GetClassNameA((HWND)lParam, cls, sizeof(cls));

                if (_stricmp(cls, "BUTTON") == 0 && code == BN_CLICKED) {
                    pm_trace("dialog button command", (unsigned long)id,
                             (unsigned long)code, (unsigned long)(DWORD)lParam);
                    (void)call_dialog_guest(hwnd, dlg, O2_WM_COMMAND,
                                            (O2MPARAM)(DWORD)id, 0);
                } else {
                    if (_stricmp(cls, "LISTBOX") == 0) {
                        if (code == LBN_DBLCLK)
                            notify = O2_LN_ENTER;
                        else if (code == LBN_SELCHANGE)
                            notify = O2_LN_SELECT;
                        else
                            notify = (O2USHORT)code;
                    } else {
                        /*
                         * Preserve the notification value for controls for
                         * which micro-PM does not yet have an explicit OS/2
                         * code map.  The important ABI distinction is that
                         * this is WM_CONTROL, not WM_COMMAND.
                         */
                        notify = (O2USHORT)code;
                    }
                    pm_trace("dialog control notify", (unsigned long)id,
                             (unsigned long)code, (unsigned long)notify);
                    (void)call_dialog_guest(hwnd, dlg, O2_WM_CONTROL,
                        (O2MPARAM)((DWORD)id | ((DWORD)notify << 16)), 0);
                }
            } else {
                pm_trace("dialog command", (unsigned long)id,
                         (unsigned long)code, 0);
                (void)call_dialog_guest(hwnd, dlg, O2_WM_COMMAND,
                                        (O2MPARAM)(DWORD)id, 0);
            }
            return 0;
        }
        break;
    case WM_TIMER:
        if (dlg) {
            (void)call_dialog_guest(hwnd, dlg, O2_WM_TIMER,
                                    (O2MPARAM)(DWORD)wParam, 0);
            return 0;
        }
        break;

    case WM_SIZE:
        if (dlg)
            (void)call_dialog_guest(hwnd, dlg, O2_WM_SIZE, 0,
                (O2MPARAM)((DWORD)LOWORD(lParam) | ((DWORD)HIWORD(lParam) << 16)));
        return 0;

    case WM_MOUSEMOVE:
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_LBUTTONDBLCLK:
        if (dlg) {
            O2USHORT om = msg == WM_MOUSEMOVE ? O2_WM_MOUSEMOVE :
                          (msg == WM_LBUTTONDOWN ? O2_WM_BUTTON1DOWN :
                           (msg == WM_LBUTTONDBLCLK ? O2_WM_BUTTON1DBLCLK : O2_WM_BUTTON1UP));
            (void)call_dialog_guest(hwnd, dlg, om, pm_mouse_point(hwnd, lParam), 0);
            return 0;
        }
        break;

    case WM_VSCROLL:
    case WM_HSCROLL:
        if (dlg) {
            if (lParam && pm_is_trackbar((HWND)lParam)) {
                O2USHORT id;
                O2USHORT notify;
                O2ULONG pos;
                id = (O2USHORT)GetDlgCtrlID((HWND)lParam);
                notify = (LOWORD(wParam) == PMCOMPAT_TB_THUMBTRACK)
                         ? O2_SLN_SLIDERTRACK : O2_SLN_CHANGE;
                pos = (O2ULONG)SendMessageA((HWND)lParam,
                                             PMCOMPAT_TBM_GETPOS, 0, 0);
                pm_trace("dialog slider notify", (unsigned long)id,
                         (unsigned long)notify, (unsigned long)pos);
                (void)call_dialog_guest(hwnd, dlg, O2_WM_CONTROL,
                    (O2MPARAM)((DWORD)id | ((DWORD)notify << 16)),
                    (O2MPARAM)pos);
                return 0;
            }
            {
                O2USHORT code = pm_scroll_code(LOWORD(wParam));
                O2MPARAM smp2 = (O2MPARAM)((DWORD)HIWORD(wParam) | ((DWORD)code << 16));
                (void)call_dialog_guest(hwnd, dlg,
                    msg == WM_HSCROLL ? O2_WM_HSCROLL : O2_WM_VSCROLL,
                    lParam ? guest_hwnd((HWND)lParam) : 0, smp2);
            }
            return 0;
        }
        break;

    case WM_KEYDOWN:
        if (dlg && wParam == VK_RETURN) {
            (void)call_dialog_guest(hwnd, dlg, O2_WM_COMMAND,
                                    (O2MPARAM)O2_DID_OK, 0);
            return 0;
        }
        if (dlg && wParam == VK_ESCAPE) {
            (void)call_dialog_guest(hwnd, dlg, O2_WM_COMMAND,
                                    (O2MPARAM)O2_DID_CANCEL, 0);
            return 0;
        }
        break;
    case HOST_WM_OS2_POST:
        if (dlg) {
            struct PMCompatPostedMsg *pm =
                (struct PMCompatPostedMsg *)(DWORD)wParam;
            if (pm) {
                (void)call_dialog_guest(hwnd, dlg, pm->msg, pm->mp1, pm->mp2);
                HeapFree(GetProcessHeap(), 0, pm);
            }
            return 0;
        }
        break;
    case WM_NCDESTROY:
        if (dlg && dlg->heap_owned) {
            SetWindowLongA(hwnd, GWL_USERDATA, 0);
            if (dlg->control_icon) DestroyIcon(dlg->control_icon);
            HeapFree(GetProcessHeap(), 0, dlg);
            return DefWindowProcA(hwnd, msg, wParam, lParam);
        }
        break;
    case WM_CLOSE:
        if (dlg) {
            (void)call_dialog_guest(hwnd, dlg, O2_WM_COMMAND,
                                    (O2MPARAM)O2_DID_CANCEL, 0);
            if (!dlg->done) {
                dlg->result = 0;
                dlg->done = 1;
                ShowWindow(hwnd, SW_HIDE);
            }
            return 0;
        }
        break;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

static int pm_ensure_dialog_class(void)
{
    WNDCLASSA wc;
    if (g_dialog_class_registered)
        return 1;
    memset(&wc, 0, sizeof(wc));
    wc.style = CS_DBLCLKS;
    wc.lpfnWndProc = pm_dialog_wndproc;
    wc.hInstance = g_instance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = g_dialog_class;
    if (!RegisterClassA(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        return 0;
    g_dialog_class_registered = 1;
    return 1;
}

static char *pm_copy_dialog_text(const unsigned char *blob, DWORD size,
                                 WORD cch, WORD off, WORD *resource_id)
{
    char *out;
    if (resource_id)
        *resource_id = 0;
    if (cch == 0)
        return NULL;
    if ((DWORD)off + (DWORD)cch > size)
        return NULL;
    if (cch == 3 && blob[off] == 0xff) {
        if (resource_id)
            *resource_id = pm_rd16(blob + off + 1);
        return NULL;
    }
    out = (char *)HeapAlloc(GetProcessHeap(), 0, (SIZE_T)cch + 1U);
    if (!out)
        return NULL;
    memcpy(out, blob + off, cch);
    out[cch] = 0;
    return out;
}

static HWND pm_create_dialog_template(O2ULONG module,
                                      const struct PMCompatResource *rr,
                                      HWND owner,
                                      struct PMCompatDialog *dlg)
{
    const unsigned char *b;
    DWORD size;
    WORD off_items, children, cch, off_text;
    const unsigned char *root;
    char *title;
    DWORD style;
    RECT wr;
    int dcx, dcy;
    HWND hwnd;
    WORD i;
    HFONT font;

    if (!rr || !dlg || rr->size < 44UL || !pm_ensure_dialog_class())
        return NULL;
    b = rr->data;
    size = rr->size;
    off_items = pm_rd16(b + 6);
    if ((DWORD)off_items + 30UL > size)
        return NULL;
    root = b + off_items;
    children = pm_rd16(root + 2);
    cch = pm_rd16(root + 8);
    off_text = pm_rd16(root + 10);
    title = pm_copy_dialog_text(b, size, cch, off_text, NULL);
    dcx = (int)(short)pm_rd16(root + 20) * 2;
    dcy = (int)(short)pm_rd16(root + 22) * 2;
    if (dcx < 160) dcx = 160;
    if (dcy < 80) dcy = 80;

    style = WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN;
    wr.left = 0; wr.top = 0; wr.right = dcx; wr.bottom = dcy;
    AdjustWindowRectEx(&wr, style, FALSE, WS_EX_DLGMODALFRAME);
    hwnd = CreateWindowExA(WS_EX_DLGMODALFRAME | WS_EX_CONTROLPARENT,
                           g_dialog_class,
                           title ? title : "",
                           style,
                           CW_USEDEFAULT, CW_USEDEFAULT,
                           wr.right - wr.left, wr.bottom - wr.top,
                           owner, NULL, g_instance, dlg);
    if (title)
        HeapFree(GetProcessHeap(), 0, title);
    if (!hwnd)
        return NULL;

    font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    for (i = 0; i < children; ++i) {
        DWORD item_off = (DWORD)off_items + 30UL * (DWORD)(i + 1U);
        const unsigned char *it;
        WORD cls, id, rid;
        DWORD os_style, ws;
        int x, y, cx, cy;
        char *text;
        const char *klass;
        HWND child;
        if (item_off + 30UL > size)
            break;
        it = b + item_off;
        cls = pm_rd16(it + 6);
        id = pm_rd16(it + 24);
        cch = pm_rd16(it + 8);
        off_text = pm_rd16(it + 10);
        rid = 0;
        text = pm_copy_dialog_text(b, size, cch, off_text, &rid);
        os_style = pm_rd32(it + 12);
        x = (int)(short)pm_rd16(it + 16) * 2;
        cx = (int)(short)pm_rd16(it + 20) * 2;
        cy = (int)(short)pm_rd16(it + 22) * 2;
        y = dcy - ((int)(short)pm_rd16(it + 18) * 2) - cy;
        ws = WS_CHILD;
        if (os_style & 0x80000000UL) ws |= WS_VISIBLE;
        if (os_style & 0x00020000UL) ws |= WS_TABSTOP;
        if (os_style & 0x00010000UL) ws |= WS_GROUP;
        klass = "STATIC";
        if (cls == 3U) {
            klass = "BUTTON";
            if ((os_style & 0x0000000fUL) == 0x0002UL)
                ws |= BS_AUTOCHECKBOX;
            else if (os_style & 0x00000400UL)
                ws |= BS_DEFPUSHBUTTON;
            else
                ws |= BS_PUSHBUTTON;
        } else if (cls == 5U) {
            if ((os_style & 0x0000000fUL) == 0x0002UL) {
                klass = "BUTTON"; ws |= BS_GROUPBOX;
            } else {
                klass = "STATIC";
                if ((os_style & 0x0000ffffUL) == 0x0003UL)
                    ws |= SS_ICON;
                else if (os_style & 0x00000100UL)
                    ws |= SS_CENTER;
                else
                    ws |= SS_LEFT;
            }
        } else if (cls == 6U) {
            klass = "EDIT";
            ws |= WS_BORDER | ES_LEFT | ES_AUTOHSCROLL;
        } else if (cls == 7U) {
            klass = "LISTBOX";
            ws |= WS_BORDER | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT;
        } else if (cls == 8U) {
            klass = "SCROLLBAR";
            ws |= (cx >= cy) ? SBS_HORZ : SBS_VERT;
        } else if (cls == O2_WC_SLIDER) {
            if (pm_ensure_trackbar_class()) {
                klass = PMCOMPAT_TRACKBAR_CLASS;
                ws |= PMCOMPAT_TBS_AUTOTICKS | PMCOMPAT_TBS_FIXEDLENGTH;
                if (cy > cx)
                    ws |= PMCOMPAT_TBS_VERT;
            }
        }
        child = CreateWindowExA(0, klass, text ? text : "", ws,
                                x, y, cx, cy, hwnd,
                                (HMENU)(UINT_PTR)id, g_instance, NULL);
        if (child && font)
            SendMessageA(child, WM_SETFONT, (WPARAM)font, TRUE);
        if (child && cls == O2_WC_SLIDER && pm_is_trackbar(child)) {
            SendMessageA(child, PMCOMPAT_TBM_SETRANGE, TRUE, MAKELONG(0, 99));
            SendMessageA(child, PMCOMPAT_TBM_SETTICFREQ, 10, 0);
            SendMessageA(child, PMCOMPAT_TBM_SETLINESIZE, 0, 1);
            SendMessageA(child, PMCOMPAT_TBM_SETPAGESIZE, 0, 10);
            pm_trace("dialog WC_SLIDER", (unsigned long)id,
                     (unsigned long)(DWORD)child, (unsigned long)os_style);
        }
        if (child && cls == 5U && rid != 0U) {
            const struct PMCompatResource *ir;
            ir = pm_find_resource(module, O2_RT_POINTER, rid);
            if (ir) {
                HICON ico = pm_create_os2_color_icon(ir->data, ir->size);
                if (ico) {
                    SendMessageA(child, STM_SETICON, (WPARAM)ico, 0);
                    if (dlg->control_icon)
                        DestroyIcon(dlg->control_icon);
                    dlg->control_icon = ico;
                }
            }
        }
        if (text)
            HeapFree(GetProcessHeap(), 0, text);
    }
    return hwnd;
}

static O2USHORT os2_vk_from_win(WPARAM vk)
{
    if (vk >= VK_F1 && vk <= VK_F24)
        return (O2USHORT)(O2_VK_F1 + (vk - VK_F1));
    switch (vk) {
    case VK_CANCEL:   return O2_VK_BREAK;
    case VK_BACK:     return O2_VK_BACKSPACE;
    case VK_TAB:
        return (GetKeyState(VK_SHIFT) & 0x8000) ? O2_VK_BACKTAB : O2_VK_TAB;
    case VK_RETURN:   return O2_VK_ENTER;
    case VK_SHIFT:
    case VK_LSHIFT:
    case VK_RSHIFT:   return O2_VK_SHIFT;
    case VK_CONTROL:
    case VK_LCONTROL:
    case VK_RCONTROL: return O2_VK_CTRL;
    case VK_MENU:
    case VK_LMENU:
    case VK_RMENU:    return O2_VK_ALT;
    case VK_PAUSE:    return O2_VK_PAUSE;
    case VK_CAPITAL:  return O2_VK_CAPSLOCK;
    case VK_ESCAPE:   return O2_VK_ESC;
    case VK_SPACE:    return O2_VK_SPACE;
    case VK_PRIOR:    return O2_VK_PAGEUP;
    case VK_NEXT:     return O2_VK_PAGEDOWN;
    case VK_END:      return O2_VK_END;
    case VK_HOME:     return O2_VK_HOME;
    case VK_LEFT:     return O2_VK_LEFT;
    case VK_UP:       return O2_VK_UP;
    case VK_RIGHT:    return O2_VK_RIGHT;
    case VK_DOWN:     return O2_VK_DOWN;
    case VK_SNAPSHOT: return O2_VK_PRINTSCRN;
    case VK_INSERT:   return O2_VK_INSERT;
    case VK_DELETE:   return O2_VK_DELETE;
    case VK_SCROLL:   return O2_VK_SCRLLOCK;
    case VK_NUMLOCK:  return O2_VK_NUMLOCK;
    default:          return 0;
    }
}

static O2ULONG sarien_function_key(WPARAM vk)
{
    switch (vk) {
    case VK_F1:  return SARIEN_KEY_F1;
    case VK_F2:  return SARIEN_KEY_F2;
    case VK_F3:  return SARIEN_KEY_F3;
    case VK_F4:  return SARIEN_KEY_F4;
    case VK_F5:  return SARIEN_KEY_F5;
    case VK_F6:  return SARIEN_KEY_F6;
    case VK_F7:  return SARIEN_KEY_F7;
    case VK_F8:  return SARIEN_KEY_F8;
    case VK_F9:  return SARIEN_KEY_F9;
    case VK_F10: return SARIEN_KEY_F10;
    case VK_F11: return SARIEN_KEY_F11;
    case VK_F12: return SARIEN_KEY_F12;
    default:     return 0;
    }
}

static O2USHORT key_flags(LPARAM lParam, int keyup)
{
    O2USHORT flags;
    flags = 0;
    if (GetKeyState(VK_SHIFT) & 0x8000) flags |= O2_KC_SHIFT;
    if (GetKeyState(VK_CONTROL) & 0x8000) flags |= O2_KC_CTRL;
    if (GetKeyState(VK_MENU) & 0x8000) flags |= O2_KC_ALT;
    if (((DWORD)lParam & 0x40000000UL) != 0) flags |= O2_KC_PREVDOWN;
    if (keyup) flags |= O2_KC_KEYUP;
    if ((((DWORD)lParam >> 16) & 0xffUL) != 0) flags |= O2_KC_SCANCODE;
    return flags;
}

static O2MPARAM make_mp1(O2USHORT flags, LPARAM lParam)
{
    DWORD rep;
    DWORD scan;
    rep = (DWORD)lParam & 0xffffUL;
    if (rep > 255UL) rep = 255UL;
    scan = ((DWORD)lParam >> 16) & 0xffUL;
    return (O2MPARAM)((DWORD)flags | (rep << 16) | (scan << 24));
}

static int key_generates_char(WPARAM vk)
{
    if ((vk >= '0' && vk <= '9') || (vk >= 'A' && vk <= 'Z'))
        return 1;
    if (vk >= VK_NUMPAD0 && vk <= VK_DIVIDE)
        return 1;
    if (vk >= VK_OEM_1 && vk <= VK_OEM_102)
        return 1;
    switch (vk) {
    case VK_BACK:
    case VK_TAB:
    case VK_RETURN:
    case VK_ESCAPE:
    case VK_SPACE:
        return 1;
    default:
        return 0;
    }
}

static LRESULT CALLBACK pm_wndproc(HWND hwnd, UINT msg,
                                   WPARAM wParam, LPARAM lParam)
{
    O2USHORT ovk;
    O2USHORT flags;
    O2MPARAM mp1;
    O2MPARAM mp2;
    O2ULONG skey;
    O2MRESULT r;

    if (pm_trace_enabled() &&
        (g_native_create_depth != 0 || g_native_show_depth != 0 ||
         msg == WM_CREATE || msg == WM_PAINT || msg == WM_CLOSE ||
         msg == WM_DESTROY || msg == WM_NCDESTROY)) {
        const char *kind;
        if (g_native_create_depth != 0)
            kind = "native create msg";
        else if (g_native_show_depth != 0)
            kind = "native show msg";
        else
            kind = "native message";
        pm_trace(kind, (unsigned long)msg, (unsigned long)(DWORD)hwnd,
                 (unsigned long)wParam);
    }

    switch (msg) {
    case WM_NCCREATE:
        {
            CREATESTRUCTA *cs = (CREATESTRUCTA *)lParam;
            struct PMCompatWindow *st = (struct PMCompatWindow *)cs->lpCreateParams;
            if (st) SetWindowLongA(hwnd, GWL_USERDATA, (LONG)(DWORD)st);
        }
        break;

    case HOST_WM_OS2_POST:
        {
            struct PMCompatPostedMsg *pm;
            pm = (struct PMCompatPostedMsg *)(DWORD)wParam;
            if (pm) {
                (void)call_guest(hwnd, pm->msg, pm->mp1, pm->mp2);
                HeapFree(GetProcessHeap(), 0, pm);
            }
            return 0;
        }

    case WM_CREATE:
        {
            char bypass[8];
            DWORD bn;
            bn = GetEnvironmentVariableA("OS2_PM_BYPASS_WM_CREATE",
                                         bypass, sizeof(bypass));
            if (bn != 0 && bn < sizeof(bypass) && bypass[0] != '0') {
                pm_trace("WM_CREATE bypass",
                         (unsigned long)(DWORD)hwnd, 0, 0);
                return 0;
            }
        }
        r = call_guest(hwnd, O2_WM_CREATE, 0, 0);
        return (LRESULT)r;

    case WM_SIZE:
        mp2 = (O2MPARAM)((DWORD)LOWORD(lParam) |
                         ((DWORD)HIWORD(lParam) << 16));
        pm_trace("WM_SIZE -> guest", (unsigned long)(DWORD)hwnd,
                 (unsigned long)LOWORD(lParam),
                 (unsigned long)HIWORD(lParam));
        (void)call_guest(hwnd, O2_WM_SIZE, 0, mp2);
        return 0;

    case WM_PAINT:
        /*
         * ShowWindow may ask USER32 to paint synchronously.  Do not re-enter
         * the guest while we are only making the native peer visible.  The
         * deferred-show path invalidates the window again immediately after
         * ShowWindow returns, so the ordinary PM message loop receives the
         * real WM_PAINT next.
         */
        if (g_native_show_depth != 0) {
            PAINTSTRUCT ps;
            HDC dc;
            dc = BeginPaint(hwnd, &ps);
            if (dc)
                EndPaint(hwnd, &ps);
            pm_trace("paint deferred", (unsigned long)(DWORD)hwnd, 0, 0);
            return 0;
        }
        (void)call_guest(hwnd, O2_WM_PAINT, 0, 0);
        /* A misbehaving guest must not leave Win32 spinning on WM_PAINT. */
        ValidateRect(hwnd, NULL);
        return 0;

    case WM_TIMER:
        pm_trace("WM_TIMER -> guest", (unsigned long)(DWORD)hwnd,
                 (unsigned long)wParam, 0);
        (void)call_guest(hwnd, O2_WM_TIMER,
                         (O2MPARAM)(DWORD)wParam, 0);
        return 0;

    case WM_ERASEBKGND:
        if (g_native_show_depth != 0) {
            pm_trace("erase deferred", (unsigned long)(DWORD)hwnd,
                     (unsigned long)wParam, 0);
            return DefWindowProcA(hwnd, msg, wParam, lParam);
        }
        (void)call_guest(hwnd, O2_WM_ERASEBACKGROUND, 0, 0);
        return 1;

    case WM_COMMAND:
        if (lParam == 0 && pm_is_syscommand((O2USHORT)LOWORD(wParam))) {
            O2USHORT sid;
            sid = (O2USHORT)LOWORD(wParam);
            pm_trace("OS/2 syscommand", (unsigned long)sid,
                     (unsigned long)(DWORD)hwnd, 0);
            if (sid == O2_SC_CLOSE) {
                SendMessageA(hwnd, WM_CLOSE, 0, 0);
                return 0;
            }
            if (sid == O2_SC_MINIMIZE) { ShowWindow(hwnd, SW_MINIMIZE); return 0; }
            if (sid == O2_SC_MAXIMIZE) { ShowWindow(hwnd, SW_MAXIMIZE); return 0; }
            if (sid == O2_SC_RESTORE)  { ShowWindow(hwnd, SW_RESTORE);  return 0; }
            (void)call_guest(hwnd, O2_WM_SYSCOMMAND, (O2MPARAM)sid, 0);
            return 0;
        }
        mp1 = (O2MPARAM)((DWORD)LOWORD(wParam));
        mp2 = 0;
        (void)call_guest(hwnd, O2_WM_COMMAND, mp1, mp2);
        return 0;

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        if (key_generates_char(wParam))
            break; /* TranslateMessage will supply WM_CHAR/WM_SYSCHAR. */
        ovk = os2_vk_from_win(wParam);
        if (ovk != 0) {
            flags = (O2USHORT)(key_flags(lParam, 0) | O2_KC_VIRTUALKEY);
            mp1 = make_mp1(flags, lParam);
            skey = sarien_function_key(wParam);
            if (skey != 0)
                mp2 = (O2MPARAM)(((DWORD)ovk << 16) | (skey & 0xffffUL));
            else
                mp2 = (O2MPARAM)((DWORD)ovk << 16);
            (void)call_guest(hwnd, O2_WM_CHAR, mp1, mp2);
            return 0;
        }
        break;

    case WM_KEYUP:
    case WM_SYSKEYUP:
        ovk = os2_vk_from_win(wParam);
        if (ovk != 0) {
            flags = (O2USHORT)(key_flags(lParam, 1) | O2_KC_VIRTUALKEY);
            mp1 = make_mp1(flags, lParam);
            mp2 = (O2MPARAM)((DWORD)ovk << 16);
            (void)call_guest(hwnd, O2_WM_CHAR, mp1, mp2);
            return 0;
        }
        break;

    case WM_CHAR:
    case WM_SYSCHAR:
        {
            O2USHORT acmd;
            acmd = pm_match_accel(msg, wParam);
            if (acmd != 0) {
                pm_trace("accelerator command", (unsigned long)acmd,
                         (unsigned long)wParam, (unsigned long)msg);
                (void)call_guest(hwnd, O2_WM_COMMAND,
                                 (O2MPARAM)(DWORD)acmd,
                                 (O2MPARAM)3UL);
                return 0;
            }
        }
        ovk = os2_vk_from_win(MapVirtualKeyA(((DWORD)lParam >> 16) & 0xffUL,
                                             MAPVK_VSC_TO_VK));
        flags = (O2USHORT)(key_flags(lParam, 0) | O2_KC_CHAR);
        if (ovk != 0)
            flags |= O2_KC_VIRTUALKEY;
        mp1 = make_mp1(flags, lParam);
        mp2 = (O2MPARAM)(((DWORD)ovk << 16) | ((DWORD)wParam & 0xffffUL));
        pm_trace("WM_CHAR translate", (unsigned long)mp1,
                 (unsigned long)mp2, (unsigned long)wParam);
        (void)call_guest(hwnd, O2_WM_CHAR, mp1, mp2);
        return 0;

    case WM_MOUSEMOVE:
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_LBUTTONDBLCLK:
        (void)call_guest(hwnd,
            msg == WM_MOUSEMOVE ? O2_WM_MOUSEMOVE :
            (msg == WM_LBUTTONDOWN ? O2_WM_BUTTON1DOWN :
             (msg == WM_LBUTTONDBLCLK ? O2_WM_BUTTON1DBLCLK : O2_WM_BUTTON1UP)),
            pm_mouse_point(hwnd, lParam), 0);
        return 0;

    case WM_VSCROLL:
    case WM_HSCROLL:
        {
            O2USHORT code = pm_scroll_code(LOWORD(wParam));
            O2MPARAM smp2 = (O2MPARAM)((DWORD)HIWORD(wParam) | ((DWORD)code << 16));
            (void)call_guest(hwnd,
                msg == WM_HSCROLL ? O2_WM_HSCROLL : O2_WM_VSCROLL,
                lParam ? guest_hwnd((HWND)lParam) : 0, smp2);
            return 0;
        }

    /*
     * A real OS/2 PM window has no Win32 IME context.  Letting these fall
     * through to USER32/IMM32 during activation can enter host IME plumbing
     * while an OS/2 guest callback frame is active.  Keep the host IME
     * detached from compatibility windows; keyboard input is translated
     * explicitly above.
     */
    case WM_IME_SETCONTEXT:
    case WM_IME_NOTIFY:
    case WM_IME_CONTROL:
    case WM_IME_COMPOSITIONFULL:
    case WM_IME_SELECT:
    case WM_IME_CHAR:
    case WM_IME_REQUEST:
    case WM_IME_KEYDOWN:
    case WM_IME_KEYUP:
    case WM_IME_STARTCOMPOSITION:
    case WM_IME_ENDCOMPOSITION:
    case WM_IME_COMPOSITION:
        pm_trace("IME suppressed", (unsigned long)msg,
                 (unsigned long)(DWORD)hwnd, (unsigned long)wParam);
        return 0;

    case WM_CLOSE:
        pm_trace("WM_CLOSE -> guest", (unsigned long)(DWORD)hwnd, 0, 0);
        (void)call_guest(hwnd, O2_WM_CLOSE, 0, 0);
        if (IsWindow(hwnd)) DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        pm_trace("WM_DESTROY", (unsigned long)(DWORD)hwnd, 0, 0);
        (void)call_guest(hwnd, O2_WM_DESTROY, 0, 0);
        if (hwnd == g_frame_hwnd) PostQuitMessage(0);
        return 0;

    case WM_NCDESTROY:
        {
            struct PMCompatWindow *st = pm_window_state(hwnd);
            WNDPROC native_proc = st ? st->native_proc : NULL;
            LRESULT native_result = 0;
            RemovePropA(hwnd, g_update_disabled_prop);
            RemovePropA(hwnd, g_object_parent_prop);
            if (native_proc)
                native_result = CallWindowProcA(native_proc, hwnd, msg, wParam, lParam);
            if (st) {
                SetWindowLongA(hwnd,GWL_USERDATA,0);
                HeapFree(GetProcessHeap(),0,st);
            }
            if (native_proc)
                return native_result;
        }
        break;
    }

    {
        struct PMCompatWindow *st;
        WNDPROC native_proc;
        st = pm_window_state(hwnd);
        native_proc = st ? st->native_proc : NULL;
        if (pm_trace_enabled() && g_native_show_depth != 0) {
            LRESULT dr;
            pm_trace(native_proc ? "CallWindowProc enter" : "DefWindowProc enter",
                     (unsigned long)msg, (unsigned long)(DWORD)hwnd,
                     (unsigned long)wParam);
            dr = native_proc ? CallWindowProcA(native_proc, hwnd, msg, wParam, lParam)
                             : DefWindowProcA(hwnd, msg, wParam, lParam);
            pm_trace(native_proc ? "CallWindowProc leave" : "DefWindowProc leave",
                     (unsigned long)msg, (unsigned long)(DWORD)hwnd,
                     (unsigned long)(DWORD)dr);
            return dr;
        }
        if (native_proc)
            return CallWindowProcA(native_proc, hwnd, msg, wParam, lParam);
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

static LONG pm_ps_height(struct CompatPS *p)
{
    RECT rc;
    if (!p)
        return 0;
    if (p->bitmap)
        return p->bitmap->height;
    if (p->hwnd && GetClientRect(p->hwnd, &rc))
        return rc.bottom - rc.top;
    if (p->dc)
        return (LONG)GetDeviceCaps(p->dc, VERTRES);
    return 0;
}

static struct CompatBitmap *pm_wrap_native_bitmap(HBITMAP hb)
{
    BITMAP bm;
    BITMAPINFO *bi;
    DWORD colors, cb;
    WORD bpp;
    HDC dc;
    struct CompatBitmap *b;
    void *bits;
    int i;

    if (!hb || !GetObjectA(hb, sizeof(bm), &bm))
        return NULL;
    bpp = (WORD)(bm.bmPlanes * bm.bmBitsPixel);
    if (bpp != 1 && bpp != 4 && bpp != 8 && bpp != 24 && bpp != 32)
        bpp = 32;
    colors = bpp <= 8 ? (1UL << bpp) : 0UL;
    cb = sizeof(BITMAPINFOHEADER) + colors * sizeof(RGBQUAD);
    bi = (BITMAPINFO *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, cb);
    if (!bi)
        return NULL;
    bi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi->bmiHeader.biWidth = bm.bmWidth;
    bi->bmiHeader.biHeight = bm.bmHeight;
    bi->bmiHeader.biPlanes = 1;
    bi->bmiHeader.biBitCount = bpp;
    bi->bmiHeader.biCompression = BI_RGB;
    bi->bmiHeader.biClrUsed = colors;
    bi->bmiHeader.biClrImportant = colors;

    b = (struct CompatBitmap *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
                                         sizeof(*b));
    if (!b) {
        HeapFree(GetProcessHeap(), 0, bi);
        return NULL;
    }
    b->magic = 0x4d42324fUL;
    b->width = bm.bmWidth;
    b->height = bm.bmHeight;
    b->bitcount = bpp;
    b->stride = (LONG)((((DWORD)b->width * bpp + 31UL) / 32UL) * 4UL);
    bi->bmiHeader.biSizeImage = (DWORD)(b->stride * b->height);
    bits = NULL;
    b->native_bitmap = CreateDIBSection(NULL, bi, DIB_RGB_COLORS, &bits,
                                        NULL, 0);
    if (!b->native_bitmap || !bits) {
        if (b->native_bitmap) DeleteObject(b->native_bitmap);
        HeapFree(GetProcessHeap(), 0, b);
        HeapFree(GetProcessHeap(), 0, bi);
        return NULL;
    }
    b->bits = (BYTE *)bits;
    dc = GetDC(NULL);
    if (!dc || !GetDIBits(dc, hb, 0, (UINT)b->height, b->bits, bi,
                          DIB_RGB_COLORS)) {
        if (dc) ReleaseDC(NULL, dc);
        DeleteObject(b->native_bitmap);
        HeapFree(GetProcessHeap(), 0, b);
        HeapFree(GetProcessHeap(), 0, bi);
        return NULL;
    }
    ReleaseDC(NULL, dc);
    for (i = 0; i < (int)colors && i < 256; ++i)
        b->palette[i] = bi->bmiColors[i];
    HeapFree(GetProcessHeap(), 0, bi);
    return b;
}

static HBITMAP pm_native_bitmap_from_compat(struct CompatBitmap *b)
{
    BITMAPINFO *bi;
    DWORD colors;
    DWORD cb;
    HDC dc;
    HBITMAP hb;
    unsigned i;
    if (!b || b->magic != 0x4d42324fUL || !b->bits)
        return NULL;
    colors = b->bitcount <= 8 ? 256UL : 0UL;
    cb = sizeof(BITMAPINFOHEADER) + colors * sizeof(RGBQUAD);
    bi = (BITMAPINFO *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, cb);
    if (!bi) return NULL;
    bi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi->bmiHeader.biWidth = b->width;
    bi->bmiHeader.biHeight = b->height;
    bi->bmiHeader.biPlanes = 1;
    bi->bmiHeader.biBitCount = b->bitcount;
    bi->bmiHeader.biCompression = BI_RGB;
    bi->bmiHeader.biSizeImage = (DWORD)(b->stride * b->height);
    bi->bmiHeader.biClrUsed = colors;
    if (colors) {
        for (i=0; i<colors; ++i) bi->bmiColors[i]=b->palette[i];
    }
    dc = GetDC(NULL);
    hb = CreateDIBitmap(dc, &bi->bmiHeader, CBM_INIT, b->bits, bi, DIB_RGB_COLORS);
    ReleaseDC(NULL, dc);
    HeapFree(GetProcessHeap(),0,bi);
    return hb;
}

static int pm_load_string_resource(O2ULONG module, O2USHORT id,
                                   char *out, int maxchars)
{
    const struct PMCompatResource *rr;
    const unsigned char *b;
    DWORD pos;
    O2USHORT bundle;
    unsigned index, i;
    unsigned len;
    if (!out || maxchars <= 0) return 0;
    out[0]=0;

    /* OS/2 RT_STRING resources are bundles of 16 ids.  The resource
       payload begins with a codepage WORD followed immediately by 16
       length-prefixed, NUL-terminated strings; empty/missing slots are
       represented by a one-byte NUL string.  BIO happened to have an empty
       id 0, which made an earlier parser accidentally look as if bytes 2/3
       were a first-id WORD.  OPENDLG has a real id-0 string and exposes that
       mistake. */
    bundle = (O2USHORT)((id / 16U) + 1U);
    index = (unsigned)(id & 15U);
    rr = pm_find_resource(module, O2_RT_STRING, bundle);
    if (!rr || rr->size < 3UL) return 0;
    b=rr->data;
    pos=2;
    for (i=0; i<=index; ++i) {
        if (pos >= rr->size) return 0;
        len=b[pos++];
        if (len == 0 || pos+len > rr->size) return 0;
        if (i==index) {
            unsigned chars=len;
            if (chars && b[pos+chars-1]==0) --chars;
            if ((int)chars >= maxchars) chars=(unsigned)(maxchars-1);
            memcpy(out,b+pos,chars); out[chars]=0;
            return (int)chars;
        }
        pos += len;
    }
    return 0;
}

/* 701 */
O2ULONG __cdecl WinAlarm(O2HWND desktop, O2USHORT type)
{
    (void)desktop; (void)type;
    return MessageBeep(MB_ICONHAND) ? 1UL : 0UL;
}


/* 702 - snapshot the immediate children of a PM window in current Z order. */
O2HENUM __cdecl WinBeginEnumWindows(O2HWND hwnd)
{
    struct PMCompatEnum *e;
    HWND parent, child;
    DWORD skipped;
    parent = native_hwnd(hwnd);
    if (!parent)
        return 0;
    e = (struct PMCompatEnum *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
                                         sizeof(*e));
    if (!e)
        return 0;
    e->magic = PMCOMPAT_ENUM_MAGIC;
    skipped = 0;
    child = GetWindow(parent, GW_CHILD);
    while (child && e->count < PMCOMPAT_ENUM_MAX_WINDOWS) {
        /* HWND_DESKTOP is a guest namespace, not permission to enumerate
           Explorer and every unrelated top-level Win32 window.  os2host32
           currently hosts one guest process, so process ownership is the
           safe V1 namespace boundary. */
        if (hwnd != O2_HWND_DESKTOP || pm_is_own_process_window(child))
            e->windows[e->count++] = child;
        else
            ++skipped;
        child = GetWindow(child, GW_HWNDNEXT);
    }
    pm_trace("WinBeginEnumWindows", (unsigned long)hwnd,
             (unsigned long)e->count, (unsigned long)skipped);
    return (O2HENUM)(DWORD)e;
}

/* 703 */
O2HPS __cdecl WinBeginPaint(O2HWND hwnd, O2HPS hps, void *prcl)
{
    HWND wh;
    struct CompatPS *p;
    RECT rc;
    pm_trace("WinBeginPaint call", (unsigned long)hwnd,
             (unsigned long)hps, (unsigned long)(DWORD)prcl);
    wh = native_hwnd(hwnd);
    if (!wh)
        return 0;

    if (hps != 0) {
        p = ps_from(hps);
        if (!p)
            return 0;
        if (prcl) {
            if (!GetUpdateRect(wh, &rc, FALSE))
                GetClientRect(wh, &rc);
            rect_win_to_os2(wh, &rc, (O2RECTL *)prcl);
        }
        ValidateRect(wh, NULL);
        return hps;
    }

    p = alloc_ps(wh, NULL, PMCOMPAT_PS_END_PAINT);
    if (!p)
        return 0;
    p->dc = BeginPaint(wh, &p->paint);
    if (!p->dc) {
        HeapFree(GetProcessHeap(), 0, p);
        return 0;
    }
    /* alloc_ps() ran before BeginPaint(), so it had no HDC to save.
       Save the real paint DC now so GPI clip/text/object state cannot
       escape this OS/2 HPS lifetime. */
    p->saved_dc = SaveDC(p->dc);
    if (prcl)
        rect_win_to_os2(wh, &p->paint.rcPaint, (O2RECTL *)prcl);
    if (prcl) {
        O2RECTL *o = (O2RECTL *)prcl;
        pm_trace("WinBeginPaint rect",
                 (unsigned long)(DWORD)o->xLeft,
                 (unsigned long)(DWORD)o->yBottom,
                 (unsigned long)(DWORD)o->xRight);
        pm_trace("WinBeginPaint rect2",
                 (unsigned long)(DWORD)o->yTop,
                 (unsigned long)(DWORD)p,
                 (unsigned long)(DWORD)p->dc);
    }
    return (O2HPS)(DWORD)p;
}

static void pm_fill_qmsg(O2QMSG *q, const MSG *m)
{
    RECT cr;
    if (!q || !m) return;
    memset(q, 0, sizeof(*q));
    q->hwnd = m->hwnd ? guest_hwnd(m->hwnd) : 0;
    q->msg = (O2USHORT)m->message;
    q->mp1 = (O2MPARAM)(DWORD)m->wParam;
    q->mp2 = (O2MPARAM)(DWORD)m->lParam;
    q->time = (O2ULONG)m->time;
    q->ptl.x = m->pt.x;
    q->ptl.y = m->pt.y;
    if (m->hwnd && GetClientRect(m->hwnd, &cr))
        q->ptl.y = cr.bottom - q->ptl.y;
}

/* 716 */
O2HMQ __cdecl WinCreateMsgQueue(O2HAB hab, O2LONG cmsg)
{
    MSG m;
    DWORD tid;
    (void)hab; (void)cmsg;
    /* Force USER32 to create this thread's message queue, then use the
       Win32 thread id as our compact HMQ token.  WinPostQueueMsg can then
       target a real PM worker queue without host-global state. */
    PeekMessageA(&m, NULL, WM_USER, WM_USER, PM_NOREMOVE);
    tid = GetCurrentThreadId();
    pm_trace("WinCreateMsgQueue", (unsigned long)hab,
             (unsigned long)cmsg, (unsigned long)tid);
    return (O2HMQ)tid;
}

/* 726 */
O2ULONG __cdecl WinDestroyMsgQueue(O2HMQ hmq)
{
    (void)hmq;
    return 1;
}

/* 728 */
O2ULONG __cdecl WinDestroyWindow(O2HWND hwnd)
{
    HWND wh;
    wh = native_hwnd(hwnd);
    if (!wh || wh == GetDesktopWindow())
        return 0;
    return DestroyWindow(wh) ? 1UL : 0UL;
}

/* 707 */
O2ULONG __cdecl WinCloseClipbrd(O2HAB hab)
{
    (void)hab;
    return CloseClipboard() ? 1UL : 0UL;
}

/* 710 */
O2ULONG __cdecl WinCopyRect(O2HAB hab, O2RECTL *dst, const O2RECTL *src)
{
    (void)hab;
    if (!dst || !src) return 0;
    *dst=*src;
    return 1;
}

/* 729 */
O2ULONG __cdecl WinDismissDlg(O2HWND hwndDlg, O2USHORT result)
{
    HWND wh;
    struct PMCompatDialog *dlg;
    wh = native_hwnd(hwndDlg);
    if (!wh)
        return 0;
    dlg = (struct PMCompatDialog *)(DWORD)GetWindowLongA(wh, GWL_USERDATA);
    if (!dlg)
        return 0;
    dlg->result = result;
    dlg->done = 1;
    ShowWindow(wh, SW_HIDE);
    return 1;
}


/* 730 - draw a GPI compatibility bitmap into a PM presentation space. */
O2ULONG __cdecl WinDrawBitmap(O2HPS hps, O2HBITMAP hbm,
                              const O2RECTL *srcRect, O2POINTL *dest,
                              O2LONG foreColor, O2LONG backColor,
                              O2ULONG flags)
{
    struct CompatPS *p;
    struct CompatBitmap *b;
    HDC memdc;
    HGDIOBJ old;
    LONG sx, sy, sw, sh, dh;
    LONG dx, dy, dw, dheight;
    DWORD rop;
    int stretch;
    BOOL ok;
    COLORREF oldText, oldBack;

    p = ps_from(hps);
    b = (struct CompatBitmap *)(DWORD)hbm;
    if (!p || !p->dc || !b || b->magic != 0x4d42324fUL ||
        !b->native_bitmap || !dest)
        return 0;

    if (srcRect) {
        sx = srcRect->xLeft;
        sw = srcRect->xRight - srcRect->xLeft;
        sh = srcRect->yTop - srcRect->yBottom;
        sy = b->height - srcRect->yTop;
    } else {
        sx = 0; sy = 0; sw = b->width; sh = b->height;
    }
    if (sw <= 0 || sh <= 0)
        return 0;

    stretch = (flags & 0x0004UL) != 0; /* DBM_STRETCH */
    dh = pm_ps_height(p);
    if (stretch) {
        const O2RECTL *dr = (const O2RECTL *)dest;
        dx = dr->xLeft;
        dw = dr->xRight - dr->xLeft;
        dheight = dr->yTop - dr->yBottom;
        dy = dh - dr->yTop;
    } else {
        dx = dest->x;
        dw = sw;
        dheight = sh;
        dy = dh - dest->y - dheight;
    }
    if (dw <= 0 || dheight <= 0)
        return 0;

    memdc = CreateCompatibleDC(p->dc);
    if (!memdc)
        return 0;
    old = SelectObject(memdc, b->native_bitmap);
    if (!old || old == HGDI_ERROR) {
        DeleteDC(memdc);
        return 0;
    }
    oldText = SetTextColor(p->dc, os2_color(foreColor));
    oldBack = SetBkColor(p->dc, os2_color(backColor));
    rop = (flags & 0x0001UL) ? NOTSRCCOPY : SRCCOPY; /* DBM_INVERT */
    if (stretch) {
        SetStretchBltMode(p->dc, (flags & 0x0002UL) ? HALFTONE : COLORONCOLOR);
        ok = StretchBlt(p->dc, (int)dx, (int)dy, (int)dw, (int)dheight,
                        memdc, (int)sx, (int)sy, (int)sw, (int)sh, rop);
    } else {
        ok = BitBlt(p->dc, (int)dx, (int)dy, (int)dw, (int)dheight,
                    memdc, (int)sx, (int)sy, rop);
    }
    SetTextColor(p->dc, oldText);
    SetBkColor(p->dc, oldBack);
    SelectObject(memdc, old);
    DeleteDC(memdc);
    pm_trace(ok ? "WinDrawBitmap OK" : "WinDrawBitmap FAIL",
             (unsigned long)hps, (unsigned long)hbm,
             (unsigned long)flags);
    return ok ? 1UL : 0UL;
}

/* 733 */
O2ULONG __cdecl WinEmptyClipbrd(O2HAB hab)
{
    (void)hab;
    return EmptyClipboard() ? 1UL : 0UL;
}

/* 736 */
O2ULONG __cdecl WinEnableWindowUpdate(O2HWND hwnd, O2ULONG enable)
{
    HWND wh;
    wh=native_hwnd(hwnd);
    if (!wh) return 0;

    /* OS/2 WinEnableWindowUpdate(FALSE) suppresses subsequent presentation
       without immediately erasing a visible window.  A later
       WinShowWindow(hwnd, TRUE) is documented to make the accumulated
       changes visible again.  WM_SETREDRAW is a good Win32 analogue for
       the suppression, but ShowWindow alone does not undo it, so remember
       that state explicitly for WinShowWindow. */
    if (!enable) {
        SetPropA(wh, g_update_disabled_prop, (HANDLE)1);
        SendMessageA(wh, WM_SETREDRAW, FALSE, 0);
        return 1;
    }

    SendMessageA(wh, WM_SETREDRAW, TRUE, 0);
    RemovePropA(wh, g_update_disabled_prop);
    InvalidateRect(wh, NULL, TRUE);
    UpdateWindow(wh);
    return 1;
}

/* 737 */
O2ULONG __cdecl WinEndEnumWindows(O2HENUM henum)
{
    struct PMCompatEnum *e;
    e = (struct PMCompatEnum *)(DWORD)henum;
    if (!e || e->magic != PMCOMPAT_ENUM_MAGIC)
        return 0;
    e->magic = 0;
    HeapFree(GetProcessHeap(), 0, e);
    return 1;
}

/* 738 */
O2ULONG __cdecl WinEndPaint(O2HPS hps)
{
    struct CompatPS *p;
    p = ps_from(hps);
    if (!p)
        return hps ? 1UL : 0UL;
    if (p->flags & PMCOMPAT_PS_END_PAINT) {
        if (p->dc && p->saved_dc)
            RestoreDC(p->dc, p->saved_dc);
        EndPaint(p->hwnd, &p->paint);
        p->magic = 0;
        HeapFree(GetProcessHeap(), 0, p);
    }
    return 1;
}

/* 743 */
O2ULONG __cdecl WinFillRect(O2HPS hps, void *prcl, O2LONG color)
{
    struct CompatPS *p;
    O2RECTL full;
    O2RECTL *or;
    RECT wr;
    HBRUSH brush;
    int owned;
    pm_trace("WinFillRect call", (unsigned long)hps,
             (unsigned long)(DWORD)prcl, (unsigned long)(DWORD)color);
    p = ps_from(hps);
    if (!p || !p->dc)
        return 0;
    if (prcl)
        or = (O2RECTL *)prcl;
    else {
        client_rect_os2(p->hwnd, &full);
        or = &full;
    }
    rect_os2_to_win(p->hwnd, or, &wr);
    owned = 0;
    switch (color) {
    case O2_SYSCLR_WINDOW:
        brush = GetSysColorBrush(COLOR_WINDOW);
        break;
    default:
        brush = CreateSolidBrush(os2_color(color));
        owned = 1;
        break;
    }
    if (!brush)
        return 0;
    pm_trace("WinFillRect rect", (unsigned long)(DWORD)or->xLeft,
             (unsigned long)(DWORD)or->yBottom,
             (unsigned long)(DWORD)or->xRight);
    pm_trace("WinFillRect rect2", (unsigned long)(DWORD)or->yTop,
             (unsigned long)(DWORD)wr.top, (unsigned long)(DWORD)wr.bottom);
    FillRect(p->dc, &wr, brush);
    if (owned)
        DeleteObject(brush);
    pm_trace("WinFillRect leave", (unsigned long)hps, 1, 0);
    return 1;
}

/* 746 */
O2ULONG __cdecl WinFocusChange(O2HWND desktop, O2HWND hwnd, O2ULONG flags)
{
    (void)desktop; (void)flags;
    SetFocus(native_hwnd(hwnd));
    return 1;
}

/* 756 */
O2HWND __cdecl WinGetNextWindow(O2HENUM henum)
{
    struct PMCompatEnum *e;
    HWND wh;
    e = (struct PMCompatEnum *)(DWORD)henum;
    if (!e || e->magic != PMCOMPAT_ENUM_MAGIC)
        return 0;
    while (e->index < e->count) {
        wh = e->windows[e->index++];
        if (wh && IsWindow(wh))
            return guest_hwnd(wh);
    }
    return 0;
}

/* 757 */
O2HPS __cdecl WinGetPS(O2HWND hwnd)
{
    HWND wh;
    HDC dc;
    struct CompatPS *p;
    wh = native_hwnd(hwnd);
    if (!wh)
        return 0;
    dc = GetDC(wh);
    if (!dc)
        return 0;
    p = alloc_ps(wh, dc, PMCOMPAT_PS_RELEASE_DC);
    if (!p) {
        ReleaseDC(wh, dc);
        pm_trace("WinGetPS alloc fail", (unsigned long)hwnd, 0, 0);
        return 0;
    }
    pm_trace("WinGetPS", (unsigned long)hwnd,
             (unsigned long)(DWORD)p, (unsigned long)(DWORD)dc);
    return (O2HPS)(DWORD)p;
}

/* 763 */
O2HAB __cdecl WinInitialize(O2ULONG reserved)
{
    if (pm_trace_enabled() && !g_trace_veh)
        g_trace_veh = AddVectoredExceptionHandler(1, pm_trace_veh);
    pm_trace("WinInitialize", (unsigned long)reserved,
             (unsigned long)(DWORD)g_trace_veh, 1);
    return 1;
}

/* 765 */
O2ULONG __cdecl WinInvalidateRect(O2HWND hwnd, void *prcl,
                                  O2ULONG includeChildren)
{
    HWND wh;
    RECT wr;
    RECT *pwr;
    UINT flags;
    wh = native_hwnd(hwnd);
    if (!wh)
        return 0;
    pwr = NULL;
    if (prcl) {
        rect_os2_to_win(wh, (O2RECTL *)prcl, &wr);
        pwr = &wr;
    }
    if (includeChildren) {
        flags = RDW_INVALIDATE | RDW_ALLCHILDREN;
        return RedrawWindow(wh, pwr, NULL, flags) ? 1UL : 0UL;
    }
    return InvalidateRect(wh, pwr, FALSE) ? 1UL : 0UL;
}

/* 767 */
O2ULONG __cdecl WinInvertRect(O2HPS hps, void *prcl)
{
    struct CompatPS *p;
    RECT wr;
    if (!prcl) return 0;
    p=ps_from(hps); if (!p || !p->dc) return 0;
    rect_os2_to_win(p->hwnd,(const O2RECTL *)prcl,&wr);
    return InvertRect(p->dc,&wr) ? 1UL : 0UL;
}

/* 775 */
O2ULONG __cdecl WinIsWindowVisible(O2HWND hwnd)
{
    HWND wh;
    wh = native_hwnd(hwnd);
    if (!wh || !IsWindow(wh))
        return 0;
    return IsWindowVisible(wh) ? 1UL : 0UL;
}

/* 778 */
O2HWND __cdecl WinLoadMenu(O2HWND parent, O2ULONG module, O2ULONG idMenu)
{
    const struct PMCompatResource *rr;
    DWORD used;
    HMENU menu;

    rr = pm_find_resource(module, O2_RT_MENU, (O2USHORT)idMenu);
    if (!rr) {
        pm_trace("WinLoadMenu missing", (unsigned long)parent,
                 (unsigned long)module, (unsigned long)idMenu);
        return 0;
    }

    used = 0;
    menu = pm_parse_menu_template(rr->data, rr->size, &used, 1);
    pm_trace(menu ? "WinLoadMenu OK" : "WinLoadMenu FAIL",
             (unsigned long)parent, (unsigned long)idMenu,
             (unsigned long)used);
    return menu ? (O2HWND)(DWORD)menu : 0;
}

/* 780 */
O2ULONG __cdecl WinLoadPointer(O2HWND desktop, O2ULONG module, O2ULONG idres)
{
    const struct PMCompatResource *rr;
    HICON icon;
    int is_pointer;

    (void)desktop;
    rr = pm_find_resource(module, O2_RT_POINTER, (O2USHORT)idres);
    if (!rr) {
        pm_trace("WinLoadPointer missing", (unsigned long)module,
                 (unsigned long)idres, 0);
        return 0;
    }

    is_pointer = 0;
    icon = pm_create_os2_pointer_icon(rr->data, rr->size, &is_pointer);
    if (icon && !pm_remember_owned_pointer((O2ULONG)(DWORD)icon)) {
        DestroyIcon(icon);
        icon = NULL;
    }
    pm_trace(icon ? "WinLoadPointer OK" : "WinLoadPointer FAIL",
             (unsigned long)idres, (unsigned long)rr->size,
             (unsigned long)is_pointer);
    return icon ? (O2ULONG)(DWORD)icon : 0;
}

/* 727 */
O2ULONG __cdecl WinDestroyPointer(O2ULONG pointer)
{
    BOOL ok;
    if (!pm_is_owned_pointer(pointer)) {
        pm_trace("WinDestroyPointer shared", (unsigned long)pointer, 0, 0);
        return 0;
    }
    ok = DestroyIcon((HICON)(DWORD)pointer);
    if (ok)
        pm_forget_owned_pointer(pointer);
    pm_trace(ok ? "WinDestroyPointer OK" : "WinDestroyPointer FAIL",
             (unsigned long)pointer, 0, 0);
    return ok ? 1UL : 0UL;
}

/* 781 */
O2LONG __cdecl WinLoadString(O2HAB hab, O2ULONG module, O2USHORT id,
                             O2LONG maxchars, char *out)
{
    (void)hab;
    return (O2LONG)pm_load_string_resource(module,id,out,(int)maxchars);
}

/* 789 */
O2USHORT __cdecl WinMessageBox(O2HWND parent, O2HWND owner,
                               const char *text, const char *caption,
                               O2USHORT id, O2ULONG style)
{
    HWND wh;
    UINT flags;
    int rc;
    (void)parent; (void)id;
    wh = native_hwnd(owner);
    if (!wh || wh == GetDesktopWindow())
        wh = NULL;
    flags = MB_OK;
    if (style & O2_MB_ICONEXCLAMATION)
        flags |= MB_ICONEXCLAMATION;
    rc = MessageBoxA(wh, text ? text : "", caption ? caption : "", flags);
    return (O2USHORT)(rc == IDOK ? O2_DID_OK : 0);
}

/* 794 */
DWORD __cdecl WinOpenWindowDC(O2HWND hwnd)
{
    return (DWORD)GetDC(native_hwnd(hwnd));
}

/* 793 */
O2ULONG __cdecl WinOpenClipbrd(O2HAB hab)
{
    (void)hab;
    return OpenClipboard(g_frame_hwnd) ? 1UL : 0UL;
}

/* 814 */
O2ULONG __cdecl WinQueryDlgItemShort(O2HWND hwndDlg, O2USHORT idItem,
                                     short *result, O2ULONG isSigned)
{
    HWND wh;
    char buf[64];
    char *end;
    long v;
    if (!result)
        return 0;
    wh = native_hwnd(hwndDlg);
    if (!wh || !GetDlgItemTextA(wh, (int)idItem, buf, sizeof(buf)))
        return 0;
    end = NULL;
    v = strtol(buf, &end, 10);
    if (end == buf)
        return 0;
    if (isSigned)
        *result = (short)v;
    else
        *result = (short)(unsigned short)v;
    return 1;
}

/* 821 */
O2ULONG __cdecl WinQueryPointer(O2HWND desktop)
{
    (void)desktop;
    return (O2ULONG)(DWORD)GetCursor();
}


/* 822 */
O2ULONG __cdecl WinQueryPointerInfo(O2ULONG pointer, O2POINTERINFO *info)
{
    ICONINFO ii;
    struct CompatBitmap *mask;
    struct CompatBitmap *color;
    if (!pointer || !info)
        return 0;
    memset(&ii, 0, sizeof(ii));
    if (!GetIconInfo((HICON)(DWORD)pointer, &ii))
        return 0;
    mask = pm_wrap_native_bitmap(ii.hbmMask);
    color = pm_wrap_native_bitmap(ii.hbmColor);
    memset(info, 0, sizeof(*info));
    info->fPointer = ii.fIcon ? 0UL : 1UL;
    info->xHotSpot = (O2LONG)ii.xHotspot;
    info->yHotSpot = (O2LONG)ii.yHotspot;
    info->hbmPointer = mask ? (O2HBITMAP)(DWORD)mask : 0;
    info->hbmColor = color ? (O2HBITMAP)(DWORD)color : 0;
    if (ii.hbmMask) DeleteObject(ii.hbmMask);
    if (ii.hbmColor) DeleteObject(ii.hbmColor);
    pm_trace("WinQueryPointerInfo", (unsigned long)pointer,
             (unsigned long)info->hbmPointer,
             (unsigned long)info->hbmColor);
    return (mask || color) ? 1UL : 0UL;
}

/* 823 */
O2ULONG __cdecl WinQueryPointerPos(O2HWND desktop, O2POINTL *ptl)
{
    POINT pt;
    (void)desktop;
    if (!ptl || !GetCursorPos(&pt))
        return 0;
    ptl->x = (O2LONG)pt.x;
    ptl->y = (O2LONG)(GetSystemMetrics(SM_CYSCREEN) - 1 - pt.y);
    return 1;
}

/* 828 */
O2ULONG __cdecl WinQuerySysPointer(O2HWND desktop, O2LONG id, O2ULONG load)
{
    LPCSTR rid;
    (void)desktop; (void)load;
    rid=IDC_ARROW;
    switch ((int)id) {
    case 2: rid=IDC_IBEAM; break;
    case 3: rid=IDC_WAIT; break;
    case 4: rid=IDC_SIZEALL; break;
    case 5: rid=IDC_SIZENWSE; break;
    case 6: rid=IDC_SIZENESW; break;
    case 7: rid=IDC_SIZEWE; break;
    case 8: rid=IDC_SIZENS; break;
    }
    return (O2ULONG)(DWORD)LoadCursorA(NULL,rid);
}

/* 829 */
O2LONG __cdecl WinQuerySysValue(O2HWND desktop, O2LONG index)
{
    (void)desktop;
    switch (index) {
    case O2_SV_CXSIZEBORDER: return (O2LONG)GetSystemMetrics(SM_CXSIZEFRAME);
    case O2_SV_CYSIZEBORDER: return (O2LONG)GetSystemMetrics(SM_CYSIZEFRAME);
    case O2_SV_CXSCREEN:     return (O2LONG)GetSystemMetrics(SM_CXSCREEN);
    case O2_SV_CYSCREEN:     return (O2LONG)GetSystemMetrics(SM_CYSCREEN);
    case O2_SV_CXBORDER:     return (O2LONG)GetSystemMetrics(SM_CXBORDER);
    case O2_SV_CYBORDER:     return (O2LONG)GetSystemMetrics(SM_CYBORDER);
    case O2_SV_CYTITLEBAR:   return (O2LONG)GetSystemMetrics(SM_CYCAPTION);
    case O2_SV_CYMENU:       return (O2LONG)GetSystemMetrics(SM_CYMENU);
    case O2_SV_CXFULLSCREEN: return (O2LONG)GetSystemMetrics(SM_CXFULLSCREEN);
    case O2_SV_CYFULLSCREEN: return (O2LONG)GetSystemMetrics(SM_CYFULLSCREEN);
    case O2_SV_CXBYTEALIGN:  return 1;
    case O2_SV_CYBYTEALIGN:  return 1;
    default:                 return 0;
    }
}

/* 833 - query the Presentation Manager environment version/revision.
   This compatibility personality deliberately advertises the OS/2 2.0 PM
   contract rather than leaking the Win32 host version. */
O2ULONG __cdecl WinQueryVersion(O2HAB hab)
{
    O2ULONG version;
    (void)hab;
    version = 0x00020000UL;
    pm_trace("WinQueryVersion", (unsigned long)hab,
             (unsigned long)version, 0);
    return version;
}

/* 834 */
O2HWND __cdecl WinQueryWindow(O2HWND hwnd, O2LONG cmd, O2ULONG lock)
{
    HWND wh;
    HWND out;
    (void)lock;
    wh = native_hwnd(hwnd);
    if (!wh)
        return 0;
    out = NULL;
    switch (cmd) {
    case O2_QW_NEXT:   out = GetWindow(wh, GW_HWNDNEXT); break;
    case O2_QW_PREV:   out = GetWindow(wh, GW_HWNDPREV); break;
    case O2_QW_TOP:    out = GetWindow(wh, GW_HWNDFIRST); break;
    case O2_QW_BOTTOM: out = GetWindow(wh, GW_HWNDLAST); break;
    case O2_QW_OWNER:  out = GetWindow(wh, GW_OWNER); break;
    case O2_QW_PARENT:
        out = GetParent(wh);
        /* The current micro-PM collapses frame and client into one HWND. */
        if (!out && wh == g_frame_hwnd)
            out = wh;
        break;
    default:
        break;
    }
    return out ? guest_hwnd(out) : 0;
}

/* 837 */
O2ULONG __cdecl WinQueryWindowPos(O2HWND hwnd, O2SWP *swp)
{
    HWND wh;
    HWND parent;
    HWND behind;
    RECT wr;
    RECT pr;
    POINT pt;
    LONG parent_h;

    if (!swp)
        return 0;
    wh = native_hwnd(hwnd);
    if (!wh || !IsWindow(wh))
        return 0;
    if (!GetWindowRect(wh, &wr))
        return 0;

    memset(swp, 0, sizeof(*swp));
    swp->cx = wr.right - wr.left;
    swp->cy = wr.bottom - wr.top;

    /* OS/2 window positions are relative to the parent and use a
       bottom-left origin.  Win32 GetWindowRect returns screen coordinates
       with a top-left origin, so translate through the native parent. */
    parent = GetParent(wh);
    if (parent) {
        pt.x = wr.left;
        pt.y = wr.top;
        if (!ScreenToClient(parent, &pt) || !GetClientRect(parent, &pr))
            return 0;
        parent_h = pr.bottom - pr.top;
        swp->x = pt.x;
        swp->y = parent_h - (pt.y + swp->cy);
    } else {
        swp->x = wr.left;
        swp->y = GetSystemMetrics(SM_CYSCREEN) - wr.bottom;
    }

    /* WinQueryWindowPos reports the current extended window state in fl.
       MOVE/SIZE are operations, not persistent state, so only mirror the
       states that have direct Win32 equivalents here. */
    if (IsIconic(wh))
        swp->fl |= O2_SWP_MINIMIZE;
    else if (IsZoomed(wh))
        swp->fl |= O2_SWP_MAXIMIZE;

    /* hwndInsertBehind names the window immediately ahead of this one in
       Z order (the window this one is behind). */
    behind = GetWindow(wh, GW_HWNDPREV);
    swp->hwndInsertBehind = behind ? guest_hwnd(behind) : 0;
    swp->hwnd = guest_hwnd(wh);

    pm_trace("WinQueryWindowPos", (unsigned long)hwnd,
             (unsigned long)(DWORD)swp->x,
             (unsigned long)(DWORD)swp->y);
    pm_trace("  SWP cx/cy/fl", (unsigned long)(DWORD)swp->cx,
             (unsigned long)(DWORD)swp->cy,
             (unsigned long)swp->fl);
    return 1;
}

/* 838 */
O2ULONG __cdecl WinQueryWindowProcess(O2HWND hwnd, O2ULONG *pid, O2ULONG *tid)
{
    DWORD process;
    DWORD thread;
    HWND wh;
    wh = native_hwnd(hwnd);
    if (!wh)
        return 0;
    process = 0;
    thread = GetWindowThreadProcessId(wh, &process);
    if (!thread)
        return 0;
    if (pid) *pid = (O2ULONG)process;
    if (tid) *tid = (O2ULONG)thread;
    pm_trace("WinQueryWindowProcess", (unsigned long)hwnd,
             (unsigned long)process, (unsigned long)thread);
    return 1;
}

/* 840 */
O2ULONG __cdecl WinQueryWindowRect(O2HWND hwnd, void *prcl)
{
    HWND wh;
    if (!prcl)
        return 0;
    wh = native_hwnd(hwnd);
    if (!wh)
        return 0;
    client_rect_os2(wh, (O2RECTL *)prcl);
    {
        O2RECTL *o = (O2RECTL *)prcl;
        pm_trace("WinQueryWindowRect", (unsigned long)hwnd,
                 (unsigned long)(DWORD)o->xRight,
                 (unsigned long)(DWORD)o->yTop);
    }
    return 1;
}

/* 841 */
O2LONG __cdecl WinQueryWindowText(O2HWND hwnd, O2LONG maxChars, char *buffer)
{
    HWND wh;
    if (!buffer || maxChars <= 0)
        return 0;
    wh = native_hwnd(hwnd);
    if (!wh)
        return 0;
    {
        int n;
        n = GetWindowTextA(wh, buffer, (int)maxChars);
        pm_trace("WinQueryWindowText", (unsigned long)hwnd,
                 (unsigned long)n, (unsigned long)maxChars);
        return (O2LONG)n;
    }
}

/* 843 */
O2ULONG __cdecl WinQueryWindowULong(O2HWND hwnd, O2LONG index)
{
    HWND wh;
    struct PMCompatDialog *dlg;
    struct PMCompatWindow *st;
    wh=native_hwnd(hwnd);
    if (!wh || index != 0) return 0;
    {
        char cls[64];
        cls[0]=0;
        GetClassNameA(wh,cls,(int)sizeof(cls));
        if (lstrcmpiA(cls,g_dialog_class)==0) {
            dlg=(struct PMCompatDialog *)(DWORD)GetWindowLongA(wh,GWL_USERDATA);
            return dlg ? dlg->user_ulong0 : 0;
        }
    }
    st=pm_window_state(wh);
    return st ? st->user_ulong0 : 0;
}

/* 844 - query a 16-bit PM window word.  QWS_ID is the public control/
   window identifier and maps naturally to the Win32 child-window ID. */
O2USHORT __cdecl WinQueryWindowUShort(O2HWND hwnd, O2LONG index)
{
    HWND wh;
    int id;
    wh = native_hwnd(hwnd);
    if (!wh || !IsWindow(wh))
        return 0;
    if (index == O2_QWS_ID) {
        id = GetDlgCtrlID(wh);
        pm_trace("WinQueryWindowUShort ID", (unsigned long)hwnd,
                 (unsigned long)(DWORD)index, (unsigned long)(WORD)id);
        return (O2USHORT)id;
    }
    pm_trace("WinQueryWindowUShort unsupported", (unsigned long)hwnd,
             (unsigned long)(DWORD)index, 0);
    return 0;
}

/* 866 */
O2ULONG __cdecl WinSetPointer(O2HWND desktop, O2ULONG pointer)
{
    (void)desktop;
    SetCursor((HCURSOR)(DWORD)pointer);
    return 1;
}

/* 877 */
O2ULONG __cdecl WinSetWindowText(O2HWND hwnd, const char *text)
{
    HWND wh;
    wh=native_hwnd(hwnd);
    if (!wh) return 0;
    return SetWindowTextA(wh,text ? text : "") ? 1UL : 0UL;
}

/* 878 */
O2ULONG __cdecl WinSetWindowULong(O2HWND hwnd, O2LONG index, O2ULONG value)
{
    HWND wh;
    struct PMCompatDialog *dlg;
    struct PMCompatWindow *st;
    char cls[64];
    wh=native_hwnd(hwnd);
    if (!wh || index != 0) return 0;
    cls[0]=0;
    GetClassNameA(wh,cls,(int)sizeof(cls));
    if (lstrcmpiA(cls,g_dialog_class)==0) {
        dlg=(struct PMCompatDialog *)(DWORD)GetWindowLongA(wh,GWL_USERDATA);
        if (!dlg) return 0;
        dlg->user_ulong0=value;
        return 1;
    }
    st=pm_window_state(wh);
    if (!st) return 0;
    st->user_ulong0=value;
    return 1;
}

/* 848 */
O2ULONG __cdecl WinReleasePS(O2HPS hps)
{
    struct CompatPS *p;
    pm_trace("WinReleasePS call", (unsigned long)hps, 0, 0);
    p = ps_from(hps);
    if (!p)
        return 0;
    if (p->flags & PMCOMPAT_PS_RELEASE_DC) {
        if (p->dc && p->saved_dc)
            RestoreDC(p->dc, p->saved_dc);
        ReleaseDC(p->hwnd, p->dc);
        p->magic = 0;
        HeapFree(GetProcessHeap(), 0, p);
    }
    pm_trace("WinReleasePS leave", (unsigned long)hps, 1, 0);
    return 1;
}

/* 849 */
O2LONG __cdecl WinScrollWindow(O2HWND hwnd, O2LONG dx, O2LONG dy,
                               void *scrollRect, void *clipRect,
                               O2ULONG updateRgn, void *updateRect,
                               O2ULONG flags)
{
    HWND wh;
    RECT sr;
    RECT cr;
    RECT ur;
    RECT *psr;
    RECT *pcr;
    int rc;
    UINT wf;
    (void)updateRgn;
    wh = native_hwnd(hwnd);
    if (!wh)
        return 0;
    psr = NULL;
    pcr = NULL;
    if (scrollRect) {
        rect_os2_to_win(wh, (O2RECTL *)scrollRect, &sr);
        psr = &sr;
    }
    if (clipRect) {
        rect_os2_to_win(wh, (O2RECTL *)clipRect, &cr);
        pcr = &cr;
    }
    memset(&ur, 0, sizeof(ur));
    wf = 0;
    if (flags & 0x0001UL) wf |= SW_SCROLLCHILDREN;
    if (flags & 0x0002UL) wf |= SW_INVALIDATE;
    pm_trace("WinScrollWindow call", (unsigned long)hwnd,
             (unsigned long)(DWORD)dx, (unsigned long)(DWORD)dy);
    rc = ScrollWindowEx(wh, (int)dx, (int)-dy, psr, pcr, NULL, &ur, wf);
    if (updateRect)
        rect_win_to_os2(wh, &ur, (O2RECTL *)updateRect);
    if (updateRect) {
        O2RECTL *o = (O2RECTL *)updateRect;
        pm_trace("WinScrollWindow update",
                 (unsigned long)(DWORD)o->xLeft,
                 (unsigned long)(DWORD)o->yBottom,
                 (unsigned long)(DWORD)o->xRight);
        pm_trace("WinScrollWindow update2",
                 (unsigned long)(DWORD)o->yTop,
                 (unsigned long)(DWORD)rc,
                 (unsigned long)wf);
    }
    pm_trace("WinScrollWindow leave", (unsigned long)(DWORD)rc, 0, 0);
    return (O2LONG)rc;
}

/* 854 */
O2ULONG __cdecl WinSetClipbrdData(O2HAB hab, O2ULONG data,
                                  O2USHORT fmt, O2USHORT info)
{
    HBITMAP hb;
    struct CompatBitmap *b;
    (void)hab; (void)info;
    if (fmt != 2U) return 0;
    b=(struct CompatBitmap *)(DWORD)data;
    hb=pm_native_bitmap_from_compat(b);
    if (!hb) return 0;
    if (!SetClipboardData(CF_BITMAP,hb)) { DeleteObject(hb); return 0; }
    return 1;
}

/* 858 */
O2ULONG __cdecl WinSetDlgItemShort(O2HWND hwndDlg, O2USHORT idItem,
                                   O2USHORT value, O2ULONG isSigned)
{
    HWND wh;
    char buf[32];
    wh = native_hwnd(hwndDlg);
    if (!wh)
        return 0;
    if (isSigned)
        wsprintfA(buf, "%d", (int)(short)value);
    else
        wsprintfA(buf, "%u", (unsigned)value);
    return SetDlgItemTextA(wh, (int)idItem, buf) ? 1UL : 0UL;
}

/* 872 - best-effort process-local analogue of OS/2 system modality. */
O2ULONG __cdecl WinSetSysModalWindow(O2HWND desktop, O2HWND hwnd)
{
    HWND wh;
    (void)desktop;
    if (hwnd == 0) {
        g_sys_modal_hwnd = NULL;
        return 1;
    }
    wh = native_hwnd(hwnd);
    if (!wh || !IsWindow(wh))
        return 0;
    g_sys_modal_hwnd = wh;
    SetForegroundWindow(wh);
    return 1;
}

/* 875 */
O2ULONG __cdecl WinSetWindowPos(O2HWND hwnd, O2HWND behind,
                                O2LONG x, O2LONG y, O2LONG cx, O2LONG cy,
                                O2ULONG flags)
{
    HWND wh;
    RECT rc;
    LONG client_h;
    LONG outer_w;
    LONG outer_h;
    LONG win_x;
    LONG win_y;
    DWORD style;
    UINT wf;

    (void)behind;
    wh = native_hwnd(hwnd);
    if (!wh)
        return 0;

    wf = 0;
    if (!(flags & O2_SWP_SIZE)) wf |= SWP_NOSIZE;
    if (!(flags & O2_SWP_MOVE)) wf |= SWP_NOMOVE;
    if (!(flags & O2_SWP_ZORDER)) wf |= SWP_NOZORDER;
    if (!(flags & O2_SWP_ACTIVATE)) wf |= SWP_NOACTIVATE;
    if (flags & O2_SWP_SHOW) wf |= SWP_SHOWWINDOW;
    if (flags & O2_SWP_HIDE) wf |= SWP_HIDEWINDOW;
    if (flags & O2_SWP_NOREDRAW) wf |= SWP_NOREDRAW;

    win_x = x;
    win_y = y;

    /*
     * OS/2 ignores x/y unless SWP_MOVE is present and ignores cx/cy unless
     * SWP_SIZE is present.  The Y-axis conversion still needs the window's
     * effective height even for a move-only operation.  Using the ignored
     * cy argument here shifts every bottom-left -> top-left conversion by the
     * current window height when callers pass cy == 0 (a normal PM idiom).
     *
     * Start from the current native size and replace it only for SWP_SIZE.
     */
    if (!GetWindowRect(wh, &rc))
        return 0;
    outer_w = rc.right - rc.left;
    outer_h = rc.bottom - rc.top;

    if (flags & O2_SWP_SIZE) {
        style = (DWORD)GetWindowLongA(wh, GWL_STYLE);
        client_h = cy - WinQuerySysValue(1, O2_SV_CYTITLEBAR) + 2;
        if (client_h <= 0)
            client_h = cy;
        rc.left = 0;
        rc.top = 0;
        rc.right = cx;
        rc.bottom = client_h;
        AdjustWindowRect(&rc, style, FALSE);
        outer_w = rc.right - rc.left;
        outer_h = rc.bottom - rc.top;
    }

    if (flags & O2_SWP_MOVE) {
        HWND par = GetParent(wh);
        if (par) {
            RECT pr; GetClientRect(par, &pr);
            win_y = (pr.bottom - pr.top) - y - outer_h;
        } else {
            win_y = GetSystemMetrics(SM_CYSCREEN) - y - outer_h;
        }
        if (win_y < 0) win_y = 0;
    }

    pm_trace("WinSetWindowPos", (unsigned long)hwnd,
             (unsigned long)(DWORD)flags, (unsigned long)(DWORD)y);
    pm_trace("  native pos/size", (unsigned long)(DWORD)win_x,
             (unsigned long)(DWORD)win_y, (unsigned long)(DWORD)outer_h);

    return SetWindowPos(wh, NULL, win_x, win_y, outer_w, outer_h, wf)
           ? 1UL : 0UL;
}

/* 883 */
O2ULONG __cdecl WinShowWindow(O2HWND hwnd, O2ULONG show)
{
    HWND wh=native_hwnd(hwnd);
    int update_was_disabled;
    if (!wh) return 0;
    if (hwnd == O2_HWND_DESKTOP)
        return show ? 1UL : 0UL;
    if (!pm_is_own_process_window(wh)) {
        pm_trace("WinShowWindow blocked foreign", (unsigned long)hwnd,
                 (unsigned long)show, 0);
        return 0;
    }

    if (show && GetPropA(wh, g_object_parent_prop) != NULL)
        return 1;
    update_was_disabled = show && GetPropA(wh, g_update_disabled_prop) != NULL;
    if (update_was_disabled) {
        SendMessageA(wh, WM_SETREDRAW, TRUE, 0);
        RemovePropA(wh, g_update_disabled_prop);
    }

    ShowWindow(wh, show ? SW_SHOW : SW_HIDE);

    if (update_was_disabled) {
        /* This is the documented OS/2 pairing used by OPENDLG after it
           bulk-populates the file and directory listboxes. */
        InvalidateRect(wh, NULL, TRUE);
        UpdateWindow(wh);
    }
    return 1;
}

/* 884 - start/reset a PM window timer. */
O2ULONG __cdecl WinStartTimer(O2HAB hab, O2HWND hwnd, O2ULONG idTimer,
                              O2ULONG timeout)
{
    HWND wh;
    UINT_PTR r;
    (void)hab;

    wh = native_hwnd(hwnd);
    if (!wh || idTimer == 0UL || idTimer > 0xffffUL)
        return 0UL;

    /* OS/2 2.x accepts millisecond intervals up to 65535.  USER32 treats
       zero as a request for its minimum timer interval, matching PM's
       fastest-possible timer semantics closely enough for this personality. */
    r = SetTimer(wh, (UINT_PTR)idTimer, (UINT)timeout, NULL);
    pm_trace(r ? "WinStartTimer OK" : "WinStartTimer FAIL",
             (unsigned long)hwnd, (unsigned long)idTimer,
             (unsigned long)timeout);
    return r ? idTimer : 0UL;
}

/* 888 */
O2ULONG __cdecl WinTerminate(O2HAB hab)
{
    (void)hab;
    return 1;
}

/* 892 */
O2ULONG __cdecl WinUpdateWindow(O2HWND hwnd)
{
    HWND wh=native_hwnd(hwnd);
    return wh && UpdateWindow(wh) ? 1UL : 0UL;
}

static int pm_is_listbox(HWND hwnd)
{
    char cls[32];
    if (!hwnd) return 0;
    cls[0]=0;
    if (!GetClassNameA(hwnd, cls, (int)sizeof(cls))) return 0;
    return lstrcmpiA(cls, "LISTBOX") == 0;
}

static O2MRESULT pm_send_listbox(HWND child, O2USHORT msg,
                                 O2MPARAM mp1, O2MPARAM mp2,
                                 int *handled)
{
    short pos;
    int count, i, insert_at, maxchars, n;
    LRESULT lr;
    char *tmp;
    const char *text;
    if (handled) *handled=0;
    if (!pm_is_listbox(child)) return 0;
    switch (msg) {
    case O2_LM_DELETEALL:
        if (handled) *handled=1;
        return (O2MRESULT)SendMessageA(child, LB_RESETCONTENT, 0, 0);
    case O2_LM_QUERYITEMCOUNT:
        if (handled) *handled=1;
        return (O2MRESULT)SendMessageA(child, LB_GETCOUNT, 0, 0);
    case O2_LM_QUERYSELECTION:
        if (handled) *handled=1;
        return (O2MRESULT)SendMessageA(child, LB_GETCURSEL, 0, 0);
    case O2_LM_SELECTITEM:
        if (handled) *handled=1;
        pos=(short)LOWORD((DWORD)mp1);
        return SendMessageA(child, LB_SETCURSEL, (WPARAM)(int)pos, 0) != LB_ERR ? 1UL : 0UL;
    case O2_LM_INSERTITEM:
        if (handled) *handled=1;
        text=(const char *)(DWORD)mp2;
        if (!text) return (O2MRESULT)(LONG)-1;
        pos=(short)LOWORD((DWORD)mp1);
        if (pos == O2_LIT_END)
            return (O2MRESULT)SendMessageA(child, LB_ADDSTRING, 0, (LPARAM)text);
        if (pos == O2_LIT_SORTASCENDING || pos == O2_LIT_SORTDESCENDING) {
            count=(int)SendMessageA(child, LB_GETCOUNT, 0, 0);
            insert_at=count < 0 ? 0 : count;
            for (i=0; i<count; ++i) {
                n=(int)SendMessageA(child, LB_GETTEXTLEN, (WPARAM)i, 0);
                if (n < 0) continue;
                tmp=(char *)HeapAlloc(GetProcessHeap(),0,(SIZE_T)n+1U);
                if (!tmp) return (O2MRESULT)(LONG)-2;
                SendMessageA(child, LB_GETTEXT, (WPARAM)i, (LPARAM)tmp);
                lr=lstrcmpiA(text,tmp);
                HeapFree(GetProcessHeap(),0,tmp);
                if ((pos == O2_LIT_SORTASCENDING && lr < 0) ||
                    (pos == O2_LIT_SORTDESCENDING && lr > 0)) {
                    insert_at=i; break;
                }
            }
            return (O2MRESULT)SendMessageA(child, LB_INSERTSTRING,
                                           (WPARAM)insert_at, (LPARAM)text);
        }
        return (O2MRESULT)SendMessageA(child, LB_INSERTSTRING,
                                       (WPARAM)(int)pos, (LPARAM)text);
    case O2_LM_QUERYITEMTEXT:
        if (handled) *handled=1;
        pos=(short)LOWORD((DWORD)mp1);
        maxchars=(int)HIWORD((DWORD)mp1);
        if (!mp2 || maxchars <= 0) return 0;
        n=(int)SendMessageA(child, LB_GETTEXTLEN, (WPARAM)(int)pos, 0);
        if (n < 0) return 0;
        tmp=(char *)HeapAlloc(GetProcessHeap(),0,(SIZE_T)n+1U);
        if (!tmp) return 0;
        if (SendMessageA(child, LB_GETTEXT, (WPARAM)(int)pos, (LPARAM)tmp) == LB_ERR) {
            HeapFree(GetProcessHeap(),0,tmp); return 0;
        }
        if (n >= maxchars) n=maxchars-1;
        memcpy((void *)(DWORD)mp2,tmp,(size_t)n);
        ((char *)(DWORD)mp2)[n]=0;
        HeapFree(GetProcessHeap(),0,tmp);
        return (O2MRESULT)n;
    default:
        return 0;
    }
}

/* 735 */
O2ULONG __cdecl WinEnableWindow(O2HWND hwnd, O2ULONG enable)
{
    HWND wh=native_hwnd(hwnd);
    if (!wh) return 0;
    EnableWindow(wh, enable ? TRUE : FALSE);
    return 1;
}

/* 748 / 751 - JIGSAW uses these only for optional PM error reporting. */
O2ULONG __cdecl WinFreeErrorInfo(void *perr)
{
    (void)perr;
    return 1;
}

void * __cdecl WinGetErrorInfo(O2HAB hab)
{
    (void)hab;
    return NULL;
}

/* 764 */
O2ULONG __cdecl WinIntersectRect(O2HAB hab, O2RECTL *dst,
                                 const O2RECTL *a, const O2RECTL *b)
{
    (void)hab;
    if (!dst || !a || !b) return 0;
    dst->xLeft = a->xLeft > b->xLeft ? a->xLeft : b->xLeft;
    dst->yBottom = a->yBottom > b->yBottom ? a->yBottom : b->yBottom;
    dst->xRight = a->xRight < b->xRight ? a->xRight : b->xRight;
    dst->yTop = a->yTop < b->yTop ? a->yTop : b->yTop;
    if (dst->xRight < dst->xLeft) dst->xRight=dst->xLeft;
    if (dst->yTop < dst->yBottom) dst->yTop=dst->yBottom;
    return (dst->xRight > dst->xLeft && dst->yTop > dst->yBottom) ? 1UL : 0UL;
}

/* 788 */
O2ULONG __cdecl WinMapWindowPoints(O2HWND from, O2HWND to,
                                   O2POINTL *pts, O2LONG count)
{
    HWND wf=native_hwnd(from), wt=native_hwnd(to);
    LONG i;
    if (!pts || count < 0) return 0;
    for (i=0; i<count; ++i) {
        POINT p;
        RECT rc;
        p.x=pts[i].x;
        if (!wf || wf==GetDesktopWindow())
            p.y=GetSystemMetrics(SM_CYSCREEN)-pts[i].y;
        else {
            GetClientRect(wf,&rc); p.y=(rc.bottom-rc.top)-pts[i].y;
            ClientToScreen(wf,&p);
        }
        if (!wt || wt==GetDesktopWindow()) {
            pts[i].x=p.x;
            pts[i].y=GetSystemMetrics(SM_CYSCREEN)-p.y;
        } else {
            ScreenToClient(wt,&p); GetClientRect(wt,&rc);
            pts[i].x=p.x; pts[i].y=(rc.bottom-rc.top)-p.y;
        }
    }
    return 1;
}

/* 773 */
O2ULONG __cdecl WinIsWindowEnabled(O2HWND hwnd)
{
    HWND wh=native_hwnd(hwnd);
    if (!wh || !IsWindow(wh)) return 0;
    return IsWindowEnabled(wh) ? 1UL : 0UL;
}

/* 797 */
O2ULONG __cdecl WinPtInRect(O2HAB hab, const O2RECTL *r, const O2POINTL *pnt)
{
    (void)hab;
    if (!r || !pnt) return 0;
    return (pnt->x >= r->xLeft && pnt->x < r->xRight &&
            pnt->y >= r->yBottom && pnt->y < r->yTop) ? 1UL : 0UL;
}

/* 804 */
O2HWND __cdecl WinQueryCapture(O2HWND desktop)
{
    HWND wh;
    (void)desktop;
    wh=GetCapture();
    return wh ? guest_hwnd(wh) : 0;
}

/* 815 */
O2USHORT __cdecl WinQueryDlgItemText(O2HWND hwndDlg, O2USHORT id,
                                     short maxchars, char *out)
{
    HWND wh=native_hwnd(hwndDlg), child;
    int n;
    if (!wh || !out || maxchars <= 0) return 0;
    child=GetDlgItem(wh,(int)id); if (!child) { out[0]=0; return 0; }
    n=GetWindowTextA(child,out,(int)maxchars);
    return (O2USHORT)(n < 0 ? 0 : n);
}

/* 817 */
O2HWND __cdecl WinQueryFocus(O2HWND desktop, O2ULONG lock)
{
    HWND wh;
    (void)desktop; (void)lock;
    wh=GetFocus();
    return wh ? guest_hwnd(wh) : 0;
}

/* 832 */
O2LONG __cdecl WinQueryUpdateRegion(O2HWND hwnd, O2ULONG hrgn)
{
    HWND wh=native_hwnd(hwnd);
    if (!wh || !hrgn) return 0;
    return (O2LONG)GetUpdateRgn(wh,(HRGN)(DWORD)hrgn,FALSE);
}

/* 852 */
O2ULONG __cdecl WinSetCapture(O2HWND desktop, O2HWND hwnd)
{
    HWND wh;
    (void)desktop;
    if (!hwnd) { ReleaseCapture(); return 1; }
    wh=native_hwnd(hwnd); if (!wh) return 0;
    SetCapture(wh); return 1;
}

/* 859 */
O2ULONG __cdecl WinSetDlgItemText(O2HWND hwndDlg, O2USHORT id, const char *text)
{
    HWND wh=native_hwnd(hwndDlg);
    return wh && SetDlgItemTextA(wh,(int)id,text ? text : "") ? 1UL : 0UL;
}

/* 860 */
O2ULONG __cdecl WinSetFocus(O2HWND desktop, O2HWND hwnd)
{
    HWND wh;
    (void)desktop;
    if (!hwnd) { SetFocus(NULL); return 1; }
    wh=native_hwnd(hwnd); if (!wh) return 0;
    SetFocus(wh); return 1;
}

/* 865 */
O2ULONG __cdecl WinSetParent(O2HWND hwnd, O2HWND newParent, O2ULONG redraw)
{
    HWND wh=native_hwnd(hwnd), np;
    if (!wh) return 0;
    if (newParent == O2_HWND_OBJECT) {
        ShowWindow(wh,SW_HIDE);
        SetParent(wh,NULL);
        SetPropA(wh,g_object_parent_prop,(HANDLE)1);
        return 1;
    }
    RemovePropA(wh,g_object_parent_prop);
    np=native_hwnd(newParent);
    if (np==GetDesktopWindow()) np=NULL;
    SetParent(wh,np);
    if (redraw) InvalidateRect(wh,NULL,TRUE);
    return 1;
}

/* 881 */
O2ULONG __cdecl WinShowPointer(O2HWND desktop, O2ULONG show)
{
    (void)desktop;
    ShowCursor(show ? TRUE : FALSE);
    return 1;
}

/* 891 */
O2ULONG __cdecl WinUnionRect(O2HAB hab, O2RECTL *dst,
                             const O2RECTL *a, const O2RECTL *b)
{
    (void)hab;
    if (!dst || !a || !b) return 0;
    dst->xLeft=a->xLeft < b->xLeft ? a->xLeft : b->xLeft;
    dst->yBottom=a->yBottom < b->yBottom ? a->yBottom : b->yBottom;
    dst->xRight=a->xRight > b->xRight ? a->xRight : b->xRight;
    dst->yTop=a->yTop > b->yTop ? a->yTop : b->yTop;
    return 1;
}

/* 896 */
O2ULONG __cdecl WinValidateRegion(O2HWND hwnd, O2ULONG hrgn, O2ULONG includeChildren)
{
    HWND wh=native_hwnd(hwnd);
    (void)includeChildren;
    if (!wh) return 0;
    return ValidateRgn(wh, hrgn ? (HRGN)(DWORD)hrgn : NULL) ? 1UL : 0UL;
}

/* 899 */
O2HWND __cdecl WinWindowFromID(O2HWND parent, O2USHORT id)
{
    HWND wh;
    HMENU menu;
    HWND child;
    wh = native_hwnd(parent);
    if (!wh)
        return 0;
    if (id == O2_FID_CLIENT)
        return parent;
    if (id == O2_FID_VERTSCROLL)
        return pm_scroll_proxy_handle(wh, SB_VERT);
    if (id == O2_FID_HORZSCROLL)
        return pm_scroll_proxy_handle(wh, SB_HORZ);
    if (id == O2_FID_TITLEBAR)
        return parent;
    if (id == O2_FID_MENU) {
        menu = GetMenu(wh);
        pm_trace("WinWindowFromID menu", (unsigned long)parent,
                 (unsigned long)(DWORD)menu, (unsigned long)id);
        return menu ? (O2HWND)(DWORD)menu : 0;
    }
    child = GetDlgItem(wh, (int)id);
    return child ? guest_hwnd(child) : 0;
}

/* 903 */
O2MRESULT __cdecl WinSendDlgItemMsg(O2HWND hwndDlg, O2USHORT idItem,
                                    O2USHORT msg, O2MPARAM mp1,
                                    O2MPARAM mp2)
{
    HWND wh;
    HWND child;
    wh = native_hwnd(hwndDlg);
    if (!wh)
        return 0;
    child = GetDlgItem(wh, (int)idItem);
    if (!child)
        return 0;
    {
        int handled;
        O2MRESULT lr;
        lr=pm_send_slider(child,msg,mp1,mp2,&handled);
        if (handled) return lr;
        lr=pm_send_listbox(child,msg,mp1,mp2,&handled);
        if (handled) return lr;
    }
    if (msg == O2_EM_SETTEXTLIMIT)
        return (O2MRESULT)SendMessageA(child, EM_LIMITTEXT,
                                       (WPARAM)LOWORD((DWORD)mp1), 0);
    if (msg == O2_BM_QUERYCHECK)
        return SendMessageA(child, BM_GETCHECK, 0, 0) == BST_CHECKED ? 1UL : 0UL;
    return (O2MRESULT)SendMessageA(child, (UINT)msg,
                                   (WPARAM)mp1, (LPARAM)mp2);
}

/* 909 - create an ordinary PM window/control.
 *
 * OS/2 public class atoms use the 0xffff000N form (WC_*).  Map the
 * common control classes to their Win32 peers while keeping private classes
 * on the existing pm_wndproc/WinRegisterClass path.  Coordinates supplied
 * by PM are relative to the lower-left of the parent; Win32 uses upper-left.
 */
O2HWND __cdecl WinCreateWindow(O2HWND parent, const char *className,
                               const char *windowName, O2ULONG osStyle,
                               O2LONG x, O2LONG y, O2LONG cx, O2LONG cy,
                               O2HWND owner, O2HWND insertBehind,
                               O2ULONG id, void *ctlData, void *presParams)
{
    DWORD classValue;
    unsigned publicClass;
    const char *nativeClass;
    const char *nativeText;
    DWORD style;
    HWND nativeParent;
    HWND nativeOwner;
    HWND createParent;
    HWND hwnd;
    int nx, ny;
    RECT pr;
    struct PMCompatWindow *wstate;
    O2PFNWP classProc;
    const struct PMCompatResource *rr;
    HICON staticIcon;
    int staticIsIcon;
    unsigned long iconId;
    char *endp;

    (void)presParams;
    if (!className || cx < 0 || cy < 0)
        return 0;

    classValue = (DWORD)className;
    publicClass = ((classValue & 0xffff0000UL) == 0xffff0000UL)
                ? (unsigned)(classValue & 0xffffUL) : 0U;
    nativeClass = NULL;
    wstate = NULL;

    switch (publicClass) {
    case O2_WC_COMBOBOX:   nativeClass = "COMBOBOX"; break;
    case O2_WC_BUTTON:     nativeClass = "BUTTON"; break;
    case O2_WC_STATIC:     nativeClass = "STATIC"; break;
    case O2_WC_ENTRYFIELD: nativeClass = "EDIT"; break;
    case O2_WC_LISTBOX:    nativeClass = "LISTBOX"; break;
    case O2_WC_SCROLLBAR:  nativeClass = "SCROLLBAR"; break;
    case O2_WC_SLIDER:
        if (!pm_ensure_trackbar_class())
            return 0;
        nativeClass = PMCOMPAT_TRACKBAR_CLASS;
        break;
    case O2_WC_FRAME:
    case O2_WC_MENU:
    case O2_WC_TITLEBAR:
        pm_trace("WinCreateWindow class", (unsigned long)publicClass,
                 (unsigned long)osStyle, 0);
        return 0;
    default:
        if (publicClass != 0U)
            return 0;
        if (IsBadStringPtrA(className, 1))
            return 0;
        classProc = pm_find_class_proc(className);
        if (!classProc)
            return 0;
        nativeClass = className;
        wstate = (struct PMCompatWindow *)HeapAlloc(GetProcessHeap(),
                                                     HEAP_ZERO_MEMORY,
                                                     sizeof(*wstate));
        if (!wstate)
            return 0;
        wstate->proc = classProc;
        strncpy(wstate->class_name, className, sizeof(wstate->class_name)-1);
        break;
    }

    nativeParent = native_hwnd(parent);
    nativeOwner = native_hwnd(owner);
    if (nativeOwner == GetDesktopWindow())
        nativeOwner = NULL;

    style = 0;
    if (osStyle & O2_WS_VISIBLE) style |= WS_VISIBLE;
    if (osStyle & 0x00020000UL) style |= WS_TABSTOP;
    if (osStyle & 0x00010000UL) style |= WS_GROUP;

    if (publicClass == O2_WC_BUTTON) {
        if ((osStyle & 0x0000000fUL) == 0x0002UL)
            style |= BS_AUTOCHECKBOX;
        else if (osStyle & 0x00000400UL)
            style |= BS_DEFPUSHBUTTON;
        else
            style |= BS_PUSHBUTTON;
    } else if (publicClass == O2_WC_STATIC) {
        if ((osStyle & 0x0000000fUL) == 0x0002UL) {
            nativeClass = "BUTTON";
            style |= BS_GROUPBOX;
        } else if ((osStyle & 0x0000ffffUL) == 0x0003UL) {
            style |= SS_ICON;
        } else if (osStyle & 0x00000100UL) {
            style |= SS_CENTER;
        } else {
            style |= SS_LEFT;
        }
    } else if (publicClass == O2_WC_ENTRYFIELD) {
        style |= WS_BORDER | ES_LEFT | ES_AUTOHSCROLL;
    } else if (publicClass == O2_WC_LISTBOX) {
        style |= WS_BORDER | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT;
    } else if (publicClass == O2_WC_SCROLLBAR) {
        style |= (cx >= cy) ? SBS_HORZ : SBS_VERT;
    } else if (publicClass == O2_WC_SLIDER) {
        style |= PMCOMPAT_TBS_AUTOTICKS | PMCOMPAT_TBS_FIXEDLENGTH;
        if (cy > cx)
            style |= PMCOMPAT_TBS_VERT;
    }

    nx = (int)x;
    ny = (int)y;
    createParent = nativeParent;
    if (nativeParent == GetDesktopWindow()) {
        createParent = nativeOwner;
        style |= WS_POPUP;
        ny = GetSystemMetrics(SM_CYSCREEN) - (int)y - (int)cy;
    } else if (nativeParent) {
        style |= WS_CHILD;
        if (GetClientRect(nativeParent, &pr))
            ny = (pr.bottom - pr.top) - (int)y - (int)cy;
    } else {
        style |= WS_POPUP;
        createParent = nativeOwner;
        ny = GetSystemMetrics(SM_CYSCREEN) - (int)y - (int)cy;
    }

    /* SS_ICON treats the text as a Win32 module resource name.  OS/2 instead
     * resolves "#N" from the application's RT_POINTER resources, so create
     * the peer with no native resource name and install the converted icon
     * immediately afterwards. */
    nativeText = windowName ? windowName : "";
    staticIcon = NULL;
    staticIsIcon = (publicClass == O2_WC_STATIC &&
                    (osStyle & 0x0000ffffUL) == 0x0003UL);
    iconId = 0UL;
    if (staticIsIcon && windowName &&
        windowName[0] == '#' && windowName[1] != '\0') {
        iconId = strtoul(windowName + 1, &endp, 10);
        if (endp != windowName + 1 && *endp == '\0' && iconId <= 0xffffUL) {
            rr = pm_find_resource(0, O2_RT_POINTER, (O2USHORT)iconId);
            if (rr)
                staticIcon = pm_create_os2_color_icon(rr->data, rr->size);
            nativeText = "";
        }
    }

    pm_trace("WinCreateWindow", (unsigned long)publicClass,
             (unsigned long)osStyle, (unsigned long)id);
    SetLastError(0);
    ++g_native_create_depth;
    hwnd = CreateWindowExA(0, nativeClass, nativeText, style,
                           nx, ny, (int)cx, (int)cy,
                           createParent,
                           (style & WS_CHILD) ? (HMENU)(UINT_PTR)id : NULL,
                           g_instance, wstate ? (LPVOID)wstate : ctlData);
    --g_native_create_depth;
    if (!hwnd) {
        pm_trace("WinCreateWindow FAIL", (unsigned long)GetLastError(),
                 (unsigned long)publicClass, (unsigned long)id);
        if (staticIcon) DestroyIcon(staticIcon);
        if (wstate) HeapFree(GetProcessHeap(), 0, wstate);
        return 0;
    }

    if (publicClass == O2_WC_SLIDER && pm_is_trackbar(hwnd)) {
        SendMessageA(hwnd, PMCOMPAT_TBM_SETRANGE, TRUE, MAKELONG(0, 99));
        SendMessageA(hwnd, PMCOMPAT_TBM_SETTICFREQ, 10, 0);
        SendMessageA(hwnd, PMCOMPAT_TBM_SETLINESIZE, 0, 1);
        SendMessageA(hwnd, PMCOMPAT_TBM_SETPAGESIZE, 0, 10);
    }

    if (staticIcon) {
        SendMessageA(hwnd, STM_SETICON, (WPARAM)staticIcon, 0);
        /* STATIC controls do not take ownership of the icon handle.  Keep it
         * alive for the life of this compatibility process; the resource set
         * is tiny and later WinDestroyPointer calls concern separately loaded
         * HPOINTER values, not this control-owned conversion. */
    }

    if (insertBehind == O2_HWND_TOP)
        SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    else if (insertBehind == O2_HWND_BOTTOM)
        SetWindowPos(hwnd, HWND_BOTTOM, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    else if (insertBehind != 0) {
        HWND behind = native_hwnd(insertBehind);
        if (behind && behind != GetDesktopWindow())
            SetWindowPos(hwnd, behind, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }

    pm_trace("WinCreateWindow OK", (unsigned long)(DWORD)hwnd,
             (unsigned long)nx, (unsigned long)ny);
    return guest_hwnd(hwnd);
}

/* 908 */
O2HWND __cdecl WinCreateStdWindow(O2HWND parent, O2ULONG frameStyle,
                                  O2ULONG *createFlags,
                                  const char *clientClass,
                                  const char *title,
                                  O2ULONG clientStyle,
                                  O2ULONG module, O2ULONG resources,
                                  O2HWND *clientOut)
{
    HWND hwnd;
    DWORD style;
    DWORD createStyle;
    O2ULONG fcf;
    HMENU nativeMenu;
    HICON nativeIcon;
    DWORD templateUsed;
    const struct PMCompatResource *rr;
    O2PFNWP classProc;
    O2ULONG classStyle;
    struct PMCompatWindow *wstate;
    HWND nativeParent;
    (void)clientStyle;

    if (!g_class_registered || !clientClass)
        return 0;
    classProc = pm_find_class_proc(clientClass);
    classStyle = pm_find_class_style(clientClass);
    if (!classProc) classProc = g_guest_proc;
    if (!classProc) return 0;
    nativeParent = native_hwnd(parent);
    if (nativeParent == GetDesktopWindow()) nativeParent = NULL;

    fcf = createFlags ? *createFlags : 0;
    style = WS_OVERLAPPED;
    if (fcf & O2_FCF_TITLEBAR) style |= WS_CAPTION;
    if (fcf & O2_FCF_SYSMENU) style |= WS_SYSMENU;
    if (fcf & O2_FCF_SIZEBORDER) style |= WS_THICKFRAME;
    if (fcf & O2_FCF_MINBUTTON) style |= WS_MINIMIZEBOX;
    if (fcf & O2_FCF_MAXBUTTON) style |= WS_MAXIMIZEBOX;
    if (fcf & O2_FCF_VERTSCROLL) style |= WS_VSCROLL;
    if (fcf & O2_FCF_HORZSCROLL) style |= WS_HSCROLL;
    if (nativeParent) style |= WS_CHILD;
    /* OS/2 CS_CLIPCHILDREN is a class attribute; Win32 expresses the
       equivalent clipping policy as WS_CLIPCHILDREN on each window.
       BIO relies on this so the main chart cannot paint over its Legend
       child window. */
    if (classStyle & O2_CS_CLIPCHILDREN)
        style |= WS_CLIPCHILDREN;
    if (frameStyle & O2_WS_VISIBLE) style |= WS_VISIBLE;
    if (style == WS_OVERLAPPED)
        style |= WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_VISIBLE;

    pm_trace("WinCreateStdWindow", (unsigned long)frameStyle,
             (unsigned long)fcf, (unsigned long)resources);
    if (classStyle & O2_CS_CLIPCHILDREN)
        pm_trace("class CS_CLIPCHILDREN", (unsigned long)(DWORD)classStyle,
                 (unsigned long)(DWORD)style, 0);

    nativeMenu = NULL;
    nativeIcon = NULL;
    if ((fcf & O2_FCF_MENU) && resources != 0) {
        rr = pm_find_resource(module, O2_RT_MENU, (O2USHORT)resources);
        if (rr) {
            templateUsed = 0;
            nativeMenu = pm_parse_menu_template(rr->data, rr->size,
                                                &templateUsed, 1);
            pm_trace(nativeMenu ? "resource menu OK" : "resource menu FAIL",
                     (unsigned long)resources, (unsigned long)rr->size,
                     (unsigned long)templateUsed);
        }
    }
    if ((fcf & O2_FCF_ICON) && resources != 0) {
        rr = pm_find_resource(module, O2_RT_POINTER, (O2USHORT)resources);
        if (rr) {
            nativeIcon = pm_create_os2_color_icon(rr->data, rr->size);
            pm_trace(nativeIcon ? "resource icon OK" : "resource icon FAIL",
                     (unsigned long)resources, (unsigned long)rr->size, 0);
        }
    }
    if ((fcf & O2_FCF_ACCELTABLE) && resources != 0) {
        g_accel_count = 0;
        rr = pm_find_resource(module, O2_RT_ACCELTABLE, (O2USHORT)resources);
        if (rr) {
            pm_load_accel_resource(rr);
            pm_trace("resource accel OK", (unsigned long)resources,
                     (unsigned long)rr->size, (unsigned long)g_accel_count);
        }
    }

    /*
     * Do not make the native window visible inside CreateWindowA.  With
     * WS_VISIBLE present, USER32 synchronously runs the show/activation/size
     * sequence before CreateWindowA returns.  At that point the OS/2 frame
     * and client bookkeeping below has not been published yet, so a reentrant
     * PM callback observes a half-created window.
     *
     * Create the native peer hidden, publish the handles, and only then honor
     * OS/2 WS_VISIBLE.  WM_CREATE still occurs synchronously, which is useful
     * and required, but the later visible-window traffic now sees a complete
     * PM object.
     */
    createStyle = style & ~WS_VISIBLE;
    SetLastError(0);
    wstate = (struct PMCompatWindow *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*wstate));
    if (!wstate) return 0;
    wstate->proc = classProc;
    strncpy(wstate->class_name, clientClass, sizeof(wstate->class_name)-1);
    ++g_native_create_depth;
    hwnd = CreateWindowA(clientClass,
                         title ? title : clientClass,
                         createStyle,
                         CW_USEDEFAULT, CW_USEDEFAULT, 320, 240,
                         nativeParent, nativeMenu, g_instance, wstate);
    --g_native_create_depth;
    if (!hwnd) {
        pm_trace("CreateWindow FAIL", (unsigned long)GetLastError(), 0, 0);
        if (nativeMenu) DestroyMenu(nativeMenu);
        if (nativeIcon) DestroyIcon(nativeIcon);
        HeapFree(GetProcessHeap(),0,wstate);
        return 0;
    }
    pm_trace("CreateWindow OK", (unsigned long)(DWORD)hwnd,
             (unsigned long)createStyle, 0);
    if (!nativeParent && !g_frame_hwnd) g_frame_hwnd = hwnd;
    if (nativeIcon) {
        if (g_frame_icon)
            DestroyIcon(g_frame_icon);
        g_frame_icon = nativeIcon;
        SendMessageA(hwnd, WM_SETICON, ICON_BIG, (LPARAM)nativeIcon);
        SendMessageA(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)nativeIcon);
    }
    if (clientOut)
        *clientOut = guest_hwnd(hwnd);
    pm_trace("PM handles published", (unsigned long)(DWORD)hwnd,
             clientOut ? (unsigned long)*clientOut : 0, 0);

    if (style & WS_VISIBLE) {
        /*
         * OS/2 asked for WS_VISIBLE, but showing the Win32 peer here causes
         * USER32 to synchronously enter the window procedure while
         * WinCreateStdWindow is still active on the guest stack.  WMCHAR
         * exposes that path.  Remember the visibility request and satisfy it
         * at the first WinGetMsg, after WinCreateStdWindow has returned and
         * the application has completed its post-create setup.
         */
        g_show_pending = 1;
        pm_trace("show deferred", (unsigned long)(DWORD)hwnd,
                 (unsigned long)style, 0);
    }
    return guest_hwnd(hwnd);
}

/* 910 */
O2MRESULT __cdecl WinDefDlgProc(O2HWND hwndDlg, O2USHORT msg,
                                O2MPARAM mp1, O2MPARAM mp2)
{
    (void)hwndDlg; (void)msg; (void)mp1; (void)mp2;
    return 0;
}

/* 911 */
O2MRESULT __cdecl WinDefWindowProc(O2HWND hwnd, O2USHORT msg,
                                   O2MPARAM mp1, O2MPARAM mp2)
{
    pm_trace("WinDefWindowProc", (unsigned long)msg,
             (unsigned long)hwnd, (unsigned long)mp1);
    (void)mp2;
    return 0;
}

/* 912 */
O2MRESULT __cdecl WinDispatchMsg(O2HAB hab, void *qmsg)
{
    MSG m;
    O2QMSG *q=(O2QMSG *)qmsg;
    (void)hab;
    if (q) {
        memset(&m,0,sizeof(m));
        m.hwnd=q->hwnd ? native_hwnd(q->hwnd) : NULL;
        m.message=(UINT)q->msg;
        m.wParam=(WPARAM)(DWORD)q->mp1;
        m.lParam=(LPARAM)(DWORD)q->mp2;
        m.time=(DWORD)q->time;
        m.pt.x=q->ptl.x; m.pt.y=q->ptl.y;
    } else {
        m=g_last_msg;
    }
    pm_trace("WinDispatchMsg", (unsigned long)m.message,
             (unsigned long)(DWORD)m.hwnd, 0);
    TranslateMessage(&m);
    return (O2MRESULT)DispatchMessageA(&m);
}

/* 913 */
O2LONG __cdecl WinDrawText(O2HPS hps, O2LONG cchText, char *text,
                           void *prcl, O2LONG fore, O2LONG back,
                           O2ULONG flags)
{
    struct CompatPS *p;
    O2RECTL local;
    O2RECTL *or;
    RECT wr;
    SIZE size;
    COLORREF oldFore;
    COLORREF oldBack;
    int oldMode;
    int n;
    char *hostText;
    pm_trace("WinDrawText call", (unsigned long)hps,
             (unsigned long)(DWORD)cchText, (unsigned long)(DWORD)flags);
    if (pm_trace_enabled() && text) {
        unsigned long packed = 0UL;
        if (!IsBadReadPtr((const void *)text, 4)) {
            const unsigned char *t = (const unsigned char *)text;
            packed = (unsigned long)t[0] | ((unsigned long)t[1] << 8) |
                     ((unsigned long)t[2] << 16) | ((unsigned long)t[3] << 24);
        }
        pm_trace("WinDrawText text", (unsigned long)(DWORD)text, packed, 0);
    }
    p = ps_from(hps);
    if (!p || !p->dc || !text)
        return 0;
    n = (int)cchText;
    if (n < 0)
        n = (int)strlen(text);
    if (n < 0)
        return 0;

    /* Never hand a guest pointer directly to GDI.  Besides being a cleaner
     * personality boundary, WMCHAR exercises stack-backed strings produced
     * by its 1990 CRT; copy the exact explicit-length byte sequence into
     * host-owned storage first. */
    hostText = (char *)HeapAlloc(GetProcessHeap(), 0,
                                 (SIZE_T)(n != 0 ? n : 1));
    if (!hostText)
        return 0;
    if (n != 0) {
        if (IsBadReadPtr((const void *)text, (UINT)n)) {
            HeapFree(GetProcessHeap(), 0, hostText);
            return 0;
        }
        memcpy(hostText, text, (size_t)n);
    } else {
        hostText[0] = 0;
    }
    if (pm_trace_enabled() && n > 0) {
        unsigned long hp = (unsigned char)hostText[0];
        if (n > 1) hp |= ((unsigned long)(unsigned char)hostText[1] << 8);
        if (n > 2) hp |= ((unsigned long)(unsigned char)hostText[2] << 16);
        if (n > 3) hp |= ((unsigned long)(unsigned char)hostText[3] << 24);
        pm_trace("WinDrawText hostcopy", (unsigned long)(DWORD)hostText,
                 hp, (unsigned long)(DWORD)n);
    }
    if (prcl)
        or = (O2RECTL *)prcl;
    else {
        client_rect_os2(p->hwnd, &local);
        or = &local;
    }

    if (flags & O2_DT_QUERYEXTENT) {
        memset(&size, 0, sizeof(size));
        if (!GetTextExtentPoint32A(p->dc, hostText, n, &size)) {
            HeapFree(GetProcessHeap(), 0, hostText);
            return 0;
        }
        or->xRight = or->xLeft + size.cx;
        or->yTop = or->yBottom + size.cy;
        HeapFree(GetProcessHeap(), 0, hostText);
        return (O2LONG)n;
    }

    rect_os2_to_win(p->hwnd, or, &wr);
    oldFore = SetTextColor(p->dc, os2_color(fore));
    oldBack = SetBkColor(p->dc, os2_color(back));
    oldMode = SetBkMode(p->dc, OPAQUE);
    pm_trace("WinDrawText rect", (unsigned long)(DWORD)wr.left,
             (unsigned long)(DWORD)wr.top, (unsigned long)(DWORD)wr.right);
    ExtTextOutA(p->dc, wr.left, wr.top, ETO_CLIPPED | ETO_OPAQUE,
                &wr, hostText, (UINT)n, NULL);
    SetBkMode(p->dc, oldMode);
    SetBkColor(p->dc, oldBack);
    SetTextColor(p->dc, oldFore);
    HeapFree(GetProcessHeap(), 0, hostText);
    pm_trace("WinDrawText leave", (unsigned long)hps,
             (unsigned long)(DWORD)n, 0);
    return (O2LONG)n;
}

/* 915 */
O2ULONG __cdecl WinGetMsg(O2HAB hab, void *qmsg, O2HWND filter,
                          O2ULONG first, O2ULONG last)
{
    int rc;
    int shown;
    HWND whFilter = filter ? native_hwnd(filter) : NULL;
    pm_trace("WinGetMsg enter", (unsigned long)hab,
             (unsigned long)g_show_pending, 0);

    if (g_show_pending && g_frame_hwnd && IsWindow(g_frame_hwnd)) {
        g_show_pending = 0;
        pm_trace("deferred ShowWindow",
                 (unsigned long)(DWORD)g_frame_hwnd, 0, 0);
        ++g_native_show_depth;
        shown = ShowWindow(g_frame_hwnd, SW_SHOW);
        --g_native_show_depth;
        pm_trace("ShowWindow returned",
                 (unsigned long)(DWORD)g_frame_hwnd,
                 (unsigned long)shown,
                 (unsigned long)IsWindowVisible(g_frame_hwnd));
        InvalidateRect(g_frame_hwnd, NULL, FALSE);
        pm_trace("paint queued", (unsigned long)(DWORD)g_frame_hwnd, 0, 0);
    }

    SetLastError(0);
    rc = GetMessageA(&g_last_msg, whFilter, (UINT)first, (UINT)last);
    if (rc > 0) {
        pm_fill_qmsg((O2QMSG *)qmsg, &g_last_msg);
        pm_trace("WinGetMsg message", (unsigned long)g_last_msg.message,
                 (unsigned long)(DWORD)g_last_msg.hwnd,
                 (unsigned long)g_last_msg.wParam);
    } else if (rc == 0) {
        pm_fill_qmsg((O2QMSG *)qmsg, &g_last_msg);
        pm_trace("WinGetMsg WM_QUIT", (unsigned long)g_last_msg.wParam, 0, 0);
    } else {
        pm_trace("WinGetMsg ERROR", (unsigned long)GetLastError(), 0, 0);
    }
    return rc > 0 ? 1UL : 0UL;
}

/* 918 */
O2ULONG __cdecl WinPeekMsg(O2HAB hab, void *qmsg, O2HWND filter,
                           O2ULONG first, O2ULONG last, O2USHORT flags)
{
    MSG m;
    HWND whFilter = filter ? native_hwnd(filter) : NULL;
    UINT remove = (flags & O2_PM_REMOVE) ? PM_REMOVE : PM_NOREMOVE;
    (void)hab;
    if (!PeekMessageA(&m, whFilter, (UINT)first, (UINT)last, remove))
        return 0;
    pm_fill_qmsg((O2QMSG *)qmsg, &m);
    pm_trace("WinPeekMsg", (unsigned long)m.message,
             (unsigned long)(DWORD)m.hwnd, (unsigned long)flags);
    return 1;
}

/* 902 */
O2ULONG __cdecl WinPostQueueMsg(O2HMQ hmq, O2USHORT msg,
                                O2MPARAM mp1, O2MPARAM mp2)
{
    pm_trace("WinPostQueueMsg", (unsigned long)hmq,
             (unsigned long)msg, (unsigned long)mp1);
    if (!hmq) return 0;
    return PostThreadMessageA((DWORD)hmq, (UINT)msg,
                              (WPARAM)(DWORD)mp1, (LPARAM)(DWORD)mp2) ? 1UL : 0UL;
}

/* 919 */
O2ULONG __cdecl WinPostMsg(O2HWND hwnd, O2USHORT msg,
                           O2MPARAM mp1, O2MPARAM mp2)
{
    HWND wh;
    struct PMCompatPostedMsg *pm;
    if (msg == O2_WM_QUIT) {
        wh = native_hwnd(hwnd);
        if (wh) {
            DWORD tid = GetWindowThreadProcessId(wh, NULL);
            return PostThreadMessageA(tid, WM_QUIT, (WPARAM)(DWORD)mp1, 0) ? 1UL : 0UL;
        }
        PostQuitMessage((int)(DWORD)mp1);
        return 1;
    }
    wh = native_hwnd(hwnd);
    if (!wh)
        return 0;
    pm = (struct PMCompatPostedMsg *)HeapAlloc(GetProcessHeap(), 0, sizeof(*pm));
    if (!pm)
        return 0;
    pm->msg = msg;
    pm->mp1 = mp1;
    pm->mp2 = mp2;
    if (!PostMessageA(wh, HOST_WM_OS2_POST, (WPARAM)(DWORD)pm, 0)) {
        HeapFree(GetProcessHeap(), 0, pm);
        return 0;
    }
    return 1;
}

/* 920 */
O2MRESULT __cdecl WinSendMsg(O2HWND hwnd, O2USHORT msg,
                             O2MPARAM mp1, O2MPARAM mp2)
{
    HMENU menu;
    UINT id;
    UINT mask;
    UINT value;
    HWND wh;
    pm_trace("WinSendMsg", (unsigned long)hwnd,
             (unsigned long)msg, (unsigned long)mp1);
    {
        struct PMCompatScrollProxy *sp = pm_scroll_proxy_from_handle(hwnd);
        if (sp) {
            if (msg == O2_SBM_SETSCROLLBAR) {
                SCROLLINFO si;
                memset(&si,0,sizeof(si)); si.cbSize=sizeof(si);
                si.fMask=SIF_RANGE|SIF_POS;
                si.nMin=(int)(short)LOWORD((DWORD)mp2);
                si.nMax=(int)(short)HIWORD((DWORD)mp2);
                si.nPos=(int)(short)LOWORD((DWORD)mp1);
                SetScrollInfo(sp->parent, sp->bar, &si, TRUE);
                return 1;
            }
            if (msg == O2_SBM_SETPOS) {
                SetScrollPos(sp->parent, sp->bar, (int)(short)LOWORD((DWORD)mp1), TRUE);
                return 1;
            }
            if (msg == O2_SBM_SETTHUMBSIZE) {
                SCROLLINFO si;
                memset(&si,0,sizeof(si)); si.cbSize=sizeof(si); si.fMask=SIF_PAGE;
                si.nPage=(UINT)(O2USHORT)LOWORD((DWORD)mp1);
                SetScrollInfo(sp->parent, sp->bar, &si, TRUE);
                return 1;
            }
            return 0;
        }
    }
    menu = (HMENU)(DWORD)hwnd;
    if (menu && IsMenu(menu)) {
        if (msg == O2_MM_QUERYITEM) {
            O2MENUITEM *out=(O2MENUITEM *)(DWORD)mp2;
            MENUITEMINFOA mi;
            id=LOWORD((DWORD)mp1);
            if (!out) return 0;
            memset(&mi,0,sizeof(mi)); mi.cbSize=sizeof(mi);
            mi.fMask=MIIM_ID|MIIM_STATE|MIIM_FTYPE|MIIM_SUBMENU;
            if (!GetMenuItemInfoA(menu,id,FALSE,&mi)) {
                HMENU im=pm_menu_for_command(menu,id);
                if (!im || !GetMenuItemInfoA(im,id,FALSE,&mi)) return 0;
            }
            memset(out,0,sizeof(*out));
            out->id=(O2USHORT)mi.wID;
            if (mi.hSubMenu) out->afStyle |= O2_MIS_SUBMENU;
            if (mi.fType & MFT_SEPARATOR) out->afStyle |= O2_MIS_SEPARATOR;
            else out->afStyle |= O2_MIS_TEXT;
            if (mi.fState & MFS_CHECKED) out->afAttribute |= O2_MIA_CHECKED;
            if (mi.fState & (MFS_DISABLED|MFS_GRAYED)) out->afAttribute |= O2_MIA_DISABLED;
            out->hwndSubMenu=(O2HWND)(DWORD)mi.hSubMenu;
            return 1;
        }
        if (msg == O2_MM_SETITEMATTR) {
            HMENU itemMenu;
            id = LOWORD((DWORD)mp1);
            mask = LOWORD((DWORD)mp2);
            value = HIWORD((DWORD)mp2);
            itemMenu = pm_menu_for_command(menu, id);
            if (!itemMenu)
                return 0;
            if (mask & O2_MIA_CHECKED)
                CheckMenuItem(itemMenu, id, MF_BYCOMMAND |
                              ((value & O2_MIA_CHECKED) ? MF_CHECKED : MF_UNCHECKED));
            if (mask & O2_MIA_DISABLED)
                EnableMenuItem(itemMenu, id, MF_BYCOMMAND |
                               ((value & O2_MIA_DISABLED) ? MF_GRAYED : MF_ENABLED));
            DrawMenuBar(g_frame_hwnd);
            return 1;
        }
        return 0;
    }
    wh = native_hwnd(hwnd);
    if (!wh) return 0;
    {
        int handled;
        O2MRESULT lr;
        lr=pm_send_slider(wh,msg,mp1,mp2,&handled);
        if (handled) return lr;
        lr=pm_send_listbox(wh,msg,mp1,mp2,&handled);
        if (handled) return lr;
    }
    if (msg == O2_SBM_SETSCROLLBAR || msg == O2_SBM_SETPOS || msg == O2_SBM_SETTHUMBSIZE) {
        char cls[32]; DWORD sty; int bar=SB_CTL;
        cls[0]=0; GetClassNameA(wh,cls,sizeof(cls)); sty=(DWORD)GetWindowLongA(wh,GWL_STYLE);
        if (_stricmp(cls,"SCROLLBAR")!=0) bar=SB_VERT;
        if (msg == O2_SBM_SETSCROLLBAR) {
            SCROLLINFO si; memset(&si,0,sizeof(si)); si.cbSize=sizeof(si);
            si.fMask=SIF_RANGE|SIF_POS; si.nMin=(int)(short)LOWORD((DWORD)mp2);
            si.nMax=(int)(short)HIWORD((DWORD)mp2); si.nPos=(int)(short)LOWORD((DWORD)mp1);
            SetScrollInfo(wh,bar,&si,TRUE);
        } else if (msg == O2_SBM_SETPOS) {
            if (bar==SB_CTL) SendMessageA(wh,SBM_SETPOS,(WPARAM)(short)LOWORD((DWORD)mp1),TRUE);
            else SetScrollPos(wh,bar,(int)(short)LOWORD((DWORD)mp1),TRUE);
        } else {
            SCROLLINFO si; memset(&si,0,sizeof(si)); si.cbSize=sizeof(si); si.fMask=SIF_PAGE;
            si.nPage=(UINT)(O2USHORT)LOWORD((DWORD)mp1); SetScrollInfo(wh,bar,&si,TRUE);
        }
        (void)sty; return 1;
    }
    if (pm_window_state(wh))
        return call_guest(wh, msg, mp1, mp2);
    return 0;
}

/* 924 - modeless resource-backed dialog. */
O2HWND __cdecl WinLoadDlg(O2HWND parent, O2HWND owner,
                          O2PFNWP dlgProc, O2ULONG module,
                          O2USHORT idDlg, void *createParams)
{
    const struct PMCompatResource *rr;
    struct PMCompatDialog *dlg;
    HWND ownerWh, parentWh, hwnd;
    RECT prc,drc;
    POINT porg;
    int tx,ty,ph,dh;
    O2MRESULT initResult;
    if (!dlgProc) return 0;
    rr=pm_find_resource(module,O2_RT_DIALOG,idDlg);
    if (!rr) return 0;
    dlg=(struct PMCompatDialog *)HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(*dlg));
    if (!dlg) return 0;
    dlg->guest_proc=dlgProc;
    dlg->heap_owned=1;
    ownerWh=native_hwnd(owner); if (ownerWh==GetDesktopWindow()) ownerWh=NULL;
    parentWh=native_hwnd(parent);
    hwnd=pm_create_dialog_template(module,rr,ownerWh,dlg);
    if (!hwnd) { HeapFree(GetProcessHeap(),0,dlg); return 0; }
    if (parentWh && rr->size >= 44UL) {
        const unsigned char *rb=rr->data; WORD roff=pm_rd16(rb+6);
        if ((DWORD)roff+30UL <= rr->size && GetClientRect(parentWh,&prc) && GetWindowRect(hwnd,&drc)) {
            const unsigned char *root=rb+roff;
            tx=(int)(short)pm_rd16(root+16)*2;
            ty=(int)(short)pm_rd16(root+18)*2;
            ph=prc.bottom-prc.top; dh=drc.bottom-drc.top;
            porg.x=0; porg.y=0; ClientToScreen(parentWh,&porg);
            SetWindowPos(hwnd,HWND_TOP,porg.x+tx,porg.y+ph-ty-dh,0,0,
                         SWP_NOSIZE|SWP_NOACTIVATE);
        }
    }
    ShowWindow(hwnd,SW_SHOW);
    UpdateWindow(hwnd);
    initResult=call_dialog_guest(hwnd,dlg,O2_WM_INITDLG,0,(O2MPARAM)(DWORD)createParams);
    (void)initResult;
    return guest_hwnd(hwnd);
}

/* 923 */
O2USHORT __cdecl WinDlgBox(O2HWND parent, O2HWND owner,
                           O2PFNWP dlgProc, O2ULONG module,
                           O2USHORT idDlg, void *createParams)
{
    const struct PMCompatResource *rr;
    struct PMCompatDialog dlg;
    HWND ownerWh;
    HWND parentWh;
    HWND hwnd;
    RECT orc, drc, prc;
    POINT porg;
    int ow, oh, dw, dh;
    int templateX, templateY, parentH;
    MSG m;
    int gm;
    O2MRESULT initResult;
    HWND firstTab;
    rr = pm_find_resource(module, O2_RT_DIALOG, idDlg);
    if (!rr || !dlgProc)
        return 0;
    memset(&dlg, 0, sizeof(dlg));
    dlg.guest_proc = dlgProc;
    ownerWh = native_hwnd(owner);
    if (ownerWh == GetDesktopWindow())
        ownerWh = NULL;
    parentWh = native_hwnd(parent);
    hwnd = pm_create_dialog_template(module, rr, ownerWh, &dlg);
    if (!hwnd)
        return 0;

    /*
     * OS/2 DLGTEMPLATE x/y are parent-relative, bottom-left coordinates.
     * Earlier micro-PM code ignored them and centered every dialog on its
     * owner.  BIO deliberately puts its narrow main frame at the far-right
     * edge while passing HWND_DESKTOP as the dialog parent; owner-centering
     * therefore pushed the Dates dialog mostly off-screen.
     */
    if (parentWh && rr->size >= 44UL) {
        const unsigned char *rb = rr->data;
        WORD roff = pm_rd16(rb + 6);
        if ((DWORD)roff + 30UL <= rr->size &&
            GetClientRect(parentWh, &prc) && GetWindowRect(hwnd, &drc)) {
            const unsigned char *root = rb + roff;
            templateX = (int)(short)pm_rd16(root + 16) * 2;
            templateY = (int)(short)pm_rd16(root + 18) * 2;
            dw = drc.right - drc.left;
            dh = drc.bottom - drc.top;
            parentH = prc.bottom - prc.top;
            porg.x = 0; porg.y = 0;
            ClientToScreen(parentWh, &porg);
            SetWindowPos(hwnd, HWND_TOP,
                         porg.x + templateX,
                         porg.y + parentH - templateY - dh,
                         0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
            pm_trace("dialog template pos", (unsigned long)templateX,
                     (unsigned long)templateY, (unsigned long)dh);
        }
    } else if (ownerWh && GetWindowRect(ownerWh, &orc) &&
               GetWindowRect(hwnd, &drc)) {
        ow = orc.right - orc.left; oh = orc.bottom - orc.top;
        dw = drc.right - drc.left; dh = drc.bottom - drc.top;
        SetWindowPos(hwnd, HWND_TOP, orc.left + (ow - dw) / 2,
                     orc.top + (oh - dh) / 2, 0, 0,
                     SWP_NOSIZE | SWP_NOACTIVATE);
    }
    if (ownerWh)
        EnableWindow(ownerWh, FALSE);
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    initResult = call_dialog_guest(hwnd, &dlg, O2_WM_INITDLG, 0,
                                   (O2MPARAM)(DWORD)createParams);
    if (initResult == 0 && !dlg.done) {
        firstTab = GetNextDlgTabItem(hwnd, NULL, FALSE);
        if (firstTab)
            SetFocus(firstTab);
    }

    while (!dlg.done) {
        gm = GetMessageA(&m, NULL, 0, 0);
        if (gm <= 0) {
            if (gm == 0)
                PostQuitMessage((int)m.wParam);
            break;
        }
        if (!IsDialogMessageA(hwnd, &m)) {
            TranslateMessage(&m);
            DispatchMessageA(&m);
        }
    }
    if (IsWindow(hwnd))
        DestroyWindow(hwnd);
    if (ownerWh) {
        EnableWindow(ownerWh, TRUE);
        SetActiveWindow(ownerWh);
    }
    if (dlg.control_icon)
        DestroyIcon(dlg.control_icon);
    return dlg.result;
}

/* 926 */
O2ULONG __cdecl WinRegisterClass(O2HAB hab, const char *name,
                                 O2PFNWP proc, O2ULONG style,
                                 O2ULONG cbWindowData)
{
    WNDCLASSA wc;
    (void)hab; (void)cbWindowData;
    if (!name || !proc)
        return 0;

    g_guest_proc = proc;
    {
        unsigned ci;
        for (ci=0; ci<g_class_count; ++ci) {
            if (strcmp(g_classes[ci].name,name)==0) { g_classes[ci].proc=proc; g_classes[ci].style=style; break; }
        }
        if (ci==g_class_count && g_class_count<PMCOMPAT_MAX_CLASSES) {
            strncpy(g_classes[ci].name,name,sizeof(g_classes[ci].name)-1);
            g_classes[ci].proc=proc; g_classes[ci].style=style; ++g_class_count;
        }
    }
    if (pm_trace_enabled() && strcmp(name, "Char Messages") == 0 &&
        (DWORD)proc >= 0x00010720UL) {
        DWORD delta = (DWORD)proc - 0x00010720UL;
        const short *gcy = (const short *)(DWORD)(delta + 0x00020910UL);
        const short *gave = (const short *)(DWORD)(delta + 0x00020912UL);
        const short *gmax = (const short *)(DWORD)(delta + 0x00020bd4UL);
        const unsigned char *fmt =
            (const unsigned char *)(DWORD)(delta + 0x000203d4UL);
        if (!IsBadReadPtr(gcy, 2) && !IsBadReadPtr(gave, 2) &&
            !IsBadReadPtr(gmax, 2)) {
            fprintf(stderr,
                    "PMWIN: WMCHAR metrics globals delta=%08lX "
                    "gcy=%d ave=%d max=%d\n",
                    (unsigned long)delta, (int)*gcy, (int)*gave, (int)*gmax);
        }
        if (!IsBadReadPtr(fmt, 4)) {
            fprintf(stderr,
                    "PMWIN: WMCHAR fmt %%u. @%08lX bytes=%02X %02X %02X %02X\n",
                    (unsigned long)(DWORD)fmt, fmt[0], fmt[1], fmt[2], fmt[3]);
        }
        fflush(stderr);
    }
    strncpy(g_guest_class, name, sizeof(g_guest_class) - 1);
    g_guest_class[sizeof(g_guest_class) - 1] = 0;

    memset(&wc, 0, sizeof(wc));
    /* Preserve the pre-M31D native redraw behavior.  OS/2 class style is
       tracked separately for policies such as CS_CLIPCHILDREN. */
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc = pm_wndproc;
    wc.hInstance = g_instance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = g_guest_class;

    if (!RegisterClassA(&wc)) {
        if (GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
            return 0;
    }
    g_class_registered = 1;
    pm_trace("WinRegisterClass OK", (unsigned long)hab,
             (unsigned long)(DWORD)proc, 1);
    return 1;
}


/*
 * Guest-callable stand-in for a public control's original OS/2 window
 * procedure.  WinSubclassWindow returns this pointer for native controls so
 * the guest subclass procedure can pass unhandled OS/2 messages to normal
 * control processing just as it would under PM.
 */
static O2MRESULT __cdecl pm_public_control_default_proc(O2HWND hwnd,
                                                        O2USHORT msg,
                                                        O2MPARAM mp1,
                                                        O2MPARAM mp2)
{
    HWND wh;
    struct PMCompatWindow *st;
    DWORD style;
    DWORD type;
    LRESULT r;

    wh = native_hwnd(hwnd);
    st = pm_window_state(wh);
    if (!wh || !st || !st->native_proc)
        return 0;

    style = (DWORD)GetWindowLongA(wh, GWL_STYLE);
    type = style & SS_TYPEMASK;

    if (msg == O2_SM_QUERYHANDLE) {
        if (type == SS_ICON)
            return (O2MRESULT)(DWORD)SendMessageA(wh, STM_GETICON, 0, 0);
        if (type == SS_BITMAP)
            return (O2MRESULT)(DWORD)SendMessageA(wh, STM_GETIMAGE,
                                                  IMAGE_BITMAP, 0);
        return 0;
    }
    if (msg == O2_SM_SETHANDLE) {
        if (type == SS_ICON) {
            r = SendMessageA(wh, STM_SETICON, (WPARAM)(DWORD)mp1, 0);
            InvalidateRect(wh, NULL, TRUE);
            return (O2MRESULT)(DWORD)r;
        }
        if (type == SS_BITMAP) {
            r = SendMessageA(wh, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)(DWORD)mp1);
            InvalidateRect(wh, NULL, TRUE);
            return (O2MRESULT)(DWORD)r;
        }
        return 0;
    }

    /* Translate the small set of ordinary PM messages that public controls
       need when a guest subclass delegates to its saved original PFNWP. */
    switch (msg) {
    case O2_WM_PAINT:
        return (O2MRESULT)(DWORD)CallWindowProcA(st->native_proc, wh,
                                                 WM_PAINT, 0, 0);
    case O2_WM_SIZE:
        return (O2MRESULT)(DWORD)CallWindowProcA(st->native_proc, wh,
                                                 WM_SIZE, 0, (LPARAM)mp2);
    case O2_WM_TIMER:
        return (O2MRESULT)(DWORD)CallWindowProcA(st->native_proc, wh,
                                                 WM_TIMER, (WPARAM)mp1, 0);
    case O2_WM_DESTROY:
        return (O2MRESULT)(DWORD)CallWindowProcA(st->native_proc, wh,
                                                 WM_DESTROY, 0, 0);
    default:
        break;
    }
    return 0;
}

/* 929 - subclass one PM window instance.
 *
 * Registered/private PM windows already run through pm_wndproc.  Preserve the
 * historical collapsed frame/client behavior for those windows so BIO and the
 * frozen M31D semantics remain unchanged.  Native public controls (STATIC,
 * BUTTON, etc.) have no PMCompatWindow state; for those, install pm_wndproc as
 * a native subclass bridge, remember the original Win32 WNDPROC, and return a
 * guest-callable adapter representing the original OS/2 control procedure.
 */
O2PFNWP __cdecl WinSubclassWindow(O2HWND hwnd, O2PFNWP proc)
{
    HWND wh;
    struct PMCompatWindow *st;
    O2PFNWP old;
    LONG previous;
    char cls[128];

    wh = native_hwnd(hwnd);
    if (!wh || !proc)
        return NULL;

    st = pm_window_state(wh);
    if (st) {
        old = st->proc ? st->proc : g_guest_proc;
        pm_trace("WinSubclassWindow compat", (unsigned long)hwnd,
                 (unsigned long)(DWORD)proc, (unsigned long)(DWORD)old);
        return old;
    }

    st = (struct PMCompatWindow *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
                                             sizeof(*st));
    if (!st)
        return NULL;
    st->proc = proc;
    cls[0] = 0;
    GetClassNameA(wh, cls, (int)sizeof(cls));
    strncpy(st->class_name, cls, sizeof(st->class_name) - 1);
    st->class_name[sizeof(st->class_name) - 1] = 0;

    SetLastError(0);
    previous = SetWindowLongA(wh, GWL_WNDPROC, (LONG)(DWORD)pm_wndproc);
    if (previous == 0 && GetLastError() != 0) {
        HeapFree(GetProcessHeap(), 0, st);
        return NULL;
    }
    st->native_proc = (WNDPROC)(DWORD)previous;
    SetWindowLongA(wh, GWL_USERDATA, (LONG)(DWORD)st);

    pm_trace("WinSubclassWindow native", (unsigned long)hwnd,
             (unsigned long)(DWORD)proc, (unsigned long)(DWORD)st->native_proc);
    return pm_public_control_default_proc;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH)
        g_instance = instance;
    return TRUE;
}
