/* WinTown for OS/2 Presentation Manager, 32-bit / C89 front end.
 *
 * The simulation modules are retained unchanged in spirit.  This file owns
 * only PM lifetime, painting, input, city-file I/O, and the small UI seam.
 */
#define INCL_DOSFILEMGR
#include "mp_ui.h"
#include "os2bmp.h"
#include "sim.h"
#include "tools.h"
#include "tiles.h"
#include "charts.h"
#include "notify.h"
#include "mapgen.h"
#include "sprite.h"
#include "pm_ids.h"
#include "pm_scroll.h"
#include "pm_minimap.h"
#include "pm_stroke.h"
#include "pm_path.h"
#include "pm_sound.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MP_CLASS "WinTownPM"
#define MP_TITLE "WinTown/2"
#define SOURCE_TILE_SIZE 16
static int mapTileSize = 16;
#define TILE_SIZE mapTileSize
#define MP_TIMER 1
#define TOOLBAR_WIDTH 108
#define TOOL_BUTTON_SIZE 36
#define TOOL_COLUMNS 3
#define TOOL_ROWS 6
#define TOOL_COUNT 17
#define DEMAND_HEIGHT 68

short Map[WORLD_X][WORLD_Y];
short ResHis[HISTLEN / 2], ComHis[HISTLEN / 2], IndHis[HISTLEN / 2];
short CrimeHis[HISTLEN / 2], PollutionHis[HISTLEN / 2], MoneyHis[HISTLEN / 2];
short MiscHis[MISCHISTLEN / 2];
HWND hwndMain = 0;
static HWND hwndLog = 0;
static HWND hwndInfo = 0;
static HWND frameMain = 0;
static HWND frameLog = 0;
static HWND frameInfo = 0;
static HWND hwndChart = 0, frameChart = 0;
static HWND hwndTiles = 0, frameTiles = 0;
static int viewedTile = 0;
static int modalActive;
char cityFileName[MAX_PATH];
int toolbarWidth = TOOLBAR_WIDTH;

static HAB hab;
static HMQ hmq;
static HPS hpsMemory;
static HDC hdcMemory;
static HBITMAP hbmTiles;
static HPS hpsFrame;
static HDC hdcFrame;
static HBITMAP hbmFrame;
static int frameWidth;
static int frameHeight;
static unsigned long frameCount;
static int clientWidth = 640;
static int clientHeight = 300;
static int xOffset;
static int yOffset;
#ifndef MP_PORT_TEST
static HWND hwndHScroll, hwndVScroll;
static int updatingScrollbars;
static HWND hwndInfoScroll;
static int infoTop, infoUpdating;
static int timingReset = 1;
static PMStroke buildStroke;
static int strokeTool;
static void stop_build(void);
static HWND hwndMini, frameMini;
static HDC miniDC;
static HPS miniPS;
static int miniMode, miniDrag, miniTick, miniSimUpdate;
#endif
static void pan(int dx, int dy);
static int mouseDown;
static int lastMouseX;
static int lastMouseY;
static int hoverMapX = -1;
static int hoverMapY = -1;
static char logText[4096];

extern void DoSimInit(void);
extern void SimFrame(void);
extern void RandomlySeedRand(void);




extern int loadScenario(int scenarioId);
extern short RValve, CValve, IValve;
extern int TreeLevel, LakeLevel, CurveLevel, CreateIsland;
extern int GenerateMap(int seed);

void CenterMapOnLocation(int x, int y)
{
    pan(x * TILE_SIZE - (clientWidth - toolbarWidth) / 2 - xOffset,
        y * TILE_SIZE - pm_map_height(clientHeight) / 2 - yOffset);
}

static void pm_trace(const char *format, ...)
{
    va_list ap;
    fputs("WINTOWN: ", stderr);
    va_start(ap, format);
    vfprintf(stderr, format, ap);
    va_end(ap);
    fputc('\n', stderr);
    fflush(stderr);
}

#ifndef MP_PORT_TEST
static void destroy_frame_buffer(void)
{
    if (hpsFrame && hbmFrame) GpiSetBitmap(hpsFrame, NULLHANDLE);
    if (hbmFrame) GpiDeleteBitmap(hbmFrame);
    if (hpsFrame) GpiDestroyPS(hpsFrame);
    if (hdcFrame) DevCloseDC(hdcFrame);
    hpsFrame = 0; hdcFrame = 0; hbmFrame = 0;
    frameWidth = 0; frameHeight = 0;
}

static int create_frame_buffer(int width, int height)
{
    SIZEL size;
    BITMAPINFOHEADER2 hdr;
    if (width <= 0 || height <= 0) return 0;
    if (hpsFrame && hbmFrame && frameWidth == width && frameHeight == height)
        return 1;
    destroy_frame_buffer();
    hdcFrame = DevOpenDC(hab, OD_MEMORY, "*", 0L, NULL, NULLHANDLE);
    if (!hdcFrame) return 0;
    size.cx = 0; size.cy = 0;
    hpsFrame = GpiCreatePS(hab, hdcFrame, &size,
                           PU_PELS | GPIT_MICRO | GPIA_ASSOC);
    if (!hpsFrame) { destroy_frame_buffer(); return 0; }
    memset(&hdr, 0, sizeof(hdr));
    hdr.cbFix = sizeof(hdr);
    hdr.cx = (ULONG)width;
    hdr.cy = (ULONG)height;
    hdr.cPlanes = 1;
    hdr.cBitCount = 32;
    hbmFrame = GpiCreateBitmap(hpsFrame, &hdr, 0L, NULL, NULL);
    if (!hbmFrame) { destroy_frame_buffer(); return 0; }
    GpiSetBitmap(hpsFrame, hbmFrame);
    frameWidth = width; frameHeight = height;
    pm_trace("frame buffer %dx%d, 32 bpp", width, height);
    return 1;
}
#endif

static void append_log(const char *prefix, const char *format, va_list ap)
{
    char text[512];
    int used;
    vsprintf(text, format, ap);
    used = strlen(logText);
    if (used + strlen(prefix) + strlen(text) + 3 >= sizeof(logText)) {
        memmove(logText, logText + sizeof(logText) / 3, sizeof(logText) * 2 / 3);
        logText[sizeof(logText) * 2 / 3] = 0;
        used = strlen(logText);
    }
    strcat(logText + used, prefix);
    strcat(logText, text);
    strcat(logText, "\r\n");
}

void addGameLog(const char *format, ...)
{
    va_list ap;
    va_start(ap, format); append_log("", format, ap); va_end(ap);
}

void addDebugLog(const char *format, ...)
{
    va_list ap;
    if (!getenv("WINTOWN_TRACE")) return;
    va_start(ap, format); append_log("[debug] ", format, ap); va_end(ap);
}

void mp_debug_string(const char *text)
{
    addDebugLog("%s", text);
}

void mp_ui_message(HWND hwnd, const char *text, const char *caption)
{
#ifndef MP_PORT_TEST
    WinMessageBox(HWND_DESKTOP, hwnd, (PSZ)text, (PSZ)caption, 0,
                  MB_OK | MB_INFORMATION | MB_MOVEABLE);
#else
    (void)hwnd; (void)text; (void)caption;
#endif
}

void mp_ui_invalidate(void)
{
#ifndef MP_PORT_TEST
    if (hwndMain) WinInvalidateRect(hwndMain, NULL, FALSE);
    if (hwndLog) WinInvalidateRect(hwndLog, NULL, FALSE);
    if (hwndInfo) WinInvalidateRect(hwndInfo, NULL, FALSE);
    if (hwndChart) WinInvalidateRect(hwndChart, NULL, FALSE);
    if (hwndMini && !miniSimUpdate) WinInvalidateRect(hwndMini, NULL, FALSE);
#endif
}

void mp_ui_set_title(const char *title)
{
#ifndef MP_PORT_TEST
    if (frameMain) WinSetWindowText(frameMain, (PSZ)title);
#else
    (void)title;
#endif
}

USHORT mp_ui_start_timer(HWND hwnd, USHORT id, ULONG milliseconds)
{
#ifndef MP_PORT_TEST
    timingReset = 1;
    return WinStartTimer(hab, hwnd, id, milliseconds);
#else
    (void)hwnd; (void)id; (void)milliseconds; return 1;
#endif
}

unsigned long mp_ui_ticks(void)
{
    /* Notification timeouts need elapsed wall time, not CPU time. Avoid
     * adding a PMWIN import absent from the existing os2host32 shim. */
    return ((unsigned long)time(NULL) * 1000UL) & 0xffffffffUL;
}

void mp_ui_stop_timer(HWND hwnd, USHORT id)
{
#ifndef MP_PORT_TEST
    WinStopTimer(hab, hwnd, id);
#else
    (void)hwnd; (void)id;
#endif
}

void UpdateSimulationMenu(HWND hwnd, int speed)
{
    (void)hwnd; (void)speed;
}

extern int loadFile(char *name);
extern int saveFile(char *name);

void ForceFullCensus(void)
{
    /* Let the current core rebuild zone populations on its next map scan.
     * Taking a census here would insert a spurious history sample. */
    TotalPop = CalculateTotalPopulation(ResPop, ComPop, IndPop);
    CityPop = CalculateCityPopulation(ResPop, ComPop, IndPop);
}

static void new_city_options(const char *name, int difficulty, int terrain)
{
    static const int trees[4] = { -1, 0, 75, 50 };
    static const int lakes[4] = { -1, 0, 20, 10 };
    static const int curves[4] = { -1, 0, 30, 20 };
    static const int islands[4] = { -1, 0, 0, 1 };
    static const long funds[3] = { 50000L, 20000L, 10000L };
    char title[100];
    int x, y;
    if (difficulty < 0 || difficulty > 2) difficulty = 0;
    if (terrain < 0 || terrain > 3) terrain = 0;
    CleanupSimTimer(hwndMain);
    for (y = 0; y < WORLD_Y; ++y) for (x = 0; x < WORLD_X; ++x) Map[x][y] = DIRT;
    memset(ResHis, 0, sizeof(ResHis)); memset(ComHis, 0, sizeof(ComHis));
    memset(IndHis, 0, sizeof(IndHis)); memset(MiscHis, 0, sizeof(MiscHis));
    memset(CrimeHis, 0, sizeof(CrimeHis)); memset(PollutionHis, 0, sizeof(PollutionHis));
    memset(MoneyHis, 0, sizeof(MoneyHis));
    ClearCensus(); CityPop = 0; CityClass = 0; TotalPop = 0;
    ScenarioID = 0; ScoreType = 0; ScoreWait = 0;
    cityFileName[0] = 0;
    memset(PopDensity, 0, sizeof(PopDensity));
    memset(TrfDensity, 0, sizeof(TrfDensity));
    memset(PollutionMem, 0, sizeof(PollutionMem));
    memset(CrimeMem, 0, sizeof(CrimeMem));
    memset(LandValueMem, 0, sizeof(LandValueMem));
    memset(FireRate, 0, sizeof(FireRate));
    memset(PoliceMapEffect, 0, sizeof(PoliceMapEffect));
    RandomlySeedRand();
    DoSimInit();
    TreeLevel = trees[terrain]; LakeLevel = lakes[terrain];
    CurveLevel = curves[terrain]; CreateIsland = islands[terrain];
    GenerateMap((int)time(NULL));
    GameLevel = difficulty;
    CleanupChartSystem();
    InitChartSystem();
    TotalFunds = funds[difficulty];
    RValve = 500; CValve = 300; IValve = 100;
    xOffset = 0; yOffset = 0;
    pan(0, 0);
    hoverMapX = -1; hoverMapY = -1;
    SelectTool(bulldozerState);
    sprintf(title, MP_TITLE " - %.63s", name && name[0] ? name : "New City");
    addGameLog("New city: %.63s; difficulty %d; funds $%ld", name && name[0] ? name : "New City",
               difficulty, (long)TotalFunds);
    mp_ui_set_title(title);
    SetSimulationSpeed(hwndMain, SPEED_MEDIUM);
}

static void new_city(void)
{
    new_city_options("New City", 0, 0);
}
#ifdef MP_SELFTEST
void mp_test_new_city(const char *name, int difficulty, int terrain)
{
    new_city_options(name, difficulty, terrain);
}
#endif

#ifndef MP_PORT_TEST
static void draw_sprites(HPS hps);
#endif

static void draw_city(HPS hps)
{
#ifndef MP_PORT_TEST
    POINTL p[4];
    int x, y, tile, sx, sy;
    RECTL r;
    r.xLeft = 0; r.yBottom = 0; r.xRight = clientWidth; r.yTop = clientHeight;
    WinFillRect(hps, &r, CLR_BLACK);
    if (hbmTiles) {
        for (y = 0; y < WORLD_Y; ++y) for (x = 0; x < WORLD_X; ++x) {
            sx = toolbarWidth + x * TILE_SIZE - xOffset;
            sy = y * TILE_SIZE - yOffset;
            if (sx < toolbarWidth - TILE_SIZE || sy < -TILE_SIZE || sx >= clientWidth || sy >= pm_map_height(clientHeight)) continue;
            tile = Map[x][y] & LOMASK;
            if (tile < 0 || tile >= 960) tile = DIRT;
            p[0].x = sx; p[0].y = clientHeight - sy - TILE_SIZE;
            p[1].x = sx + TILE_SIZE; p[1].y = clientHeight - sy;
            p[2].x = (tile % 32) * SOURCE_TILE_SIZE;
            p[2].y = 480 - ((tile / 32) + 1) * SOURCE_TILE_SIZE;
            p[3].x = p[2].x + SOURCE_TILE_SIZE; p[3].y = p[2].y + SOURCE_TILE_SIZE;
            GpiBitBlt(hps, hpsMemory, 4L, p, ROP_SRCCOPY, BBO_IGNORE);
        }
    }
    draw_sprites(hps);
#else
    (void)hps;
#endif
}

#ifndef MP_PORT_TEST
static const int toolbarTools[TOOL_COUNT] = {
    residentialState, commercialState, industrialState, fireState,
    policeState, wireState, roadState, railState,
    parkState, stadiumState, seaportState, powerState,
    nuclearState, airportState, bulldozerState, queryState, noToolState
};

