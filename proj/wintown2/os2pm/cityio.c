/* Classic 27120-byte city format. Decode bytes explicitly: the hosted
 * verifier may have 64-bit longs; the target has 32-bit longs. */
#include "sim.h"
#include "charts.h"
#include "cityio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CITY_BYTES 27120
extern void DoSimInit(void);
extern void ForceFullCensus(void);
extern char cityFileName[MAX_PATH];

static unsigned short get16(const unsigned char *p)
{
    return (unsigned short)(((unsigned int)p[0] << 8) | p[1]);
}

static void put16(unsigned char *p, unsigned short v)
{
    p[0] = (unsigned char)(v >> 8); p[1] = (unsigned char)v;
}

static long get32(const short *p)
{
    unsigned long v;
    v = ((unsigned long)(unsigned short)p[0] << 16) | (unsigned short)p[1];
    if (v & 0x80000000UL) return -1L - (long)(0xffffffffUL - v);
    return (long)v;
}

static void put32(short *p, long v)
{
    p[0] = (short)((unsigned long)v >> 16); p[1] = (short)v;
}

static int load_city_data(char *name, int scenario)
{
    FILE *fp;
    unsigned char *data, *p;
    short *history[7];
    int i, j, count, x, y, speed;
    long bytes;
    fp = fopen(name, "rb");
    if (!fp) return 0;
    if (fseek(fp, 0L, SEEK_END) || (bytes = ftell(fp)) != CITY_BYTES ||
        fseek(fp, 0L, SEEK_SET)) { fclose(fp); return 0; }
    data = (unsigned char *)malloc(CITY_BYTES);
    if (!data) { fclose(fp); return 0; }
    if (fread(data, 1, CITY_BYTES, fp) != CITY_BYTES) {
        free(data); fclose(fp); return 0;
    }
    fclose(fp);
    /* Commit only after the complete input is available. */
    CleanupSimTimer(hwndMain);
    ClearCensus(); TotalPop = 0; CityPop = 0;
    DoSimInit();
    history[0] = ResHis; history[1] = ComHis; history[2] = IndHis;
    history[3] = CrimeHis; history[4] = PollutionHis;
    history[5] = MoneyHis; history[6] = MiscHis;
    p = data;
    for (i = 0; i < 7; ++i) {
        count = i == 6 ? MISCHISTLEN / 2 : HISTLEN / 2;
        for (j = 0; j < count; ++j, p += 2) history[i][j] = (short)get16(p);
    }
    for (x = 0; x < WORLD_X; ++x) for (y = 0; y < WORLD_Y; ++y, p += 2)
        Map[x][y] = (short)get16(p);
    free(data);
    if (scenario) {
        /* Classic scenario resources have stale application-specific data
         * in MiscHis. Keep the defaults established by DoSimInit/InitBudget;
         * scenario.c supplies the authoritative year and starting funds. */
        memset(MiscHis, 0, sizeof(MiscHis));
        AutoBulldoze = 1; AutoGo = 0;
        speed = SPEED_MEDIUM;
    } else {
        TotalFunds = get32(MiscHis + 50);
        CityTime = (int)get32(MiscHis + 8);
        if (CityTime < 0) CityTime = 0;
        CityYear = 1900 + CityTime / 48;
        CityMonth = (CityTime % 48) / 4;
        EMarket = (float)MiscHis[1];
        ResPop = MiscHis[2]; ComPop = MiscHis[3]; IndPop = MiscHis[4];
        RValve = MiscHis[5]; CValve = MiscHis[6]; IValve = MiscHis[7];
        CrimeRamp = MiscHis[10]; PolluteRamp = MiscHis[11];
        LVAverage = MiscHis[12]; CrimeAverage = MiscHis[13];
        PolluteAverage = MiscHis[14];
        GameLevel = MiscHis[15]; CityClass = MiscHis[16]; CityScore = MiscHis[17];
        AutoBulldoze = MiscHis[52] != 0; AutoBudget = MiscHis[53] != 0;
        AutoGo = MiscHis[54] != 0;
        CityTax = MiscHis[56];
        if (CityTax < 0 || CityTax > 20) CityTax = 7;
        if (GameLevel < 0 || GameLevel > 2) GameLevel = 0;
        RoadPercent = get32(MiscHis + 58) / 65536.0f;
        PolicePercent = get32(MiscHis + 60) / 65536.0f;
        FirePercent = get32(MiscHis + 62) / 65536.0f;
        if (RoadPercent < 0 || RoadPercent > 1) RoadPercent = 1;
        if (PolicePercent < 0 || PolicePercent > 1) PolicePercent = 1;
        if (FirePercent < 0 || FirePercent > 1) FirePercent = 1;
        speed = MiscHis[57];
        if (speed < 0 || speed > 3) speed = SPEED_MEDIUM;
    }
    ScenarioID = ScoreType = ScoreWait = DisasterEvent = DisasterWait = 0;
    strncpy(cityFileName, name, MAX_PATH - 1); cityFileName[MAX_PATH - 1] = 0;
    ForceFullCensus();
    CleanupChartSystem(); InitChartSystem(); UpdateChartData();
    SetSimulationSpeed(hwndMain, speed);
    mp_ui_set_title(name); mp_ui_invalidate();
    return 1;
}

