/*
 * cmdfile.c - OS/2-facing file-command layer for CMD32OS2 (through M29P)
 *
 * C89-oriented semantic reconstruction shaped after the recovered Dec-1991
 * NT CMD cfile.c/cpwork.c boundary:
 *
 *   eCopy   -> cpwork.c copy engine
 *   eDelete -> DelWork
 *   eRename -> RenWork
 *   eMove   -> MoveParse / Move
 *
 * This is not source-identical Microsoft code.  M28A removes direct Win32
 * filesystem calls from this module.  All file operations cross cmdos2.h and
 * therefore DOSCALLS.dll, making this source suitable for an eventual LX CMD.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "cmdfile.h"
#include "cmdos2.h"

#ifndef MAX_PATH
#define MAX_PATH 260
#endif

#define CF_ARG_MAX 1024

#define CF_COPY_MAX_SOURCES 64
#define CF_COPY_BUF 32768UL

static int cf_is_space(int ch);
static int cf_has_wild(const char *s);
static void cf_error(const char *verb, const char *path, CmdO2Rc e);

static int cf_copy_tokenize(const char *tail, char tok[][CF_ARG_MAX], int max_tok)
{
    const char *p;
    int count;
    int quoted;
    size_t n;

    p = tail;
    count = 0;
    while (*p != '\0') {
        while (*p != '\0' && cf_is_space((unsigned char)*p))
            ++p;
        if (*p == '\0')
            break;
        if (count >= max_tok)
            return -1;
        if (*p == '+') {
            tok[count][0] = '+';
            tok[count][1] = '\0';
            ++count;
            ++p;
            continue;
        }
        quoted = 0;
        n = 0;
        while (*p != '\0') {
            if (*p == '"') {
                quoted = !quoted;
                ++p;
                continue;
            }
            if (!quoted && (cf_is_space((unsigned char)*p) || *p == '+'))
                break;
            if (n + 1 >= CF_ARG_MAX)
                return -1;
            tok[count][n++] = *p++;
        }
        if (quoted)
            return -1;
        tok[count][n] = '\0';
        ++count;
    }
    return count;
}

static int cf_is_switch(const char *s, const char *name)
{
    if (s == NULL || name == NULL)
        return 0;
    if (s[0] != '/' && s[0] != '-')
        return 0;
    ++s;
    while (*s != '\0' && *name != '\0') {
        if (toupper((unsigned char)*s) != toupper((unsigned char)*name))
            return 0;
        ++s;
        ++name;
    }
    return *s == '\0' && *name == '\0';
}

static int cf_write_all(CmdO2Handle h, const unsigned char *buf,
                        unsigned long bytes, CmdO2Rc *perr)
{
    unsigned long done;
    unsigned long wrote;
    CmdO2Rc rc;

    done = 0;
    while (done < bytes) {
        wrote = 0;
        rc = CmdO2Write(h, buf + done, bytes - done, &wrote);
        if (rc != 0 || wrote == 0) {
            if (perr != NULL)
                *perr = rc != 0 ? rc : CMDO2_ERROR_INVALID_PARAMETER;
            return 0;
        }
        done += wrote;
    }
    return 1;
}

static int cf_append_binary_file(CmdO2Handle hout, const char *src)
{
    CmdO2Handle hin;
    unsigned char *buf;
    unsigned long got;
    CmdO2Rc rc;
    int ok;

    /* M29P deliberately keeps the COPY transfer buffer off the C/386 stack.
     * The historical shell has a comparatively small stack and COPY is a
     * long-lived builtin; there is no reason for a 32 KiB transfer window to
     * consume automatic storage. */
    buf = (unsigned char *)malloc((size_t)CF_COPY_BUF);
    if (buf == NULL) {
        fprintf(stderr, "COPY: insufficient memory for transfer buffer.\n");
        return 0;
    }

    rc = CmdO2OpenRead(src, &hin);
    if (rc != 0) {
        free(buf);
        cf_error("COPY", src, rc);
        return 0;
    }
    ok = 1;
    for (;;) {
        got = 0;
        rc = CmdO2Read(hin, buf, CF_COPY_BUF, &got);
        if (rc != 0) {
            cf_error("COPY", src, rc);
            ok = 0;
            break;
        }
        if (got == 0)
            break;
        if (!cf_write_all(hout, buf, got, &rc)) {
            cf_error("COPY", src, rc);
            ok = 0;
            break;
        }
    }
    (void)CmdO2Close(hin);
    free(buf);
    return ok;
}

