/*
 * cmdbatch.c - Milestone 24 cbatch.c semantic lift
 *
 * C89 clean-room reconstruction shaped around retained Dec-1991 NT CMD COFF
 * symbols: BatProc, BatLoop, SetBat, eFor, FWork, SubFor, eGoto, eIf,
 * eErrorLevel, eExist, eNot, eStrCmp, eShift, eCall and CallWork.
 *
 * This module deliberately contains no Win32 API calls.  The host supplies a
 * command executor and path-existence callback so the batch language remains
 * portable and testable independently of OS2HOST32.
 */

#include "cmdbatch.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

struct CmdBatchFrame {
    char *path;
    char **lines;
    int line_count;
    int pc;
    char **argv;
    int argc;
    int shift;
    char *arg_tail;
    int owns_lines;
    struct CmdBatchFrame *prev;
};

static char *cb_dup(const char *s)
{
    size_t n;
    char *p;
    if (s == NULL)
        s = "";
    n = strlen(s) + 1;
    p = (char *)malloc(n);
    if (p != NULL)
        memcpy(p, s, n);
    return p;
}

static char *cb_skip_ws(char *p)
{
    while (*p == ' ' || *p == '\t')
        ++p;
    return p;
}

static const char *cb_skip_ws_c(const char *p)
{
    while (*p == ' ' || *p == '\t')
        ++p;
    return p;
}

static void cb_trim_end(char *p)
{
    size_t n;
    n = strlen(p);
    while (n != 0 && (p[n - 1] == '\r' || p[n - 1] == '\n' ||
                      p[n - 1] == ' ' || p[n - 1] == '\t')) {
        p[n - 1] = '\0';
        --n;
    }
}

static int cb_ci_char(int a, int b)
{
    return toupper((unsigned char)a) == toupper((unsigned char)b);
}

