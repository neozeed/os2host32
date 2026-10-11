/* Bounded DOS path helpers for the OPENDLG-style PM browser. C89. */
#ifndef PM_PATH_H
#define PM_PATH_H
#include <string.h>
static int pm_path_join(char *out, unsigned int cap, const char *dir, const char *name)
{
    unsigned int n, m, src, dst, start, len;
    char temp[260];
    if ((name[0] == '\\' || name[0] == '/') &&
        (name[1] == '\\' || name[1] == '/')) return 0;
    if (!name[0] || strchr(name, '*') || strchr(name, '?')) return 0;
    if (name[1] == ':') {
        if (name[2] != '\\' && name[2] != '/') return 0;
        n = 0;
    } else if (name[0] == '\\' || name[0] == '/') {
        if (strlen(dir) < 2 || dir[1] != ':') return 0;
        temp[0] = dir[0]; temp[1] = ':'; n = 2;
    } else {
        n = strlen(dir);
        if (n + 1 >= sizeof(temp)) return 0;
        memcpy(temp, dir, n);
        if (n && temp[n-1] != '\\' && temp[n-1] != '/') temp[n++] = '\\';
    }
    m = strlen(name);
    if (n + m >= cap || n + m >= sizeof(temp)) return 0;
    memcpy(temp+n, name, m+1);
    for (n = 0; temp[n]; ++n) if (temp[n] == '/') temp[n] = '\\';
    if (strlen(temp) < 3 || temp[1] != ':' || temp[2] != '\\') return 0;
    /* Collapse dot segments without touching any process/drive directory. */
    src = dst = 3;
    while (temp[src]) {
        while (temp[src] == '\\') ++src;
        start = src;
        while (temp[src] && temp[src] != '\\') ++src;
        len = src - start;
        if (!len || (len == 1 && temp[start] == '.')) continue;
        if (len == 2 && temp[start] == '.' && temp[start+1] == '.') {
            while (dst > 3 && temp[dst-1] != '\\') --dst;
            if (dst > 3) --dst;
        } else {
            if (dst > 3) temp[dst++] = '\\';
            memmove(temp+dst, temp+start, len); dst += len;
        }
    }
    temp[dst] = 0;
    strcpy(out, temp); return 1;
}
static void pm_path_parent(char *path)
{
    unsigned int n;
    n = strlen(path);
    while (n > 3 && (path[n-1] == '\\' || path[n-1] == '/')) path[--n] = 0;
    while (n > 3 && path[n-1] != '\\' && path[n-1] != '/') path[--n] = 0;
    if (n > 3) path[n-1] = 0;
}
#endif