static int cf_copy_concat_binary(char srcs[][CF_ARG_MAX], int nsrc,
                                 const char *dst)
{
    CmdO2Handle hout;
    CmdO2Rc rc;
    int i;
    int ok;

    rc = CmdO2OpenWriteReplace(dst, &hout);
    if (rc != 0) {
        cf_error("COPY", dst, rc);
        return 0;
    }
    ok = 1;
    for (i = 0; i < nsrc; ++i) {
        if (cf_has_wild(srcs[i])) {
            fprintf(stderr,
                    "COPY: wildcards in concatenated sources are not implemented yet.\n");
            ok = 0;
            break;
        }
        if (!cf_append_binary_file(hout, srcs[i])) {
            ok = 0;
            break;
        }
    }
    (void)CmdO2Close(hout);
    if (!ok)
        (void)CmdO2Delete(dst);
    return ok;
}

static int cf_is_space(int ch)
{
    return ch == ' ' || ch == '\t';
}

static int cf_next_arg(const char **pp, char *out, size_t cap)
{
    const char *p;
    size_t n;
    int quoted;

    p = *pp;
    while (*p != '\0' && cf_is_space((unsigned char)*p))
        ++p;
    if (*p == '\0') {
        if (cap != 0)
            out[0] = '\0';
        *pp = p;
        return 0;
    }

    n = 0;
    quoted = 0;
    while (*p != '\0') {
        if (*p == '"') {
            quoted = !quoted;
            ++p;
            continue;
        }
        if (!quoted && cf_is_space((unsigned char)*p))
            break;
        if (n + 1 >= cap)
            return -1;
        out[n++] = *p++;
    }
    if (quoted)
        return -1;
    out[n] = '\0';
    while (*p != '\0' && cf_is_space((unsigned char)*p))
        ++p;
    *pp = p;
    return 1;
}

static int cf_has_more(const char *p)
{
    while (*p != '\0' && cf_is_space((unsigned char)*p))
        ++p;
    return *p != '\0';
}

static int cf_has_wild(const char *s)
{
    return strchr(s, '*') != NULL || strchr(s, '?') != NULL;
}

static int cf_ci_equal(const char *a, const char *b)
{
    unsigned char ca;
    unsigned char cb;
    while (*a != '\0' && *b != '\0') {
        ca = (unsigned char)toupper((unsigned char)*a);
        cb = (unsigned char)toupper((unsigned char)*b);
        if (ca != cb)
            return 0;
        ++a;
        ++b;
    }
    return *a == '\0' && *b == '\0';
}


static int cf_path_sep(int ch)
{
    return ch == '\\' || ch == '/';
}

/* Convert a DOS/OS2 path into a simple absolute lexical form using the
 * current directory supplied by the backend.  This is deliberately a
 * lexical normalizer rather than a host filesystem canonicalizer: CMD32
 * needs enough identity checking to catch C:\\DIR\\FILE == .\\FILE before
 * COPY opens/truncates the destination, without introducing Win32 APIs into
 * the OS/2-facing file layer. */
