#define INCL_DOS
#define INCL_WIN
#define INCL_GPI
#define INCL_DOSPROCESS
#define INCL_DOSSEMAPHORES
#define INCL_DOSQUEUES
#define INCL_DOSERRORS
#include <os2.h>

#include <stdlib.h>
#include <string.h>

#include "blood_os2_pm.h"

#define BLOODPM_QUEUE 64
#define BLOODPM_SW (BLOODPM_W * BLOODPM_SCALE)
#define BLOODPM_SH (BLOODPM_H * BLOODPM_SCALE)

struct BloodPM {
 HAB hab;
 HMQ hmq;
 HWND frame;
 HWND client;
 HDC hdc_screen;
 HPS hps_screen;
 HDC hdc_mem;
 HPS hps_mem;
 HBITMAP hbm;
 HMTX lock;
 PBITMAPINFO2 pbmi;
 unsigned char pixels[BLOODPM_SW * BLOODPM_SH];
 unsigned char ega_map[16];
 int mouse_x;
 int mouse_y;
 int left_down;
 int right_down;
 int right_latch;
};

static BloodPM *g_pm;
static BloodPMEvent g_queue[BLOODPM_QUEUE];
static int g_qread;
static int g_qwrite;
static uint8_t g_keys[2048];
static ULONG g_start_ms;
static HQUEUE g_beep_queue;
static TID g_beep_tid;
static int g_beep_ready;
static PSZ g_beep_queue_name = "\\QUEUES\\BLOODPM.QUE";

static LONG os2_palette[256] = {
 0x00000000L,0x000000aaL,0x0000aa00L,0x0000aaaaL,
 0x00aa0000L,0x00aa00aaL,0x00aa5500L,0x00aaaaaaL,
 0x00555555L,0x005555ffL,0x0055ff55L,0x0055ffffL,
 0x00ff5555L,0x00ff55ffL,0x00ffff55L,0x00ffffffL
};

static MRESULT EXPENTRY os2_wndproc(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2);
static int os2_queue_event(BloodPMEvent *event);
static int os2_pop_event(BloodPMEvent *event);
static int os2_pm_init(BloodPM *pm, const char *title);
static void os2_prepare_palette_map(BloodPM *pm);
static void os2_beeper(ULONG arg);
static void os2_beep_init(void);
static uint32_t os2_now_ms(void);
static int os2_key_from_char(USHORT vk, USHORT ch);
static int os2_client_to_game_x(int x);
static int os2_client_to_game_y(int y);

int blood_pm_platform_init(void)
{
 g_start_ms = os2_now_ms();
 os2_beep_init();
 return 0;
}

BloodPM *blood_pm_create(const char *title)
{
 BloodPM *pm;
 pm = (BloodPM *)calloc(1, sizeof(*pm));
 if (!pm) return 0;
 g_pm = pm;
 if (os2_pm_init(pm, title)) {
  g_pm = 0;
  free(pm);
  return 0;
 }
 return pm;
}

void blood_pm_destroy(BloodPM *pm)
{
 if (!pm) return;
 if (pm->hbm) GpiDeleteBitmap(pm->hbm);
 if (pm->hps_mem) GpiDestroyPS(pm->hps_mem);
 if (pm->hdc_mem) DevCloseDC(pm->hdc_mem);
 if (pm->hps_screen) GpiDestroyPS(pm->hps_screen);
 if (pm->pbmi) DosFreeMem(pm->pbmi);
 if (pm->lock) DosCloseMutexSem(pm->lock);
 if (g_pm == pm) g_pm = 0;
 free(pm);
}

void blood_pm_set_title(BloodPM *pm,const char *title)
{
 if (pm && pm->frame) WinSetWindowText(pm->frame, (PSZ)title);
}