static const char *toolbarFiles[TOOL_COUNT - 1] = {
    "resident", "commerce", "industrl", "firest", "policest",
    "powerln", "road", "rail", "park", "stadium", "seaport",
    "coal", "nuclear", "airport", "bulldzr", "query"
};
static HBITMAP toolbarBitmaps[TOOL_COUNT];
static int toolbarWidths[TOOL_COUNT], toolbarHeights[TOOL_COUNT];
static HDC hdcIcons;
static HPS hpsIcons;

static void load_toolbar(void)
{
    SIZEL size;
    MPBITMAP *raw;
    char path[80];
    int i, loaded;
    size.cx = 0; size.cy = 0;
    hdcIcons = DevOpenDC(hab, OD_MEMORY, (PSZ)"*", 0L, NULL, NULLHANDLE);
    if (hdcIcons)
        hpsIcons = GpiCreatePS(hab, hdcIcons, &size,
                              PU_PELS | GPIT_MICRO | GPIA_ASSOC);
    loaded = 0;
    for (i = 0; i < TOOL_COUNT - 1; ++i) {
        sprintf(path, "assets\\%s.bmp", toolbarFiles[i]);
        raw = mp_bmp_load(path);
        if (!raw) {
            sprintf(path, "assets/%s.bmp", toolbarFiles[i]);
            raw = mp_bmp_load(path);
        }
        toolbarWidths[i] = mp_bmp_width(raw);
        toolbarHeights[i] = mp_bmp_height(raw);
        if (raw && hpsIcons)
            toolbarBitmaps[i] = mp_bmp_create_gpi(raw, hpsIcons);
        pm_trace("icon %s: %dx%d %s", path, toolbarWidths[i],
                 toolbarHeights[i], toolbarBitmaps[i] ? "OK" : "FAILED");
        if (toolbarBitmaps[i]) ++loaded;
        mp_bmp_free(raw);
    }
    pm_trace("toolbar: %d/16 BMPs loaded; button 17 = no tool", loaded);
}

/* Each sprite frame has its own magenta-key mask. */
static const int spriteFrames[8] = {5, 8, 11, 8, 4, 16, 3, 6};
static HBITMAP spriteColors[8][16], spriteMasks[8][16];
static int spriteWidths[8][16], spriteHeights[8][16];

static void load_sprites(void)
{
    static const char *names[8] = {"train", "helicopter", "airplane",
        "ship", "bus", "monster", "tornado", "explosion"};
    int t, f, loaded;
    char path[80];
    MPBITMAP *raw;
    loaded = 0;
    if (!hpsIcons) return;
    for (t = 0; t < 8; ++t) for (f = 0; f < spriteFrames[t]; ++f) {
        sprintf(path, "assets/%s-%d.bmp", names[t], f);
        raw = mp_bmp_load(path);
        if (!raw) { pm_trace("missing sprite %s", path); continue; }
        spriteWidths[t][f] = mp_bmp_width(raw);
        spriteHeights[t][f] = mp_bmp_height(raw);
        spriteColors[t][f] = mp_bmp_create_sprite(raw, hpsIcons, 0);
        spriteMasks[t][f] = mp_bmp_create_sprite(raw, hpsIcons, 1);
        if (spriteColors[t][f] && spriteMasks[t][f]) ++loaded;
        else pm_trace("cannot create sprite %s", path);
        mp_bmp_free(raw);
    }
    pm_trace("sprites: %d/61 frames loaded", loaded);
}

static void destroy_sprites(void)
{
    int t, f;
    for (t = 0; t < 8; ++t) for (f = 0; f < spriteFrames[t]; ++f) {
        if (spriteColors[t][f]) GpiDeleteBitmap(spriteColors[t][f]);
        if (spriteMasks[t][f]) GpiDeleteBitmap(spriteMasks[t][f]);
    }
}

static void draw_sprites(HPS hps)
{
    int i, t, f, x, y, width, height, visible, sourceVisible, progress, line;
    POINTL ripple;

    SimSprite *s;
    POINTL p[4];
    for (i = 0; i < MAX_SPRITES; ++i) {
        s = GetSpriteByIndex(i);
        if (!s || s->frame <= 0 || s->type < 1 || s->type > 8) continue;
        t = s->type - 1; f = (s->frame - 1) % spriteFrames[t];
        if (!spriteColors[t][f] || !spriteMasks[t][f]) continue;
        x = toolbarWidth + (s->x + s->x_offset) * TILE_SIZE / SOURCE_TILE_SIZE - xOffset;
        y = (s->y + s->y_offset) * TILE_SIZE / SOURCE_TILE_SIZE - yOffset;
        width = s->width * TILE_SIZE / SOURCE_TILE_SIZE;
        height = s->height * TILE_SIZE / SOURCE_TILE_SIZE;
        if (x + width <= toolbarWidth || x >= clientWidth ||
            y + height <= 0 || y >= pm_map_height(clientHeight)) continue;
        p[0].x = x; p[0].y = clientHeight - y - height;
        p[1].x = x + width; p[1].y = clientHeight - y;
        p[2].x = 0; p[2].y = 0;
        p[3].x = spriteWidths[t][f]; p[3].y = spriteHeights[t][f];
        if (s->type == SPRITE_MONSTER && s->water_phase) {
            progress = s->water_phase == 1 ? MONSTER_WATER_TICKS - s->water_ticks : s->water_ticks;
            if (progress < 1) progress = 1;
            visible = height * progress / MONSTER_WATER_TICKS;
            sourceVisible = spriteHeights[t][f] * progress / MONSTER_WATER_TICKS;
            if (visible < 1) visible = 1;
            if (sourceVisible < 1) sourceVisible = 1;
            p[1].y = p[0].y + visible;
            p[2].y = spriteHeights[t][f] - sourceVisible;
        }
        GpiSetBitmap(hpsIcons, spriteMasks[t][f]);
        GpiBitBlt(hps, hpsIcons, 4L, p, ROP_SRCAND, BBO_IGNORE);
        GpiSetBitmap(hpsIcons, spriteColors[t][f]);
        GpiBitBlt(hps, hpsIcons, 4L, p, ROP_SRCPAINT, BBO_IGNORE);
        if (s->type == SPRITE_MONSTER && s->water_phase) {

            for (line = 0; line < 3; ++line) {
                int inset;
                inset = width / 8 + ((s->water_ticks / 3 + line) % 3) * (width / 12);
                ripple.x = x + inset;
                ripple.y = clientHeight - y - height + line * 2;
                GpiSetColor(hps, line == 1 ? CLR_BLUE : CLR_CYAN);
                GpiMove(hps, &ripple);
                ripple.x = x + width - inset;
                GpiLine(hps, &ripple);
            }
            GpiSetColor(hps, CLR_BLACK);
        }
    }
}

/* Reserve room for signed demand bars even in the 640x300 frame. */
static int toolbar_row_height(void)
{
    int height;
    height = (clientHeight - DEMAND_HEIGHT - 4) / TOOL_ROWS;
    if (height > TOOL_BUTTON_SIZE) height = TOOL_BUTTON_SIZE;
    if (height < 1) height = 1;
    return height;
}

/* Return a toolbar index, never the unused eighteenth cell or the graph. */
static int toolbar_tool_at(int x, int y)
{
    int row, index;
    if (x < 0 || x >= toolbarWidth || y < 0 || y >= clientHeight) return -1;
    y = clientHeight - 1 - y;
    if (y < 4) return -1;
    row = (y - 4) / toolbar_row_height();
    if (row >= TOOL_ROWS) return -1;
    index = row * TOOL_COLUMNS + x / TOOL_BUTTON_SIZE;
    return index < TOOL_COUNT ? index : -1;
}

static void draw_toolbar_icon(HPS hps, int i, int x, int top, int rowHeight)
{
    POINTL p[4];
    int w, h, limit;
    char label[16];
    if (!toolbarBitmaps[i]) {
        if (i == TOOL_COUNT - 1) strcpy(label, "--");
        else sprintf(label, "%d", i + 1);
        p[0].x = x + 8; p[0].y = clientHeight - top - rowHeight + 5;
        GpiSetColor(hps, CLR_BLACK);
        GpiCharStringAt(hps, p, (LONG)strlen(label), (PCH)label);
        return;
    }
    w = toolbarWidths[i]; h = toolbarHeights[i];
    limit = TOOL_BUTTON_SIZE - 6;
    if (w > limit) { h = h * limit / w; w = limit; }
    limit = rowHeight - 6;
    if (limit < 1) return;
    if (h > limit) { w = w * limit / h; h = limit; }
    if (w < 1) w = 1;
    p[0].x = x + (TOOL_BUTTON_SIZE - w) / 2;
    p[0].y = clientHeight - top - (rowHeight + h) / 2;
    p[1].x = p[0].x + w; p[1].y = p[0].y + h;
    p[2].x = 0; p[2].y = 0;
    p[3].x = toolbarWidths[i]; p[3].y = toolbarHeights[i];
    GpiSetBitmap(hpsIcons, toolbarBitmaps[i]);
    GpiBitBlt(hps, hpsIcons, 4L, p, ROP_SRCCOPY, BBO_IGNORE);
}

static void draw_demand(HPS hps, int top)
{
    RECTL r;
    POINTL p;
    int values[3];
    LONG colors[3];
    int i, extent, baseline, height;
    char label[2];
    values[0] = RValve; values[1] = CValve; values[2] = IValve;
    colors[0] = CLR_DARKGREEN; colors[1] = CLR_DARKBLUE;
    colors[2] = CLR_YELLOW;
    height = (clientHeight - top - 20) / 2;
    if (height > 22) height = 22;
    if (height < 1) return;
    baseline = clientHeight - top - height - 2;
    for (i = 0; i < 3; ++i) {
        if (values[i] > 2000) values[i] = 2000;
        if (values[i] < -2000) values[i] = -2000;
        extent = (int)((long)values[i] * height / 2000L);
        r.xLeft = i * TOOL_BUTTON_SIZE + 8; r.xRight = r.xLeft + 20;
        r.yBottom = baseline + (extent < 0 ? extent : 0);
        r.yTop = baseline + (extent > 0 ? extent : 0);
        if (extent) WinFillRect(hps, &r, colors[i]);
        p.x = r.xLeft + 5; p.y = baseline - height - 14;
        label[0] = "RCI"[i]; label[1] = 0;
        GpiSetColor(hps, CLR_BLACK);
        GpiCharStringAt(hps, &p, 1L, (PCH)label);
    }
    r.xLeft = 4; r.xRight = TOOLBAR_WIDTH - 4;
    r.yBottom = baseline; r.yTop = baseline + 1;
    WinFillRect(hps, &r, CLR_BLACK);
}

static void draw_toolbar(HPS hps)
{
    RECTL r;
    POINTL p;
    int i;
    int col;
    int row;
    int left;
    int top;
    int rowHeight;

    r.xLeft = 0; r.yBottom = 0; r.xRight = toolbarWidth; r.yTop = clientHeight;
    WinFillRect(hps, &r, CLR_PALEGRAY);
    rowHeight = toolbar_row_height();
    for (i = 0; i < TOOL_COUNT; ++i) {
        col = i % TOOL_COLUMNS;
        row = i / TOOL_COLUMNS;
        left = col * (TOOLBAR_WIDTH / TOOL_COLUMNS);
        top = 4 + row * rowHeight;
        r.xLeft = left + 2;
        r.xRight = left + (TOOLBAR_WIDTH / TOOL_COLUMNS) - 2;
        r.yTop = clientHeight - top;
        r.yBottom = clientHeight - top - rowHeight + 2;
        WinFillRect(hps, &r, CLR_WHITE);
        draw_toolbar_icon(hps, i, left, top, rowHeight);
        if (GetCurrentTool() == toolbarTools[i]) {
            GpiSetColor(hps, CLR_RED);
            p.x = r.xLeft; p.y = r.yBottom;
            GpiMove(hps, &p);
            p.x = r.xRight - 1; p.y = r.yTop - 1;
            GpiBox(hps, DRO_OUTLINE, &p, 0L, 0L);
        }
    }
    draw_demand(hps, 4 + TOOL_ROWS * rowHeight);
}

static void draw_hover_cell(HPS hps)
{
    POINTL p;
    int sx;
    int sy, cells, startX, startY;
    if (GetCurrentTool() == noToolState) return;
    if (hoverMapX < 0 || hoverMapX >= WORLD_X || hoverMapY < 0 || hoverMapY >= WORLD_Y)
        return;
    cells = GetToolSize(GetCurrentTool());
    startX = hoverMapX - (cells > 1 ? 1 : 0);
    startY = hoverMapY - (cells > 1 ? 1 : 0);
    sx = toolbarWidth + startX * TILE_SIZE - xOffset;
    sy = startY * TILE_SIZE - yOffset;
    if (sx < toolbarWidth || sy < 0 || sx + cells * TILE_SIZE > clientWidth || sy + cells * TILE_SIZE > pm_map_height(clientHeight))
        return;
    GpiSetColor(hps, CLR_WHITE);
    p.x = sx;
    p.y = clientHeight - sy - cells * TILE_SIZE;
    GpiMove(hps, &p);
    p.x = sx + cells * TILE_SIZE - 1;
    p.y = clientHeight - sy - 1;
    GpiBox(hps, DRO_OUTLINE, &p, 0L, 0L);
}
#endif

/* Fixed, bounded fields only: the full message history belongs in Log. */
static void format_status(char *status)
{
    static const char *names[19] = {
        "Residential", "Commercial", "Industrial", "Fire", "Police",
        "Wire", "Road", "Rail", "Park", "Stadium", "Seaport", "Coal",
        "Nuclear", "Airport", "Network", "Bulldozer", "Query", "Pan", "None"
    };
    static const char *speeds[4] = {"Paused", "Slow", "Normal", "Fast"};
    int tool, speed;
    tool = GetCurrentTool(); speed = SimSpeed;
    if (tool < 0 || tool >= 19) tool = noToolState;
    if (speed < 0 || speed > 3) speed = 0;
    /* 120 bytes covers these labels and 32-bit integer fields. */
    sprintf(status, "%s   $%ld   %02d/%d W%d   %s", names[tool],
            (long)TotalFunds, CityMonth + 1, CityYear, (CityTime & 3) + 1, speeds[speed]);
}