static int cf_normalize_path_against_cwd(const char *path, const char *cwd,
                                         char *out, size_t cap)
{
    char merged[MAX_PATH];
    const char *rest;
    size_t n;
    size_t pos;
    size_t seglen;
    const char *seg;
    int drive;

    if (path == NULL || cwd == NULL || out == NULL || cap < 4U)
        return 0;
    if (cwd[0] == '\0' || cwd[1] != ':')
        return 0;

    drive = toupper((unsigned char)cwd[0]);
    merged[0] = '\0';

    if (isalpha((unsigned char)path[0]) && path[1] == ':') {
        if (cf_path_sep((unsigned char)path[2])) {
            if (strlen(path) + 1U > sizeof(merged))
                return 0;
            strcpy(merged, path);
        } else {
            if (toupper((unsigned char)path[0]) != drive)
                return 0;
            n = strlen(cwd);
            if (n + 1U + strlen(path + 2) + 1U > sizeof(merged))
                return 0;
            strcpy(merged, cwd);
            if (n != 0U && !cf_path_sep((unsigned char)merged[n - 1U]))
                strcat(merged, "\\");
            strcat(merged, path + 2);
        }
    } else if (cf_path_sep((unsigned char)path[0])) {
        if (3U + strlen(path) + 1U > sizeof(merged))
            return 0;
        merged[0] = (char)drive;
        merged[1] = ':';
        merged[2] = '\\';
        merged[3] = '\0';
        rest = path;
        while (cf_path_sep((unsigned char)*rest))
            ++rest;
        strcat(merged, rest);
    } else {
        n = strlen(cwd);
        if (n + 1U + strlen(path) + 1U > sizeof(merged))
            return 0;
        strcpy(merged, cwd);
        if (n != 0U && !cf_path_sep((unsigned char)merged[n - 1U]))
            strcat(merged, "\\");
        strcat(merged, path);
    }

    if (!isalpha((unsigned char)merged[0]) || merged[1] != ':')
        return 0;

    if (cap < 4U)
        return 0;
    out[0] = (char)toupper((unsigned char)merged[0]);
    out[1] = ':';
    out[2] = '\\';
    out[3] = '\0';
    pos = 3U;

    rest = merged + 2;
    while (cf_path_sep((unsigned char)*rest))
        ++rest;

    while (*rest != '\0') {
        seg = rest;
        while (*rest != '\0' && !cf_path_sep((unsigned char)*rest))
            ++rest;
        seglen = (size_t)(rest - seg);

        if (seglen == 1U && seg[0] == '.') {
            /* nothing */
        } else if (seglen == 2U && seg[0] == '.' && seg[1] == '.') {
            if (pos > 3U) {
                while (pos > 3U && out[pos - 1U] != '\\')
                    --pos;
                if (pos > 3U)
                    --pos;
                out[pos] = '\0';
            }
        } else if (seglen != 0U) {
            if (pos > 3U) {
                if (pos + 1U >= cap)
                    return 0;
                out[pos++] = '\\';
            }
            if (pos + seglen + 1U > cap)
                return 0;
            memcpy(out + pos, seg, seglen);
            pos += seglen;
            out[pos] = '\0';
        }

        while (cf_path_sep((unsigned char)*rest))
            ++rest;
    }
    return 1;
}

static int cf_paths_same_with_cwd(const char *a, const char *b,
                                  const char *cwd)
{
    char na[MAX_PATH];
    char nb[MAX_PATH];

    if (cf_ci_equal(a, b))
        return 1;
    if (!cf_normalize_path_against_cwd(a, cwd, na, sizeof(na)))
        return 0;
    if (!cf_normalize_path_against_cwd(b, cwd, nb, sizeof(nb)))
        return 0;
    return cf_ci_equal(na, nb);
}

static int cf_paths_same(const char *a, const char *b)
{
    char cwd[MAX_PATH];

    if (cf_ci_equal(a, b))
        return 1;
    if (CmdO2QueryCurrentDir(cwd, (unsigned long)sizeof(cwd)) != 0)
        return 0;
    return cf_paths_same_with_cwd(a, b, cwd);
}

static int cf_copy_default_destination(int nsrc, int saw_plus,
                                       int expect_source, char *dst,
                                       size_t cap)
{
    if (!expect_source && nsrc == 1 && !saw_plus && dst[0] == '\0') {
        if (cap < 2U)
            return 0;
        dst[0] = '.';
        dst[1] = '\0';
        return 1;
    }
    return 0;
}

/* Apply DOS/OS2-style rename wildcards to one filename component.  '*'
 * carries the remaining source characters and '?' carries one source
 * character when available.  The name and extension are handled separately
 * by cf_rename_wild_name(), which gives the common *.TXT -> *.BAK behavior
 * without assuming 8.3-only names. */
static int cf_wild_component(const char *src, size_t src_len,
                             const char *pat, size_t pat_len,
                             char *out, size_t cap, size_t *used)
{
    size_t si;
    size_t pi;
    size_t oi;

    si = 0;
    oi = *used;
    for (pi = 0; pi < pat_len; ++pi) {
        if (pat[pi] == '*') {
            while (si < src_len) {
                if (oi + 1U >= cap)
                    return 0;
                out[oi++] = src[si++];
            }
        } else if (pat[pi] == '?') {
            if (si < src_len) {
                if (oi + 1U >= cap)
                    return 0;
                out[oi++] = src[si++];
            }
        } else {
            if (oi + 1U >= cap)
                return 0;
            out[oi++] = pat[pi];
            if (si < src_len)
                ++si;
        }
    }
    out[oi] = '\0';
    *used = oi;
    return 1;
}