void blood_pm_present(BloodPM *pm,const uint8_t *pixels)
{
 POINTL pts[3];
 int x, y, sx, sy;
 unsigned char *dst;
 const uint8_t *src;
 unsigned char color;
 int row;
 if (!pm || !pixels) return;
 dst = pm->pixels;
 for (y = 0; y < BLOODPM_H; y++) {
  src = pixels + y * BLOODPM_W;
  for (x = 0; x < BLOODPM_W; x++) {
   color = pm->ega_map[src[x] & 15];
   for (sy = 0; sy < BLOODPM_SCALE; sy++) {
    row = (BLOODPM_SH - 1 - (y * BLOODPM_SCALE + sy)) * BLOODPM_SW +
          x * BLOODPM_SCALE;
    for (sx = 0; sx < BLOODPM_SCALE; sx++) dst[row + sx] = color;
   }
  }
 }
 pts[0].x = 0; pts[0].y = 0;
 pts[1].x = BLOODPM_SW;
 pts[1].y = BLOODPM_SH;
 pts[2].x = 0; pts[2].y = 0;
 DosRequestMutexSem(pm->lock, SEM_INDEFINITE_WAIT);
 GpiSetBitmapBits(pm->hps_mem, 0L, (LONG)BLOODPM_SH, pm->pixels, pm->pbmi);
 GpiBitBlt(pm->hps_screen, pm->hps_mem, 3L, pts, ROP_SRCCOPY, BBO_AND);
 DosReleaseMutexSem(pm->lock);
}

int blood_pm_poll(BloodPM *pm,BloodPMEvent *event)
{
 QMSG qmsg;
 if (os2_pop_event(event)) return 1;
 if (!pm) return 0;
 while (WinPeekMsg(pm->hab, &qmsg, 0, 0, 0, PM_REMOVE)) {
  WinDispatchMsg(pm->hab, &qmsg);
  if (os2_pop_event(event)) return 1;
 }
 return 0;
}

uint32_t blood_pm_ticks(void)
{
 return os2_now_ms() - g_start_ms;
}

void blood_pm_sleep(uint32_t ms)
{
 DosSleep(ms);
}

void blood_pm_get_mouse(BloodPM *pm,int *x,int *y,int *left,int *right)
{
 int r;
 if (!pm) {
  if (x) *x = 0;
  if (y) *y = 0;
  if (left) *left = 0;
  if (right) *right = 0;
  return;
 }
#ifdef VK_BUTTON1
 if (WinGetKeyState(HWND_DESKTOP, VK_BUTTON1) & 0x8000) pm->left_down = 1;
#endif
#ifdef VK_BUTTON2
 if (WinGetKeyState(HWND_DESKTOP, VK_BUTTON2) & 0x8000) pm->right_down = 1;
#endif
#ifdef VK_BUTTON3
 if (WinGetKeyState(HWND_DESKTOP, VK_BUTTON3) & 0x8000) pm->right_down = 1;
#endif
 if (x) *x = pm->mouse_x;
 if (y) *y = pm->mouse_y;
 r = pm->right_down || pm->right_latch;
 if (left) *left = pm->left_down;
 if (right) *right = r;
 pm->right_latch = 0;
}

void blood_pm_warp_mouse(BloodPM *pm,int x,int y)
{
 if (!pm) return;
 /* Soft recenter only.  Moving the host pointer is too risky under
  * os2host32 because a coordinate mismatch can click outside the PM client. */
 pm->mouse_x = x;
 pm->mouse_y = y;
}

const uint8_t *blood_pm_keys(void)
{
 return g_keys;
}

int blood_pm_queue_beep(int freq,int duration)
{
 if (!g_beep_ready) {
  DosBeep((ULONG)freq, (ULONG)duration);
  return 0;
 }
 if (freq < 0) freq = 0;
 if (freq && freq < 37) freq = 37;
 if (freq > 32767) freq = 32767;
 if (duration < 1) duration = 1;
 if (duration > 32767) duration = 32767;
 DosWriteQueue(g_beep_queue, 12345L, sizeof(freq), (PVOID)freq, 0L);
 DosWriteQueue(g_beep_queue, 12345L, sizeof(duration), (PVOID)duration, 0L);
 return 0;
}