#ifndef MP_PORT_TEST
static void draw_status(HPS hps)
{
    char status[120];
    RECTL r;
    POINTL p;
    if (clientWidth <= toolbarWidth) return;
    r.xLeft = toolbarWidth; r.xRight = clientWidth;
    r.yBottom = 0; r.yTop = MP_STATUS_HEIGHT;
    WinFillRect(hps, &r, CLR_PALEGRAY);
    format_status(status);
    p.x = toolbarWidth + 4; p.y = 4;
    GpiSetColor(hps, CLR_BLACK);
    GpiCharStringAt(hps, &p, (LONG)strlen(status), status);
}
#endif

#if defined(MP_PORT_TEST) && defined(MP_SELFTEST)
void mp_test_clear_log(void) { logText[0] = 0; }
const char *mp_test_game_log(void) { return logText; }
void mp_test_status(char *out) { format_status(out); }
#endif

static void present_city(HPS hps)
{
#ifndef MP_PORT_TEST
    POINTL p[3];
    if (!create_frame_buffer(clientWidth, clientHeight)) {
        pm_trace("frame buffer unavailable; painting direct");
        draw_city(hps);
        draw_toolbar(hps);
        draw_hover_cell(hps);
        draw_status(hps);
        return;
    }
    draw_city(hpsFrame);
    draw_toolbar(hpsFrame);
    draw_hover_cell(hpsFrame);
    draw_status(hpsFrame);
    p[0].x = 0; p[0].y = 0;
    p[1].x = clientWidth; p[1].y = clientHeight;
    p[2].x = 0; p[2].y = 0;
    GpiBitBlt(hps, hpsFrame, 3L, p, ROP_SRCCOPY, BBO_IGNORE);
    ++frameCount;
    if (getenv("WINTOWN_TRACE") && (frameCount % 100UL) == 0UL)
        pm_trace("presented %lu frames", frameCount);
#else
    (void)hps;
#endif
}

#define INFO_ROWS 24
/* Read-only report: painting must not run another evaluation or consume RNG. */
static void info_line(int row, char text[160])
{
    int problem, rank, votes;
    const char *level;
    level = GameLevel == 0 ? "Easy" : GameLevel == 1 ? "Medium" : "Hard";
    text[0] = 0;
    switch (row) {
    case 0: sprintf(text, "Date: %02d/%d", CityMonth+1, CityYear); break;
    case 1: sprintf(text, "Funds: $%ld", (long)TotalFunds); break;
    case 2: sprintf(text, "Population: %ld (%+ld)", (long)CityPop, (long)deltaCityPop); break;
    case 3: sprintf(text, "Class: %.30s / %s", GetCityClassName(), level); break;
    case 4: sprintf(text, "R %d  C %d  I %d", ResPop, ComPop, IndPop); break;
    case 6: strcpy(text, EvalValid ? "CITY EVALUATION (latest results)" : "CITY EVALUATION (pending)"); break;
    case 7:
        if (!EvalValid || CityYes+CityNo == 0) strcpy(text, "Approval: no survey yet");
        else sprintf(text, "Approval: %d%% yes / %d%% no", CityYes, CityNo);
        break;
    case 8: sprintf(text, "Score: %d/1000 (%+d)", CityScore, deltaCityScore); break;
    case 9: sprintf(text, "Assessed value: $%ld", (long)GetCityAssessedValue()); break;
    case 11: strcpy(text, "TOP PROBLEMS (survey votes)"); break;
    case 12: case 13: case 14: case 15:
        rank = row-12; problem = ProblemOrder[rank];
        /* 7 is the engine's no-problem sentinel; never index with it. */
        votes = problem >= 0 && problem < 7 ? GetProblemVotes(problem) : 0;
        if (!EvalValid || votes <= 0) sprintf(text, "%d. None reported", rank+1);
        else sprintf(text, "%d. %.40s: %d votes", rank+1, GetProblemText(problem), votes);
        break;
    case 17: sprintf(text, "Tax: %d%% / auto budget %s", CityTax, AutoBudget ? "on" : "off"); break;
    case 18: sprintf(text, "Funding road/fire/police: %d/%d/%d%%", (int)(RoadPercent*100),
                       (int)(FirePercent*100), (int)(PolicePercent*100)); break;
    case 19: sprintf(text, "Power: %d zones on / %d off", PwrdZCnt, unPwrdZCnt); break;
    case 20: sprintf(text, "Crime %d / pollution %d / traffic %d", CrimeAverage, PolluteAverage, TrafficAverage); break;
    case 22: strcpy(text, "Score and population changes are since"); break;
    case 23: strcpy(text, "the previous city evaluation."); break;
    }
}
#ifdef MP_SELFTEST
void mp_test_info_line(int row, char text[160]) { info_line(row, text); }
#endif

#ifndef MP_PORT_TEST
/* The original NT front end has independent Log and Info windows.  Keep
 * these as distinct PM clients: each gets its own paint HPS and therefore
 * cannot interfere with the city double buffer. */
static void paint_text_line(HPS hps, LONG x, LONG y, const char *text)
{
    POINTL p;
    p.x = x;
    p.y = y;
    GpiCharStringAt(hps, &p, (LONG)strlen(text), (PSZ)text);
}

static const char *month_name(int month)
{
    static const char *names[12] = {
        "January", "February", "March", "April", "May", "June",
        "July", "August", "September", "October", "November", "December"
    };
    if (month < 0 || month > 11) return "Unknown";
    return names[month];
}

static void paint_log_window(HWND hwnd, HPS hps)
{
    RECTL r;
    const char *p;
    const char *line;
    char text[256];
    int lines;
    int n;
    LONG y;

    WinQueryWindowRect(hwnd, &r);
    WinFillRect(hps, &r, CLR_WHITE);
    GpiSetColor(hps, CLR_BLACK);
    paint_text_line(hps, 8, r.yTop - 18, "WinTown Log");

    /* Start at a recent complete line so a busy simulation does not push
     * useful messages below the bottom of the small log window. */
    p = logText;
    lines = 0;
    while (*p) { if (*p++ == '\n') ++lines; }
    p = logText;
    while (lines > 14 && *p) {
        if (*p++ == '\n') --lines;
    }
    y = r.yTop - 36;
    while (*p && y > 6) {
        line = p;
        n = 0;
        while (*p && *p != '\r' && *p != '\n' && n < (int)sizeof(text) - 1) {
            text[n++] = *p++;
        }
        text[n] = 0;
        if (n) paint_text_line(hps, 8, y, text);
        while (*p == '\r' || *p == '\n') ++p;
        (void)line;
        y -= 16;
    }
}

static int info_visible_rows(void)
{
    RECTL r;
    int rows;
    WinQueryWindowRect(hwndInfo, &r);
    rows = ((int)r.yTop-36)/16;
    return rows > 0 ? rows : 1;
}

static void update_info_scroll(void)
{
    int limit, pass;
    if (!hwndInfo || !hwndInfoScroll || infoUpdating) return;
    infoUpdating = 1;
    /* Some hosts change the client size when hiding a zero-range bar. */
    for (pass = 0; pass < 3; ++pass) {
        limit = pm_scroll_limit(INFO_ROWS, info_visible_rows());
        infoTop = pm_scroll_clamp(infoTop, limit);
        WinSendMsg(hwndInfoScroll, SBM_SETSCROLLBAR,
                   MPFROMSHORT(infoTop), MPFROM2SHORT(0, limit));
    }
    infoUpdating = 0;
}

static void scroll_info(int command, int thumb)
{
    int action, rows, limit;
    switch (command) {
    case SB_LINEUP: action = MP_SCROLL_BACK; break;
    case SB_LINEDOWN: action = MP_SCROLL_FORWARD; break;
    case SB_PAGEUP: action = MP_SCROLL_PAGE_BACK; break;
    case SB_PAGEDOWN: action = MP_SCROLL_PAGE_FORWARD; break;
    case SB_SLIDERPOSITION: case SB_SLIDERTRACK: action = MP_SCROLL_THUMB; break;
    default: return;
    }
    rows = info_visible_rows(); limit = pm_scroll_limit(INFO_ROWS, rows);
    infoTop = pm_scroll_position(infoTop, rows, limit, action, thumb, 1);
    update_info_scroll(); WinInvalidateRect(hwndInfo, NULL, FALSE);
}

static void paint_info_window(HWND hwnd, HPS hps)
{
    RECTL r;
    char text[160];
    LONG y;
    int row;
    WinQueryWindowRect(hwnd, &r);
    WinFillRect(hps, &r, CLR_WHITE);
    GpiSetColor(hps, CLR_BLACK);
    paint_text_line(hps, 8, r.yTop-18, "WinTown City Report");
    y = r.yTop-38;
    for (row = infoTop; row < INFO_ROWS && y >= 6; ++row, y -= 16) {
        info_line(row, text); paint_text_line(hps, 8, y, text);
    }
}

#endif

static void clamp_view(void)
{
    xOffset = pm_scroll_clamp(xOffset,
        pm_scroll_limit(WORLD_X * TILE_SIZE, clientWidth - toolbarWidth));
    yOffset = pm_scroll_clamp(yOffset,
        pm_scroll_limit(WORLD_Y * TILE_SIZE, pm_map_height(clientHeight)));
}

#ifndef MP_PORT_TEST
static void update_scrollbars(void)
{
    int oldWidth, oldHeight, pass;
    if (updatingScrollbars || !hwndHScroll || !hwndVScroll) return;
    updatingScrollbars = 1;
    /* A host may hide a zero-range bar and synchronously send WM_SIZE.
     * Recompute using the resulting client size, without recursive updates. */
    for (pass = 0; pass < 3; ++pass) {
        oldWidth = clientWidth; oldHeight = clientHeight;
        clamp_view();
        WinSendMsg(hwndHScroll, SBM_SETSCROLLBAR, MPFROMSHORT(xOffset),
            MPFROM2SHORT(0, pm_scroll_limit(WORLD_X * TILE_SIZE,
                                           clientWidth - toolbarWidth)));
        WinSendMsg(hwndVScroll, SBM_SETSCROLLBAR, MPFROMSHORT(yOffset),
            MPFROM2SHORT(0, pm_scroll_limit(WORLD_Y * TILE_SIZE, pm_map_height(clientHeight))));
        if (oldWidth == clientWidth && oldHeight == clientHeight) break;
    }
    /* Use the standard fixed-size thumb. PM SETTHUMBSIZE is a ratio;
     * older PMWIN hosts interpret it as a Win32 page and shorten the range. */
    updatingScrollbars = 0;
}

static void scroll_view(int vertical, USHORT command, int thumb)
{
    int action, visible, limit, position;
    switch (command) {
    case SB_LINEUP: action = MP_SCROLL_BACK; break;
    case SB_LINEDOWN: action = MP_SCROLL_FORWARD; break;
    case SB_PAGEUP: action = MP_SCROLL_PAGE_BACK; break;
    case SB_PAGEDOWN: action = MP_SCROLL_PAGE_FORWARD; break;
    case SB_SLIDERTRACK:
    case SB_SLIDERPOSITION: action = MP_SCROLL_THUMB; break;
    default: return;
    }
    visible = vertical ? pm_map_height(clientHeight) : clientWidth - toolbarWidth;
    limit = pm_scroll_limit((vertical ? WORLD_Y : WORLD_X) * TILE_SIZE, visible);
    position = pm_scroll_position(vertical ? yOffset : xOffset,
                                  visible, limit, action, thumb, TILE_SIZE);
    if (vertical) pan(0, position - yOffset);
    else pan(position - xOffset, 0);
}
#endif

static void pan(int dx, int dy)
{
    int oldX, oldY;
    oldX = xOffset; oldY = yOffset;
#ifndef MP_PORT_TEST
    stop_build();
#endif
    xOffset += dx; yOffset += dy;
    clamp_view();
    /* A previously hovered tile is no longer under a stationary pointer. */
    if (oldX != xOffset || oldY != yOffset) hoverMapX = hoverMapY = -1;
#ifndef MP_PORT_TEST
    update_scrollbars();
#endif
    mp_ui_invalidate();
}

static Notification lastAlert;
static int haveAlert;
static unsigned char alertSeen[44];
static int alertPopupCount;
void mp_pm_clear_notification(void)
{
    memset(&lastAlert, 0, sizeof(lastAlert));
    haveAlert = 0;
    memset(alertSeen, 0, sizeof(alertSeen));
    alertPopupCount = 0;
}

#ifndef MP_PORT_TEST
static MRESULT EXPENTRY alert_proc(HWND hwnd, USHORT msg, MPARAM mp1, MPARAM mp2)
{
    char location[80];
    switch (msg) {
    case WM_INITDLG:
        WinSetWindowText(hwnd, lastAlert.title);
        WinSetDlgItemText(hwnd, ID_ALERT_MESSAGE, lastAlert.message);
        WinSetDlgItemText(hwnd, ID_ALERT_DETAIL, lastAlert.explanation);
        WinSetDlgItemText(hwnd, ID_ALERT_ADVICE, lastAlert.advice);
        if (lastAlert.hasLocation)
            sprintf(location, "Map location: %d, %d", lastAlert.locationX, lastAlert.locationY);
        else strcpy(location, "No specific map location reported.");
        WinSetDlgItemText(hwnd, ID_ALERT_LOCATION, location);
        WinEnableWindow(WinWindowFromID(hwnd, ID_ALERT_GOTO), lastAlert.hasLocation);
        return 0;
    case WM_COMMAND:
        if (SHORT1FROMMP(mp1) == ID_ALERT_GOTO && lastAlert.hasLocation) {
            CenterMapOnLocation(lastAlert.locationX, lastAlert.locationY);
            WinDismissDlg(hwnd, DID_OK); return 0;
        }
        if (SHORT1FROMMP(mp1) == DID_OK || SHORT1FROMMP(mp1) == DID_CANCEL) {
            WinDismissDlg(hwnd, DID_OK); return 0;
        }
        break;
    }
    return WinDefDlgProc(hwnd, msg, mp1, mp2);
}

static void show_last_alert(void)
{
    int previousModal;
    if (!haveAlert) {
        mp_ui_message(hwndMain, "No emergency has been reported for this city yet.", MP_TITLE);
        return;
    }
    stop_build();
    previousModal = modalActive;
    modalActive = 1;
    WinDlgBox(HWND_DESKTOP, hwndMain, alert_proc, (HMODULE)0, ID_ALERT_DIALOG, NULL);
    modalActive = previousModal;
}
#endif

