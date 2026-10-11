/* OS/2 services for the current WinTown simulation. */
#include "sim.h"
#include "tools.h"
#include "notify.h"
#include "resource.h"
#include "cityio.h"
#include <stdio.h>
#include <string.h>

extern void addGameLog(const char *format, ...);
int disastersDisabled = 0;
int AutoGo = 0;
BOOL isToolDragging = FALSE;

void SetGameSpeed(int speed) { SetSimulationSpeed(hwndMain, speed); }
void SetGameLevel(int level)
{
    if (level < LEVEL_EASY) level = LEVEL_EASY;
    if (level > LEVEL_HARD) level = LEVEL_HARD;
    GameLevel = level;
}

void DoEarthQuake(void)
{
    /* The core applies damage; the NT callback only shakes the view. */
    addGameLog("Earthquake!");
    mp_ui_invalidate();
}

void CreateNotificationDialog(Notification *n)
{
    if (!n) return;
    /* doMessage already appended the alert once; a rich PM dialog is a
     * later UI stage. Do not duplicate the same event in the game log. */
    mp_ui_invalidate();
    /* Keep the simulator's timer out of a nested modal message loop. */
}

int ShowBudgetWindowAndWait(HWND hwnd)
{
    char text[400];
    sprintf(text, "Funds: $%ld\nTax income: $%ld\nRoads: $%ld\n"
            "Police: $%ld\nFire: $%ld\n\n"
            "Current allocations will be used.\nTax %d%%; funding R/F/P %d/%d/%d%%",
            (long)TotalFunds, (long)TaxFund, (long)RoadFund,
            (long)PoliceFund, (long)FireFund, CityTax,
            (int)(RoadPercent * 100), (int)(FirePercent * 100),
            (int)(PolicePercent * 100));
    addGameLog("Budget: funds $%ld, income $%ld", (long)TotalFunds, (long)TaxFund);
    mp_ui_message(hwnd, text, "WinTown/2 Budget");
    return 0;
}

int loadScenarioFromResource(int id, char *name)
{
    static const char *files[8] = {
        "bern", "boston", "detroit", "dullsville", "hamburg", "rio",
        "sanfrancisco", "tokyo"
    };
    char path[96];
    int index;
    (void)name;
    index = id - IDR_SCENARIO_BERN;
    if (index < 0 || index >= 8) return 0;
    sprintf(path, "cities/%s.scn", files[index]);
    return mp_load_scenario_file(path);
}