static int cf_rename_wild_name(const char *src, const char *pat,
                               char *out, size_t cap)
{
    const char *sdot;
    const char *pdot;
    size_t sn;
    size_t se;
    size_t pn;
    size_t pe;
    size_t used;

    sdot = strrchr(src, '.');
    pdot = strrchr(pat, '.');
    if (sdot == src)
        sdot = NULL;
    if (pdot == pat)
        pdot = NULL;

    sn = sdot != NULL ? (size_t)(sdot - src) : strlen(src);
    se = sdot != NULL ? strlen(sdot + 1) : 0U;
    pn = pdot != NULL ? (size_t)(pdot - pat) : strlen(pat);
    pe = pdot != NULL ? strlen(pdot + 1) : 0U;

    used = 0U;
    if (!cf_wild_component(src, sn, pat, pn, out, cap, &used))
        return 0;

    if (pdot != NULL) {
        if (used + 2U > cap)
            return 0;
        out[used++] = '.';
        out[used] = '\0';
        if (!cf_wild_component(sdot != NULL ? sdot + 1 : "", se,
                               pdot + 1, pe, out, cap, &used))
            return 0;
    } else if (!cf_has_wild(pat) && sdot != NULL) {
        /* A literal destination without an extension is literal: do not
         * silently retain the source extension. */
    }
    return 1;
}

static int cf_is_directory(const char *path)
{
    char probe[CF_ARG_MAX];
    size_t n;

    if (path == NULL || path[0] == '\0')
        return 0;
    n = strlen(path);
    if (n + 1U > sizeof(probe))
        return 0;
    strcpy(probe, path);

    /* OS/2 file APIs are not entirely consistent about accepting a trailing
     * path separator for an attribute query.  Commands should nevertheless
     * treat directory operands such as bin\ exactly like bin.  Normalize
     * only the probe, preserving filesystem roots and the caller's spelling. */
    while (n > 1U && (probe[n - 1U] == '\\' || probe[n - 1U] == '/')) {
        if (n == 3U && probe[1] == ':' &&
            (probe[2] == '\\' || probe[2] == '/'))
            break;
        probe[--n] = '\0';
    }
    return CmdO2IsDirectory(probe);
}

static const char *cf_basename(const char *path)
{
    const char *a;
    const char *b;
    const char *p;

    a = strrchr(path, '\\');
    b = strrchr(path, '/');
    p = a;
    if (b != NULL && (p == NULL || b > p))
        p = b;
    if (p != NULL)
        return p + 1;
    if (path[0] != '\0' && path[1] == ':')
        return path + 2;
    return path;
}

static int cf_dir_prefix(const char *path, char *out, size_t cap)
{
    const char *a;
    const char *b;
    const char *p;
    size_t n;

    a = strrchr(path, '\\');
    b = strrchr(path, '/');
    p = a;
    if (b != NULL && (p == NULL || b > p))
        p = b;
    if (p == NULL) {
        out[0] = '\0';
        return 1;
    }
    n = (size_t)(p - path + 1);
    if (n + 1 > cap)
        return 0;
    memcpy(out, path, n);
    out[n] = '\0';
    return 1;
}

static int cf_join(char *out, size_t cap, const char *dir, const char *name)
{
    size_t nd;
    size_t nn;
    int need_sep;

    nd = strlen(dir);
    nn = strlen(name);
    need_sep = nd != 0 && dir[nd - 1] != '\\' && dir[nd - 1] != '/';
    if (nd + (need_sep ? 1U : 0U) + nn + 1U > cap)
        return 0;
    strcpy(out, dir);
    if (need_sep)
        strcat(out, "\\");
    strcat(out, name);
    return 1;
}