void mp_pm_show_notification(Notification *n)
{
    int popup;
    if (!n) return;
    lastAlert = *n;
    lastAlert.title[sizeof(lastAlert.title)-1] = 0;
    lastAlert.message[sizeof(lastAlert.message)-1] = 0;
    lastAlert.explanation[sizeof(lastAlert.explanation)-1] = 0;
    lastAlert.advice[sizeof(lastAlert.advice)-1] = 0;
    lastAlert.hasLocation = n->hasLocation && n->locationX >= 0 &&
        n->locationX < WORLD_X && n->locationY >= 0 && n->locationY < WORLD_Y;
    if (!lastAlert.explanation[0]) {
        switch (n->id) {
        case 20: strcpy(lastAlert.explanation, "Fire has broken out in the city."); break;
        case 21: strcpy(lastAlert.explanation, "A monster is damaging the city."); break;
        case 22: strcpy(lastAlert.explanation, "A tornado is crossing the city."); break;
        case 23: strcpy(lastAlert.explanation, "An earthquake has damaged buildings and infrastructure."); break;
        case 24: strcpy(lastAlert.explanation, "An aircraft has crashed. Check the surrounding area for fires."); break;
        case 25: strcpy(lastAlert.explanation, "A ship has been wrecked."); break;
        case 30: strcpy(lastAlert.explanation, "Firebombing has started fires in the city."); break;
        case 42: strcpy(lastAlert.explanation, "Flooding is damaging the city."); break;
        case 43: strcpy(lastAlert.explanation, "A nuclear plant has suffered a meltdown. The area may be contaminated."); break;
        default: strcpy(lastAlert.explanation, "Review the affected area and city services."); break;
        }
    }
    if (!lastAlert.advice[0]) {
        if (n->id == 20 || n->id == 24 || n->id == 30)
            strcpy(lastAlert.advice, "Check fire station coverage and funding. Restore damaged roads and power lines.");
        else if (n->id == 43)
            strcpy(lastAlert.advice, "Inspect the affected area and restore power from another plant. Avoid contaminated tiles.");
        else strcpy(lastAlert.advice, "Inspect the damage. Restore roads and power, then rebuild when the danger has passed.");
    }
    haveAlert = 1;
    popup = n->id < 0 || n->id >= 44 || !alertSeen[n->id];
    if (n->id >= 0 && n->id < 44) alertSeen[n->id] = 1;
    if (popup) {
        ++alertPopupCount;
        pm_sound_play(PM_SOUND_ALERT);
#ifndef MP_PORT_TEST
        show_last_alert();
#endif
    }

}

#if defined(MP_PORT_TEST) && defined(MP_SELFTEST)
int mp_test_alert_popups(void) { return alertPopupCount; }
const Notification *mp_test_last_alert(void) { return haveAlert ? &lastAlert : NULL; }
#endif

#ifndef MP_PORT_TEST
static char pathText[MAX_PATH] = "city.scn";
/* Directory/file lists follow Microsoft OPENDLG FILE.C/FILE1.C.
 * Keep an explicit browsing path: never change the game's working directory,
 * and never open/create files merely to select a name. */
static char browseDir[MAX_PATH];
static int browseSave;

static int browse_fill(HWND hwnd, const char *directory)
{
    FILESTATUS status;
    FILEFINDBUF found;
    HDIR search;
    ULONG count, rc, drive, drives;
    int i, entries, inserted;
    char summary[140];
    char pattern[MAX_PATH], driveName[4];
    memset(&status, 0, sizeof(status));
    rc = DosQueryPathInfo((PSZ)directory, FIL_STANDARD, &status, sizeof(status));
    pm_trace("BROWSE-R20 path=%s query=%lu attr=%04x status-size=%u find-size=%u name-offset=%u",
        directory, rc, (unsigned int)status.attrFile, (unsigned int)sizeof(status),
        (unsigned int)sizeof(found), (unsigned int)((char *)found.achName-(char *)&found));
    pm_trace("BROWSE-R20 controls dirs=%08lx files=%08lx",
        (ULONG)WinWindowFromID(hwnd, ID_PATH_DIRS), (ULONG)WinWindowFromID(hwnd, ID_PATH_FILES));
    if (strlen(directory) > MAX_PATH - 5 || rc || !(status.attrFile & FILE_DIRECTORY)) {
        WinSetDlgItemText(hwnd, ID_PATH_STATUS, "Cannot read that directory.");
        return 0;
    }
    strcpy(pattern, directory);
    if (pattern[strlen(pattern)-1] != '\\') strcat(pattern, "\\");
    strcat(pattern, "*.*");
    search = (HDIR)0xffff; count = 1;
    memset(&found, 0, sizeof(found));
    entries = 0;
    rc = DosFindFirst(pattern, &search, FILE_DIRECTORY | FILE_READONLY |
                      FILE_HIDDEN | FILE_SYSTEM | FILE_ARCHIVED,
                      &found, sizeof(found), &count, FIL_STANDARD);
    pm_trace("BROWSE-R20 find pattern=%s rc=%lu count=%lu", pattern, rc, count);
    if (rc && rc != 18 && rc != 2) {
        WinSetDlgItemText(hwnd, ID_PATH_STATUS, "Directory listing failed; try another folder.");
        return 0;
    }
    if (directory != browseDir) strcpy(browseDir, directory);
    WinSetDlgItemText(hwnd, ID_PATH_FOLDER, browseDir);
    WinSendDlgItemMsg(hwnd, ID_PATH_DIRS, LM_DELETEALL, 0, 0);
    WinSendDlgItemMsg(hwnd, ID_PATH_FILES, LM_DELETEALL, 0, 0);
    if (!rc) {
        do {
            found.achName[sizeof(found.achName)-1] = 0;
            if (strcmp((char *)found.achName, ".") && strcmp((char *)found.achName, "..")) {
                inserted = (SHORT)WinSendDlgItemMsg(hwnd,
                    (found.attrFile & FILE_DIRECTORY) ? ID_PATH_DIRS : ID_PATH_FILES,
                    LM_INSERTITEM, MPFROMSHORT(LIT_SORTASCENDING), MPFROMP(found.achName));
                if (entries < 12)
                    pm_trace("BROWSE-R20 entry name=%s attr=%04x len=%u insert=%d",
                        found.achName, (unsigned int)found.attrFile,
                        (unsigned int)found.cchName, inserted);
                ++entries;
            }
            memset(&found, 0, sizeof(found));
            count = 1;
            rc = DosFindNext(search, &found, sizeof(found), &count);
        } while (!rc && count);
        DosFindClose(search);
    }
    if (!DosQueryCurrentDisk(&drive, &drives)) {
        for (i = 0; i < 26; ++i) if (drives & (1UL << i)) {
            driveName[0] = (char)('A' + i); driveName[1] = ':';
            driveName[2] = '\\'; driveName[3] = 0;
            WinSendDlgItemMsg(hwnd, ID_PATH_DIRS, LM_INSERTITEM,
                              MPFROMSHORT(LIT_END), MPFROMP(driveName));
        }
    }
    sprintf(summary, "Folders/drives: %d  Files: %d. Double-click to choose.",
        (SHORT)WinSendDlgItemMsg(hwnd, ID_PATH_DIRS, LM_QUERYITEMCOUNT, 0, 0),
        (SHORT)WinSendDlgItemMsg(hwnd, ID_PATH_FILES, LM_QUERYITEMCOUNT, 0, 0));
    pm_trace("BROWSE-R20 end entries=%d rc=%lu %s", entries, rc, summary);
    WinSetDlgItemText(hwnd, ID_PATH_STATUS, summary);
    return 1;
}

static MRESULT EXPENTRY path_proc(HWND hwnd, USHORT msg, MPARAM mp1, MPARAM mp2)
{
    char text[MAX_PATH], full[MAX_PATH], current[MAX_PATH];
    ULONG drive, drives, size, diskRc, dirRc;
    SHORT item;
    USHORT id;
    FILESTATUS status;
    switch (msg) {
    case WM_INITDLG:
        WinSetWindowText(hwnd, browseSave ? "Save city as" : "Open city");
        WinSetDlgItemText(hwnd, DID_OK, browseSave ? "Save" : "Open");
        WinSendDlgItemMsg(hwnd, ID_PATH_TEXT, EM_SETTEXTLIMIT, MPFROMSHORT(MAX_PATH-1), 0);
        WinSendDlgItemMsg(hwnd, ID_PATH_FOLDER, EM_SETTEXTLIMIT, MPFROMSHORT(MAX_PATH-1), 0);
        WinSetDlgItemText(hwnd, ID_PATH_TEXT, pathText);
        if (!browseDir[0]) {
            size = sizeof(current);
            current[0] = 0; drive = drives = 0;
            diskRc = DosQueryCurrentDisk(&drive, &drives);
            dirRc = diskRc ? diskRc : DosQueryCurrentDir(drive, (PBYTE)current, &size);
            current[sizeof(current)-1] = 0;
            pm_trace("BROWSE-R20 cwd disk-rc=%lu drive=%lu dir-rc=%lu path=%s",
                     diskRc, drive, dirRc, current);
            if (!diskRc && !dirRc && strlen(current) < MAX_PATH-3)
                sprintf(browseDir, "%c:\\%s", (int)('A'+drive-1), current);
        }
        if (browseDir[0]) browse_fill(hwnd, browseDir);
        else WinSetDlgItemText(hwnd, ID_PATH_STATUS, "Enter a full folder path, then click Go.");
        return 0;
    case WM_CONTROL:
        id = SHORT1FROMMP(mp1);
        if ((id == ID_PATH_FILES || id == ID_PATH_DIRS) &&
            (SHORT2FROMMP(mp1) == LN_SELECT || SHORT2FROMMP(mp1) == LN_ENTER)) {
            item = (SHORT)WinSendDlgItemMsg(hwnd, id, LM_QUERYSELECTION, MPFROMSHORT(LIT_FIRST), 0);
            if (item < 0) return 0;
            text[0] = 0;
            WinSendDlgItemMsg(hwnd, id, LM_QUERYITEMTEXT,
                              MPFROM2SHORT(item, sizeof(text)), MPFROMP(text));
            if (id == ID_PATH_FILES) {
                WinSetDlgItemText(hwnd, ID_PATH_TEXT, text);
                if (SHORT2FROMMP(mp1) == LN_ENTER)
                    WinSendMsg(hwnd, WM_COMMAND, MPFROMSHORT(DID_OK), 0);
            } else if (SHORT2FROMMP(mp1) == LN_ENTER &&
                       pm_path_join(full, sizeof(full), browseDir, text)) browse_fill(hwnd, full);
            return 0;
        }
        break;
    case WM_COMMAND:
        id = SHORT1FROMMP(mp1);
        if (id == ID_PATH_UP) {
            strcpy(full, browseDir); pm_path_parent(full);
            if (full[0]) browse_fill(hwnd, full);
            return 0;
        }
        if (id == ID_PATH_GO) {
            WinQueryDlgItemText(hwnd, ID_PATH_FOLDER, sizeof(text), text);
            if (pm_path_join(full, sizeof(full), browseDir, text)) browse_fill(hwnd, full);
            else WinSetDlgItemText(hwnd, ID_PATH_STATUS, "Use a full folder path, such as C:\\cities.");
            return 0;
        }
        if (id == DID_OK) {
            WinQueryDlgItemText(hwnd, ID_PATH_TEXT, sizeof(text), text);
            if (!pm_path_join(full, sizeof(full), browseDir, text)) {
                WinSetDlgItemText(hwnd, ID_PATH_STATUS, "Enter a filename or full path; no wildcards."); return 0;
            }
            if (!DosQueryPathInfo(full, FIL_STANDARD, &status, sizeof(status))) {
                if (status.attrFile & FILE_DIRECTORY) { browse_fill(hwnd, full); return 0; }
            } else if (!browseSave) {
                WinSetDlgItemText(hwnd, ID_PATH_STATUS, "File not found. Choose an existing city file."); return 0;
            }
            strcpy(pathText, full);
            WinDismissDlg(hwnd, DID_OK); return 0;
        }
        if (id == DID_CANCEL) { WinDismissDlg(hwnd, DID_CANCEL); return 0; }
        break;
    }
    return WinDefDlgProc(hwnd, msg, mp1, mp2);
}

static const char *tilesetFiles[10] = {
    "default.bmp", "classic.bmp", "classic95.bmp", "ancientasia.bmp",
    "dux.bmp", "futureeurope.bmp", "futureusa.bmp", "medievaltimes.bmp",
    "mooncolony.bmp", "wildwest.bmp"
};
static char activeTileset[MAX_PATH] = "tilesets\\default.bmp";
static char pendingTileset[MAX_PATH];
static int activeTilesetIndex = 0;

static void tileset_menu_sync(void)
{
    HWND menu;
    int i;
    menu = WinWindowFromID(frameMain, FID_MENU);
    if (!menu) return;
    for (i = 0; i < 10; ++i)
        WinCheckMenuItem(menu, IDM_TILESET_FIRST + i, activeTilesetIndex == i);
    WinCheckMenuItem(menu, IDM_TILESET_CUSTOM, activeTilesetIndex == -1);
}

/* Prepare and select the replacement before releasing the old bitmap. */
static int select_tileset(const char *path, int index)
{
    MPBITMAP *raw;
    HBITMAP replacement, previous;
    raw = mp_bmp_load(path);
    if (!raw || mp_bmp_width(raw) != 512 || mp_bmp_height(raw) != 480) {
        pm_trace("tileset rejected: %s (expected 512x480 BMP)", path);
        mp_bmp_free(raw);
        mp_ui_message(hwndMain, "Cannot load that tileset. Choose a 512x480 Windows BMP using the WinTown tile layout. Current graphics are unchanged.", MP_TITLE);
        return 0;
    }
    replacement = mp_bmp_create_gpi(raw, hpsMemory);
    mp_bmp_free(raw);
    if (!replacement || replacement == HBM_ERROR) {
        pm_trace("tileset bitmap creation failed: %s", path);
        mp_ui_message(hwndMain, "Cannot create the tileset bitmap. Current graphics are unchanged.", MP_TITLE);
        return 0;
    }
    previous = GpiSetBitmap(hpsMemory, replacement);
    if (previous == HBM_ERROR) {
        GpiDeleteBitmap(replacement);
        pm_trace("tileset selection failed: %s", path);
        mp_ui_message(hwndMain, "Cannot select the tileset bitmap. Current graphics are unchanged.", MP_TITLE);
        return 0;
    }
    if (hbmTiles) GpiDeleteBitmap(hbmTiles);
    hbmTiles = replacement;
    strcpy(activeTileset, path);
    activeTilesetIndex = index;
    tileset_menu_sync();
    if (hwndTiles) WinInvalidateRect(hwndTiles, NULL, FALSE);
    hoverMapX = hoverMapY = -1;
    mp_ui_invalidate();
    addGameLog("Tileset: %s", activeTileset);
    pm_trace("tileset switched: %s (512x480)", activeTileset);
    return 1;
}

