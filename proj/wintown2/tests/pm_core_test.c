#include "sim.h"
#include "tools.h"
#include "sprite.h"
#include "charts.h"
#include "os2bmp.h"
#include "notify.h"
#include "cityio.h"
#include "pm_stroke.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int GenerateMap(int seed);
extern int saveFile(char *name);
extern void SimFrame(void);
extern int loadScenario(int scenario);
extern void mp_test_clear_log(void);
extern const char *mp_test_game_log(void);
extern void mp_test_status(char *out);
extern void addGameLog(const char *format, ...);

extern void mp_test_new_city(const char *name, int difficulty, int terrain);
extern void mp_test_info_line(int row, char text[160]);
/* sim.c is compiled with rand redirected here, reproducing MSC's range. */
int mp_test_rand15(void) { return rand() & 32767; }
static void check_15bit_growth(void)
{
    int i, value, negative, positive, growthRolls, houses, x, y;
    srand(1234); negative = positive = growthRolls = 0;
    for (i = 0; i < 4096; ++i) {
        value = Rand16Signed();
        if (value < 0) ++negative; else ++positive;
        if (value < -26380) ++growthRolls;
    }
    assert(negative > 1000 && positive > 1000 && growthRolls > 100);
    mp_test_new_city("Growth Test", 0, 1);
    SelectTool(residentialState); assert(ApplyTool(10, 10) == TOOLRESULT_OK);
    SETPOWERBIT(10, 10); RValve = 2000;
    srand(1234);
    houses = 0;
    /* Exercise the real residential immigration code with a powered empty
     * zone. Stop after the first house so transport demand is not a factor. */
    for (i = 0; i < 256 && !houses; ++i) {
        SMapX = 10; SMapY = 10;
        CChr = Map[10][10]; CChr9 = CChr & LOMASK;
        DoZone();
        for (x = 9; x <= 11; ++x) for (y = 9; y <= 11; ++y)
            if ((Map[x][y] & LOMASK) >= LHTHR && (Map[x][y] & LOMASK) <= HHTHR) ++houses;
    }
    assert(houses > 0);
    puts("PASS MSC-range RNG: both signs, immigration threshold reached, powered residential zone builds houses");
}

static void check_overwrite(void)
{
    mp_test_new_city("Overwrite test", 0, 1);
    TotalFunds = 11111; Map[20][20] = DIRT;
    assert(saveFile("build/check/overwrite.scn"));
    TotalFunds = 22222; Map[20][20] = WOODS; CityTime = 93;
    assert(saveFile("build/check/overwrite.scn"));
    TotalFunds = 0; Map[20][20] = DIRT; CityTime = 0;
    assert(loadFile("build/check/overwrite.scn"));
    assert(TotalFunds == 22222 && Map[20][20] == WOODS && CityTime == 93);
    puts("PASS overwrite: existing file replaced and updated funds/map/calendar reload");
}

static void check_simulation_speeds(void)
{
    int speed, n, before, expected[4];
    expected[0] = 0; expected[1] = 3; expected[2] = 5; expected[3] = 15;
    for (speed = 0; speed <= 3; ++speed) {
        mp_test_new_city("Speed test", 0, 1);
        SetSimulationSpeed(hwndMain, speed);
        before = CityTime;
        for (n = 0; n < 240; ++n) SimFrame();
        assert(CityTime-before == expected[speed]);
        assert(CityMonth == (CityTime%48)/4 && CityYear == 1900+CityTime/48);
    }
    SetSimulationSpeed(hwndMain, SPEED_PAUSED); before = CityTime;
    for (n = 0; n < 240; ++n) SimFrame();
    assert(CityTime == before);
    SetSimulationSpeed(hwndMain, SPEED_FAST);
    for (n = 0; n < 16; ++n) SimFrame();
    assert(CityTime == before+1);
    puts("PASS speeds: 240 ticks => paused 0, slow 3, medium 5, fast 15 city units; pause/resume");
}