static void cf_error(const char *verb, const char *path, CmdO2Rc e)
{
    if (e == CMDO2_ERROR_ALREADY_EXISTS || e == CMDO2_ERROR_FILE_EXISTS) {
        fprintf(stderr, "A file with the specified name already exists - %s\n",
                path != NULL ? path : "");
        return;
    }
    if (e == CMDO2_ERROR_FILE_NOT_FOUND || e == CMDO2_ERROR_PATH_NOT_FOUND) {
        fprintf(stderr, "File not found - %s\n", path != NULL ? path : "");
        return;
    }
    if (e == CMDO2_ERROR_ACCESS_DENIED) {
        fprintf(stderr, "Access denied - %s\n", path != NULL ? path : "");
        return;
    }
    fprintf(stderr, "%s failed for %s (OS/2 rc %lu).\n",
            verb, path != NULL ? path : "", (unsigned long)e);
}

static int cf_copy_stream(const char *src, const char *dst)
{
    CmdO2Handle hout;
    CmdO2Rc rc;
    int ok;

    rc = CmdO2OpenWriteReplace(dst, &hout);
    if (rc != 0) {
        cf_error("COPY", dst, rc);
        return 0;
    }
    ok = cf_append_binary_file(hout, src);
    (void)CmdO2Close(hout);
    if (!ok)
        (void)CmdO2Delete(dst);
    return ok;
}

static int cf_copy_one(const char *src, const char *dst_arg)
{
    char dst[CF_ARG_MAX];

    if (cf_is_directory(dst_arg)) {
        if (!cf_join(dst, sizeof(dst), dst_arg, cf_basename(src))) {
            fprintf(stderr, "The path is too long.\n");
            return 0;
        }
    } else {
        if (strlen(dst_arg) + 1 > sizeof(dst)) {
            fprintf(stderr, "The path is too long.\n");
            return 0;
        }
        strcpy(dst, dst_arg);
    }

    if (cf_paths_same(src, dst)) {
        fprintf(stderr, "The file cannot be copied onto itself - %s\n", src);
        return 0;
    }
    if (!cf_copy_stream(src, dst))
        return 0;
    return 1;
}

static int cf_copy_wild(const char *srcpat, const char *dst)
{
    struct CmdO2FindData fd;
    CmdO2FindHandle h;
    CmdO2Rc rc;
    char prefix[CF_ARG_MAX];
    char src[CF_ARG_MAX];
    unsigned long count;

    if (!cf_is_directory(dst)) {
        fprintf(stderr,
                "COPY: wildcard source currently requires a destination directory.\n");
        return -1;
    }
    if (!cf_dir_prefix(srcpat, prefix, sizeof(prefix))) {
        fprintf(stderr, "The path is too long.\n");
        return -1;
    }

    rc = CmdO2FindFirst(srcpat, &h, &fd);
    if (rc != 0) {
        fprintf(stderr, "File not found - %s\n", srcpat);
        return -1;
    }
    count = 0;
    for (;;) {
        if ((fd.attr & CMDO2_ATTR_DIRECTORY) == 0) {
            if (!cf_join(src, sizeof(src), prefix, fd.name)) {
                (void)CmdO2FindClose(h);
                fprintf(stderr, "The path is too long.\n");
                return -1;
            }
            if (!cf_copy_one(src, dst)) {
                (void)CmdO2FindClose(h);
                return -1;
            }
            ++count;
        }
        rc = CmdO2FindNext(h, &fd);
        if (rc != 0)
            break;
    }
    (void)CmdO2FindClose(h);
    if (rc != CMDO2_ERROR_NO_MORE_FILES && rc != 0) {
        cf_error("COPY", srcpat, rc);
        return -1;
    }
    return (int)count;
}