static MRESULT EXPENTRY tileset_proc(HWND hwnd, USHORT msg, MPARAM mp1, MPARAM mp2)
{
    switch (msg) {
    case WM_INITDLG:
        WinSetDlgItemText(hwnd, ID_TILESET_PATH, pendingTileset);
        WinSendDlgItemMsg(hwnd, ID_TILESET_PATH, EM_SETTEXTLIMIT,
                          MPFROMSHORT(MAX_PATH - 1), 0);
        return 0;
    case WM_COMMAND:
        if (SHORT1FROMMP(mp1) == DID_OK) {
            WinQueryDlgItemText(hwnd, ID_TILESET_PATH, sizeof(pendingTileset), pendingTileset);
            if (pendingTileset[0] && select_tileset(pendingTileset, -1))
                WinDismissDlg(hwnd, DID_OK);
            return 0;
        }
        if (SHORT1FROMMP(mp1) == DID_CANCEL) {
            WinDismissDlg(hwnd, DID_CANCEL); return 0;
        }
        break;
    }
    return WinDefDlgProc(hwnd, msg, mp1, mp2);
}

static void choose_tileset(int index)
{
    char path[MAX_PATH];
    stop_build();
    modalActive = 1;
    if (index >= 0 && index < 10) {
        sprintf(path, "tilesets\\%s", tilesetFiles[index]);
        select_tileset(path, index);
    } else {
        strcpy(pendingTileset, activeTileset);
        WinDlgBox(HWND_DESKTOP, hwndMain, tileset_proc, (HMODULE)0,
                  ID_TILESET_DIALOG, NULL);
    }
    modalActive = 0;
}

/* Dialog edits pending choices only; Cancel never touches simulation state. */
static int newDifficulty, newTerrain;
static char newName[64];
static const char *difficultyNames[3] = {
    "Easy ($50,000)", "Medium ($20,000)", "Hard ($10,000)"
};
static const char *terrainNames[4] = {
    "Random", "Flat / clear", "Wooded rivers", "Island"
};
static const char *terrainDescriptions[4] = {
    "Random trees, lakes and rivers; possible island.",
    "Clear, level land without water or trees.",
    "Dense trees, lakes and winding rivers.",
    "Water around the edges, trees and inland water."
};
static void new_city_labels(HWND hwnd)
{
    WinSetDlgItemText(hwnd, ID_NEW_DIFFICULTY, (PSZ)difficultyNames[newDifficulty]);
    WinSetDlgItemText(hwnd, ID_NEW_TERRAIN, (PSZ)terrainNames[newTerrain]);
    WinSetDlgItemText(hwnd, ID_NEW_DESCRIPTION, (PSZ)terrainDescriptions[newTerrain]);
}
static MRESULT EXPENTRY new_city_proc(HWND hwnd, USHORT msg, MPARAM mp1, MPARAM mp2)
{
    int i, nonspace;
    switch (msg) {
    case WM_INITDLG:
        WinSetDlgItemText(hwnd, ID_NEW_NAME, newName);
        WinSendDlgItemMsg(hwnd, ID_NEW_NAME, EM_SETTEXTLIMIT, MPFROMSHORT(63), 0);
        new_city_labels(hwnd); return 0;
    case WM_COMMAND:
        switch (SHORT1FROMMP(mp1)) {
        case ID_NEW_DIFFICULTY:
            newDifficulty = (newDifficulty+1)%3; new_city_labels(hwnd); return 0;
        case ID_NEW_TERRAIN:
            newTerrain = (newTerrain+1)%4; new_city_labels(hwnd); return 0;
        case DID_OK:
            WinQueryDlgItemText(hwnd, ID_NEW_NAME, sizeof(newName), newName);
            newName[sizeof(newName)-1] = 0;
            nonspace = 0;
            for (i = 0; newName[i]; ++i)
                if (newName[i] != ' ' && newName[i] != '\t') nonspace = 1;
            if (!nonspace) {
                mp_ui_message(hwnd, "Enter a city name.", MP_TITLE); return 0;
            }
            WinDismissDlg(hwnd, DID_OK); return 0;
        case DID_CANCEL: WinDismissDlg(hwnd, DID_CANCEL); return 0;
        }
        break;
    }
    return WinDefDlgProc(hwnd, msg, mp1, mp2);
}
static void new_city_dialog(void)
{
    ULONG result;
    newDifficulty = 0; newTerrain = 0;
    strcpy(newName, "New City");
    modalActive = 1;
    result = WinDlgBox(HWND_DESKTOP, hwndMain, new_city_proc, (HMODULE)0,
                      ID_NEW_DIALOG, NULL);
    if (result == DID_OK) new_city_options(newName, newDifficulty, newTerrain);
    modalActive = 0;
}

static void city_file_dialog(int save)
{
    FILE *fp;
    int ok;
    USHORT answer;
    ULONG style;
    browseSave = save;
    modalActive = 1;
    if (WinDlgBox(HWND_DESKTOP, hwndMain, path_proc, (HMODULE)0,
                  ID_PATH_DIALOG, NULL) == DID_OK) {
        ok = 1;
        if (save) {
            fp = fopen(pathText, "rb");
            if (fp) {
                fclose(fp);
                style = MB_YESNO | MB_QUERY | MB_MOVEABLE;
                pm_trace("SAVE-R11 overwrite path=%s style=%08lx YES=%u NO=%u", pathText,
                         (unsigned long)style, (unsigned)MBID_YES, (unsigned)MBID_NO);
                answer = WinMessageBox(HWND_DESKTOP, hwndMain, "Replace existing city file?",
                    MP_TITLE, 0, style);
                pm_trace("SAVE-R11 confirmation result=%u", (unsigned)answer);
                ok = answer == MBID_YES;
                if (!ok && answer != MBID_NO && answer != MBID_CANCEL)
                    pm_trace("SAVE-R11 unexpected result; overwrite NOT performed");
            }
        }
        if (!ok) {
            pm_trace("SAVE-R11 cancelled; unchanged: %s", pathText);
            addGameLog("Save cancelled; existing file unchanged: %s", pathText);
        } else if (!(save ? saveFile(pathText) : loadFile(pathText))) {
            pm_trace("SAVE-R11 %s failed: %s", save ? "save" : "load", pathText);
            addGameLog("%s failed: %s", save ? "Save" : "Load", pathText);
            mp_ui_message(hwndMain, "City file operation failed. Check the path and permissions.", MP_TITLE);
        } else if (save) {
            pm_trace("SAVE-R11 saved: %s", pathText);
            addGameLog("Saved city: %s", pathText);
        }
        mp_ui_invalidate();
    }
    modalActive = 0;
}

/* View settings survive city changes; the recorded history does not. */
static int chartMask = 14;
static int chartRange = 0;
static const char *chartLabels[15] = {
    "Population (x100 people)", "Residential (units)", "Commercial (units)",
    "Industrial (units)", "Funds ($1000, 0..255)", "Crime (index)",
    "Pollution (index)", "Land value (index)", "Transport (x10 tiles)",
    "Power (%)", "Growth (%)", "Approval (%)",
    "R demand (index)", "C demand (index)", "I demand (index)"
};
static LONG chartColors[15] = {
    CLR_BLACK, CLR_DARKGREEN, CLR_DARKBLUE, CLR_RED, CLR_BROWN,
    CLR_DARKRED, CLR_DARKPINK, CLR_DARKCYAN, CLR_BLUE, CLR_PINK,
    CLR_GREEN, CLR_CYAN, CLR_DARKGREEN, CLR_DARKBLUE, CLR_RED
};

static void chart_menu_sync(void)
{
    HWND menu;
    int i;
    menu = WinWindowFromID(frameChart, FID_MENU);
    if (!menu) return;
    for (i = 0; i < CHART_SERIES_COUNT; ++i)
        WinCheckMenuItem(menu, IDM_CHART_SERIES_FIRST + i,
                         (chartMask & (1 << i)) != 0);
    WinCheckMenuItem(menu, IDM_CHART_MONTHLY, chartRange == 0);
    WinCheckMenuItem(menu, IDM_CHART_YEARLY, chartRange == 1);
}

static long chart_value(int series, int index)
{
    long value;
    if (chartRange == 0)
        value = g_chartData->series[series].monthlyData[index];
    else value = g_chartData->series[series].yearlyData[120 + index];
    if (series == CHART_GROWTH_RATE) value -= 128;
    if (series >= CHART_R_DEMAND) value = value * 16 - 2000;
    return value;
}

static void paint_chart_window(HWND hwnd, HPS hps)
{
    RECTL r;
    POINTL p;
    int series, i, n, selected, entry, rows;
    LONG bottom, top, left, right;
    long value, minimum, maximum;
    char text[120];
    WinQueryWindowRect(hwnd, &r);
    WinFillRect(hps, &r, CLR_WHITE);
    GpiSetColor(hps, CLR_BLACK);
    paint_text_line(hps, 8, r.yTop - 18, chartRange == 0 ?
        "10 years / monthly samples (session history)" :
        "120 years / annual snapshots (session history)");
    if (!g_chartData || r.yTop < 100 || r.xRight < 200) return;
    n = chartRange == 0 ? g_chartData->monthlyCount : g_chartData->yearlyCount;
    selected = 0; minimum = 0; maximum = 1;
    for (series = 0; series < 15; ++series) if (chartMask & (1 << series)) {
        ++selected;
        for (i = 0; i < n; ++i) {
            value = chart_value(series, i);
            if (value < minimum) minimum = value;
            if (value > maximum) maximum = value;
        }
    }
    rows = (selected + 2) / 3;
    bottom = 36 + rows * 18; top = r.yTop - 62;
    left = 64; right = r.xRight - 12;
    paint_text_line(hps, 8, r.yTop - 36, "Shared scale; units in legend. Series / Range menus select data.");
    if (top <= bottom) {
        paint_text_line(hps, 8, r.yTop - 54, "Enlarge this window to display the chart.");
        return;
    }
    sprintf(text, "%ld", maximum); paint_text_line(hps, 4, top - 4, text);
    sprintf(text, "%ld", minimum); paint_text_line(hps, 4, bottom - 4, text);
    p.x = left; p.y = top; GpiMove(hps, &p);
    p.y = bottom; GpiLine(hps, &p); p.x = right; GpiLine(hps, &p);
    p.x = left;
    p.y = bottom + (LONG)((double)(-minimum) * (top-bottom) / (maximum-minimum));
    GpiMove(hps, &p); p.x = right; GpiLine(hps, &p);
    paint_text_line(hps, left, bottom - 16, chartRange == 0 ? "119 months ago" : "119 years ago");
    paint_text_line(hps, right - 48, bottom - 16, "Latest");
    if (!selected || !n)
        paint_text_line(hps, left + 12, top - 24, !selected ?
            "Select a series from the Series menu." : "Waiting for history samples...");
    entry = 0;
    for (series = 0; series < 15; ++series) if (chartMask & (1 << series)) {
        GpiSetColor(hps, chartColors[series]);
        paint_text_line(hps, 8 + (entry % 3) * (r.xRight / 3),
                        10 + (rows - 1 - entry / 3) * 18, chartLabels[series]);
        ++entry;
        for (i = n - 1; i >= 0; --i) {
            value = chart_value(series, i);
            p.x = left + (LONG)(119 - i) * (right-left) / 119;
            p.y = bottom + (LONG)((double)(value-minimum) * (top-bottom) / (maximum-minimum));
            if (i == n - 1) { GpiMove(hps, &p); GpiSetPel(hps, &p); }
            else GpiLine(hps, &p);
        }
    }
}

/* True-color overview avoids logical palette differences in PM hosts. */
static const char *miniNames[13] = {
    "All zones", "Residential", "Commercial", "Industrial", "Power",
    "Transport", "Population", "Traffic", "Pollution", "Crime",
    "Land value", "Fire coverage", "Police coverage"
};
static const char *miniLegends[13] = {
    "R green / C blue / I yellow / water blue",
    "Brighter green = higher density", "Brighter blue = higher density",
    "Brighter yellow = higher density", "Yellow powered / dark red unpowered",
    "Road white / rail gray", "Brighter magenta = higher population",
    "Green low / yellow medium / red high", "Green low / yellow medium / red high",
    "Yellow low / red high", "Pale yellow low / green high",
    "Pale red weak / red strong", "Pale blue weak / blue strong"
};