static int os2_pm_init(BloodPM *pm, const char *title)
{
 ULONG flags;
 SIZEL sizl;
 BITMAPINFOHEADER2 bmih;
 BITMAPINFOHEADER2 qh;
 int i;
 ULONG size;

 for (i = 16; i < 256; i++) os2_palette[i] = os2_palette[i & 15];

 pm->hab = WinInitialize(0);
 if (!pm->hab) return -1;
 pm->hmq = WinCreateMsgQueue(pm->hab, 0);
 if (!pm->hmq) return -1;
 if (!WinRegisterClass(pm->hab, (PSZ)"BloodOS2PM", os2_wndproc,
                       CS_SIZEREDRAW, sizeof(PVOID))) return -1;
 flags = FCF_TITLEBAR | FCF_MINBUTTON | FCF_SYSMENU | FCF_TASKLIST;
 pm->frame = WinCreateStdWindow(HWND_DESKTOP, WS_VISIBLE, &flags,
                                (PSZ)"BloodOS2PM", (PSZ)title,
                                WS_VISIBLE, 0, 1, &pm->client);
 if (!pm->frame) return -1;
 WinSetWindowPos(pm->frame, 0, 60, 60, BLOODPM_SW,
                 BLOODPM_SH +
                 WinQuerySysValue(HWND_DESKTOP, SV_CYTITLEBAR),
                 SWP_SIZE | SWP_MOVE | SWP_SHOW);
 DosCreateMutexSem((PSZ)0, &pm->lock, 0, FALSE);

 pm->hdc_screen = WinOpenWindowDC(pm->client);
 sizl.cx = 0; sizl.cy = 0;
 pm->hps_screen = GpiCreatePS(pm->hab, pm->hdc_screen, &sizl,
                              PU_PELS | GPIT_MICRO | GPIA_ASSOC | GPIF_DEFAULT);
 pm->hdc_mem = DevOpenDC(pm->hab, OD_MEMORY, (PSZ)"*", 0L, 0L, 0L);
 pm->hps_mem = GpiCreatePS(pm->hab, pm->hdc_mem, &sizl,
                           PU_PELS | GPIT_MICRO | GPIA_ASSOC | GPIF_DEFAULT);
 memset(&bmih, 0, sizeof(bmih));
 bmih.cbFix = sizeof(bmih);
 bmih.cx = BLOODPM_SW;
 bmih.cy = BLOODPM_SH;
 bmih.cPlanes = 1;
 bmih.cBitCount = 8;
 pm->hbm = GpiCreateBitmap(pm->hps_mem, &bmih, 0L, NULL, NULL);
 GpiSetBitmap(pm->hps_mem, pm->hbm);
 GpiCreateLogColorTable(pm->hps_mem, LCOL_PURECOLOR, LCOLF_CONSECRGB,
                        0L, 256L, (PLONG)os2_palette);
 GpiSetBackMix(pm->hps_mem, BM_OVERPAINT);
 memset(&qh, 0, sizeof(qh));
 qh.cbFix = 16L;
 GpiQueryBitmapInfoHeader(pm->hbm, &qh);
 size = sizeof(BITMAPINFO2) + sizeof(RGB2) * 256;
 if (DosAllocMem((PPVOID)&pm->pbmi, size, PAG_COMMIT | PAG_READ | PAG_WRITE)) return -1;
 memset(pm->pbmi, 0, size);
 pm->pbmi->cbFix = qh.cbFix;
 pm->pbmi->cx = qh.cx;
 pm->pbmi->cy = qh.cy;
 pm->pbmi->cPlanes = qh.cPlanes;
 pm->pbmi->cBitCount = qh.cBitCount;
 pm->pbmi->ulCompression = qh.ulCompression;
 pm->pbmi->cbImage = qh.cbImage;
 pm->pbmi->cclrUsed = 256;
 pm->pbmi->cclrImportant = 256;
 os2_prepare_palette_map(pm);
 WinFocusChange(HWND_DESKTOP, pm->client, 0L);
 return 0;
}

static void os2_prepare_palette_map(BloodPM *pm)
{
 POINTL pt;
 unsigned char sample[BLOODPM_W];
 int i;
 for (i = 0; i < 16; i++) pm->ega_map[i] = (unsigned char)i;
 if (!pm || !pm->hps_mem || !pm->pbmi) return;
 memset(sample, 0, sizeof(sample));
 DosRequestMutexSem(pm->lock, SEM_INDEFINITE_WAIT);
 for (i = 0; i < 16; i++) {
  GpiSetColor(pm->hps_mem, (LONG)i);
  pt.x = i;
  pt.y = 0;
  GpiSetPel(pm->hps_mem, &pt);
 }
 GpiQueryBitmapBits(pm->hps_mem, 0L, 1L, sample, pm->pbmi);
 DosReleaseMutexSem(pm->lock);
 for (i = 0; i < 16; i++) pm->ega_map[i] = sample[i];
}