int CmdFileCopy(const char *tail)
{
    char (*tok)[CF_ARG_MAX];
    char (*srcs)[CF_ARG_MAX];
    char dst[CF_ARG_MAX];
    int ntok;
    int i;
    int nsrc;
    int saw_plus;
    int expect_source;
    int count;
    int result;

    /* M29P removes the old ~260 KiB automatic token/source workspace.  That
     * was larger than the complete 60 KiB historical shell stack before COPY
     * had even opened a file. */
    tok = (char (*)[CF_ARG_MAX])malloc((size_t)(CF_COPY_MAX_SOURCES * 3) *
                                      (size_t)CF_ARG_MAX);
    srcs = (char (*)[CF_ARG_MAX])malloc((size_t)CF_COPY_MAX_SOURCES *
                                        (size_t)CF_ARG_MAX);
    if (tok == NULL || srcs == NULL) {
        if (tok != NULL)
            free(tok);
        if (srcs != NULL)
            free(srcs);
        fprintf(stderr, "COPY: insufficient memory for command workspace.\n");
        return 1;
    }

    result = 1;
    ntok = cf_copy_tokenize(tail, tok, CF_COPY_MAX_SOURCES * 3);
    if (ntok <= 0) {
        fprintf(stderr, "The syntax of the command is incorrect.\n");
        goto done;
    }

    nsrc = 0;
    saw_plus = 0;
    expect_source = 1;
    dst[0] = '\0';

    for (i = 0; i < ntok; ++i) {
        if (cf_is_switch(tok[i], "B"))
            continue;
        if (cf_is_switch(tok[i], "A")) {
            fprintf(stderr, "COPY /A text-mode semantics are not implemented yet.\n");
            goto done;
        }
        if ((tok[i][0] == '/' || tok[i][0] == '-') && tok[i][1] != '\0') {
            fprintf(stderr, "Invalid COPY switch - %s\n", tok[i]);
            goto done;
        }
        if (strcmp(tok[i], "+") == 0) {
            if (expect_source || nsrc == 0 || dst[0] != '\0') {
                fprintf(stderr, "The syntax of the command is incorrect.\n");
                goto done;
            }
            saw_plus = 1;
            expect_source = 1;
            continue;
        }

        if (expect_source) {
            if (nsrc >= CF_COPY_MAX_SOURCES) {
                fprintf(stderr, "Too many COPY source files.\n");
                goto done;
            }
            strcpy(srcs[nsrc++], tok[i]);
            expect_source = 0;
            continue;
        }

        if (dst[0] != '\0') {
            fprintf(stderr, "The syntax of the command is incorrect.\n");
            goto done;
        }
        strcpy(dst, tok[i]);
    }

    /* DOS/OS2 COPY permits the destination to be omitted for the ordinary
     * one-source form.  Treat that exactly as an explicit current-directory
     * destination; concatenation keeps its existing explicit-destination
     * requirement because its historical omitted-target semantics differ. */
    (void)cf_copy_default_destination(nsrc, saw_plus, expect_source,
                                      dst, sizeof(dst));

    if (expect_source || nsrc == 0 || dst[0] == '\0') {
        fprintf(stderr, "The syntax of the command is incorrect.\n");
        goto done;
    }

    if (saw_plus) {
        if (nsrc < 2) {
            fprintf(stderr, "The syntax of the command is incorrect.\n");
            goto done;
        }
        if (!cf_copy_concat_binary(srcs, nsrc, dst))
            goto done;
        printf("%10d file(s) copied.\n", nsrc);
        result = 0;
        goto done;
    }

    if (nsrc != 1) {
        fprintf(stderr, "The syntax of the command is incorrect.\n");
        goto done;
    }
    if (cf_has_wild(srcs[0])) {
        count = cf_copy_wild(srcs[0], dst);
        if (count < 0)
            goto done;
    } else {
        if (!cf_copy_one(srcs[0], dst))
            goto done;
        count = 1;
    }
    printf("%10d file(s) copied.\n", count);
    result = 0;

done:
    free(srcs);
    free(tok);
    return result;
}

static int cf_delete_spec(const char *spec)
{
    struct CmdO2FindData fd;
    CmdO2FindHandle h;
    CmdO2Rc rc;
    char prefix[CF_ARG_MAX];
    char full[CF_ARG_MAX];
    int count;

    if (!cf_has_wild(spec)) {
        if (cf_is_directory(spec)) {
            fprintf(stderr, "DEL cannot delete a directory - %s\n", spec);
            return -1;
        }
        rc = CmdO2Delete(spec);
        if (rc != 0) {
            cf_error("DEL", spec, rc);
            return -1;
        }
        return 1;
    }

    if (!cf_dir_prefix(spec, prefix, sizeof(prefix))) {
        fprintf(stderr, "The path is too long.\n");
        return -1;
    }
    rc = CmdO2FindFirst(spec, &h, &fd);
    if (rc != 0) {
        fprintf(stderr, "File not found - %s\n", spec);
        return -1;
    }
    count = 0;
    for (;;) {
        if ((fd.attr & CMDO2_ATTR_DIRECTORY) == 0) {
            if (!cf_join(full, sizeof(full), prefix, fd.name)) {
                (void)CmdO2FindClose(h);
                fprintf(stderr, "The path is too long.\n");
                return -1;
            }
            rc = CmdO2Delete(full);
            if (rc != 0) {
                (void)CmdO2FindClose(h);
                cf_error("DEL", full, rc);
                return -1;
            }
            ++count;
        }
        rc = CmdO2FindNext(h, &fd);
        if (rc != 0)
            break;
    }
    (void)CmdO2FindClose(h);
    if (rc != CMDO2_ERROR_NO_MORE_FILES && rc != 0) {
        cf_error("DEL", spec, rc);
        return -1;
    }
    if (count == 0) {
        fprintf(stderr, "File not found - %s\n", spec);
        return -1;
    }
    return count;
}