static int cb_ci_cmp(const char *a, const char *b)
{
    while (*a != '\0' && *b != '\0') {
        int ca;
        int cb;
        ca = toupper((unsigned char)*a);
        cb = toupper((unsigned char)*b);
        if (ca != cb)
            return ca - cb;
        ++a;
        ++b;
    }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

static int cb_word(const char *p, const char *word, const char **after)
{
    const char *q;
    p = cb_skip_ws_c(p);
    q = word;
    while (*q != '\0') {
        if (*p == '\0' || !cb_ci_char(*p, *q))
            return 0;
        ++p;
        ++q;
    }
    if (*p != '\0' && *p != ' ' && *p != '\t' && *p != '(')
        return 0;
    if (after != NULL)
        *after = p;
    return 1;
}

static int cb_append(char *dst, size_t cap, size_t *used,
                     const char *src, size_t n)
{
    if (*used + n + 1 > cap)
        return 0;
    if (n != 0)
        memcpy(dst + *used, src, n);
    *used += n;
    dst[*used] = '\0';
    return 1;
}

static int cb_token(const char **pp, char *dst, size_t cap)
{
    const char *p;
    size_t n;
    int quoted;

    p = cb_skip_ws_c(*pp);
    n = 0;
    quoted = 0;
    if (*p == '"') {
        quoted = 1;
        ++p;
    }
    while (*p != '\0') {
        if (quoted) {
            if (*p == '"') {
                ++p;
                break;
            }
        } else if (*p == ' ' || *p == '\t') {
            break;
        }
        if (n + 1 >= cap)
            return 0;
        dst[n++] = *p++;
    }
    dst[n] = '\0';
    *pp = cb_skip_ws_c(p);
    return n != 0;
}


/* Batch positional parameters preserve the spelling supplied by the caller.
 * In particular, classic batch %1 retains surrounding quotes, and an empty
 * quoted argument ("") is still a real positional parameter. */
static int cb_arg_token(const char **pp, char *dst, size_t cap)
{
    const char *p;
    size_t n;
    int quoted;

    p = cb_skip_ws_c(*pp);
    if (*p == '\0')
        return 0;
    n = 0;
    quoted = 0;
    while (*p != '\0') {
        if (*p == '"')
            quoted = !quoted;
        else if (!quoted && (*p == ' ' || *p == '\t'))
            break;
        if (n + 1 >= cap)
            return -1;
        dst[n++] = *p++;
    }
    dst[n] = '\0';
    *pp = cb_skip_ws_c(p);
    return 1;
}

static void cb_free_argv(struct CmdBatchFrame *f)
{
    int i;
    free(f->arg_tail);
    f->arg_tail = NULL;
    if (f->argv == NULL)
        return;
    for (i = 0; i < f->argc; ++i)
        free(f->argv[i]);
    free(f->argv);
    f->argv = NULL;
    f->argc = 0;
}

static int cb_build_argv(struct CmdBatchFrame *f,
                         const char *arg0,
                         const char *tail)
{
    char **v;
    int argc;
    const char *p;
    char token[CMDBATCH_LINE_MAX];

    v = (char **)calloc(CMDBATCH_MAX_ARGS, sizeof(char *));
    if (v == NULL)
        return 0;
    argc = 0;
    v[argc] = cb_dup(arg0);
    if (v[argc] == NULL) {
        free(v);
        return 0;
    }
    ++argc;

    p = tail != NULL ? tail : "";
    f->arg_tail = cb_dup(cb_skip_ws_c(p));
    if (f->arg_tail == NULL) {
        int i;
        for (i = 0; i < argc; ++i)
            free(v[i]);
        free(v);
        return 0;
    }
    while (*cb_skip_ws_c(p) != '\0' && argc < CMDBATCH_MAX_ARGS) {
        int trc;
        trc = cb_arg_token(&p, token, sizeof(token));
        if (trc == 0)
            break;
        if (trc < 0) {
            int i;
            for (i = 0; i < argc; ++i)
                free(v[i]);
            free(v);
            free(f->arg_tail);
            f->arg_tail = NULL;
            return 0;
        }
        v[argc] = cb_dup(token);
        if (v[argc] == NULL) {
            int i;
            for (i = 0; i < argc; ++i)
                free(v[i]);
            free(v);
            free(f->arg_tail);
            f->arg_tail = NULL;
            return 0;
        }
        ++argc;
    }

    f->argv = v;
    f->argc = argc;
    f->shift = 0;
    return 1;
}

static void cb_free_lines(char **lines, int count)
{
    int i;
    if (lines == NULL)
        return;
    for (i = 0; i < count; ++i)
        free(lines[i]);
    free(lines);
}

static int cb_load_lines(const char *path, char ***out_lines, int *out_count)
{
    FILE *fp;
    char **lines;
    int count;
    int cap;
    char buf[CMDBATCH_LINE_MAX];

    fp = fopen(path, "rb");
    if (fp == NULL)
        return 0;
    lines = NULL;
    count = 0;
    cap = 0;
    while (fgets(buf, sizeof(buf), fp) != NULL) {
        char *line;
        size_t n;
        n = strlen(buf);
        if (n != 0 && buf[n - 1] != '\n' && !feof(fp)) {
            cb_free_lines(lines, count);
            fclose(fp);
            return 0;
        }
        line = cb_dup(buf);
        if (line == NULL) {
            cb_free_lines(lines, count);
            fclose(fp);
            return 0;
        }
        if (count == cap) {
            char **new_lines;
            int new_cap;
            new_cap = cap == 0 ? 32 : cap * 2;
            new_lines = (char **)realloc(lines,
                                         (size_t)new_cap * sizeof(char *));
            if (new_lines == NULL) {
                free(line);
                cb_free_lines(lines, count);
                fclose(fp);
                return 0;
            }
            lines = new_lines;
            cap = new_cap;
        }
        lines[count++] = line;
    }
    fclose(fp);
    *out_lines = lines;
    *out_count = count;
    return 1;
}

static int cb_is_label(const char *line)
{
    line = cb_skip_ws_c(line);
    return *line == ':' && line[1] != ':';
}

static int cb_label_matches(const char *line, const char *wanted)
{
    char label[256];
    size_t n;
    const char *p;

    p = cb_skip_ws_c(line);
    if (*p != ':')
        return 0;
    ++p;
    while (*p == ' ' || *p == '\t')
        ++p;
    n = 0;
    while (*p != '\0' && *p != '\r' && *p != '\n' &&
           *p != ' ' && *p != '\t' && n + 1 < sizeof(label)) {
        label[n++] = *p++;
    }
    label[n] = '\0';
    return cb_ci_cmp(label, wanted) == 0;
}

static int cb_find_label(const struct CmdBatchFrame *f, const char *label)
{
    int i;
    if (*label == ':')
        ++label;
    for (i = 0; i < f->line_count; ++i) {
        if (cb_label_matches(f->lines[i], label))
            return i;
    }
    return -1;
}

static void cb_destroy_frame(struct CmdBatchFrame *f)
{
    if (f == NULL)
        return;
    if (f->owns_lines)
        cb_free_lines(f->lines, f->line_count);
    cb_free_argv(f);
    free(f->path);
    free(f);
}

static int cb_run_frame(struct CmdBatchState *state,
                        struct CmdBatchFrame *f,
                        int *want_exit)
{
    struct CmdBatchFrame *saved;
    int rc;

    saved = state->current;
    f->prev = saved;
    state->current = f;
    rc = 0;
    while (!*want_exit && f->pc < f->line_count) {
        char *line;
        const char *raw;

        raw = f->lines[f->pc++];
        if (strlen(raw) + 1U > CMDBATCH_LINE_MAX) {
            rc = 1;
            break;
        }
        line = cb_dup(raw);
        if (line == NULL) {
            rc = 1;
            break;
        }
        cb_trim_end(line);
        if (*cb_skip_ws(line) == '\0' || cb_is_label(line)) {
            free(line);
            continue;
        }
        rc = state->run_line(state->host_ctx, line, want_exit);
        free(line);
        if (state->current != f)
            break;
    }
    if (state->current == f)
        state->current = saved;
    return rc;
}

void CmdBatchInit(struct CmdBatchState *state,
                  void *host_ctx,
                  CmdBatchRunLineFn run_line,
                  CmdBatchExistsFn path_exists)
{
    memset(state, 0, sizeof(*state));
    state->host_ctx = host_ctx;
    state->run_line = run_line;
    state->path_exists = path_exists;
}

void CmdBatchDone(struct CmdBatchState *state)
{
    /* Frames are normally stack-scoped by CmdBatchRunFile/CallLabel.  If a
     * host tears down during an abnormal exit, unlink without double-freeing
     * shared subroutine line arrays. */
    while (state->current != NULL) {
        struct CmdBatchFrame *f;
        f = state->current;
        state->current = f->prev;
        cb_destroy_frame(f);
    }
    memset(state, 0, sizeof(*state));
}

int CmdBatchActive(const struct CmdBatchState *state)
{
    return state->current != NULL;
}

const char *CmdBatchCurrentFile(const struct CmdBatchState *state)
{
    return state->current != NULL ? state->current->path : NULL;
}

void CmdBatchEndCurrent(struct CmdBatchState *state)
{
    if (state != NULL && state->current != NULL)
        state->current->pc = state->current->line_count;
}

int CmdBatchRunFile(struct CmdBatchState *state,
                    const char *path,
                    const char *arg_tail,
                    int *want_exit)
{
    struct CmdBatchFrame *f;
    int rc;

    if (state->run_line == NULL)
        return 1;
    f = (struct CmdBatchFrame *)calloc(1, sizeof(*f));
    if (f == NULL)
        return 1;
    f->path = cb_dup(path);
    if (f->path == NULL || !cb_load_lines(path, &f->lines, &f->line_count) ||
        !cb_build_argv(f, path, arg_tail)) {
        cb_destroy_frame(f);
        return 1;
    }
    f->owns_lines = 1;
    f->pc = 0;
    rc = cb_run_frame(state, f, want_exit);
    if (state->current == f)
        state->current = f->prev;
    cb_destroy_frame(f);
    return rc;
}

static const char *cb_param(const struct CmdBatchFrame *f, int index)
{
    int actual;
    if (index == 0)
        return f->argc > 0 ? f->argv[0] : "";
    actual = index + f->shift;
    if (actual < 0 || actual >= f->argc)
        return "";
    return f->argv[actual];
}

static int cb_star(const struct CmdBatchFrame *f,
                   char *dst, size_t cap, size_t *used)
{
    const char *p;
    /* %* is the original argument tail.  SHIFT changes %1..%9 but does not
     * rewrite the caller's original tail. */
    p = f->arg_tail != NULL ? f->arg_tail : "";
    return cb_append(dst, cap, used, p, strlen(p));
}

int CmdBatchExpandLine(struct CmdBatchState *state,
                       const char *src,
                       char *dst,
                       size_t cap)
{
    const struct CmdBatchFrame *f;
    size_t i;
    size_t used;

    f = state->current;
    if (cap == 0)
        return 0;
    dst[0] = '\0';
    if (f == NULL) {
        if (strlen(src) + 1 > cap)
            return 0;
        strcpy(dst, src);
        return 1;
    }

    i = 0;
    used = 0;
    while (src[i] != '\0') {
        if (src[i] != '%') {
            if (!cb_append(dst, cap, &used, src + i, 1))
                return 0;
            ++i;
            continue;
        }
        if (src[i + 1] == '%') {
            /* Batch FOR spelling: %%i becomes %i before cparse/cbatch sees it. */
            if (!cb_append(dst, cap, &used, "%", 1))
                return 0;
            i += 2;
            continue;
        }
        if (src[i + 1] >= '0' && src[i + 1] <= '9') {
            const char *v;
            v = cb_param(f, src[i + 1] - '0');
            if (!cb_append(dst, cap, &used, v, strlen(v)))
                return 0;
            i += 2;
            continue;
        }
        if (src[i + 1] == '*') {
            if (!cb_star(f, dst, cap, &used))
                return 0;
            i += 2;
            continue;
        }
        /* Leave %NAME% and interactive-style %i untouched for clex.c/SubVar
         * or eFor respectively. */
        if (!cb_append(dst, cap, &used, "%", 1))
            return 0;
        ++i;
    }
    return 1;
}

int CmdBatchGoto(struct CmdBatchState *state, const char *tail)
{
    struct CmdBatchFrame *f;
    char label[256];
    const char *p;
    int where;

    f = state->current;
    if (f == NULL) {
        fprintf(stderr, "GOTO was unexpected at this time.\n");
        return 1;
    }
    p = cb_skip_ws_c(tail);
    if (*p == ':')
        ++p;
    if (!cb_token(&p, label, sizeof(label))) {
        fprintf(stderr, "The syntax of the command is incorrect.\n");
        return 1;
    }
    if (cb_ci_cmp(label, "EOF") == 0) {
        f->pc = f->line_count;
        return 0;
    }
    where = cb_find_label(f, label);
    if (where < 0) {
        fprintf(stderr, "Label not found - %s\n", label);
        return 1;
    }
    f->pc = where + 1;
    return 0;
}

int CmdBatchShift(struct CmdBatchState *state)
{
    struct CmdBatchFrame *f;
    f = state->current;
    if (f == NULL) {
        fprintf(stderr, "SHIFT was unexpected at this time.\n");
        return 1;
    }
    if (1 + f->shift < f->argc)
        ++f->shift;
    return 0;
}

int CmdBatchCallLabel(struct CmdBatchState *state,
                      const char *tail,
                      int *want_exit)
{
    struct CmdBatchFrame *parent;
    struct CmdBatchFrame *f;
    const char *p;
    char label[256];
    int where;
    int rc;

    parent = state->current;
    if (parent == NULL)
        return 1;
    p = cb_skip_ws_c(tail);
    if (*p == ':')
        ++p;
    if (!cb_token(&p, label, sizeof(label)))
        return 1;
    where = cb_find_label(parent, label);
    if (where < 0) {
        fprintf(stderr, "Label not found - %s\n", label);
        return 1;
    }

    f = (struct CmdBatchFrame *)calloc(1, sizeof(*f));
    if (f == NULL)
        return 1;
    f->path = cb_dup(parent->path);
    f->lines = parent->lines;
    f->line_count = parent->line_count;
    f->pc = where + 1;
    f->owns_lines = 0;
    if (f->path == NULL || !cb_build_argv(f, parent->path, p)) {
        cb_destroy_frame(f);
        return 1;
    }
    rc = cb_run_frame(state, f, want_exit);
    if (state->current == f)
        state->current = parent;
    cb_destroy_frame(f);
    return rc;
}

static int cb_run_conditional(struct CmdBatchState *state,
                              int condition,
                              int negate,
                              const char *command,
                              int last_rc,
                              int *want_exit)
{
    char *line;
    int rc;

    if (negate)
        condition = !condition;
    if (!condition)
        return last_rc;
    command = cb_skip_ws_c(command);
    if (*command == '\0') {
        fprintf(stderr, "The syntax of the command is incorrect.\n");
        return 1;
    }
    if (strlen(command) + 1U > CMDBATCH_LINE_MAX)
        return 1;
    line = cb_dup(command);
    if (line == NULL)
        return 1;
    rc = state->run_line(state->host_ctx, line, want_exit);
    free(line);
    return rc;
}

static const char *cb_parse_rhs_token(const char *p,
                                      char *dst, size_t cap)
{
    size_t n;
    int quoted;

    p = cb_skip_ws_c(p);
    n = 0;
    quoted = 0;
    if (*p == '"')
        quoted = 1;
    while (*p != '\0') {
        if (!quoted && (*p == ' ' || *p == '\t'))
            break;
        if (n + 1 >= cap)
            return NULL;
        dst[n++] = *p;
        if (*p == '"') {
            if (quoted && n > 1) {
                ++p;
                break;
            }
        }
        ++p;
    }
    dst[n] = '\0';
    return cb_skip_ws_c(p);
}

int CmdBatchIf(struct CmdBatchState *state,
               const char *tail,
               int last_rc,
               int *want_exit)
{
    const char *p;
    const char *after;
    int negate;
    int condition;

    p = cb_skip_ws_c(tail);
    negate = 0;
    if (cb_word(p, "NOT", &after)) {
        negate = 1;
        p = cb_skip_ws_c(after);
    }

    if (cb_word(p, "ERRORLEVEL", &after)) {
        char *endp;
        long level;
        p = cb_skip_ws_c(after);
        level = strtol(p, &endp, 10);
        if (endp == p) {
            fprintf(stderr, "The syntax of the command is incorrect.\n");
            return 1;
        }
        condition = last_rc >= (int)level;
        return cb_run_conditional(state, condition, negate, endp,
                                  last_rc, want_exit);
    }

    if (cb_word(p, "EXIST", &after)) {
        char *path;
        int rc;

        path = (char *)malloc(CMDBATCH_LINE_MAX);
        if (path == NULL)
            return 1;
        p = after;
        if (!cb_token(&p, path, CMDBATCH_LINE_MAX)) {
            free(path);
            fprintf(stderr, "The syntax of the command is incorrect.\n");
            return 1;
        }
        condition = state->path_exists != NULL ?
                    state->path_exists(state->host_ctx, path) : 0;
        free(path);
        rc = cb_run_conditional(state, condition, negate, p,
                                last_rc, want_exit);
        return rc;
    }

    /* Historical eStrCmp: IF lhs==rhs command.  Preserve quote characters in
     * the comparison, matching classic CMD behavior.  M29K1 keeps the two
     * potentially 4 KB operands off the C/386 stack because IF reparses its
     * selected command recursively. */
    {
        const char *eq;
        const char *q;
        int quoted;
        char *lhs;
        char *rhs;
        size_t ln;
        const char *command;
        int rc;

        lhs = (char *)malloc(CMDBATCH_LINE_MAX);
        rhs = (char *)malloc(CMDBATCH_LINE_MAX);
        if (lhs == NULL || rhs == NULL) {
            free(lhs);
            free(rhs);
            return 1;
        }

        eq = NULL;
        quoted = 0;
        q = p;
        while (*q != '\0') {
            if (*q == '"')
                quoted = !quoted;
            if (!quoted && q[0] == '=' && q[1] == '=') {
                eq = q;
                break;
            }
            ++q;
        }
        if (eq == NULL) {
            free(lhs);
            free(rhs);
            fprintf(stderr, "The syntax of the command is incorrect.\n");
            return 1;
        }
        ln = (size_t)(eq - p);
        while (ln != 0 && (p[ln - 1] == ' ' || p[ln - 1] == '\t'))
            --ln;
        while (ln != 0 && (*p == ' ' || *p == '\t')) {
            ++p;
            --ln;
        }
        if (ln + 1U > CMDBATCH_LINE_MAX) {
            free(lhs);
            free(rhs);
            return 1;
        }
        memcpy(lhs, p, ln);
        lhs[ln] = '\0';
        command = cb_parse_rhs_token(eq + 2, rhs, CMDBATCH_LINE_MAX);
        if (command == NULL || rhs[0] == '\0') {
            free(lhs);
            free(rhs);
            fprintf(stderr, "The syntax of the command is incorrect.\n");
            return 1;
        }
        condition = strcmp(lhs, rhs) == 0;
        free(lhs);
        free(rhs);
        rc = cb_run_conditional(state, condition, negate, command,
                                last_rc, want_exit);
        return rc;
    }
}

static int cb_for_substitute(const char *src,
                             char var,
                             const char *value,
                             char *dst,
                             size_t cap)
{
    size_t i;
    size_t used;
    size_t vn;
    used = 0;
    dst[0] = '\0';
    vn = strlen(value);
    i = 0;
    while (src[i] != '\0') {
        if (src[i] == '%' && src[i + 1] != '\0' &&
            src[i + 1] == var) {
            if (!cb_append(dst, cap, &used, value, vn))
                return 0;
            i += 2;
        } else {
            if (!cb_append(dst, cap, &used, src + i, 1))
                return 0;
            ++i;
        }
    }
    return 1;
}

static int cb_next_set_item(const char **pp, const char *end,
                            char *dst, size_t cap)
{
    const char *p;
    size_t n;
    int quoted;

    p = *pp;
    while (p < end && (*p == ' ' || *p == '\t' || *p == ','))
        ++p;
    if (p >= end) {
        *pp = p;
        return 0;
    }
    quoted = 0;
    if (*p == '"') {
        quoted = 1;
        ++p;
    }
    n = 0;
    while (p < end) {
        if (quoted) {
            if (*p == '"') {
                ++p;
                break;
            }
        } else if (*p == ' ' || *p == '\t' || *p == ',') {
            break;
        }
        if (n + 1 >= cap)
            return -1;
        dst[n++] = *p++;
    }
    dst[n] = '\0';
    *pp = p;
    return 1;
}

int CmdBatchFor(struct CmdBatchState *state,
                const char *tail,
                int *want_exit)
{
    const char *p;
    const char *after;
    const char *set_start;
    const char *set_end;
    const char *body;
    char var;
    char *item;
    char *command;
    int rc;
    int got;

    p = cb_skip_ws_c(tail);
    if (*p != '%' || p[1] == '\0') {
        fprintf(stderr, "The syntax of the command is incorrect.\n");
        return 1;
    }
    var = p[1];
    p += 2;
    if (!cb_word(p, "IN", &after)) {
        fprintf(stderr, "The syntax of the command is incorrect.\n");
        return 1;
    }
    p = cb_skip_ws_c(after);
    if (*p != '(') {
        fprintf(stderr, "The syntax of the command is incorrect.\n");
        return 1;
    }
    set_start = ++p;
    {
        int quoted;
        quoted = 0;
        while (*p != '\0') {
            if (*p == '"')
                quoted = !quoted;
            else if (!quoted && *p == ')')
                break;
            ++p;
        }
    }
    if (*p != ')') {
        fprintf(stderr, "The syntax of the command is incorrect.\n");
        return 1;
    }
    set_end = p;
    ++p;
    if (!cb_word(p, "DO", &after)) {
        fprintf(stderr, "The syntax of the command is incorrect.\n");
        return 1;
    }
    body = cb_skip_ws_c(after);
    if (*body == '\0') {
        fprintf(stderr, "The syntax of the command is incorrect.\n");
        return 1;
    }

    item = (char *)malloc(CMDBATCH_LINE_MAX);
    command = (char *)malloc(CMDBATCH_LINE_MAX);
    if (item == NULL || command == NULL) {
        free(item);
        free(command);
        return 1;
    }

    rc = 0;
    p = set_start;
    for (;;) {
        got = cb_next_set_item(&p, set_end, item, CMDBATCH_LINE_MAX);
        if (got == 0)
            break;
        if (got < 0) {
            rc = 1;
            break;
        }
        if (!cb_for_substitute(body, var, item, command, CMDBATCH_LINE_MAX)) {
            rc = 1;
            break;
        }
        rc = state->run_line(state->host_ctx, command, want_exit);
        if (*want_exit)
            break;
    }
    free(item);
    free(command);
    return rc;
}
