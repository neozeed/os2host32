/* Decode standard classes in packed OS/2 dialog templates. */
#ifndef PM_DIALOG_CLASS_H
#define PM_DIALOG_CLASS_H
#include <string.h>
static unsigned int pm_dialog_class(const unsigned char *blob,
    unsigned long size, unsigned int count, unsigned int offset)
{
    unsigned int i, value;
    char name[32];
    if (!count) return offset;
    if (count >= sizeof(name) || offset > size || count > size-offset) return 0;
    memcpy(name, blob+offset, count); name[count] = 0;
    /* RC may store the public class name (for example "#7") rather than
       an ordinal in offClassName. Do not interpret an offset as a class. */
    if (name[0] == '#') {
        value = 0;
        for (i = 1; name[i]; ++i) {
            if (name[i] < '0' || name[i] > '9' || value > 6553U) return 0;
            value = value * 10 + (unsigned int)(name[i]-'0');
            if (value > 65535U) return 0;
        }
        return i > 1 ? value : 0;
    }
    return 0;
}
#endif