int CmdFileDelete(const char *tail)
{
    const char *p;
    char spec[CF_ARG_MAX];
    int r;
    int total;
    int one;

    p = tail;
    total = 0;
    for (;;) {
        r = cf_next_arg(&p, spec, sizeof(spec));
        if (r == 0)
            break;
        if (r < 0) {
            fprintf(stderr, "The syntax of the command is incorrect.\n");
            return 1;
        }
        if (spec[0] == '/') {
            fprintf(stderr, "DEL switches are not implemented yet.\n");
            return 1;
        }
        one = cf_delete_spec(spec);
        if (one < 0)
            return 1;
        total += one;
    }
    if (total == 0) {
        fprintf(stderr, "The syntax of the command is incorrect.\n");
        return 1;
    }
    return 0;
}

static int cf_destination_in_source_dir(const char *src, const char *dstarg,
                                        char *dst, size_t cap)
{
    char prefix[CF_ARG_MAX];

    if (strchr(dstarg, '\\') != NULL || strchr(dstarg, '/') != NULL ||
        (dstarg[0] != '\0' && dstarg[1] == ':')) {
        if (strlen(dstarg) + 1 > cap)
            return 0;
        strcpy(dst, dstarg);
        return 1;
    }
    if (!cf_dir_prefix(src, prefix, sizeof(prefix)))
        return 0;
    return cf_join(dst, cap, prefix, dstarg);
}

int CmdFileRename(const char *tail)
{
    const char *p;
    char src[CF_ARG_MAX];
    char dstarg[CF_ARG_MAX];
    char dst[CF_ARG_MAX];
    char prefix[CF_ARG_MAX];
    char fullsrc[CF_ARG_MAX];
    char newname[CF_ARG_MAX];
    struct CmdO2FindData fd;
    CmdO2FindHandle h;
    CmdO2Rc rc;
    int count;
    int wildcard;

    p = tail;
    if (cf_next_arg(&p, src, sizeof(src)) <= 0 ||
        cf_next_arg(&p, dstarg, sizeof(dstarg)) <= 0 || cf_has_more(p)) {
        fprintf(stderr, "The syntax of the command is incorrect.\n");
        return 1;
    }

    wildcard = cf_has_wild(src) || cf_has_wild(dstarg);
    if (!wildcard) {
        if (!cf_destination_in_source_dir(src, dstarg, dst, sizeof(dst))) {
            fprintf(stderr, "The path is too long.\n");
            return 1;
        }
        rc = CmdO2Move(src, dst);
        if (rc != 0) {
            cf_error("RENAME", src, rc);
            return 1;
        }
        return 0;
    }

    /* RENAME never moves files to another directory.  A destination path is
     * therefore rejected even when its final component contains wildcards. */
    if (strchr(dstarg, '\\') != NULL || strchr(dstarg, '/') != NULL ||
        (dstarg[0] != '\0' && dstarg[1] == ':')) {
        fprintf(stderr, "RENAME cannot move files to a different directory.\n");
        return 1;
    }
    if (!cf_dir_prefix(src, prefix, sizeof(prefix))) {
        fprintf(stderr, "The path is too long.\n");
        return 1;
    }

    rc = CmdO2FindFirst(src, &h, &fd);
    if (rc != 0) {
        fprintf(stderr, "File not found - %s\n", src);
        return 1;
    }
    count = 0;
    for (;;) {
        if ((fd.attr & CMDO2_ATTR_DIRECTORY) == 0) {
            if (!cf_join(fullsrc, sizeof(fullsrc), prefix, fd.name) ||
                !cf_rename_wild_name(fd.name, dstarg, newname,
                                     sizeof(newname)) ||
                !cf_join(dst, sizeof(dst), prefix, newname)) {
                (void)CmdO2FindClose(h);
                fprintf(stderr, "The path is too long.\n");
                return 1;
            }
            if (!cf_ci_equal(fullsrc, dst)) {
                rc = CmdO2Move(fullsrc, dst);
                if (rc != 0) {
                    (void)CmdO2FindClose(h);
                    cf_error("RENAME", fullsrc, rc);
                    return 1;
                }
            }
            ++count;
        }
        rc = CmdO2FindNext(h, &fd);
        if (rc != 0)
            break;
    }
    (void)CmdO2FindClose(h);
    if (rc != CMDO2_ERROR_NO_MORE_FILES && rc != 0) {
        cf_error("RENAME", src, rc);
        return 1;
    }
    if (count == 0) {
        fprintf(stderr, "File not found - %s\n", src);
        return 1;
    }
    return 0;
}

