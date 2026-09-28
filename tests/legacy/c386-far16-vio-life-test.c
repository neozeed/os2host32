/*
 * c386-far16-vio-life-test.c - narrow C/386 bridge regression for the four
 * VIO calls added by the Life acceptance milestone.
 */

typedef unsigned char UCHAR;
typedef unsigned short USHORT;
typedef unsigned long ULONG;

struct VIOCURSORINFO16 {
    USHORT yStart;
    USHORT cEnd;
    USHORT cx;
    USHORT attr;
};

USHORT _far16 _pascal DOSSLEEP(ULONG milliseconds);
USHORT _far16 _pascal VIOWRTCHARSTR(char _far16 *text, USHORT count,
                                     USHORT row, USHORT col, USHORT hvio);
USHORT _far16 _pascal VIOGETCURTYPE(struct VIOCURSORINFO16 _far16 *cursor,
                                     USHORT hvio);
USHORT _far16 _pascal VIOSETCURTYPE(struct VIOCURSORINFO16 _far16 *cursor,
                                     USHORT hvio);
USHORT _far16 _pascal VIOWRTCHARSTRATT(char _far16 *text, USHORT count,
                                        USHORT row, USHORT col,
                                        UCHAR _far16 *attribute,
                                        USHORT hvio);

int main(void)
{
    static char plain[] = "VIO13";
    static char colored[] = "48";
    static UCHAR attribute = 0x1eU;
    struct VIOCURSORINFO16 saved;
    struct VIOCURSORINFO16 hidden;
    USHORT rc;

    rc = VIOGETCURTYPE(&saved, 0U);
    if (rc != 0U)
        return (int)rc;

    hidden = saved;
    hidden.attr = 0xffffU;
    rc = VIOSETCURTYPE(&hidden, 0U);
    if (rc != 0U)
        return (int)rc;

    rc = VIOWRTCHARSTR(plain, (USHORT)(sizeof(plain) - 1U), 0U, 0U, 0U);
    if (rc != 0U)
        return (int)rc;

    rc = VIOWRTCHARSTRATT(colored, (USHORT)(sizeof(colored) - 1U),
                          0U, 6U, &attribute, 0U);
    if (rc != 0U)
        return (int)rc;

    rc = DOSSLEEP(1UL);
    if (rc != 0U)
        return (int)rc;

    return (int)VIOSETCURTYPE(&saved, 0U);
}
