#ifndef PM_CITYIO_H
#define PM_CITYIO_H

int loadFile(char *name);
int saveFile(char *name);
/* Scenario resources contain a map/history, not reliable player settings. */
int mp_load_scenario_file(char *name);

#endif