static void os2_beep_init(void)
{
 if (g_beep_ready) return;
 if (DosCreateQueue(&g_beep_queue, QUE_FIFO, g_beep_queue_name)) return;
 if (DosCreateThread(&g_beep_tid, os2_beeper, 0UL, 0UL, 8192UL)) return;
 g_beep_ready = 1;
}

static void os2_beeper(ULONG arg)
{
 PID owner;
 HQUEUE queue;
 REQUESTDATA request;
 PVOID data;
 BYTE priority;
 ULONG len;
 ULONG freq;
 ULONG duration;
 (void)arg;
 owner = 0;
 queue = 0;
 if (DosOpenQueue(&owner, &queue, g_beep_queue_name)) queue = g_beep_queue;
 for (;;) {
  data = 0;
  len = 0;
  priority = 0;
  memset(&request, 0, sizeof(request));
  if (!DosReadQueue(queue, &request, &len, &data, 0L, DCWW_WAIT, &priority, 0L)) {
   freq = (ULONG)data;
   data = 0;
   len = 0;
   priority = 0;
   memset(&request, 0, sizeof(request));
   if (!DosReadQueue(queue, &request, &len, &data, 0L, DCWW_WAIT, &priority, 0L)) {
    duration = (ULONG)data;
    if (duration) {
     if (freq) DosBeep(freq, duration);
     else DosSleep(duration);
    }
   }
  }
 }
}

static MRESULT EXPENTRY os2_wndproc(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
 BloodPM *pm;
 BloodPMEvent e;
 int x, y;
 int key;
 USHORT vk, ch;
 (void)hwnd;
 pm = g_pm;
 memset(&e, 0, sizeof(e));
 switch (msg) {
 case WM_PAINT:
 case WM_ERASEBACKGROUND:
  if (pm) {
   POINTL pts[3];
   pts[0].x = 0; pts[0].y = 0;
   pts[1].x = BLOODPM_SW;
   pts[1].y = BLOODPM_SH;
   pts[2].x = 0; pts[2].y = 0;
   DosRequestMutexSem(pm->lock, SEM_INDEFINITE_WAIT);
   WinBeginPaint(hwnd, pm->hps_screen, NULL);
   GpiBitBlt(pm->hps_screen, pm->hps_mem, 3L, pts, ROP_SRCCOPY, BBO_AND);
   WinEndPaint(pm->hps_screen);
   DosReleaseMutexSem(pm->lock);
  }
  break;
 case WM_CLOSE:
  e.type = BLOODPM_EVENT_QUIT;
  os2_queue_event(&e);
  break;
 case WM_MOUSEMOVE:
  if (pm) {
   pm->mouse_x = os2_client_to_game_x(SHORT1FROMMP(mp1));
   pm->mouse_y = os2_client_to_game_y(SHORT2FROMMP(mp1));
  }
  break;
 case WM_BUTTON1DOWN:
 case WM_BUTTON1UP:
#ifdef WM_BUTTON1DBLCLK
 case WM_BUTTON1DBLCLK:
#endif
 case WM_BUTTON2DOWN:
 case WM_BUTTON2UP:
#ifdef WM_BUTTON2DBLCLK
 case WM_BUTTON2DBLCLK:
#endif
#ifdef WM_BUTTON3DOWN
 case WM_BUTTON3DOWN:
 case WM_BUTTON3UP:
#endif
#ifdef WM_BUTTON3DBLCLK
 case WM_BUTTON3DBLCLK:
#endif
  if (!pm) break;
  x = os2_client_to_game_x(SHORT1FROMMP(mp1));
  y = os2_client_to_game_y(SHORT2FROMMP(mp1));
  pm->mouse_x = x;
  pm->mouse_y = y;
  e.x = x;
  e.y = y;
  if (msg == WM_BUTTON1DOWN
#ifdef WM_BUTTON1DBLCLK
      || msg == WM_BUTTON1DBLCLK
#endif
     ) {
   WinSetFocus(HWND_DESKTOP, pm->client);
   WinSetCapture(HWND_DESKTOP, pm->client);
   pm->left_down = 1;
   e.type = BLOODPM_EVENT_MOUSEDOWN;
   e.button = BLOODPM_BUTTON_LEFT;
   os2_queue_event(&e);
  } else if (msg == WM_BUTTON1UP) {
   pm->left_down = 0;
   if (!pm->right_down) WinSetCapture(HWND_DESKTOP, 0);
   e.type = BLOODPM_EVENT_MOUSEUP;
   e.button = BLOODPM_BUTTON_LEFT;
   os2_queue_event(&e);
  } else if (msg == WM_BUTTON2UP
#ifdef WM_BUTTON3UP
             || msg == WM_BUTTON3UP
#endif
            ) {
   pm->right_down = 0;
   pm->right_latch = 1;
   if (!pm->left_down) WinSetCapture(HWND_DESKTOP, 0);
   e.type = BLOODPM_EVENT_MOUSEDOWN;
   e.button = BLOODPM_BUTTON_RIGHT;
   os2_queue_event(&e);
  } else {
   WinSetFocus(HWND_DESKTOP, pm->client);
   WinSetCapture(HWND_DESKTOP, pm->client);
   pm->right_down = 1;
   e.type = BLOODPM_EVENT_MOUSEDOWN;
   e.button = BLOODPM_BUTTON_RIGHT;
   os2_queue_event(&e);
  }
  break;
 case WM_CHAR:
  vk = SHORT2FROMMP(mp2);
  ch = SHORT1FROMMP(mp2);
  key = os2_key_from_char(vk, ch);
  if (key > 0 && key < (int)sizeof(g_keys)) {
   if (SHORT1FROMMP(mp1) & KC_KEYUP) g_keys[key] = 0;
   else {
    g_keys[key] = 1;
    e.type = BLOODPM_EVENT_KEYDOWN;
    e.key = key;
    os2_queue_event(&e);
   }
  }
  break;
 default:
  return WinDefWindowProc(hwnd, msg, mp1, mp2);
 }
 return (MRESULT)FALSE;
}