static void check_city_report(void)
{
    char text[160];
    int row;
    mp_test_new_city("Report Test", 0, 1);
    mp_test_info_line(7, text); assert(strstr(text, "no survey yet"));
    mp_test_info_line(12, text); assert(strstr(text, "None reported"));
    EvalValid = 1; CityYes = 72; CityNo = 28;
    CityScore = 654; deltaCityScore = -12; CityPop = 12340; deltaCityPop = -40;
    ProblemOrder[0] = 4; ProblemVotes[4] = 33;
    ProblemOrder[1] = 0; ProblemVotes[0] = 21;
    ProblemOrder[2] = 7; ProblemOrder[3] = -1;
    mp_test_info_line(7, text); assert(strstr(text, "72% yes / 28% no"));
    mp_test_info_line(8, text); assert(strstr(text, "654/1000 (-12)"));
    mp_test_info_line(2, text); assert(strstr(text, "12340 (-40)"));
    mp_test_info_line(12, text); assert(strstr(text, "1. Traffic: 33 votes"));
    mp_test_info_line(13, text); assert(strstr(text, "2. Crime: 21 votes"));
    mp_test_info_line(14, text); assert(strstr(text, "None reported"));
    mp_test_info_line(15, text); assert(strstr(text, "None reported"));
    for (row = 0; row < 24; ++row) {
        memset(text, 0x7f, sizeof(text)); mp_test_info_line(row, text);
        assert(memchr(text, 0, sizeof(text)) != NULL);
    }
    assert(CityScore == 654 && deltaCityScore == -12 && CityYes == 72);
    assert(ProblemOrder[0] == 4 && ProblemVotes[4] == 33 && CityPop == 12340);
    EvalValid = 0; mp_test_info_line(6, text); assert(strstr(text, "pending"));
    mp_test_info_line(12, text); assert(strstr(text, "None reported"));
    puts("PASS city report: approval, signed deltas, ranked votes, absent survey, sentinel safety, read-only rendering");
}

extern void mp_test_new_city(const char *name, int difficulty, int terrain);
static void check_new_cities(void)
{
    int level, terrain, x, y, water, trees;
    long funds[3];
    funds[0] = 50000L; funds[1] = 20000L; funds[2] = 10000L;
    for (level = 0; level < 3; ++level) {
        for (terrain = 0; terrain < 4; ++terrain) {
            ScenarioID = 7; ScoreType = 7; ScoreWait = 100;
            CityTime = 1234; PopDensity[0][0] = 255; TrfDensity[0][0] = 255;
            FireRate[0][0] = 1000; PoliceMapEffect[0][0] = 1000;
            mp_test_new_city("Test City", level, terrain);
            assert(GameLevel == level && TotalFunds == funds[level]);
            assert(CityTime == 0 && CityYear == 1900 && CityMonth == 0);
            assert(ScenarioID == 0 && ScoreType == 0 && ScoreWait == 0);
            assert(PopDensity[0][0] == 0 && TrfDensity[0][0] == 0);
            assert(FireRate[0][0] == 0 && PoliceMapEffect[0][0] == 0);
            assert(CityTax == 7 && AutoBudget == 1);
            water = trees = 0;
            for (x = 0; x < WORLD_X; ++x) for (y = 0; y < WORLD_Y; ++y) {
                int tile;
                tile = Map[x][y] & LOMASK;
                if (tile >= RIVER && tile <= LASTRIVEDGE) ++water;
                if (tile >= TREEBASE && tile <= WOODS5) ++trees;
                if (terrain == 1) assert(tile == DIRT);
            }
            if (terrain == 2 || terrain == 3) assert(water > 0 && trees > 0);
            /* Shore smoothing can change the exact corner tile. */
        }
    }
    assert(saveFile("build/check/new-city.scn"));
    GameLevel = 0; TotalFunds = 1;
    assert(loadFile("build/check/new-city.scn"));
    assert(GameLevel == 2 && TotalFunds == 10000);
    puts("PASS new cities: 12 difficulty/terrain combinations, funds, calendar, scenario reset, overlays, save/reload");
}