static LONG mini_color(int x, int y)
{
    int raw, tile, value;
    raw = (unsigned short)Map[x][y]; tile = raw & LOMASK;
    value = 0;
    switch (miniMode) {
    case 1: if (tile >= RESBASE && tile < HOSPITAL) value = RZPop(tile) * 25; break;
    case 2: if (tile >= COMBASE && tile < INDBASE) value = CZPop(tile) * 25; break;
    case 3: if (tile >= INDBASE && tile < PORTBASE) value = IZPop(tile) * 25; break;
    case 4: return (raw & POWERBIT) ? 0xffff00L : (raw & CONDBIT) ? 0x800000L : 0L;
    case 5:
        if (tile >= ROADBASE && tile <= LASTROAD) return 0xffffffL;
        return tile >= RAILBASE && tile <= LASTRAIL ? 0xc0c0c0L : 0L;
    case 6: value = PopDensity[x/2][y/2] * 2; break;
    case 7: value = TrfDensity[x/2][y/2];
        return value >= 120 ? 0xff0000L : value >= 80 ? 0xff8000L :
            value >= 40 ? 0xffff00L : value >= 20 ? 0x80ff00L : value > 0 ? 0x00ff80L : 0L;
    case 8: value = PollutionMem[x/2][y/2];
        return value >= 200 ? 0xff0000L : value >= 150 ? 0xff8000L :
            value >= 100 ? 0xffff00L : value >= 50 ? 0x80ff00L : value > 0 ? 0x00ff00L : 0L;
    case 9: value = CrimeMem[x/2][y/2];
        return value >= 200 ? 0xff0000L : value >= 150 ? 0xff4000L :
            value >= 100 ? 0xff8000L : value >= 50 ? 0xffc000L : value > 0 ? 0xffff00L : 0L;
    case 10: value = LandValueMem[x/2][y/2];
        return value >= 200 ? 0x00ff00L : value >= 150 ? 0x40ff40L :
            value >= 100 ? 0x80ff80L : value >= 50 ? 0xc0ffc0L : value > 0 ? 0xffffc0L : 0L;
    case 11: case 12:
        value = (miniMode == 11 ? FireRate[x/8][y/8] : PoliceMapEffect[x/8][y/8]) / 4;
        if (value <= 0) return 0L;
        value = value >= 200 ? 0 : value >= 150 ? 64 : value >= 100 ? 128 : value >= 50 ? 192 : 224;
        return miniMode == 11 ? 0xff0000L + value * 257L : 255L + value * 65792L;
    default:
        if (tile >= RESBASE && tile < HOSPITAL) return 0x00ff00L;
        if (tile >= COMBASE && tile < INDBASE) return 0x0000ffL;
        if (tile >= INDBASE && tile < PORTBASE) return 0xffff00L;
        if (tile >= ROADBASE && tile <= LASTROAD) return 0x808080L;
        if (tile >= RAILBASE && tile <= LASTRAIL) return 0xc0c0c0L;
        if (tile >= POWERBASE && tile <= LASTPOWER) return 0xff0000L;
        if (tile >= RIVER && tile <= LASTRIVEDGE) return 0x0080ffL;
        if (tile >= TREEBASE && tile <= WOODS5) return 0x008000L;
        return tile ? 0x404040L : 0x202020L;
    }
    if (value < 0) value = 0;
    if (value > 255) value = 255;
    if (miniMode == 1) return value * 256L;
    if (miniMode == 2) return value;
    if (miniMode == 3) return value * 65792L;
    return value * 65537L;
}

static void mini_rect(RECTL *r)
{
    RECTL client;
    LONG w, h;
    WinQueryWindowRect(hwndMini, &client);
    w = client.xRight - 8; h = client.yTop - 48;
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    if (w * WORLD_Y > h * WORLD_X) w = h * WORLD_X / WORLD_Y;
    else h = w * WORLD_Y / WORLD_X;
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    r->xLeft = (client.xRight - w) / 2;
    r->xRight = r->xLeft + w;
    r->yTop = client.yTop - 4;
    r->yBottom = r->yTop - h;
}

static void mini_navigate(int x, int y)
{
    RECTL r;
    int nx, ny;
    mini_rect(&r);
    nx = pm_mini_position(x, (int)r.xLeft, (int)(r.xRight-r.xLeft),
                          WORLD_X*TILE_SIZE, clientWidth-toolbarWidth);
    ny = pm_mini_position((int)r.yTop-y, 0, (int)(r.yTop-r.yBottom),
                          WORLD_Y*TILE_SIZE, pm_map_height(clientHeight));
    pan(nx-xOffset, ny-yOffset);
}

static void paint_minimap(HPS hps)
{
    RECTL r, client, cell;
    POINTL p;
    LONG color, w, h;
    int x, y, vw, vh, offset;
    static unsigned char pixels[WORLD_X * WORLD_Y * 4];
    BITMAPINFOHEADER2 header;
    BITMAPINFO2 info;
    HBITMAP bitmap;
    POINTL points[4];
    WinQueryWindowRect(hwndMini, &client);
    mini_rect(&r); w = r.xRight-r.xLeft; h = r.yTop-r.yBottom;
    /* Clear only the margins, avoiding a background flash over the map. */
    cell = client; cell.yTop = r.yBottom; WinFillRect(hps, &cell, CLR_BLACK);
    cell = client; cell.yBottom = r.yTop; WinFillRect(hps, &cell, CLR_BLACK);
    cell = client; cell.xRight = r.xLeft; WinFillRect(hps, &cell, CLR_BLACK);
    cell = client; cell.xLeft = r.xRight; WinFillRect(hps, &cell, CLR_BLACK);
    for (y = 0; y < WORLD_Y; ++y) {
        for (x = 0; x < WORLD_X; ++x) {
            color = mini_color(x, y);
            offset = ((WORLD_Y-1-y)*WORLD_X+x)*4;
            pixels[offset] = (unsigned char)color;
            pixels[offset+1] = (unsigned char)(color >> 8);
            pixels[offset+2] = (unsigned char)(color >> 16);
            pixels[offset+3] = 0;
        }
    }
    memset(&header, 0, sizeof(header));
    header.cbFix = sizeof(header); header.cx = WORLD_X; header.cy = WORLD_Y;
    header.cPlanes = 1; header.cBitCount = 32;
    header.cbImage = sizeof(pixels);
    memset(&info, 0, sizeof(info)); memcpy(&info, &header, sizeof(header));
    bitmap = GpiCreateBitmap(miniPS, &header, CBM_INIT, pixels, &info);
    if (bitmap) {
        GpiSetBitmap(miniPS, bitmap);
        points[0].x = r.xLeft; points[0].y = r.yBottom;
        points[1].x = r.xRight; points[1].y = r.yTop;
        points[2].x = 0; points[2].y = 0;
        points[3].x = WORLD_X; points[3].y = WORLD_Y;
        GpiBitBlt(hps, miniPS, 4L, points, ROP_SRCCOPY, BBO_IGNORE);
        GpiSetBitmap(miniPS, NULLHANDLE); GpiDeleteBitmap(bitmap);
    } else WinFillRect(hps, &r, CLR_BLACK);
    vw = clientWidth-toolbarWidth; vh = pm_map_height(clientHeight);
    if (vw < 1) vw = 1;
    if (vw > WORLD_X*TILE_SIZE) vw = WORLD_X*TILE_SIZE;
    if (vh > WORLD_Y*TILE_SIZE) vh = WORLD_Y*TILE_SIZE;
    p.x = r.xLeft + xOffset*w/(WORLD_X*TILE_SIZE);
    p.y = r.yTop - yOffset*h/(WORLD_Y*TILE_SIZE) - 1;
    GpiSetColor(hps, CLR_WHITE); GpiMove(hps, &p);
    p.x = r.xLeft + (xOffset+vw)*w/(WORLD_X*TILE_SIZE) - 1;
    p.y = r.yTop - (yOffset+vh)*h/(WORLD_Y*TILE_SIZE);
    GpiBox(hps, DRO_OUTLINE, &p, 0L, 0L);
    p.x = 4; p.y = 25;
    GpiCharStringAt(hps, &p, (LONG)strlen(miniNames[miniMode]), (PCH)miniNames[miniMode]);
    p.y = 7;
    GpiCharStringAt(hps, &p, (LONG)strlen(miniLegends[miniMode]), (PCH)miniLegends[miniMode]);
}

static void stop_build(void)
{
    if (!buildStroke.active) return;
    buildStroke.active = 0;
    if (WinQueryCapture(HWND_DESKTOP, FALSE) == hwndMain)
        WinSetCapture(HWND_DESKTOP, NULLHANDLE);
}

static int build_tool(int tool)
{
    return tool == roadState || tool == railState || tool == wireState ||
           tool == bulldozerState;
}

static int build_point(int x, int y, int *tx, int *ty)
{
    if (x < toolbarWidth || x >= clientWidth ||
        y < MP_STATUS_HEIGHT || y >= clientHeight) return 0;
    *tx = (x-toolbarWidth+xOffset)/TILE_SIZE;
    *ty = (clientHeight-1-y+yOffset)/TILE_SIZE;
    return *tx >= 0 && *tx < WORLD_X && *ty >= 0 && *ty < WORLD_Y;
}

static void continue_build(int x, int y)
{
    int tx, ty, result;
    if (!buildStroke.active) return;
    if (WinQueryCapture(HWND_DESKTOP, FALSE) != hwndMain ||
        GetCurrentTool() != strokeTool || !build_point(x, y, &tx, &ty)) {
        stop_build(); return;
    }
    result = pm_stroke_move(&buildStroke, tx, ty, ApplyTool);
    if (result) {
        /* Traversal already stopped. Release capture before reporting. */
        if (WinQueryCapture(HWND_DESKTOP, FALSE) == hwndMain)
            WinSetCapture(HWND_DESKTOP, NULLHANDLE);
        addGameLog(result == TOOLRESULT_NO_MONEY ? "Construction stopped: not enough money." :
                  "Construction stopped: tool blocked at this tile.");
    }
    mp_ui_invalidate();
}

/* A shown window may still be behind another frame or minimized. */
static void reopen_window(HWND frame, HWND client)
{
    if (!frame || !client) {
        pm_trace("reopen failed: window destroyed; apply PMWIN close fix");
        return;
    }
    WinSetWindowPos(frame, HWND_TOP, 0, 0, 0, 0,
                    SWP_SHOW | SWP_RESTORE | SWP_ZORDER | SWP_ACTIVATE);
    WinInvalidateRect(client, NULL, FALSE);
}

/* The full 32x30 sheet always fits; the selected cell has a 64px preview. */
static int tile_view_cell(int width, int height)
{
    int cell, vertical;
    cell = (width - 16) / 32;
    vertical = (height - 104) / 30;
    if (vertical < cell) cell = vertical;
    return cell > 0 ? cell : 0;
}

static void paint_tile_viewer(HWND hwnd, HPS hps)
{
    RECTL r;
    POINTL p[4];
    int cell, top, x, y;
    char text[MAX_PATH + 40];
    WinQueryWindowRect(hwnd, &r);
    WinFillRect(hps, &r, CLR_WHITE);
    GpiSetColor(hps, CLR_BLACK);
    if (!hbmTiles) return;
    cell = tile_view_cell((int)r.xRight, (int)r.yTop);
    if (!cell) {
        paint_text_line(hps, 8, r.yTop - 18, "Enlarge the tile viewer."); return;
    }
    sprintf(text, "Tile %d (0x%03X) / row %d, column %d", viewedTile,
            viewedTile, viewedTile / 32, viewedTile % 32);
    paint_text_line(hps, 80, r.yTop - 20, text);
    paint_text_line(hps, 80, r.yTop - 38, "Click a tile; arrow keys select; Home/End.");
    paint_text_line(hps, 80, r.yTop - 56, activeTileset);
    p[0].x = 8; p[0].y = r.yTop - 72;
    p[1].x = 72; p[1].y = r.yTop - 8;
    p[2].x = (viewedTile % 32) * 16;
    p[2].y = 480 - (viewedTile / 32 + 1) * 16;
    p[3].x = p[2].x + 16; p[3].y = p[2].y + 16;
    GpiBitBlt(hps, hpsMemory, 4L, p, ROP_SRCCOPY, BBO_IGNORE);
    top = r.yTop - 96;
    p[0].x = 8; p[0].y = top - 30 * cell;
    p[1].x = 8 + 32 * cell; p[1].y = top;
    p[2].x = 0; p[2].y = 0; p[3].x = 512; p[3].y = 480;
    GpiBitBlt(hps, hpsMemory, 4L, p, ROP_SRCCOPY, BBO_IGNORE);
    x = 8 + (viewedTile % 32) * cell;
    y = top - (viewedTile / 32 + 1) * cell;
    GpiSetColor(hps, CLR_WHITE);
    p[0].x = x; p[0].y = y; GpiMove(hps, &p[0]);
    p[0].x = x + cell - 1; p[0].y = y + cell - 1;
    GpiBox(hps, DRO_OUTLINE, &p[0], 0, 0);
}

static void tile_view_click(HWND hwnd, int x, int y)
{
    RECTL r;
    int cell, row, column, top;
    WinQueryWindowRect(hwnd, &r);
    cell = tile_view_cell((int)r.xRight, (int)r.yTop);
    top = r.yTop - 96;
    if (!cell || x < 8 || x >= 8 + cell * 32 ||
        y >= top || y < top - cell * 30) return;
    column = (x - 8) / cell; row = (top - 1 - y) / cell;
    viewedTile = row * 32 + column;
    WinSetFocus(HWND_DESKTOP, hwnd);
    WinInvalidateRect(hwnd, NULL, FALSE);
}

static void zoom_menu_sync(void)
{
    HWND menu;
    menu = WinWindowFromID(frameMain, FID_MENU);
    if (!menu) return;
    WinCheckMenuItem(menu, IDM_ZOOM_HALF, TILE_SIZE == 8);
    WinCheckMenuItem(menu, IDM_ZOOM_NORMAL, TILE_SIZE == 16);
    WinCheckMenuItem(menu, IDM_ZOOM_DOUBLE, TILE_SIZE == 32);
}

static void set_map_zoom(int tile)
{
    int nx, ny;
    if (tile != 8 && tile != 16 && tile != 32) return;
    stop_build();
    nx = pm_zoom_offset(xOffset, clientWidth-toolbarWidth, TILE_SIZE, tile, WORLD_X);
    ny = pm_zoom_offset(yOffset, pm_map_height(clientHeight), TILE_SIZE, tile, WORLD_Y);
    mapTileSize = tile;
    xOffset = nx; yOffset = ny;
    hoverMapX = hoverMapY = -1;
    clamp_view();
    update_scrollbars();
    zoom_menu_sync();
    mp_ui_invalidate();
    pm_trace("map zoom: %d%% (%d pixel tiles)", tile * 100 / SOURCE_TILE_SIZE, tile);
}