static int os2_key_from_char(USHORT vk, USHORT ch)
{
 if (vk == VK_ESC) return BLOODPM_KEY_ESC;
 if (vk == VK_ENTER) return BLOODPM_KEY_ENTER;
 if (vk == VK_BACKSPACE) return BLOODPM_KEY_BACKSPACE;
 if (vk == VK_F8) return BLOODPM_KEY_F8;
 if (vk == VK_F9) return BLOODPM_KEY_F9;
 if (vk == VK_F12) return BLOODPM_KEY_F12;
 if (vk == VK_PAGEUP) return BLOODPM_KEY_PAGEUP;
 if (vk == VK_PAGEDOWN) return BLOODPM_KEY_PAGEDOWN;
 if (ch >= 32 && ch < 127) return (int)ch;
 return (int)vk;
}

static int os2_client_to_game_x(int x)
{
 x /= BLOODPM_SCALE;
 if (x < 0) x = 0;
 if (x >= BLOODPM_W) x = BLOODPM_W - 1;
 return x;
}

static int os2_client_to_game_y(int y)
{
 y /= BLOODPM_SCALE;
 y = BLOODPM_H - 1 - y;
 if (y < 0) y = 0;
 if (y >= BLOODPM_H) y = BLOODPM_H - 1;
 return y;
}

static int os2_queue_event(BloodPMEvent *event)
{
 int next;
 next = (g_qwrite + 1) % BLOODPM_QUEUE;
 if (next == g_qread) return 0;
 g_queue[g_qwrite] = *event;
 g_qwrite = next;
 return 1;
}

static int os2_pop_event(BloodPMEvent *event)
{
 if (g_qread == g_qwrite) return 0;
 *event = g_queue[g_qread];
 g_qread = (g_qread + 1) % BLOODPM_QUEUE;
 return 1;
}

static uint32_t os2_now_ms(void)
{
 ULONG ms;
 if (!DosQuerySysInfo(QSV_MS_COUNT, QSV_MS_COUNT, &ms, sizeof(ms))) return (uint32_t)ms;
 return 0;
}