static int cf_move_one(const char *src, const char *dstarg)
{
    char dst[CF_ARG_MAX];
    CmdO2Rc rc;

    if (cf_is_directory(dstarg)) {
        if (!cf_join(dst, sizeof(dst), dstarg, cf_basename(src))) {
            fprintf(stderr, "The path is too long.\n");
            return 0;
        }
    } else {
        if (strlen(dstarg) + 1 > sizeof(dst)) {
            fprintf(stderr, "The path is too long.\n");
            return 0;
        }
        strcpy(dst, dstarg);
    }

    rc = CmdO2Move(src, dst);
    if (rc == 0)
        return 1;

    if (rc == CMDO2_ERROR_NOT_SAME_DEVICE && !cf_is_directory(src)) {
        if (cf_copy_stream(src, dst)) {
            rc = CmdO2Delete(src);
            if (rc == 0)
                return 1;
        }
    }
    cf_error("MOVE", src, rc);
    return 0;
}

static int cf_move_wild(const char *srcpat, const char *dst)
{
    struct CmdO2FindData fd;
    CmdO2FindHandle h;
    CmdO2Rc rc;
    char prefix[CF_ARG_MAX];
    char src[CF_ARG_MAX];
    int count;

    if (!cf_is_directory(dst)) {
        fprintf(stderr,
                "MOVE: wildcard source currently requires a destination directory.\n");
        return -1;
    }
    if (!cf_dir_prefix(srcpat, prefix, sizeof(prefix))) {
        fprintf(stderr, "The path is too long.\n");
        return -1;
    }
    rc = CmdO2FindFirst(srcpat, &h, &fd);
    if (rc != 0) {
        fprintf(stderr, "File not found - %s\n", srcpat);
        return -1;
    }
    count = 0;
    for (;;) {
        if ((fd.attr & CMDO2_ATTR_DIRECTORY) == 0) {
            if (!cf_join(src, sizeof(src), prefix, fd.name)) {
                (void)CmdO2FindClose(h);
                fprintf(stderr, "The path is too long.\n");
                return -1;
            }
            if (!cf_move_one(src, dst)) {
                (void)CmdO2FindClose(h);
                return -1;
            }
            ++count;
        }
        rc = CmdO2FindNext(h, &fd);
        if (rc != 0)
            break;
    }
    (void)CmdO2FindClose(h);
    if (rc != CMDO2_ERROR_NO_MORE_FILES && rc != 0) {
        cf_error("MOVE", srcpat, rc);
        return -1;
    }
    return count;
}

int CmdFileMove(const char *tail)
{
    const char *p;
    char src[CF_ARG_MAX];
    char dst[CF_ARG_MAX];
    int count;

    p = tail;
    if (cf_next_arg(&p, src, sizeof(src)) <= 0 ||
        cf_next_arg(&p, dst, sizeof(dst)) <= 0 || cf_has_more(p)) {
        fprintf(stderr, "The syntax of the command is incorrect.\n");
        return 1;
    }

    if (cf_has_wild(src)) {
        count = cf_move_wild(src, dst);
        if (count < 0)
            return 1;
    } else {
        if (!cf_move_one(src, dst))
            return 1;
        count = 1;
    }
    printf("%10d file(s) moved.\n", count);
    return 0;
}