static void menu_command(USHORT command)
{
    stop_build();
    if (command >= IDM_TILESET_FIRST && command <= IDM_TILESET_CUSTOM) {
        choose_tileset(command == IDM_TILESET_CUSTOM ? -1 : command - IDM_TILESET_FIRST);
        return;
    }
    if (command >= IDM_ZOOM_HALF && command <= IDM_ZOOM_DOUBLE) {
        set_map_zoom(command == IDM_ZOOM_HALF ? 8 : command == IDM_ZOOM_NORMAL ? 16 : 32);
        return;
    }
    if (command >= IDM_CHART_SERIES_FIRST && command < IDM_CHART_SERIES_FIRST + 15) {
        chartMask ^= 1 << (command - IDM_CHART_SERIES_FIRST);
        chart_menu_sync(); WinInvalidateRect(hwndChart, NULL, FALSE); return;
    }
    if (command >= IDM_CHART_MONTHLY && command <= IDM_CHART_NONE) {
        switch (command) {
        case IDM_CHART_MONTHLY: chartRange = 0; break;
        case IDM_CHART_YEARLY: chartRange = 1; break;
        case IDM_CHART_RCI: chartMask = 14; break;
        case IDM_CHART_DEMAND: chartMask = 28672; break;
        case IDM_CHART_ALL: chartMask = 32767; break;
        case IDM_CHART_NONE: chartMask = 0; break;
        }
        chart_menu_sync(); WinInvalidateRect(hwndChart, NULL, FALSE); return;
    }
    if (command >= IDM_MINIMAP_FIRST && command < IDM_MINIMAP_FIRST + 13) {
        miniMode = command - IDM_MINIMAP_FIRST;
        WinInvalidateRect(hwndMini, NULL, FALSE);
        return;
    }
    if (command >= IDM_TOOL_FIRST && command < IDM_TOOL_FIRST + TOOL_COUNT) {
        SelectTool(toolbarTools[command - IDM_TOOL_FIRST]);
        mp_ui_invalidate();
        return;
    }
    if (command >= IDM_SCENARIO_FIRST && command < IDM_SCENARIO_FIRST + 8) {
        if (!loadScenario(command - IDM_SCENARIO_FIRST + 1))
            mp_ui_message(hwndMain, "Scenario file missing from cities directory.", MP_TITLE);
        mp_ui_invalidate(); return;
    }
    switch (command) {
    case IDM_DISASTER_FIRE:
        addGameLog("Manual fire requested (requires a burnable tile).");
        MakeFire(); break;
    case IDM_DISASTER_FLOOD:
        addGameLog("Manual flood requested (requires a suitable shoreline).");
        MakeFlood(); break;
    case IDM_DISASTER_TORNADO:
        if (GetSprite(SPRITE_TORNADO))
            addGameLog("A tornado is already active; no second tornado created.");
        else MakeTornado();
        break;
    case IDM_DISASTER_EARTHQUAKE:
        addGameLog("Manual earthquake triggered.");
        MakeEarthquake(); break;
    case IDM_DISASTER_MONSTER:
        if (GetSprite(SPRITE_MONSTER))
            addGameLog("Existing monster renewed; only one monster is active.");
        MakeMonster(); break;
    case IDM_DISASTER_MELTDOWN:
        {
            int x, y, found;
            found = 0;
            for (x = 0; x < WORLD_X - 1 && !found; ++x)
                for (y = 0; y < WORLD_Y - 1; ++y)
                    if ((Map[x][y] & LOMASK) == NUCLEAR) { found = 1; break; }
            if (found) MakeMeltdown();
            else {
                addGameLog("Meltdown unavailable: no nuclear plant in this city.");
                mp_ui_message(hwndMain, "There is no nuclear plant to melt down.", MP_TITLE);
            }
        }
        break;
    case IDM_DISASTER_FIREBOMB:
        addGameLog("Manual firebombing triggered.");
        DropFireBombs(); break;
    case IDM_LAST_ALERT: show_last_alert(); break;
    case IDM_TILE_VIEWER: reopen_window(frameTiles, hwndTiles); break;
    case IDM_NEW: new_city_dialog(); break;
    case IDM_OPEN: city_file_dialog(0); break;
    case IDM_SAVE: city_file_dialog(1); break;
    case IDM_MINIMAP: reopen_window(frameMini, hwndMini); break;
    case IDM_CHART: chart_menu_sync(); reopen_window(frameChart, hwndChart); break;
    case IDM_BUDGET: modalActive = 1; ShowBudgetWindowAndWait(hwndMain); modalActive = 0; break;
    case IDM_TAX_UP: if (CityTax < 20) ++CityTax; break;
    case IDM_TAX_DOWN: if (CityTax > 0) --CityTax; break;
    case IDM_ROAD_FUND: RoadPercent += 0.25f; if (RoadPercent > 1) RoadPercent = 0; break;
    case IDM_FIRE_FUND: FirePercent += 0.25f; if (FirePercent > 1) FirePercent = 0; break;
    case IDM_POLICE_FUND: PolicePercent += 0.25f; if (PolicePercent > 1) PolicePercent = 0; break;
    case IDM_AUTOBUDGET: AutoBudget = !AutoBudget; break;
    case IDM_EXIT: WinPostMsg(hwndMain, WM_CLOSE, 0, 0); break;
    case IDM_SOUND:
        pm_sound_enable(!pm_sound_enabled());
        WinCheckMenuItem(WinWindowFromID(frameMain, FID_MENU), IDM_SOUND, pm_sound_enabled());
        break;
    case IDM_SOUND_TEST: pm_sound_play(PM_SOUND_ALERT); break;
    case IDM_PAUSE: SetSimulationSpeed(hwndMain, SPEED_PAUSED); break;
    case IDM_SLOW: SetSimulationSpeed(hwndMain, SPEED_SLOW); break;
    case IDM_MEDIUM: SetSimulationSpeed(hwndMain, SPEED_MEDIUM); break;
    case IDM_FAST: SetSimulationSpeed(hwndMain, SPEED_FAST); break;
    case IDM_LOG: reopen_window(frameLog, hwndLog); break;
    case IDM_INFO: reopen_window(frameInfo, hwndInfo); break;
    case IDM_ABOUT:
        mp_ui_message(hwndMain,
            "WinTown/2\n17 tools, 3 columns; R/C/I = demand.\n"
            "Left click: build/query. Right drag or scroll bars: pan.\n"
            "Arrows: one tile. PgUp/PgDn: page. Home/End: corners.\n"
            "Ctrl+N: new city. Ctrl+Q: quit.\n"
            "Ctrl+P/L/M/F: pause/slow/normal/fast.\n"
            "1-8: bulldozer, road, rail, wire, R, C, I, park.", MP_TITLE);
        break;
    }
    mp_ui_invalidate();
}

/* Optional low-volume timing diagnostics; no new PM imports required. */
static void report_sim_timing(void)
{
    static time_t start;
    static int ticks, firstCityTime, firstSpeed;
    time_t now;
    long elapsed;
    if (!getenv("WINTOWN_TIMING")) return;
    now = time(NULL);
    if (timingReset || !start || now < start || CityTime < firstCityTime || SimSpeed != firstSpeed) {
        start = now; ticks = 0; firstCityTime = CityTime; firstSpeed = SimSpeed;
        timingReset = 0; return;
    }
    ++ticks;
    elapsed = (long)(now-start);
    if (elapsed >= 5) {
        pm_trace("timing: %ld sec, %d timer ticks, %d city units, speed %d, paused %d, date %02d/%d W%d",
                 elapsed, ticks, CityTime-firstCityTime, SimSpeed, SimPaused,
                 CityMonth+1, CityYear, (CityTime & 3)+1);
        start = now; ticks = 0; firstCityTime = CityTime;
    }
}

static MRESULT EXPENTRY client_proc(HWND hwnd, USHORT msg, MPARAM mp1, MPARAM mp2)
{
    HPS hps;
    RECTL r;
    int x, y, result;
    switch (msg) {
    case WM_COMMAND:
        menu_command(SHORT1FROMMP(mp1));
        return 0;
    case WM_PAINT:
        hps = WinBeginPaint(hwnd, NULLHANDLE, &r);
        if (hwnd == hwndMain) present_city(hps);
        else if (hwnd == hwndLog) paint_log_window(hwnd, hps);
        else if (hwnd == hwndInfo) paint_info_window(hwnd, hps);
        else if (hwnd == hwndChart) paint_chart_window(hwnd, hps);
        else if (hwnd == hwndTiles) paint_tile_viewer(hwnd, hps);
        else if (hwnd == hwndMini) paint_minimap(hps);
        WinEndPaint(hps);
        return 0;
    case WM_HSCROLL:
    case WM_VSCROLL:
        if (hwnd == hwndInfo && msg == WM_VSCROLL) {
            if (!infoUpdating) scroll_info(SHORT2FROMMP(mp2), (short)SHORT1FROMMP(mp2));
            return 0;
        }
        if (hwnd != hwndMain) break;
        /* PM packs position in mp2.low and command in mp2.high.
         * mp1 is not a Win32 scroll command; some host versions pass 0. */
        scroll_view(msg == WM_VSCROLL, SHORT2FROMMP(mp2),
                    (int)(short)SHORT1FROMMP(mp2));
        return 0;
    case WM_SIZE:
        if (hwnd == hwndInfo) { update_info_scroll(); return 0; }
        if (hwnd != hwndMain) return 0;
        clientWidth = SHORT1FROMMP(mp2); clientHeight = SHORT2FROMMP(mp2);
        create_frame_buffer(clientWidth, clientHeight);
        pan(0, 0); return 0;
    case WM_TIMER:
        if (hwnd == hwndMain && !modalActive && SHORT1FROMMP(mp1) == MP_TIMER) {
            modalActive = 1; SimFrame(); modalActive = 0;
            report_sim_timing();
            miniSimUpdate = (++miniTick < 20);
            if (!miniSimUpdate) miniTick = 0;
            mp_ui_invalidate(); miniSimUpdate = 0;
        }
        return 0;
    case WM_BUTTON1UP:
        if (hwnd == hwndMain) {
            continue_build((short)SHORT1FROMMP(mp1), (short)SHORT2FROMMP(mp1));
            stop_build();
        }
        if (hwnd == hwndMini && miniDrag) {
            mini_navigate((short)SHORT1FROMMP(mp1), (short)SHORT2FROMMP(mp1));
            miniDrag = 0; WinSetCapture(HWND_DESKTOP, NULLHANDLE);
        }
        return 0;
    case WM_BUTTON1DOWN:
        if (hwnd == hwndTiles) {
            tile_view_click(hwnd, (short)SHORT1FROMMP(mp1), (short)SHORT2FROMMP(mp1));
            return 0;
        }
        if (hwnd == hwndMini) {
            x = (short)SHORT1FROMMP(mp1); y = (short)SHORT2FROMMP(mp1);
            mini_rect(&r);
            if (x >= r.xLeft && x < r.xRight && y >= r.yBottom && y < r.yTop) {
                miniDrag = WinSetCapture(HWND_DESKTOP, hwnd);
                mini_navigate(x, y);
            }
            return 0;
        }
        if (hwnd != hwndMain) return 0;
        stop_build(); mouseDown = 0;
        x = (short)SHORT1FROMMP(mp1); y = (short)SHORT2FROMMP(mp1);
        if (x >= 0 && x < toolbarWidth) {
            result = toolbar_tool_at(x, y);
            if (result >= 0) {
                SelectTool(toolbarTools[result]);
                pm_trace("toolbar tool %d selected", GetCurrentTool());
                mp_ui_invalidate();
            }
            return 0;
        }
        if (GetCurrentTool() == noToolState || !build_point(x, y, &x, &y)) return 0;
        hoverMapX = x; hoverMapY = y;
        if (build_tool(GetCurrentTool()) && WinSetCapture(HWND_DESKTOP, hwnd)) {
            strokeTool = GetCurrentTool();
            pm_stroke_begin(&buildStroke, x, y);
            result = pm_stroke_move(&buildStroke, x, y, ApplyTool);
            if (result) WinSetCapture(HWND_DESKTOP, NULLHANDLE);
        } else result = ApplyTool(x, y);
        if (GetCurrentTool() != queryState)
            pm_sound_play(result ? PM_SOUND_ERROR : PM_SOUND_BUILD);
        if (result == TOOLRESULT_NO_MONEY) mp_ui_message(hwnd, "Not enough money.", MP_TITLE);
        else if (result == TOOLRESULT_NEED_BULLDOZE) mp_ui_message(hwnd, "Bulldoze this area first.", MP_TITLE);
        else if (result == TOOLRESULT_FAILED) mp_ui_message(hwnd, "That tool cannot be used there.", MP_TITLE);
        mp_ui_invalidate();
        return 0;
    case WM_BUTTON2DOWN:
        if (hwnd != hwndMain) return 0;
        stop_build();
        mouseDown = 1; lastMouseX = SHORT1FROMMP(mp1); lastMouseY = SHORT2FROMMP(mp1);
        WinSetCapture(HWND_DESKTOP, hwnd); return 0;
    case WM_BUTTON2UP:
        if (hwnd != hwndMain) return 0;
        mouseDown = 0; WinSetCapture(HWND_DESKTOP, NULLHANDLE); return 0;
    case WM_MOUSEMOVE:
        if (hwnd == hwndMini) {
            if (miniDrag && WinQueryCapture(HWND_DESKTOP, FALSE) != hwnd) miniDrag = 0;
            if (miniDrag) mini_navigate((short)SHORT1FROMMP(mp1), (short)SHORT2FROMMP(mp1));
            return 0;
        }
        if (hwnd != hwndMain) return 0;
        x = (short)SHORT1FROMMP(mp1); y = (short)SHORT2FROMMP(mp1);
        continue_build(x, y);
        if (mouseDown) { pan(lastMouseX - x, y - lastMouseY); lastMouseX = x; lastMouseY = y; }
        if (x >= toolbarWidth && y >= MP_STATUS_HEIGHT) {
            x = (x - toolbarWidth + xOffset) / TILE_SIZE;
            y = (clientHeight - 1 - y + yOffset) / TILE_SIZE;
            if (x != hoverMapX || y != hoverMapY) {
                hoverMapX = x; hoverMapY = y;
                mp_ui_invalidate();
            }
        } else if (hoverMapX != -1 || hoverMapY != -1) {
            hoverMapX = -1; hoverMapY = -1;
            mp_ui_invalidate();
        }
        return 0;
    case WM_CHAR:
        if (hwnd == hwndTiles && !(SHORT1FROMMP(mp1) & KC_KEYUP) &&
            (SHORT1FROMMP(mp1) & KC_VIRTUALKEY)) {
            switch (SHORT2FROMMP(mp2)) {
            case VK_LEFT: if (viewedTile > 0) --viewedTile; break;
            case VK_RIGHT: if (viewedTile < 959) ++viewedTile; break;
            case VK_UP: if (viewedTile >= 32) viewedTile -= 32; break;
            case VK_DOWN: if (viewedTile < 928) viewedTile += 32; break;
            case VK_HOME: viewedTile = 0; break;
            case VK_END: viewedTile = 959; break;
            default: return WinDefWindowProc(hwnd, msg, mp1, mp2);
            }
            WinInvalidateRect(hwnd, NULL, FALSE); return 0;
        }
        if (hwnd == hwndInfo && !(SHORT1FROMMP(mp1) & KC_KEYUP) &&
            (SHORT1FROMMP(mp1) & KC_VIRTUALKEY)) {
            switch (SHORT2FROMMP(mp2)) {
            case VK_UP: scroll_info(SB_LINEUP, 0); return 0;
            case VK_DOWN: scroll_info(SB_LINEDOWN, 0); return 0;
            case VK_PAGEUP: scroll_info(SB_PAGEUP, 0); return 0;
            case VK_PAGEDOWN: scroll_info(SB_PAGEDOWN, 0); return 0;
            case VK_HOME: scroll_info(SB_SLIDERPOSITION, 0); return 0;
            case VK_END: scroll_info(SB_SLIDERPOSITION, INFO_ROWS); return 0;
            }
        }
        if (hwnd != hwndMain || (SHORT1FROMMP(mp1) & KC_KEYUP)) break;
        stop_build();
        if (SHORT1FROMMP(mp1) & (KC_CTRL | KC_ALT)) break;
        if (SHORT1FROMMP(mp1) & KC_VIRTUALKEY) {
            switch (SHORT2FROMMP(mp2)) {
            case VK_LEFT: scroll_view(0, SB_LINELEFT, 0); return 0;
            case VK_RIGHT: scroll_view(0, SB_LINERIGHT, 0); return 0;
            case VK_UP: scroll_view(1, SB_LINEUP, 0); return 0;
            case VK_DOWN: scroll_view(1, SB_LINEDOWN, 0); return 0;
            case VK_PAGEUP: scroll_view(1, SB_PAGEUP, 0); return 0;
            case VK_PAGEDOWN: scroll_view(1, SB_PAGEDOWN, 0); return 0;
            case VK_HOME: pan(-xOffset, -yOffset); return 0;
            case VK_END:
                pan(WORLD_X * TILE_SIZE - xOffset,
                    WORLD_Y * TILE_SIZE - yOffset); return 0;
            }
        }
        if (SHORT1FROMMP(mp1) & KC_CHAR) {
            switch (CHAR1FROMMP(mp2)) {
            case '1': SelectTool(bulldozerState); break; case '2': SelectTool(roadState); break;
            case '3': SelectTool(railState); break; case '4': SelectTool(wireState); break;
            case '5': SelectTool(residentialState); break; case '6': SelectTool(commercialState); break;
            case '7': SelectTool(industrialState); break; case '8': SelectTool(parkState); break;
            case 'n': case 'N': new_city(); break; case 'p': case 'P': SetSimulationSpeed(hwnd, SPEED_PAUSED); break;
            case 's': case 'S': SetSimulationSpeed(hwnd, SPEED_MEDIUM); break;
            case 'q': case 'Q': WinPostMsg(hwnd, WM_CLOSE, 0, 0); break;
            }
            mp_ui_invalidate();
        }
        return 0;
    case WM_CLOSE:
        stop_build();
        if (hwnd == hwndMini) {
            if (miniDrag && WinQueryCapture(HWND_DESKTOP, FALSE) == hwnd)
                WinSetCapture(HWND_DESKTOP, NULLHANDLE);
            miniDrag = 0; WinShowWindow(frameMini, FALSE); return 0;
        }
        if (hwnd == hwndMain) WinPostMsg(hwnd, WM_QUIT, 0, 0);
        else if (hwnd == hwndLog) WinShowWindow(frameLog, FALSE);
        else if (hwnd == hwndInfo) WinShowWindow(frameInfo, FALSE);
        else if (hwnd == hwndChart) WinShowWindow(frameChart, FALSE);
        else if (hwnd == hwndTiles) WinShowWindow(frameTiles, FALSE);
        return 0;
    case WM_DESTROY:
        if (hwnd == hwndMini) { hwndMini = 0; miniDrag = 0; }
        else if (hwnd == hwndLog) hwndLog = 0;
        else if (hwnd == hwndInfo) hwndInfo = 0;
        else if (hwnd == hwndChart) hwndChart = 0;
        else if (hwnd == hwndTiles) hwndTiles = 0;
        else if (hwnd == hwndMain) WinPostMsg(HWND_DESKTOP, WM_QUIT, 0, 0);
        return 0;
    }
    return WinDefWindowProc(hwnd, msg, mp1, mp2);
}
#endif