int loadFile(char *name)
{
    return load_city_data(name, 0);
}

int mp_load_scenario_file(char *name)
{
    return load_city_data(name, 1);
}

int saveFile(char *name)
{
    FILE *fp;
    unsigned char *data, *p;
    short *history[7];
    int i, j, count, x, y, ok;
    data = (unsigned char *)malloc(CITY_BYTES);
    if (!data) return 0;
    MiscHis[1] = (short)EMarket;
    MiscHis[2] = ResPop; MiscHis[3] = ComPop; MiscHis[4] = IndPop;
    MiscHis[5] = RValve; MiscHis[6] = CValve; MiscHis[7] = IValve;
    put32(MiscHis + 8, CityTime);
    MiscHis[10] = CrimeRamp; MiscHis[11] = PolluteRamp;
    MiscHis[12] = LVAverage; MiscHis[13] = CrimeAverage;
    MiscHis[14] = PolluteAverage; MiscHis[15] = (short)GameLevel;
    MiscHis[16] = (short)CityClass; MiscHis[17] = CityScore;
    put32(MiscHis + 50, TotalFunds);
    MiscHis[52] = (short)AutoBulldoze; MiscHis[53] = (short)AutoBudget;
    MiscHis[54] = (short)AutoGo; MiscHis[56] = CityTax;
    MiscHis[57] = (short)SimSpeed;
    put32(MiscHis + 58, (long)(RoadPercent * 65536.0f));
    put32(MiscHis + 60, (long)(PolicePercent * 65536.0f));
    put32(MiscHis + 62, (long)(FirePercent * 65536.0f));
    history[0] = ResHis; history[1] = ComHis; history[2] = IndHis;
    history[3] = CrimeHis; history[4] = PollutionHis;
    history[5] = MoneyHis; history[6] = MiscHis;
    p = data;
    for (i = 0; i < 7; ++i) {
        count = i == 6 ? MISCHISTLEN / 2 : HISTLEN / 2;
        for (j = 0; j < count; ++j, p += 2) put16(p, (unsigned short)history[i][j]);
    }
    for (x = 0; x < WORLD_X; ++x) for (y = 0; y < WORLD_Y; ++y, p += 2)
        put16(p, (unsigned short)Map[x][y]);
    fp = fopen(name, "wb");
    if (!fp) { free(data); return 0; }
    ok = fwrite(data, 1, CITY_BYTES, fp) == CITY_BYTES;
    if (fclose(fp)) ok = 0;
    free(data);
    return ok;
}