static void check_strokes(void)
{
    static PMStroke stroke;
    int tools[3], costs[3], n, x, y, count;
    long funds;
    tools[0] = roadState; tools[1] = railState; tools[2] = wireState;
    costs[0] = 10; costs[1] = 20; costs[2] = 5;
    for (n = 0; n < 3; ++n) {
        memset(Map, 0, sizeof(Map)); TotalFunds = 10000;
        SelectTool(tools[n]); pm_stroke_begin(&stroke, 10, 10);
        assert(pm_stroke_move(&stroke, 15, 15, ApplyTool) == 0);
        count = 0;
        for (x = 0; x < WORLD_X; ++x)
            for (y = 0; y < WORLD_Y; ++y)
                if ((Map[x][y] & LOMASK) != DIRT) ++count;
        assert(count == 11 && TotalFunds == 10000 - 11*costs[n]);
        funds = TotalFunds;
        assert(pm_stroke_move(&stroke, 15, 15, ApplyTool) == 0);
        assert(pm_stroke_move(&stroke, 10, 10, ApplyTool) == 0);
        assert(TotalFunds == funds);
        memset(Map, 0, sizeof(Map)); TotalFunds = 10000;
        pm_stroke_begin(&stroke, 0, 0);
        assert(pm_stroke_move(&stroke, 119, 0, ApplyTool) == 0);
        funds = TotalFunds;
        assert(pm_stroke_move(&stroke, 0, 0, ApplyTool) == 0);
        assert(TotalFunds == funds);
        SelectTool(bulldozerState); pm_stroke_begin(&stroke, 0, 0);
        assert(pm_stroke_move(&stroke, 119, 0, ApplyTool) == 0);
        assert(TotalFunds == funds-120);
        funds = TotalFunds;
        assert(pm_stroke_move(&stroke, 0, 0, ApplyTool) == 0);
        assert(TotalFunds == funds);
    }
    memset(Map, 0, sizeof(Map)); TotalFunds = 25; SelectTool(roadState);
    pm_stroke_begin(&stroke, 0, 0);
    assert(pm_stroke_move(&stroke, 10, 0, ApplyTool) == TOOLRESULT_NO_MONEY);
    assert(!stroke.active && TotalFunds == 5 && Map[2][0] == DIRT);
    assert(pm_stroke_move(&stroke, 10, 0, ApplyTool) == 0 && TotalFunds == 5);
    pm_stroke_begin(&stroke, 0, 0);
    assert(pm_stroke_move(&stroke, -1, 0, ApplyTool) == 0 && !stroke.active);
    puts("PASS construction: real road/rail/wire costs, diagonal connectivity, bulldozer, retrace, funds exhaustion and bounds");
}

static void check_stage1(void)
{
    char status[120], statusBefore[120];
    char logBefore[2048];
    int i, yearBefore, monthBefore, ticksBefore;

    /* Intentional zero funding in an ordinary save is not a scenario default. */
    RoadPercent = PolicePercent = FirePercent = 0;
    assert(saveFile("build/check/zero-funding.scn"));
    RoadPercent = PolicePercent = FirePercent = 1;
    assert(loadFile("build/check/zero-funding.scn"));
    assert(RoadPercent == 0 && PolicePercent == 0 && FirePercent == 0);

    /* A displayed year now spans 48 ticks, the same units as a classic save. */
    CityTime = 0; CityYear = 1900; CityMonth = 0;
    for (i = 0; i < 3; ++i) DoTimeStuff();
    assert(CityMonth == 0 && CityYear == 1900);
    DoTimeStuff(); assert(CityMonth == 1);
    for (i = 0; i < 44; ++i) DoTimeStuff();
    assert(CityTime == 48 && CityYear == 1901 && CityMonth == 0);
    for (i = 0; i < 17; ++i) DoTimeStuff();
    yearBefore = CityYear; monthBefore = CityMonth; ticksBefore = CityTime;
    assert(saveFile("build/check/calendar.scn"));
    CityYear = 9999; CityMonth = 11; CityTime = 0;
    assert(loadFile("build/check/calendar.scn"));
    assert(CityYear == yearBefore && CityMonth == monthBefore && CityTime == ticksBefore);
    for (i = 0; i < 3; ++i) DoTimeStuff();
    assert(CityYear == 1901 && CityMonth == 5 && CityTime == 68);
    puts("PASS funding/calendar: zero funding survives ordinary save, four ticks/month, 48/year, reload continues date");

    ClearMes(); mp_test_clear_log();
    SendMesAt(-43, 10, 20); doMessage();
    assert(strstr(mp_test_game_log(), "ALERT: NUCLEAR MELTDOWN! at (10,20)"));
    strcpy(logBefore, mp_test_game_log());
    for (i = 0; i < 200; ++i) doMessage();
    assert(!strcmp(logBefore, mp_test_game_log()));
    assert(!strstr(mp_test_game_log(), "CITY: A Nuclear"));
    ClearMes(); assert(MesNum == 0 && MessagePort == 0);
    SendMes(1); doMessage();
    strcpy(logBefore, mp_test_game_log());
    for (i = 0; i < 200; ++i) doMessage();
    assert(!strcmp(logBefore, mp_test_game_log()));
    SendMesAt(-43, 20, 30);
    assert(loadFile("build/check/calendar.scn"));
    assert(MessagePort == 0 && MesNum == 0);
    puts("PASS messages: one log per arrival, no repeated text/picture replay, load clears pending alerts");

    mp_test_status(statusBefore);
    for (i = 0; i < 100; ++i) addGameLog("status regression %d\r\nsecond line", i);
    mp_test_status(status);
    assert(!strcmp(status, statusBefore));
    assert(!strchr(status, '\r') && !strchr(status, '\n'));
    assert(strlen(status) < sizeof(status));
    assert(!strstr(status, "status regression"));
    puts("PASS status: single bounded line independent of multiline/rolling game log");
}