int main(int argc, char **argv)
{
#ifndef MP_PORT_TEST
    QMSG qmsg;
    ULONG frameFlags;
    SIZEL size;
    RECTL clientRect;
    MPBITMAP *raw;
    const char *tilesetName;
    int i;
    hab = WinInitialize(0);
    if (!hab) { pm_trace("WinInitialize failed"); return 1; }
    hmq = WinCreateMsgQueue(hab, 0);
    if (!hmq || !WinRegisterClass(hab, MP_CLASS, client_proc, CS_SIZEREDRAW, 0)) {
        pm_trace("message queue or window registration failed"); return 1;
    }
    frameFlags = FCF_TITLEBAR | FCF_SYSMENU | FCF_MINMAX |
                 FCF_SIZEBORDER | FCF_TASKLIST | FCF_MENU | FCF_ACCELTABLE |
                 FCF_HORZSCROLL | FCF_VERTSCROLL;
    frameMain = WinCreateStdWindow(HWND_DESKTOP, 0, &frameFlags, (PSZ)MP_CLASS,
                                  (PSZ)MP_TITLE, 0L, (HMODULE)0,
                                  ID_PM_RESOURCES, &hwndMain);
    if (!frameMain || !hwndMain) {
        pm_trace("main window failed; verify matching RC bound wintownpm.res"); return 1;
    }
    zoom_menu_sync();
    hwndHScroll = WinWindowFromID(frameMain, FID_HORZSCROLL);
    hwndVScroll = WinWindowFromID(frameMain, FID_VERTSCROLL);
    pm_trace("map scrollbars: horizontal %s, vertical %s",
        hwndHScroll ? "OK" : "MISSING", hwndVScroll ? "OK" : "MISSING");
    frameFlags &= ~(FCF_MENU | FCF_ACCELTABLE | FCF_HORZSCROLL | FCF_VERTSCROLL);
    frameLog = WinCreateStdWindow(HWND_DESKTOP, 0, &frameFlags, (PSZ)MP_CLASS,
                                 (PSZ)"WinTown Log", 0L, (HMODULE)0, 0, &hwndLog);
    frameFlags |= FCF_VERTSCROLL;
    frameInfo = WinCreateStdWindow(HWND_DESKTOP, 0, &frameFlags, (PSZ)MP_CLASS,
                                  (PSZ)"WinTown Info / Evaluation", 0L, (HMODULE)0, 0, &hwndInfo);
    if (frameInfo) hwndInfoScroll = WinWindowFromID(frameInfo, FID_VERTSCROLL);
    frameFlags &= ~FCF_VERTSCROLL;
    frameTiles = WinCreateStdWindow(HWND_DESKTOP, 0, &frameFlags, (PSZ)MP_CLASS,
                                  (PSZ)"WinTown Tile Viewer", 0L, (HMODULE)0, 0, &hwndTiles);
    frameFlags |= FCF_MENU;
    frameChart = WinCreateStdWindow(HWND_DESKTOP, 0, &frameFlags, (PSZ)MP_CLASS,
                                  (PSZ)"WinTown Charts", 0L, (HMODULE)0, ID_CHART_RESOURCES, &hwndChart);
    chart_menu_sync();
    miniDC = DevOpenDC(hab, OD_MEMORY, "*", 0L, NULL, NULLHANDLE);
    size.cx = 0; size.cy = 0;
    if (miniDC) miniPS = GpiCreatePS(hab, miniDC, &size,
                                   PU_PELS | GPIF_DEFAULT | GPIT_MICRO | GPIA_ASSOC);
    if (!miniPS) { pm_trace("minimap memory PS failed"); return 1; }
    frameFlags |= FCF_MENU;
    frameMini = WinCreateStdWindow(HWND_DESKTOP, 0, &frameFlags, (PSZ)MP_CLASS,
                                  (PSZ)"WinTown Minimap", 0L, (HMODULE)0,
                                  ID_MINIMAP_RESOURCES, &hwndMini);
    if (!frameLog || !frameInfo || !frameChart || !frameMini || !frameTiles) {
        pm_trace("auxiliary window failed; rebuild and bind wintownpm.res"); return 1;
    }
    WinSetWindowPos(frameMini, HWND_TOP, 40, 40, 390, 400,
                    SWP_MOVE | SWP_SIZE | SWP_SHOW);
    WinSetWindowPos(frameTiles, HWND_TOP, 60, 40, 560, 650, SWP_MOVE | SWP_SIZE);
    WinSetWindowPos(frameChart, HWND_TOP, 40, 80, 760, 420, SWP_MOVE | SWP_SIZE);
    WinSetWindowPos(frameMain, HWND_TOP, 20, 400, 640, 300,
                    SWP_MOVE | SWP_SIZE | SWP_SHOW);
    WinSetWindowPos(frameLog, HWND_TOP, 675, 400, 360, 250,
                    SWP_MOVE | SWP_SIZE | SWP_SHOW);
    WinSetWindowPos(frameInfo, HWND_TOP, 675, 70, 440, 420,
                    SWP_MOVE | SWP_SIZE | SWP_SHOW);
    update_info_scroll();
    WinQueryWindowRect(hwndMain, &clientRect);
    clientWidth = (int)(clientRect.xRight - clientRect.xLeft);
    clientHeight = (int)(clientRect.yTop - clientRect.yBottom);
    pan(0, 0);
    hdcMemory = DevOpenDC(hab, OD_MEMORY, "*", 0L, NULL, NULLHANDLE);
    if (!hdcMemory) { pm_trace("tile memory DC failed"); return 1; }
    size.cx = 0; size.cy = 0;
    hpsMemory = GpiCreatePS(hab, hdcMemory, &size, PU_PELS | GPIT_MICRO | GPIA_ASSOC);
    if (!hpsMemory) { pm_trace("tile memory PS failed"); return 1; }
    /* Match NT: textured default tiles, with classic as a fallback. */
    tilesetName = "default.bmp";
    raw = mp_bmp_load("tilesets\\default.bmp");
    if (!raw) raw = mp_bmp_load("tilesets/default.bmp");
    if (!raw) {
        tilesetName = "classic.bmp";
        raw = mp_bmp_load("tilesets\\classic.bmp");
        if (!raw) raw = mp_bmp_load("tilesets/classic.bmp");
    }
    if (raw) pm_trace("tileset %s loaded: %dx%d", tilesetName,
                      mp_bmp_width(raw), mp_bmp_height(raw));
    else pm_trace("neither default.bmp nor classic.bmp could be loaded");
    if (!raw || mp_bmp_width(raw) != 512 || mp_bmp_height(raw) != 480) {
        mp_bmp_free(raw);
        mp_ui_message(hwndMain, "Cannot load a 512x480 tileset. Run from the directory containing tilesets and assets.", MP_TITLE);
        return 1;
    }
    hbmTiles = mp_bmp_create_gpi(raw, hpsMemory); mp_bmp_free(raw);
    if (hbmTiles) GpiSetBitmap(hpsMemory, hbmTiles);
    else {
        pm_trace("tileset bitmap creation failed");
        mp_ui_message(hwndMain, "GpiCreateBitmap failed for the tileset.", MP_TITLE);
        return 1;
    }
    sprintf(activeTileset, "tilesets\\%s", tilesetName);
    activeTilesetIndex = strcmp(tilesetName, "classic.bmp") == 0 ? 1 : 0;
    tileset_menu_sync();
    load_toolbar();
    load_sprites();
    pm_sound_init();
    WinCheckMenuItem(WinWindowFromID(frameMain, FID_MENU), IDM_SOUND, pm_sound_enabled());
    create_frame_buffer(clientWidth, clientHeight);
    new_city();
    if (argc > 1 && !loadFile(argv[1]))
        mp_ui_message(hwndMain, "Could not load the city named on the command line.", MP_TITLE);
    while (WinGetMsg(hab, &qmsg, NULLHANDLE, 0, 0)) WinDispatchMsg(hab, &qmsg);
    CleanupSimTimer(hwndMain);
    pm_sound_stop();
    WinDestroyWindow(frameMini);
    GpiDestroyPS(miniPS); DevCloseDC(miniDC);
    WinDestroyWindow(frameTiles);
    WinDestroyWindow(frameChart);
    WinDestroyWindow(frameLog);
    WinDestroyWindow(frameInfo);
    WinDestroyWindow(frameMain);
    destroy_frame_buffer();
    if (hpsIcons) GpiSetBitmap(hpsIcons, NULLHANDLE);
    destroy_sprites();
    CleanupChartSystem();
    for (i = 0; i < TOOL_COUNT; ++i)
        if (toolbarBitmaps[i]) GpiDeleteBitmap(toolbarBitmaps[i]);
    if (hpsIcons) GpiDestroyPS(hpsIcons);
    if (hdcIcons) DevCloseDC(hdcIcons);
    if (hpsMemory) GpiSetBitmap(hpsMemory, NULLHANDLE);
    if (hbmTiles) GpiDeleteBitmap(hbmTiles);
    if (hpsMemory) GpiDestroyPS(hpsMemory);
    if (hdcMemory) DevCloseDC(hdcMemory);
    WinDestroyMsgQueue(hmq); WinTerminate(hab);
#else
    (void)argc; (void)argv; new_city();
#ifdef MP_SELFTEST
    {
        extern int mp_selftest(void);
        return mp_selftest();
    }
#endif
#endif
    return 0;
}

