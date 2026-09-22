/*
 * M29N2b.2 test-fixture helper.
 *
 * The recovered prerelease C/386 toolchain does not ship the small SETENTRY /
 * DLLINIT object used by Microsoft's documented custom-DLL-startup recipe.
 * This utility performs the equivalent final-header operation for the lifecycle
 * regression: it points the LE/LX library entry at an already-exported ordinal
 * and marks initialization/termination as per-process.
 *
 * It is intentionally a build helper only.  OS2HOST32 itself never requires or
 * modifies guest binaries at run time.
 */
#include <stdio.h>
#include <stdlib.h>

static unsigned short rd16(const unsigned char *p)
{
    return (unsigned short)(p[0] | ((unsigned short)p[1] << 8));
}

static unsigned long rd32(const unsigned char *p)
{
    return (unsigned long)p[0] |
           ((unsigned long)p[1] << 8) |
           ((unsigned long)p[2] << 16) |
           ((unsigned long)p[3] << 24);
}

static void wr32(unsigned char *p, unsigned long v)
{
    p[0] = (unsigned char)(v & 255UL);
    p[1] = (unsigned char)((v >> 8) & 255UL);
    p[2] = (unsigned char)((v >> 16) & 255UL);
    p[3] = (unsigned char)((v >> 24) & 255UL);
}

static int find_ordinal(const unsigned char *b, unsigned long n,
                        unsigned long h, unsigned long wanted,
                        unsigned long *obj, unsigned long *off)
{
    unsigned long p, ordinal, i;
    unsigned int count, type;
    unsigned long object;

    p = h + rd32(b + h + 0x5c);
    if (p >= n)
        return 0;
    ordinal = 1;
    while (p < n) {
        count = b[p++];
        if (count == 0)
            return 0;
        if (p >= n)
            return 0;
        type = b[p++] & 0x7f;
        if (type == 0) {
            ordinal += count;
            continue;
        }
        if (p + 2 > n)
            return 0;
        object = rd16(b + p);
        p += 2;
        for (i = 0; i < (unsigned long)count; ++i, ++ordinal) {
            if (type == 1) {
                if (p + 3 > n)
                    return 0;
                ++p; /* entry flags */
                if (ordinal == wanted) {
                    *obj = object;
                    *off = rd16(b + p);
                    return 1;
                }
                p += 2;
            } else if (type == 3) {
                if (p + 5 > n)
                    return 0;
                ++p; /* entry flags */
                if (ordinal == wanted) {
                    *obj = object;
                    *off = rd32(b + p);
                    return 1;
                }
                p += 4;
            } else if (type == 2) {
                /* 286 call gate: flags + offset16 + callgate selector16 */
                if (p + 5 > n)
                    return 0;
                p += 5;
            } else if (type == 4) {
                if (p + 7 > n)
                    return 0;
                p += 7;
            } else {
                return 0;
            }
        }
    }
    return 0;
}

int main(int argc, char **argv)
{
    FILE *f;
    unsigned char *b;
    long size;
    unsigned long n, h, ord, obj, off, flags;

    if (argc != 3) {
        fprintf(stderr, "usage: m29n2b2-setentry DLL ordinal\n");
        return 2;
    }
    ord = strtoul(argv[2], 0, 0);
    if (ord == 0) {
        fprintf(stderr, "entry ordinal must be nonzero\n");
        return 2;
    }

    f = fopen(argv[1], "rb");
    if (!f) {
        perror(argv[1]);
        return 1;
    }
    if (fseek(f, 0, SEEK_END) != 0 || (size = ftell(f)) < 0 ||
        fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return 1;
    }
    n = (unsigned long)size;
    b = (unsigned char *)malloc((size_t)n);
    if (!b) {
        fclose(f);
        return 1;
    }
    if (fread(b, 1, (size_t)n, f) != (size_t)n) {
        free(b); fclose(f); return 1;
    }
    fclose(f);

    if (n < 0x40UL || b[0] != 'M' || b[1] != 'Z') {
        fprintf(stderr, "%s: missing MZ header\n", argv[1]);
        free(b); return 1;
    }
    h = rd32(b + 0x3c);
    if (h + 0xa4UL > n ||
        !((b[h] == 'L' && b[h+1] == 'E') ||
          (b[h] == 'L' && b[h+1] == 'X'))) {
        fprintf(stderr, "%s: not an LE/LX image\n", argv[1]);
        free(b); return 1;
    }
    if (!find_ordinal(b, n, h, ord, &obj, &off)) {
        fprintf(stderr, "%s: export ordinal %lu not found\n", argv[1], ord);
        free(b); return 1;
    }

    flags = rd32(b + h + 0x10);
    flags |= 0x00000004UL;   /* per-process library initialization */
    flags |= 0x40000000UL;   /* per-process library termination */
    wr32(b + h + 0x10, flags);
    wr32(b + h + 0x18, obj);
    wr32(b + h + 0x1c, off);

    f = fopen(argv[1], "wb");
    if (!f) {
        perror(argv[1]);
        free(b); return 1;
    }
    if (fwrite(b, 1, (size_t)n, f) != (size_t)n) {
        fclose(f); free(b); return 1;
    }
    fclose(f);
    free(b);

    printf("%s: DLL entry = object %lu + 0x%08lX, flags=0x%08lX\n",
           argv[1], obj, off, flags);
    return 0;
}