int mp_selftest(void)
{
    static short snapshot[WORLD_X][WORLD_Y];
    MPBITMAP *bmp;
    SimSprite *retired;
    int x, y, i, water, trees, timeBefore;
    long funds;
    FILE *fp;
    unsigned char bytes[4];
    GenerateMap(12345);
    water = 0; trees = 0;
    for (x = 0; x < WORLD_X; ++x) for (y = 0; y < WORLD_Y; ++y) {
        i = Map[x][y] & LOMASK;
        assert(i < 960);
        if (i >= RIVER && i < TREEBASE) ++water;
        if (i >= TREEBASE && i <= LASTTREE) ++trees;
    }
    assert(water > 0 && trees > 0);
    printf("PASS terrain: %d water, %d tree tiles\n", water, trees);

    memset(Map, 0, sizeof(Map));
    TotalFunds = 50000;
    SelectTool(residentialState);
    assert(ApplyTool(40, 40) == TOOLRESULT_OK);
    assert(Map[40][40] & ZONEBIT);
    assert(TotalFunds < 50000);
    assert(!(Map[42][42] & ZONEBIT));
    SelectTool(roadState);
    assert(ApplyTool(40, 42) == TOOLRESULT_OK);
    assert((Map[40][42] & LOMASK) >= ROADBASE);
    puts("PASS tools: residential footprint, flags, cost, road");

    TotalFunds = 0x1234fedcL; CityTime = 0x12345;
    RoadPercent = 0.5f; PolicePercent = 0.25f; FirePercent = 0.75f;
    ResHis[0] = 1234; ResHis[239] = -42;
    memcpy(snapshot, Map, sizeof(Map));
    assert(saveFile("build/check/roundtrip.scn"));
    fp = fopen("build/check/roundtrip.scn", "rb"); assert(fp);
    assert(!fseek(fp, 2880L + 100L, SEEK_SET));
    assert(fread(bytes, 1, 4, fp) == 4); fclose(fp);
    assert(bytes[0] == 0x12 && bytes[1] == 0x34 && bytes[2] == 0xfe && bytes[3] == 0xdc);
    TotalFunds = 0; memset(Map, 0, sizeof(Map));
    assert(loadFile("build/check/roundtrip.scn"));
    assert(TotalFunds == 0x1234fedcL && CityTime == 0x12345);
    assert(!memcmp(snapshot, Map, sizeof(Map)));
    assert(ResHis[0] == 1234 && ResHis[239] == -42);
    assert(RoadPercent == 0.5f && PolicePercent == 0.25f && FirePercent == 0.75f);
    TotalFunds = -40000; assert(saveFile("build/check/roundtrip.scn"));
    assert(loadFile("build/check/roundtrip.scn") && TotalFunds == -40000);
    fp = fopen("build/check/truncated.scn", "wb"); assert(fp);
    fputs("bad", fp); fclose(fp);
    funds = TotalFunds;
    assert(!loadFile("build/check/truncated.scn"));
    assert(TotalFunds == funds && !memcmp(snapshot, Map, sizeof(Map)));
    puts("PASS city I/O: endian bytes, signed funds, flags, history, funding, failed-load preservation");

    bmp = mp_bmp_load("tilesets/default.bmp"); assert(bmp);
    assert(mp_bmp_width(bmp) == 512 && mp_bmp_height(bmp) == 480); mp_bmp_free(bmp);
    bmp = mp_bmp_load("assets/airplane-0.bmp"); assert(bmp);
    assert(mp_bmp_width(bmp) == 48); mp_bmp_free(bmp);
    bmp = mp_bmp_load("assets/resident.bmp"); assert(bmp); mp_bmp_free(bmp);
    puts("PASS bitmap decoding: current tileset, toolbar, 24-bit sprite");
    bmp = mp_bmp_load("tilesets/default.bmp"); assert(bmp);
    assert(mp_bmp_width(bmp) == 512 && mp_bmp_height(bmp) == 480);
    mp_bmp_free(bmp);
    bmp = mp_bmp_load("tilesets/classic.bmp"); assert(bmp);
    assert(mp_bmp_width(bmp) == 512 && mp_bmp_height(bmp) == 480);
    mp_bmp_free(bmp);
    bmp = mp_bmp_load("tilesets/classic95.bmp"); assert(bmp);
    assert(mp_bmp_width(bmp) == 512 && mp_bmp_height(bmp) == 480);
    mp_bmp_free(bmp);
    bmp = mp_bmp_load("tilesets/ancientasia.bmp"); assert(bmp);
    assert(mp_bmp_width(bmp) == 512 && mp_bmp_height(bmp) == 480);
    mp_bmp_free(bmp);
    bmp = mp_bmp_load("tilesets/dux.bmp"); assert(bmp);
    assert(mp_bmp_width(bmp) == 512 && mp_bmp_height(bmp) == 480);
    mp_bmp_free(bmp);
    bmp = mp_bmp_load("tilesets/futureeurope.bmp"); assert(bmp);
    assert(mp_bmp_width(bmp) == 512 && mp_bmp_height(bmp) == 480);
    mp_bmp_free(bmp);
    bmp = mp_bmp_load("tilesets/futureusa.bmp"); assert(bmp);
    assert(mp_bmp_width(bmp) == 512 && mp_bmp_height(bmp) == 480);
    mp_bmp_free(bmp);
    bmp = mp_bmp_load("tilesets/medievaltimes.bmp"); assert(bmp);
    assert(mp_bmp_width(bmp) == 512 && mp_bmp_height(bmp) == 480);
    mp_bmp_free(bmp);
    bmp = mp_bmp_load("tilesets/mooncolony.bmp"); assert(bmp);
    assert(mp_bmp_width(bmp) == 512 && mp_bmp_height(bmp) == 480);
    mp_bmp_free(bmp);
    bmp = mp_bmp_load("tilesets/wildwest.bmp"); assert(bmp);
    assert(mp_bmp_width(bmp) == 512 && mp_bmp_height(bmp) == 480);
    mp_bmp_free(bmp);
    puts("PASS all ten tilesets decode at 512x480");

    check_15bit_growth();
    check_overwrite();
    check_simulation_speeds();
    check_city_report();
    check_new_cities();
    check_strokes();
    check_stage1();
    for (i = 1; i <= 8; ++i) {
        assert(loadScenario(i));
        assert(ScenarioID == i && ScoreType == i && ScoreWait > 0);
        assert(RoadPercent == 1 && PolicePercent == 1 && FirePercent == 1);
        assert(CityTax == 7 && AutoBudget == 1 && GameLevel == 0);
        assert(CityYear == 1900 + CityTime / 48 && CityMonth == (CityTime % 48) / 4);
        assert(MessagePort == 0 && MesNum == 0);
        assert(TotalFunds == (i == 1 ? 5000 : 20000));
        timeBefore = CityTime;
        for (x = 0; x < 512; ++x) SimFrame();
        assert(CityTime > timeBefore);
        assert(CityYear == 1900 + CityTime / 48 && CityMonth == (CityTime % 48) / 4);
        if (i == 7) assert(DisasterEvent == 0 && DisasterWait == 0);
    }
    puts("PASS all eight scenarios and simulation progression");
    InitSprites();
    retired = NewSprite(SPRITE_MONSTER, 500, 500); assert(retired);
    retired->frame = 0;
    MoveSprites();
    assert(retired->type == SPRITE_UNDEFINED);
    puts("PASS retired sprite cleanup");
    {
        Notification alert;
        const Notification *saved;
        extern const Notification *mp_test_last_alert(void);
        extern int mp_test_alert_popups(void);
        extern void mp_pm_clear_notification(void);
        mp_pm_clear_notification();
        memset(&alert, 0, sizeof(alert));
        alert.id = 20; alert.hasLocation = 1;
        alert.locationX = 0; alert.locationY = 0;
        strcpy(alert.message, "Test fire");
        CreateNotificationDialog(&alert);
        saved = mp_test_last_alert();
        assert(saved && saved->hasLocation && saved->explanation[0] && saved->advice[0]);
        strcpy(alert.message, "Changed caller");
        assert(strcmp(saved->message, "Test fire") == 0);
        alert.locationX = WORLD_X;
        CreateNotificationDialog(&alert);
        assert(!mp_test_last_alert()->hasLocation);
        assert(mp_test_alert_popups() == 1);
        ClearMes();
        CreateNotificationDialog(&alert);
        assert(mp_test_alert_popups() == 1);
        alert.id = 21;
        CreateNotificationDialog(&alert);
        assert(mp_test_alert_popups() == 2);
        mp_pm_clear_notification();
        assert(mp_test_last_alert() == NULL);
        CreateNotificationDialog(&alert);
        assert(mp_test_alert_popups() == 1);
        mp_pm_clear_notification();
        puts("PASS alerts: owned copy, details/advice, location bounds and city reset");
    }

    {
        SimSprite *monster;
        int mx, my, step;
        InitSprites();
        for (mx = 0; mx < WORLD_X; ++mx)
            for (my = 0; my < WORLD_Y; ++my) Map[mx][my] = RIVER;
        monster = NewSprite(SPRITE_MONSTER, 20 * 16, 20 * 16);
        assert(monster != NULL);
        monster->dest_x = 90 * 16; monster->dest_y = 70 * 16;
        assert(monster->water_phase == 1);
        for (step = 0; step < MONSTER_WATER_TICKS + 20; ++step) {
            DoMonsterSprite(monster);
            assert(monster->frame > 0 && !monster->flag);
        }
        MakeMonster();
        assert(GetSpriteCount() == 1 && GetSprite(SPRITE_MONSTER) == monster);
        monster->flag = 1;
        monster->dest_x = 90 * 16; monster->dest_y = 70 * 16;
        DoMonsterSprite(monster); assert(monster->frame > 0 && monster->water_phase == 2);
        for (step = 0; step < MONSTER_WATER_TICKS; ++step) DoMonsterSprite(monster);
        assert(monster->frame == 0);
        InitSprites();
        monster = NewSprite(SPRITE_MONSTER, 20 * 16, 20 * 16);
        monster->dest_x = 90 * 16; monster->dest_y = 70 * 16;
        monster->water_phase = 0;
        monster->count = 1;
        DoMonsterSprite(monster); assert(monster->frame > 0 && monster->water_phase == 2);
        MakeMonster(); assert(monster->water_phase == 1 && !monster->flag);
        for (step = 0; step < MONSTER_WATER_TICKS; ++step) DoMonsterSprite(monster);
        assert(monster->frame > 0);
        InitSprites();
        puts("PASS monster: survives water emergence, single instance, return/expiry submergence");
    }

    ClearChartData();
    assert(g_chartData->monthlyCount == 0 && g_chartData->yearlyCount == 0);
    CityTime = 4; ResPop = 80;
    UpdateChartData();
    assert(g_chartData->monthlyCount == 1 && g_chartData->yearlyCount == 0);
    assert(g_chartData->series[CHART_RESIDENTIAL].monthlyData[0] == 10);
    CityTime = 48; ResPop = 160;
    UpdateChartData();
    assert(g_chartData->monthlyCount == 2 && g_chartData->yearlyCount == 1);
    assert(g_chartData->series[CHART_RESIDENTIAL].monthlyData[1] == 10);
    assert(g_chartData->series[CHART_RESIDENTIAL].yearlyData[120] == 20);
    for (i = 0; i < 130; ++i) { CityTime += 48; UpdateChartData(); }
    assert(g_chartData->monthlyCount == 120 && g_chartData->yearlyCount == 120);
    ClearChartData();
    assert(g_chartData->monthlyCount == 0 && g_chartData->yearlyCount == 0);
    assert(g_chartData->series[CHART_RESIDENTIAL].yearlyData[120] == 0);
    puts("PASS chart history: monthly/yearly counts, newest-first order, bounds, reset");
    CleanupChartSystem();
    puts("All hosted C89 core checks passed (PM rendering requires target runtime).");
    return 0;
}

