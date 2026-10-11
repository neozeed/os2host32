#include "../../dlls/pmwin/pm_dialog_class.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    const unsigned char names[] = "#7\0#6\0#3\0#65536\0#x\0";
    assert(pm_dialog_class(names, sizeof(names), 0, 7) == 7);
    assert(pm_dialog_class(names, sizeof(names), 2, 0) == 7);
    assert(pm_dialog_class(names, sizeof(names), 3, 0) == 7);
    assert(pm_dialog_class(names, sizeof(names), 2, 3) == 6);
    assert(pm_dialog_class(names, sizeof(names), 2, 6) == 3);
    assert(pm_dialog_class(names, sizeof(names), 6, 9) == 0);
    assert(pm_dialog_class(names, sizeof(names), 2, 16) == 0);
    assert(pm_dialog_class(names, sizeof(names), 2, sizeof(names)) == 0);
    puts("PASS dialog class ordinals, named classes, bounds and malformed values");
    return 0;
}
