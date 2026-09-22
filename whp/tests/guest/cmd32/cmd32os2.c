/*
 * cmd32os2.c - Milestone 29A CMD personality bootstrap
 *
 * C89-oriented scaffolding shaped after the recovered Dec-1991 NT CMD source
 * tree.  This is still NOT a source-identical reconstruction of Microsoft's
 * command processor.  M28A begins removing the Win32 host API from CMD proper:
 * cfile/path/process-facing operations now cross cmdos2.h into DOSCALLS.dll.
 * Console rendering remains native-host bootstrap work for later milestones.
 * M28B moved SET/SETLOCAL and percent expansion onto the CMD-owned OS/2
 * environment block.  M28C moves file redirection off the CMD Win32/CRT
 * layer and behind the OS/2-facing standard-handle boundary.  M28D moves
 * pipeline creation, asynchronous shell-worker execution, and waits through
 * DosCreatePipe / DosExecPgm(EXEC_ASYNCRESULT) / DosWaitChild.  M29A moves
 * interactive prompt/line input, PAUSE, and CLS behind VIO/KBD services.
 * M29P hardens file builtins for the small C/386 stack and tightens common
 * DIR/COPY/RENAME filesystem behavior.
 *
 *   built-ins -> recovered NT-style e* handlers
 *   external  -> DOSCALLS.283 (DosExecPgm) -> OS2HOST32 -> original LX child
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "cmdparse.h"
#include "cmdbatch.h"
#include "cmdfile.h"
#include "cmdos2.h"

#ifndef __cdecl
#define __cdecl
#endif

#ifndef MAX_PATH
#define MAX_PATH 260
#endif

#define CMDLINE_MAX 4096
#define CMD32_STARTUP_CMD "C:\\OS2\\ENV\\STARTUP.CMD"
#define CMD_HISTORY_MAX 30U

/* IBM PC / OS/2 KbdCharIn scan codes for the enhanced editing keys. */
#define CMD_SCAN_HOME   0x47U
#define CMD_SCAN_UP     0x48U
#define CMD_SCAN_LEFT   0x4bU
#define CMD_SCAN_RIGHT  0x4dU
#define CMD_SCAN_END    0x4fU
#define CMD_SCAN_DOWN   0x50U
#define CMD_SCAN_DELETE 0x53U

struct EnvFrame {
    char *block;
    struct EnvFrame *next;
};


struct CmdHistory {
    char lines[CMD_HISTORY_MAX][CMDLINE_MAX];
    char draft[CMDLINE_MAX];
    unsigned first;
    unsigned count;
};

struct ShellState {
    struct CmdBatchState batch;
    struct EnvFrame *env_stack;
    int last_rc;
    int echo_enabled;
    int batch_call_depth;
};

static int run_line(struct ShellState *s, char *line, int *want_exit);
static int resultcodes_errorlevel(const struct CmdO2ResultCodes *results);
static int batch_host_run_line(void *ctx, char *line, int *want_exit);
static int batch_host_exists(void *ctx, const char *path);
static int env_depth(const struct ShellState *s);
static void env_unwind_to(struct ShellState *s, int depth);
static int run_batch_file(struct ShellState *s, const char *path,
                          const char *tail, int *want_exit);
static char *snapshot_environment(void);
static void print_prompt(void);

struct Builtin;
typedef int (*BuiltinFn)(struct ShellState *, char *, int *);

#define BUILTIN_KEEP_ERRORLEVEL 0x0001U

struct Builtin {
    const char *name;
    BuiltinFn fn;
    const char *nt_module;
    const char *nt_symbol;
    unsigned flags;
};

static int ci_cmp(const char *a, const char *b)
{
    unsigned char ca;
    unsigned char cb;
    while (*a && *b) {
        ca = (unsigned char)toupper((unsigned char)*a);
        cb = (unsigned char)toupper((unsigned char)*b);
        if (ca != cb)
            return (int)ca - (int)cb;
        ++a;
        ++b;
    }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

static int ci_prefix(const char *s, const char *prefix)
{
    while (*prefix != '\0') {
        if (*s == '\0')
            return 0;
        if (toupper((unsigned char)*s) != toupper((unsigned char)*prefix))
            return 0;
        ++s;
        ++prefix;
    }
    return 1;
}

static char *skip_ws(char *p)
{
    while (*p == ' ' || *p == '\t')
        ++p;
    return p;
}

static void trim_end(char *p)
{
    size_t n;
    n = strlen(p);
    while (n != 0 && (p[n - 1] == '\r' || p[n - 1] == '\n' ||
                      p[n - 1] == ' ' || p[n - 1] == '\t')) {
        p[n - 1] = '\0';
        --n;
    }
}

static void copy_unquoted(char *dst, size_t cap, const char *src)
{
    size_t n;
    const char *start;
    const char *end;

    while (*src == ' ' || *src == '\t')
        ++src;
    start = src;
    end = src + strlen(src);
    if (*start == '"' && end > start + 1 && end[-1] == '"') {
        ++start;
        --end;
    }
    n = (size_t)(end - start);
    if (n >= cap)
        n = cap - 1;
    memcpy(dst, start, n);
    dst[n] = '\0';
}

/* Small quoted-argument scanner used by file/info builtins.  It deliberately
 * does not perform environment or batch expansion; that has already happened
 * before the builtin is dispatched. */
static int next_builtin_arg(const char **pp, char *out, size_t cap)
{
    const char *p;
    size_t n;
    int quoted;

    if (pp == NULL || *pp == NULL || out == NULL || cap == 0U)
        return -1;
    p = *pp;
    while (*p == ' ' || *p == '\t')
        ++p;
    if (*p == '\0') {
        out[0] = '\0';
        *pp = p;
        return 0;
    }
    n = 0U;
    quoted = 0;
    while (*p != '\0') {
        if (*p == '"') {
            quoted = !quoted;
            ++p;
            continue;
        }
        if (!quoted && (*p == ' ' || *p == '\t'))
            break;
        if (n + 1U >= cap)
            return -1;
        out[n++] = *p++;
    }
    if (quoted)
        return -1;
    out[n] = '\0';
    while (*p == ' ' || *p == '\t')
        ++p;
    *pp = p;
    return 1;
}


#define PIPE_COMMAND_MAX 8192

static int pipe_trace_enabled(void)
{
    const char *p;
    p = getenv("CMD32_TRACE_PIPE");
    return p != NULL && p[0] != '\0' && p[0] != '0';
}

struct RedirectState {
    struct CmdO2StdToken saved[3];
    int have[3];
};

static void redirect_state_init(struct RedirectState *st)
{
    int i;
    for (i = 0; i < 3; ++i) {
        st->saved[i].value = CMDO2_HANDLE_ALLOCATE;
        st->saved[i].os2_value = CMDO2_HANDLE_ALLOCATE;
        st->have[i] = 0;
    }
}

static int redirect_one_file(unsigned long stdHandle, const char *name,
                             int type)
{
    int o2type;
    CmdO2Rc rc;

    if (type == CMD_REDIR_INPUT)
        o2type = CMDO2_REDIR_INPUT;
    else if (type == CMD_REDIR_APPEND)
        o2type = CMDO2_REDIR_APPEND;
    else
        o2type = CMDO2_REDIR_OUTPUT;

    rc = CmdO2StdRedirectPath(stdHandle, name, o2type);
    if (rc != 0) {
        fprintf(stderr,
                "cmd32os2: cannot redirect handle %lu to %s (OS/2 rc=%lu)\n",
                stdHandle, name, rc);
        return 0;
    }
    return 1;
}

static void restore_redirections(struct RedirectState *st)
{
    int i;

    fflush(stdout);
    fflush(stderr);

    for (i = 0; i < 3; ++i) {
        if (!st->have[i])
            continue;
        (void)CmdO2StdRestore((unsigned long)i, &st->saved[i]);
        st->have[i] = 0;
        if (i == 0)
            clearerr(stdin);
        else if (i == 1)
            clearerr(stdout);
        else
            clearerr(stderr);
    }
}

static int apply_redirections(const struct CmdRedir *r,
                              struct RedirectState *st)
{
    char target[MAX_PATH];
    CmdO2Rc rc;
    int fd;

    redirect_state_init(st);
    fflush(stdout);
    fflush(stderr);

    /*
     * Apply strictly left-to-right.  This is essential for the classic
     * difference between:
     *     command >both.txt 2>&1
     * and command 2>&1 >stdout.txt
     */
    while (r != NULL) {
        fd = r->dest_fd;
        if (fd < 0 || fd > 2) {
            fprintf(stderr, "cmd32os2: invalid redirection handle %d\n", fd);
            goto fail;
        }

        if (!st->have[fd]) {
            rc = CmdO2StdSave((unsigned long)fd, &st->saved[fd]);
            if (rc != 0)
                goto fail;
            st->have[fd] = 1;
        }

        if (r->type == CMD_REDIR_DUP) {
            if (r->source_fd < 0 || r->source_fd > 2) {
                fprintf(stderr, "cmd32os2: invalid duplication source %d\n",
                        r->source_fd);
                goto fail;
            }
            rc = CmdO2StdRedirectHandle((unsigned long)fd,
                                        (CmdO2Handle)r->source_fd);
            if (rc != 0) {
                fprintf(stderr,
                        "cmd32os2: cannot duplicate handle %d onto %d (OS/2 rc=%lu)\n",
                        r->source_fd, fd, rc);
                goto fail;
            }
        } else {
            copy_unquoted(target, sizeof(target), r->target);
            if (target[0] == '\0') {
                fprintf(stderr, "cmd32os2: empty redirection target\n");
                goto fail;
            }
            if (!redirect_one_file((unsigned long)fd, target, r->type))
                goto fail;
        }
        r = r->next;
    }
    return 1;

fail:
    restore_redirections(st);
    return 0;
}


static int text_append(char *dst, size_t cap, const char *src)
{
    size_t a;
    size_t b;
    a = strlen(dst);
    b = strlen(src);
    if (a + b + 1 > cap)
        return 0;
    memcpy(dst + a, src, b + 1);
    return 1;
}

static int text_append_char(char *dst, size_t cap, char ch)
{
    size_t a;
    a = strlen(dst);
    if (a + 2 > cap)
        return 0;
    dst[a] = ch;
    dst[a + 1] = '\0';
    return 1;
}

/* Quote one command-tail argument using the Microsoft C runtime rules used by
 * both the native bootstrap and the recovered C/386 shell build. */
static int append_shell_arg(char *dst, size_t cap, const char *arg)
{
    const char *p;
    unsigned slashes;
    unsigned i;

    if (!text_append_char(dst, cap, '"'))
        return 0;
    p = arg;
    slashes = 0;
    while (*p != '\0') {
        if (*p == '\\') {
            ++slashes;
            ++p;
            continue;
        }
        if (*p == '"') {
            for (i = 0; i < slashes * 2U + 1U; ++i)
                if (!text_append_char(dst, cap, '\\'))
                    return 0;
            if (!text_append_char(dst, cap, '"'))
                return 0;
            slashes = 0;
            ++p;
            continue;
        }
        for (i = 0; i < slashes; ++i)
            if (!text_append_char(dst, cap, '\\'))
                return 0;
        slashes = 0;
        if (!text_append_char(dst, cap, *p++))
            return 0;
    }
    for (i = 0; i < slashes * 2U; ++i)
        if (!text_append_char(dst, cap, '\\'))
            return 0;
    return text_append_char(dst, cap, '"');
}

static int render_redirs(const struct CmdRedir *r, char *dst, size_t cap)
{
    char op[32];
    int default_fd;

    while (r != NULL) {
        if (r->type == CMD_REDIR_DUP) {
            sprintf(op, " %d>&%d", r->dest_fd, r->source_fd);
            if (!text_append(dst, cap, op))
                return 0;
            r = r->next;
            continue;
        }

        if (r->type == CMD_REDIR_INPUT)
            default_fd = 0;
        else
            default_fd = 1;

        op[0] = '\0';
        if (r->dest_fd != default_fd)
            sprintf(op, " %d", r->dest_fd);
        else
            strcpy(op, " ");

        if (r->type == CMD_REDIR_INPUT)
            strcat(op, "< ");
        else if (r->type == CMD_REDIR_APPEND)
            strcat(op, ">> ");
        else
            strcat(op, "> ");

        if (!text_append(dst, cap, op))
            return 0;

        if (strchr(r->target, ' ') != NULL || strchr(r->target, '\t') != NULL) {
            if (!text_append_char(dst, cap, '"') ||
                !text_append(dst, cap, r->target) ||
                !text_append_char(dst, cap, '"'))
                return 0;
        } else if (!text_append(dst, cap, r->target)) {
            return 0;
        }
        r = r->next;
    }
    return 1;
}


static int render_node_command(const struct CmdNode *node,
                               char *dst, size_t cap)
{
    const char *op;

    if (node == NULL)
        return 1;
    if (node->quiet && !text_append_char(dst, cap, '@'))
        return 0;

    switch (node->type) {
    case CMD_NODE_SIMPLE:
        if (node->text != NULL && !text_append(dst, cap, node->text))
            return 0;
        break;
    case CMD_NODE_GROUP:
        if (!text_append_char(dst, cap, '(') ||
            !render_node_command(node->left, dst, cap) ||
            !text_append_char(dst, cap, ')'))
            return 0;
        break;
    case CMD_NODE_SEQUENCE:
        op = " & ";
        goto binary;
    case CMD_NODE_OR:
        op = " || ";
        goto binary;
    case CMD_NODE_AND:
        op = " && ";
        goto binary;
    case CMD_NODE_PIPE:
        op = " | ";
        goto binary;
    binary:
        if (!text_append_char(dst, cap, '(') ||
            !render_node_command(node->left, dst, cap) ||
            !text_append(dst, cap, op) ||
            !render_node_command(node->right, dst, cap) ||
            !text_append_char(dst, cap, ')'))
            return 0;
        break;
    default:
        return 0;
    }
    return render_redirs(node->redirs, dst, cap);
}

static int build_shell_arg_block(const char *module, const char *command,
                                 char *dst, size_t cap)
{
    size_t namebytes;
    char *tail;
    size_t tailcap;
    size_t tailbytes;

    namebytes = strlen(module) + 1U;
    if (namebytes + 2U > cap)
        return 0;
    memcpy(dst, module, namebytes);

    tail = dst + namebytes;
    tailcap = cap - namebytes;
    tail[0] = '\0';
    if (!text_append(tail, tailcap, "-c ") ||
        !append_shell_arg(tail, tailcap, command))
        return 0;

    tailbytes = strlen(tail) + 1U;
    if (namebytes + tailbytes + 1U > cap)
        return 0;
    dst[namebytes + tailbytes] = '\0';
    return 1;
}

static int start_shell_child(const char *module, const char *command,
                             unsigned long *pid)
{
    char args[PIPE_COMMAND_MAX];
    char fail[MAX_PATH];
    struct CmdO2ResultCodes results;
    CmdO2Rc rc;

    if (pid == NULL)
        return 0;
    *pid = 0UL;
    if (!build_shell_arg_block(module, command, args, sizeof(args)))
        return 0;

    fail[0] = '\0';
    results.codeTerminate = 0UL;
    results.codeResult = 0UL;
    rc = CmdO2ExecPgm(fail, (long)sizeof(fail),
                      CMDO2_EXEC_ASYNCRESULT,
                      args, (const char *)0, &results, module);
    if (rc != 0) {
        fprintf(stderr,
                "cmd32os2: cannot start pipeline worker %s (OS/2 rc=%lu, object=[%s])\n",
                module, rc, fail);
        return 0;
    }
    *pid = results.codeTerminate;
    return *pid != 0UL;
}

static int wait_shell_child(unsigned long pid, int *exitCode)
{
    struct CmdO2ResultCodes results;
    unsigned long waited;
    CmdO2Rc rc;

    results.codeTerminate = 0UL;
    results.codeResult = 0UL;
    waited = 0UL;
    rc = CmdO2WaitChild(CMDO2_WAIT_PROCESS, CMDO2_WAIT,
                        &results, &waited, pid);
    if (rc != 0) {
        fprintf(stderr, "cmd32os2: DosWaitChild(%lu) failed (OS/2 rc=%lu)\n",
                pid, rc);
        return 0;
    }
    if (exitCode != NULL)
        *exitCode = resultcodes_errorlevel(&results);
    return 1;
}

static int execute_pipeline_node(const struct CmdNode *node)
{
    CmdO2Handle readh;
    CmdO2Handle writeh;
    struct CmdO2StdToken saved_in;
    struct CmdO2StdToken saved_out;
    char module[MAX_PATH];
    char left_cmd[CMDLINE_MAX];
    char right_cmd[CMDLINE_MAX];
    unsigned long left_pid;
    unsigned long right_pid;
    CmdO2Rc rc;
    int have_saved_in;
    int have_saved_out;
    int left_started;
    int right_started;
    int left_code;
    int right_code;

    left_cmd[0] = '\0';
    right_cmd[0] = '\0';
    if (!render_node_command(node->left, left_cmd, sizeof(left_cmd)) ||
        !render_node_command(node->right, right_cmd, sizeof(right_cmd))) {
        fprintf(stderr, "cmd32os2: pipeline command is too long\n");
        return 1;
    }
    if (pipe_trace_enabled()) {
        fprintf(stderr, "[pipe] left = [%s]\n", left_cmd);
        fprintf(stderr, "[pipe] right= [%s]\n", right_cmd);
    }

    rc = CmdO2QueryProgramPath(module, (unsigned long)sizeof(module));
    if (rc != 0) {
        fprintf(stderr,
                "cmd32os2: cannot determine command-processor path (OS/2 rc=%lu)\n",
                rc);
        return 1;
    }

    readh = 0;
    writeh = 0;
    rc = CmdO2CreatePipe(&readh, &writeh, 4096UL);
    if (rc != 0) {
        fprintf(stderr, "cmd32os2: DosCreatePipe failed (OS/2 rc=%lu)\n", rc);
        return 1;
    }

    saved_in.value = CMDO2_HANDLE_ALLOCATE;
    saved_in.os2_value = CMDO2_HANDLE_ALLOCATE;
    saved_out.value = CMDO2_HANDLE_ALLOCATE;
    saved_out.os2_value = CMDO2_HANDLE_ALLOCATE;
    have_saved_in = 0;
    have_saved_out = 0;
    left_started = 0;
    right_started = 0;
    left_pid = 0UL;
    right_pid = 0UL;
    left_code = 1;
    right_code = 1;

    /* Left side: duplicate the pipe writer onto HFILE 1, close the parent's
     * original writer, then EXEC_ASYNCRESULT the shell worker.  Restoring
     * stdout immediately leaves only the child with a live pipe writer. */
    rc = CmdO2StdSave(1UL, &saved_out);
    if (rc != 0)
        goto fail;
    have_saved_out = 1;
    rc = CmdO2StdRedirectHandle(1UL, writeh);
    if (rc != 0)
        goto fail;
    (void)CmdO2Close(writeh);
    writeh = CMDO2_HANDLE_ALLOCATE;

    if (!start_shell_child(module, left_cmd, &left_pid))
        goto fail;
    left_started = 1;
    if (pipe_trace_enabled())
        fprintf(stderr, "[pipe] left pid=%lu\n", left_pid);

    (void)CmdO2StdRestore(1UL, &saved_out);
    have_saved_out = 0;

    /* Right side: same operation for HFILE 0. */
    rc = CmdO2StdSave(0UL, &saved_in);
    if (rc != 0)
        goto fail;
    have_saved_in = 1;
    rc = CmdO2StdRedirectHandle(0UL, readh);
    if (rc != 0)
        goto fail;
    (void)CmdO2Close(readh);
    readh = CMDO2_HANDLE_ALLOCATE;

    if (!start_shell_child(module, right_cmd, &right_pid))
        goto fail;
    right_started = 1;
    if (pipe_trace_enabled())
        fprintf(stderr, "[pipe] right pid=%lu\n", right_pid);

    (void)CmdO2StdRestore(0UL, &saved_in);
    have_saved_in = 0;

    /* Pipeline status follows the right side.  Waiting right-first is safe:
     * both processes are already running concurrently. */
    (void)wait_shell_child(right_pid, &right_code);
    (void)wait_shell_child(left_pid, &left_code);
    if (pipe_trace_enabled())
        fprintf(stderr, "[pipe] done left=%d right=%d\n", left_code, right_code);
    (void)left_code;
    return right_code;

fail:
    if (have_saved_in)
        (void)CmdO2StdRestore(0UL, &saved_in);
    if (have_saved_out)
        (void)CmdO2StdRestore(1UL, &saved_out);
    if (readh != CMDO2_HANDLE_ALLOCATE)
        (void)CmdO2Close(readh);
    if (writeh != CMDO2_HANDLE_ALLOCATE)
        (void)CmdO2Close(writeh);

    /* If the left worker was already started, closing the parent's read/write
     * endpoints causes it to see a broken pipe rather than leaving it orphaned. */
    if (right_started)
        (void)wait_shell_child(right_pid, &right_code);
    if (left_started)
        (void)wait_shell_child(left_pid, &left_code);
    return 1;
}

static int shell_init(struct ShellState *s)
{
    memset(s, 0, sizeof(*s));

    if (!CmdO2PrepareLoader()) {
        fprintf(stderr,
                "cmd32os2: cannot locate sibling os2host32.exe; "
                "set OS2HOST32_LOADER explicitly\n");
        return 0;
    }
    if (!CmdO2Init()) {
        fprintf(stderr, "cmd32os2: %s\n", CmdO2InitError());
        return 0;
    }
    CmdBatchInit(&s->batch, s, batch_host_run_line, batch_host_exists);
    s->last_rc = 0;
    s->echo_enabled = 1;
    return 1;
}

static void shell_done(struct ShellState *s)
{
    CmdBatchDone(&s->batch);
    env_unwind_to(s, 0);
    CmdO2Done();
    memset(s, 0, sizeof(*s));
}


/* Split one command line into executable token + untouched argument tail.
 * Quotes around argv[0] are removed.  The returned tail points into line. */
static char *split_program(char *line, char *program, size_t cap)
{
    char *p;
    char *start;
    size_t n;
    int quoted;

    p = skip_ws(line);
    quoted = 0;
    if (*p == '"') {
        quoted = 1;
        ++p;
    }
    start = p;
    if (quoted) {
        while (*p && *p != '"')
            ++p;
    } else {
        while (*p && *p != ' ' && *p != '\t')
            ++p;
    }
    n = (size_t)(p - start);
    if (n + 1 > cap)
        return NULL;
    memcpy(program, start, n);
    program[n] = '\0';
    if (quoted && *p == '"')
        ++p;
    return skip_ws(p);
}

static int has_extension(const char *name)
{
    const char *p;
    const char *dot;
    const char *slash;
    const char *slash2;

    dot = strrchr(name, '.');
    slash = strrchr(name, '\\');
    slash2 = strrchr(name, '/');
    p = slash;
    if (slash2 != NULL && (p == NULL || slash2 > p))
        p = slash2;
    return dot != NULL && (p == NULL || dot > p);
}

static int path_exists_any(const char *path)
{
    return CmdO2PathExists(path);
}

static int path_is_file(const char *path)
{
    unsigned long attr;
    if (CmdO2QueryPathAttr(path, &attr) != 0)
        return 0;
    return (attr & CMDO2_ATTR_DIRECTORY) == 0;
}

static int ext_is(const char *name, const char *ext)
{
    const char *dot;
    dot = strrchr(name, '.');
    if (dot == NULL)
        return 0;
    return ci_cmp(dot, ext) == 0;
}

enum CommandKind {
    COMMAND_NOT_FOUND = 0,
    COMMAND_EXECUTABLE = 1,
    COMMAND_BATCH = 2,
    COMMAND_DOS_COM = 3
};

static const char *command_kind_name(int kind)
{
    if (kind == COMMAND_BATCH)
        return "batch";
    if (kind == COMMAND_DOS_COM)
        return "DOS .COM";
    if (kind == COMMAND_EXECUTABLE)
        return "executable";
    return "not-found";
}

static int resolve_trace_enabled(void)
{
    const char *p;
    p = getenv("CMD32_TRACE_RESOLVE");
    return p != NULL && p[0] != '\0' && p[0] != '0';
}

static int kind_from_name(const char *name)
{
    if (ext_is(name, ".cmd") || ext_is(name, ".bat"))
        return COMMAND_BATCH;
    if (ext_is(name, ".com"))
        return COMMAND_DOS_COM;
    return COMMAND_EXECUTABLE;
}

/* M29N command-name probing.  MS OS/2 CMD searches extensionless external
 * commands as .COM, .EXE, then .CMD.  We retain .BAT as a final compatibility
 * candidate because the recovered batch layer already understands it.
 *
 * A command with an explicit extension is tested exactly as written: we do
 * not silently substitute another executable type. */
static int candidate_exists(char *out, size_t cap, const char *base,
                            int *kind)
{
    static const char *suffixes[] = { ".com", ".exe", ".cmd", ".bat" };
    size_t i;
    size_t n;

    *kind = COMMAND_NOT_FOUND;
    n = strlen(base);
    if (n + 1 > cap)
        return 0;
    strcpy(out, base);
    if (has_extension(base)) {
        if (!path_is_file(out))
            return 0;
        *kind = kind_from_name(out);
        return 1;
    }

    for (i = 0; i < sizeof(suffixes) / sizeof(suffixes[0]); ++i) {
        size_t sn;
        sn = strlen(suffixes[i]);
        if (n + sn + 1 > cap)
            continue;
        strcpy(out, base);
        strcat(out, suffixes[i]);
        if (path_is_file(out)) {
            *kind = kind_from_name(out);
            return 1;
        }
    }
    return 0;
}

static void trace_resolution(const char *typed, const char *resolved,
                             int kind, const char *where)
{
    if (!resolve_trace_enabled())
        return;
    fprintf(stderr, "CMD RESOLVE: typed=[%s] resolved=[%s] kind=%s via=%s\n",
            typed, resolved != NULL ? resolved : "",
            command_kind_name(kind), where != NULL ? where : "none");
}

/* Resolve current directory first, then each element of CMD's private PATH.
 * The lookup is intentionally owned by CMD rather than delegated to Win32 or
 * to the host process environment.  Quoted PATH elements are accepted so an
 * old OS/2 shell can search directories containing spaces. */
static int resolve_command(char *program, size_t cap, int *kind)
{
    char typed[MAX_PATH];
    char trial[MAX_PATH];
    char base[MAX_PATH];
    char pathenv[CMDLINE_MAX];
    char where[MAX_PATH + 16];
    const char *p;
    const char *q;
    const char *sep;
    size_t dn;
    size_t pn;

    if (strlen(program) + 1 > sizeof(typed)) {
        *kind = COMMAND_NOT_FOUND;
        return 0;
    }
    strcpy(typed, program);

    if (candidate_exists(trial, sizeof(trial), program, kind)) {
        if (strlen(trial) + 1 <= cap)
            strcpy(program, trial);
        trace_resolution(typed, program, *kind, "current/explicit");
        return 1;
    }

    /* A path-bearing command is never searched through PATH.  candidate_exists
     * above has already tried the extension probes in that explicit directory. */
    if (strchr(program, '\\') != NULL || strchr(program, '/') != NULL ||
        strchr(program, ':') != NULL) {
        *kind = COMMAND_NOT_FOUND;
        trace_resolution(typed, program, *kind, "explicit");
        return 0;
    }

    if (CmdO2EnvGet("PATH", pathenv, (unsigned long)sizeof(pathenv)) != 0) {
        *kind = COMMAND_NOT_FOUND;
        trace_resolution(typed, program, *kind, "no-PATH");
        return 0;
    }

    p = pathenv;
    pn = strlen(program);
    while (*p != '\0') {
        sep = strchr(p, ';');
        q = sep != NULL ? sep : p + strlen(p);
        while (p < q && (*p == ' ' || *p == '\t'))
            ++p;
        while (q > p && (q[-1] == ' ' || q[-1] == '\t'))
            --q;
        if (q > p) {
            const char *ds;
            const char *de;
            ds = p;
            de = q;
            if (de - ds >= 2 && *ds == '"' && de[-1] == '"') {
                ++ds;
                --de;
            }
            dn = (size_t)(de - ds);
            if (dn + 1 + pn + 5 < sizeof(base)) {
                memcpy(base, ds, dn);
                base[dn] = '\0';
                if (dn != 0 && base[dn - 1] != '\\' && base[dn - 1] != '/')
                    strcat(base, "\\");
                strcat(base, program);
                if (candidate_exists(trial, sizeof(trial), base, kind)) {
                    if (strlen(trial) + 1 <= cap)
                        strcpy(program, trial);
                    if (dn + 1 < sizeof(where)) {
                        memcpy(where, ds, dn);
                        where[dn] = '\0';
                    } else {
                        strcpy(where, "PATH");
                    }
                    trace_resolution(typed, program, *kind, where);
                    return 1;
                }
            }
        }
        if (sep == NULL)
            break;
        p = sep + 1;
    }

    *kind = COMMAND_NOT_FOUND;
    trace_resolution(typed, program, *kind, "PATH");
    return 0;
}

static int resultcodes_errorlevel(const struct CmdO2ResultCodes *results)
{
    unsigned long value;

    if (results == NULL)
        return 1;
    if (results->codeTerminate == CMDO2_TC_EXIT)
        value = results->codeResult;
    else if (results->codeResult != 0UL)
        value = results->codeResult;
    else
        value = results->codeTerminate;
    return (int)value;
}

static int exec_resolved_mode(const char *typed_program, const char *program,
                              const char *tail, unsigned long exec_flag)
{
    char argblock[CMDLINE_MAX];
    char objectName[MAX_PATH];
    size_t pn;
    size_t tn;
    struct CmdO2ResultCodes results;
    CmdO2Rc rc;

    pn = strlen(typed_program);
    tn = strlen(tail);
    if (pn + tn + 3 > sizeof(argblock)) {
        fprintf(stderr, "The command line is too long.\n");
        return 1;
    }
    memcpy(argblock, typed_program, pn + 1);
    memcpy(argblock + pn + 1, tail, tn + 1);
    argblock[pn + 1 + tn + 1] = '\0';

    if (resolve_trace_enabled())
        fprintf(stderr, "CMD ARGS: argv0=[%s] tail=[%s]\n", typed_program, tail);

    objectName[0] = '\0';
    results.codeTerminate = 0;
    results.codeResult = 0;
    rc = CmdO2ExecPgm(objectName, (long)sizeof(objectName),
                      exec_flag, argblock, NULL, &results, program);
    if (rc != 0) {
        fprintf(stderr, "cmd32os2: DosExecPgm(%s) rc=%lu",
                program, (unsigned long)rc);
        if (objectName[0] != '\0')
            fprintf(stderr, " object=[%s]", objectName);
        fputc('\n', stderr);
        return 1;
    }
    if (exec_flag == CMDO2_EXEC_SYNC)
        return resultcodes_errorlevel(&results);
    return 0;
}

/* Plain invocation of a WINDOWAPI executable should behave like START /PM:
 * create a related PM session and return the CMD prompt immediately.  This
 * also makes the process visible to the SESMGR-backed PS/KILL commands. */
static int start_pm_resolved(const char *title, const char *program,
                             const char *args)
{
    struct CmdO2StartOptions o;
    char object[MAX_PATH];
    char *env;
    unsigned long sid;
    unsigned long pid;
    CmdO2Rc rc;

    env = snapshot_environment();
    if (env == NULL) {
        fprintf(stderr, "Out of memory.\n");
        return 1;
    }

    memset(&o, 0, sizeof(o));
    object[0] = '\0';
    o.title = title;
    o.program = program;
    o.inputs = args;
    o.environment = env;
    o.related = CMDO2_SSF_RELATED_CHILD;
    o.fgbg = CMDO2_SSF_FGBG_FORE;
    o.traceOpt = CMDO2_SSF_TRACEOPT_NONE;
    o.inheritOpt = CMDO2_SSF_INHERTOPT_PARENT;
    o.sessionType = CMDO2_SSF_TYPE_PM;
    o.pgmControl = CMDO2_SSF_CONTROL_VISIBLE;
    o.objectBuffer = object;
    o.objectBufferLen = (unsigned long)sizeof(object);

    sid = 0UL;
    pid = 0UL;
    rc = CmdO2StartSessionEx(&o, &sid, &pid);
    free(env);
    if (rc != 0UL) {
        fprintf(stderr, "cmd32os2: DosStartSession(%s) rc=%lu",
                program, (unsigned long)rc);
        if (object[0] != '\0')
            fprintf(stderr, " object=[%s]", object);
        fputc('\n', stderr);
        return 1;
    }
    if (resolve_trace_enabled())
        fprintf(stderr, "CMD PM AUTO: task=%lu pid=%lu program=[%s]\n",
                sid, pid, program);
    return 0;
}

static int exec_external_mode(struct ShellState *s, char *line,
                              int *want_exit, unsigned long exec_flag,
                              int keep_pm_console)
{
    char typed_program[MAX_PATH];
    char program[MAX_PATH];
    char *tail;
    CmdO2Rc rc;
    unsigned long appType;
    int kind;

    tail = split_program(line, typed_program, sizeof(typed_program));
    if (tail == NULL || typed_program[0] == '\0')
        return 1;
    strcpy(program, typed_program);
    if (!resolve_command(program, sizeof(program), &kind)) {
        fprintf(stderr, "cmd32os2: command not found: %s\n", typed_program);
        return 1;
    }

    if (kind == COMMAND_BATCH) {
        int via_call;
        int brc;

        if (exec_flag != CMDO2_EXEC_SYNC) {
            fprintf(stderr,
                    "DETACH of CMD/BAT files is not implemented yet.\n");
            return 1;
        }
        /* Classic batch chaining is different from CALL.  The M29O marker
         * originally remained set for the *entire* called child, which meant
         * a direct child.cmd inside `CALL parent.cmd` incorrectly inherited
         * CALL semantics.  Consume/suspend the marker only around this one
         * launch: the called frame is retained, but commands executed by that
         * frame once again distinguish their own CALL versus direct chain. */
        via_call = s->batch_call_depth > 0;
        if (via_call)
            --s->batch_call_depth;
        if (CmdBatchActive(&s->batch) && !via_call)
            CmdBatchEndCurrent(&s->batch);
        brc = run_batch_file(s, program, tail, want_exit);
        if (via_call)
            ++s->batch_call_depth;
        return brc;
    }
    if (kind == COMMAND_DOS_COM) {
        fprintf(stderr,
                "cmd32os2: %s resolves to DOS .COM; a DOS-session personality is not implemented yet.\n",
                typed_program);
        return 1;
    }

    /* Restore the loader-aware behavior of historical CMD.  DosQAppType is
     * advisory here: if an unusual executable cannot be classified, retain
     * the old synchronous DosExecPgm path rather than refusing to run it. */
    if (exec_flag == CMDO2_EXEC_SYNC && !keep_pm_console) {
        appType = CMDO2_FAPPTYP_NOTSPEC;
        rc = CmdO2QueryAppType(program, &appType);
        if (rc == CMDO2_NO_ERROR &&
            (appType & CMDO2_FAPPTYP_MASK) == CMDO2_FAPPTYP_WINDOWAPI)
            return start_pm_resolved(typed_program, program, tail);
    }

    return exec_resolved_mode(typed_program, program, tail, exec_flag);
}

static int exec_external(struct ShellState *s, char *line, int *want_exit)
{
    return exec_external_mode(s, line, want_exit, CMDO2_EXEC_SYNC, 0);
}

static void dump_environment(void)
{
    unsigned long bytes;
    unsigned long actual;
    char *block;
    char *p;

    bytes = CmdO2EnvSize();
    block = (char *)malloc((size_t)bytes);
    if (block == NULL)
        return;
    actual = 0;
    if (CmdO2EnvExport(block, bytes, &actual) != 0) {
        free(block);
        return;
    }
    p = block;
    while (*p != '\0') {
        /* Win32's hidden =C:=... drive entries are host implementation
         * details and are not part of the user-facing OS/2 SET listing. */
        if (*p != '=')
            puts(p);
        p += strlen(p) + 1;
    }
    free(block);
}

static char *snapshot_environment(void)
{
    unsigned long bytes;
    unsigned long actual;
    char *copy;

    bytes = CmdO2EnvSize();
    copy = (char *)malloc((size_t)bytes);
    if (copy == NULL)
        return NULL;
    actual = 0;
    if (CmdO2EnvExport(copy, bytes, &actual) != 0) {
        free(copy);
        return NULL;
    }
    return copy;
}

static void restore_environment(const char *block)
{
    if (block != NULL)
        (void)CmdO2EnvImport(block);
}

static int env_depth(const struct ShellState *s)
{
    const struct EnvFrame *e;
    int n;
    n = 0;
    e = s->env_stack;
    while (e != NULL) {
        ++n;
        e = e->next;
    }
    return n;
}

static int env_push(struct ShellState *s)
{
    struct EnvFrame *e;
    e = (struct EnvFrame *)calloc(1, sizeof(*e));
    if (e == NULL)
        return 0;
    e->block = snapshot_environment();
    if (e->block == NULL) {
        free(e);
        return 0;
    }
    e->next = s->env_stack;
    s->env_stack = e;
    return 1;
}

static int env_pop(struct ShellState *s)
{
    struct EnvFrame *e;
    if (s->env_stack == NULL)
        return 0;
    e = s->env_stack;
    s->env_stack = e->next;
    restore_environment(e->block);
    free(e->block);
    free(e);
    return 1;
}

static void env_unwind_to(struct ShellState *s, int depth)
{
    while (env_depth(s) > depth)
        env_pop(s);
}

static int batch_host_run_line(void *ctx, char *line, int *want_exit)
{
    return run_line((struct ShellState *)ctx, line, want_exit);
}

static int batch_host_exists(void *ctx, const char *path)
{
    (void)ctx;
    return path_exists_any(path);
}

static int run_batch_file(struct ShellState *s, const char *path,
                          const char *tail, int *want_exit)
{
    int depth;
    int rc;
    depth = env_depth(s);
    rc = CmdBatchRunFile(&s->batch, path, tail, want_exit);
    /* SETLOCAL is automatically unwound when a batch invocation returns. */
    env_unwind_to(s, depth);
    return rc;
}

/* --------------------------------------------------------------------- */
/* Recovered NT CMD-style built-in boundary.                             */
/* Names/source modules below come from the Dec-1991 COFF symbol table.  */
/* --------------------------------------------------------------------- */

static int eEcho(struct ShellState *s, char *tail, int *want_exit)
{
    (void)want_exit;
    tail = skip_ws(tail);
    if (ci_cmp(tail, "OFF") == 0) {
        s->echo_enabled = 0;
        return 0;
    }
    if (ci_cmp(tail, "ON") == 0) {
        s->echo_enabled = 1;
        return 0;
    }
    if (*tail == '\0') {
        printf("ECHO is %s.\n", s->echo_enabled ? "on" : "off");
        return 0;
    }
    puts(tail);
    return 0;
}

static CmdO2Rc set_guest_environment(const char *name, const char *value)
{
    CmdO2Rc rc;

    rc = CmdO2EnvSet(name, value);
    if (rc != 0)
        return rc;

    /* OS2PATH is the host/personality-facing seed; PATH is the historical
     * guest variable.  Keep the two synchronized whichever spelling the
     * operator uses from CMD. */
    if (ci_cmp(name, "OS2PATH") == 0)
        return CmdO2EnvSet("PATH", value);
    if (ci_cmp(name, "PATH") == 0)
        return CmdO2EnvSet("OS2PATH", value);
    return 0;
}

static int eSet(struct ShellState *s, char *tail, int *want_exit)
{
    char *eq;
    char value[CMDLINE_MAX];
    CmdO2Rc rc;
    (void)s;
    (void)want_exit;

    if (*tail == '\0') {
        dump_environment();
        return 0;
    }
    eq = strchr(tail, '=');
    if (eq == NULL) {
        rc = CmdO2EnvGet(tail, value, (unsigned long)sizeof(value));
        if (rc == 0)
            printf("%s=%s\n", tail, value);
        return 0;
    }
    *eq = '\0';
    rc = set_guest_environment(tail, eq[1] == '\0' ? (const char *)0 : eq + 1);
    return rc == 0 ? 0 : 1;
}

static int ePath(struct ShellState *s, char *tail, int *want_exit)
{
    char value[CMDLINE_MAX];
    CmdO2Rc rc;
    (void)s;
    (void)want_exit;

    tail = skip_ws(tail);
    if (*tail == '\0') {
        rc = CmdO2EnvGet("PATH", value, (unsigned long)sizeof(value));
        if (rc == 0)
            printf("PATH=%s\n", value);
        else
            puts("PATH=");
        return 0;
    }
    if (*tail == '=')
        tail = skip_ws(tail + 1);
    if (tail[0] == ';' && tail[1] == '\0')
        tail = "";
    rc = set_guest_environment("PATH", *tail == '\0' ? (const char *)0 : tail);
    return rc == 0 ? 0 : 1;
}

static int eChdir(struct ShellState *s, char *tail, int *want_exit)
{
    char cwd[MAX_PATH];
    char path[MAX_PATH];
    CmdO2Rc rc;
    (void)s;
    (void)want_exit;

    if (*tail == '\0') {
        rc = CmdO2QueryCurrentDir(cwd, (unsigned long)sizeof(cwd));
        if (rc == 0)
            puts(cwd);
        return rc == 0 ? 0 : 1;
    }
    copy_unquoted(path, sizeof(path), tail);
    rc = CmdO2SetCurrentDir(path);
    if (rc != 0) {
        fprintf(stderr, "The system cannot find the path specified.\n");
        return 1;
    }
    return 0;
}

static int eMkdir(struct ShellState *s, char *tail, int *want_exit)
{
    char path[MAX_PATH];
    CmdO2Rc rc;
    (void)s;
    (void)want_exit;
    copy_unquoted(path, sizeof(path), tail);
    rc = path[0] == '\0' ? CMDO2_ERROR_INVALID_PARAMETER : CmdO2CreateDir(path);
    if (rc != 0) {
        fprintf(stderr, "Unable to create directory.\n");
        return 1;
    }
    return 0;
}

static int eRmdir(struct ShellState *s, char *tail, int *want_exit)
{
    char path[MAX_PATH];
    CmdO2Rc rc;
    (void)s;
    (void)want_exit;
    copy_unquoted(path, sizeof(path), tail);
    rc = path[0] == '\0' ? CMDO2_ERROR_INVALID_PARAMETER : CmdO2DeleteDir(path);
    if (rc != 0) {
        fprintf(stderr, "Unable to remove directory.\n");
        return 1;
    }
    return 0;
}

static int eExit(struct ShellState *s, char *tail, int *want_exit)
{
    (void)s;
    (void)tail;
    *want_exit = 1;
    return 0;
}

static int eVersion(struct ShellState *s, char *tail, int *want_exit)
{
    (void)s;
    (void)tail;
    (void)want_exit;
    puts("OS/2 CMD32 personality bootstrap - M31G QoL TABCOMP R3");
    return 0;
}

static int eType(struct ShellState *s, char *tail, int *want_exit)
{
    CmdO2Handle h;
    CmdO2Rc rc;
    unsigned long got;
    char path[MAX_PATH];
    char *buffer;
    unsigned long buffer_size;
    size_t wrote;
    (void)s;
    (void)want_exit;

    copy_unquoted(path, sizeof(path), tail);
    if (path[0] == '\0') {
        fprintf(stderr, "The syntax of the command is incorrect.\n");
        return 1;
    }
    rc = CmdO2OpenRead(path, &h);
    if (rc != 0) {
        fprintf(stderr, "The system cannot find the file specified.\n");
        return 1;
    }

    /* Like COPY's M29P transfer window, keep TYPE's I/O buffer out of the
     * historical C/386 stack. */
    buffer_size = 16384UL;
    buffer = (char *)malloc((size_t)buffer_size);
    if (buffer == NULL) {
        (void)CmdO2Close(h);
        fprintf(stderr, "Insufficient memory.\n");
        return 1;
    }
    for (;;) {
        got = 0;
        rc = CmdO2Read(h, buffer, buffer_size, &got);
        if (rc != 0) {
            free(buffer);
            (void)CmdO2Close(h);
            return 1;
        }
        if (got == 0)
            break;
        wrote = fwrite(buffer, 1, (size_t)got, stdout);
        if (wrote != (size_t)got) {
            free(buffer);
            (void)CmdO2Close(h);
            return 1;
        }
    }
    free(buffer);
    (void)CmdO2Close(h);
    return 0;
}

static void append_star(char *dst, size_t cap)
{
    size_t n;
    n = strlen(dst);
    if (n != 0 && dst[n - 1] != '\\' && dst[n - 1] != '/') {
        if (n + 1 < cap) {
            dst[n++] = '\\';
            dst[n] = '\0';
        }
    }
    if (n + 2 < cap)
        strcat(dst, "*");
}

static void directory_from_pattern(const char *pattern, char *out, size_t cap)
{
    const char *slash;
    const char *slash2;
    const char *last;
    size_t n;
    CmdO2Rc rc;

    slash = strrchr(pattern, '\\');
    slash2 = strrchr(pattern, '/');
    last = slash;
    if (slash2 != NULL && (last == NULL || slash2 > last))
        last = slash2;

    if (last == NULL) {
        rc = CmdO2QueryCurrentDir(out, (unsigned long)cap);
        if (rc != 0)
            strcpy(out, ".");
        return;
    }
    n = (size_t)(last - pattern);
    if (n == 2 && pattern[1] == ':')
        ++n; /* preserve C:\ */
    if (n >= cap)
        n = cap - 1;
    memcpy(out, pattern, n);
    out[n] = '\0';
}

static void unpack_dos_datetime(unsigned short d, unsigned short t,
                                unsigned *year, unsigned *month,
                                unsigned *day, unsigned *hour,
                                unsigned *minute)
{
    *day = (unsigned)(d & 31U);
    *month = (unsigned)((d >> 5) & 15U);
    *year = 1980U + (unsigned)((d >> 9) & 127U);
    *minute = (unsigned)((t >> 5) & 63U);
    *hour = (unsigned)((t >> 11) & 31U);
}

static int eDirectory(struct ShellState *s, char *tail, int *want_exit)
{
    struct CmdO2FindData fd;
    CmdO2FindHandle h;
    CmdO2Rc rc;
    char request[MAX_PATH];
    char pattern[MAX_PATH + 3];
    char display[MAX_PATH];
    char arg[MAX_PATH];
    const char *p;
    unsigned long attrs;
    unsigned long files;
    unsigned long dirs;
    double bytes;
    int found;
    int bare;
    int show_all;
    int ar;
    unsigned year, month, day, hour, minute;
    (void)s;
    (void)want_exit;

    request[0] = '\0';
    bare = 0;
    show_all = 0;
    p = tail;
    for (;;) {
        ar = next_builtin_arg(&p, arg, sizeof(arg));
        if (ar == 0)
            break;
        if (ar < 0) {
            fprintf(stderr, "The syntax of the command is incorrect.\n");
            return 1;
        }
        if (ci_cmp(arg, "/B") == 0 || ci_cmp(arg, "-B") == 0) {
            bare = 1;
            continue;
        }
        if (ci_cmp(arg, "/A") == 0 || ci_cmp(arg, "-A") == 0) {
            show_all = 1;
            continue;
        }
        if ((arg[0] == '/' || arg[0] == '-') && arg[1] != '\0') {
            fprintf(stderr, "Invalid DIR switch - %s\n", arg);
            return 1;
        }
        if (request[0] != '\0') {
            fprintf(stderr, "The syntax of the command is incorrect.\n");
            return 1;
        }
        if (strlen(arg) + 1U > sizeof(request)) {
            fprintf(stderr, "The path is too long.\n");
            return 1;
        }
        strcpy(request, arg);
    }
    if (request[0] == '\0')
        strcpy(request, ".");

    if (strlen(request) + 3U >= sizeof(pattern)) {
        fprintf(stderr, "The path is too long.\n");
        return 1;
    }
    strcpy(pattern, request);
    /* A trailing separator already says that the operand is a directory.
     * Some OS/2 path-query backends reject "dir\\" even though
     * FindFirst("dir\\*") is valid.  This also makes DIR cooperate
     * naturally with tab completion, which appends a backslash to unique
     * directory matches. */
    {
        size_t request_len;
        request_len = strlen(request);
        if (request_len != 0U &&
            (request[request_len - 1U] == '\\' ||
             request[request_len - 1U] == '/')) {
            append_star(pattern, sizeof(pattern));
        } else {
            rc = CmdO2QueryPathAttr(request, &attrs);
            if (rc == 0 && (attrs & CMDO2_ATTR_DIRECTORY))
                append_star(pattern, sizeof(pattern));
        }
    }

    if (!bare) {
        directory_from_pattern(pattern, display, sizeof(display));
        printf(" Directory of %s\n\n", display);
    }

    rc = CmdO2FindFirst(pattern, &h, &fd);
    if (rc != 0) {
        if (!bare)
            puts("File not found");
        return 1;
    }

    files = 0;
    dirs = 0;
    bytes = 0.0;
    found = 0;
    for (;;) {
        if (strcmp(fd.name, ".") != 0 && strcmp(fd.name, "..") != 0 &&
            (show_all || (fd.attr & (CMDO2_ATTR_HIDDEN | CMDO2_ATTR_SYSTEM)) == 0UL)) {
            found = 1;
            if (bare) {
                puts(fd.name);
                if (fd.attr & CMDO2_ATTR_DIRECTORY)
                    ++dirs;
                else {
                    ++files;
                    bytes += (double)fd.size;
                }
            } else {
                unpack_dos_datetime(fd.date, fd.time, &year, &month, &day,
                                    &hour, &minute);
                printf("%04u-%02u-%02u  %02u:%02u  ",
                       year, month, day, hour, minute);
                if (fd.attr & CMDO2_ATTR_DIRECTORY) {
                    printf("<DIR>          %s\n", fd.name);
                    ++dirs;
                } else {
                    printf("%12lu  %s\n", fd.size, fd.name);
                    bytes += (double)fd.size;
                    ++files;
                }
            }
        }
        rc = CmdO2FindNext(h, &fd);
        if (rc != 0)
            break;
    }
    (void)CmdO2FindClose(h);
    if (rc != CMDO2_ERROR_NO_MORE_FILES && rc != 0)
        return 1;

    if (!found) {
        if (!bare)
            puts("File not found");
        return 1;
    }
    if (!bare) {
        printf("%10lu File(s) %12.0f bytes\n", files, bytes);
        printf("%10lu Dir(s)\n", dirs);
    }
    return 0;
}

static int eCls(struct ShellState *s, char *tail, int *want_exit)
{
    CmdO2Rc rc;
    (void)s;
    (void)tail;
    (void)want_exit;
    rc = CmdO2VioClear();
    return rc == 0 ? 0 : 1;
}

static int ePause(struct ShellState *s, char *tail, int *want_exit)
{
    struct CmdO2KbdKeyInfo key;
    static const char msg[] = "Press any key to continue . . .";
    static const char nl[] = "\r\n";
    CmdO2Rc rc;
    (void)s;
    (void)tail;
    (void)want_exit;
    (void)CmdO2VioWrtTTY(msg, (unsigned long)(sizeof(msg) - 1U));
    (void)CmdO2KbdFlushBuffer();
    rc = CmdO2KbdCharIn(&key, CMDO2_IO_WAIT);
    (void)CmdO2VioWrtTTY(nl, 2UL);
    return rc == 0 ? 0 : 1;
}

static int eRem(struct ShellState *s, char *tail, int *want_exit)
{
    (void)s;
    (void)tail;
    (void)want_exit;
    return 0;
}

/* --------------------------------------------------------------------- */
/* M25: recovered cbatch.c command boundaries.                           */
/* --------------------------------------------------------------------- */

static int eGoto(struct ShellState *s, char *tail, int *want_exit)
{
    (void)want_exit;
    return CmdBatchGoto(&s->batch, tail);
}

static int eShift(struct ShellState *s, char *tail, int *want_exit)
{
    (void)tail;
    (void)want_exit;
    return CmdBatchShift(&s->batch);
}

static int eIf(struct ShellState *s, char *tail, int *want_exit)
{
    return CmdBatchIf(&s->batch, tail, s->last_rc, want_exit);
}

static int eFor(struct ShellState *s, char *tail, int *want_exit)
{
    return CmdBatchFor(&s->batch, tail, want_exit);
}

static int eSetlocal(struct ShellState *s, char *tail, int *want_exit)
{
    (void)tail;
    (void)want_exit;
    if (!env_push(s)) {
        fprintf(stderr, "Unable to localize environment.\n");
        return 1;
    }
    return 0;
}

static int eEndlocal(struct ShellState *s, char *tail, int *want_exit)
{
    (void)tail;
    (void)want_exit;
    if (!env_pop(s)) {
        fprintf(stderr, "ENDLOCAL was unexpected at this time.\n");
        return 1;
    }
    return 0;
}

static int eCall(struct ShellState *s, char *tail, int *want_exit)
{
    char line[CMDLINE_MAX];
    char *p;
    int depth;
    int rc;

    p = skip_ws(tail);
    if (*p == '\0') {
        fprintf(stderr, "The syntax of the command is incorrect.\n");
        return 1;
    }
    if (*p == ':') {
        /* A SETLOCAL opened by a batch subroutine is scoped to that CALL
         * frame even when the subroutine returns via GOTO :EOF without an
         * explicit ENDLOCAL. */
        depth = env_depth(s);
        rc = CmdBatchCallLabel(&s->batch, p, want_exit);
        env_unwind_to(s, depth);
        return rc;
    }

    if (strlen(p) + 1 > sizeof(line))
        return 1;
    strcpy(line, p);
    ++s->batch_call_depth;
    rc = run_line(s, line, want_exit);
    --s->batch_call_depth;
    return rc;
}

/* --------------------------------------------------------------------- */
/* M26: recovered cfile.c command boundary.                              */
/* --------------------------------------------------------------------- */

static int eCopy(struct ShellState *s, char *tail, int *want_exit)
{
    (void)s;
    (void)want_exit;
    return CmdFileCopy(tail);
}

static int eDelete(struct ShellState *s, char *tail, int *want_exit)
{
    (void)s;
    (void)want_exit;
    return CmdFileDelete(tail);
}

static int eRename(struct ShellState *s, char *tail, int *want_exit)
{
    (void)s;
    (void)want_exit;
    return CmdFileRename(tail);
}

static int eMove(struct ShellState *s, char *tail, int *want_exit)
{
    (void)s;
    (void)want_exit;
    return CmdFileMove(tail);
}

/* START tokenization is intentionally local rather than using the main shell
 * lexer: START has the historical leading quoted-title ambiguity and /PGM is
 * the escape hatch for a quoted executable pathname.  Tokens are carved out
 * of a private mutable copy; the returned remainder remains the PgmInputs. */
static char *start_next_token(char *p, char **token, int *quoted)
{
    char *q;

    p = skip_ws(p);
    if (*p == '\0') {
        *token = NULL;
        if (quoted != NULL)
            *quoted = 0;
        return p;
    }
    if (*p == '"') {
        if (quoted != NULL)
            *quoted = 1;
        ++p;
        *token = p;
        q = strchr(p, '"');
        if (q == NULL)
            return p + strlen(p);
        *q = '\0';
        return q + 1;
    }
    if (quoted != NULL)
        *quoted = 0;
    *token = p;
    q = p;
    while (*q != '\0' && *q != ' ' && *q != '\t')
        ++q;
    if (*q != '\0') {
        *q = '\0';
        ++q;
    }
    return q;
}

static void start_help(void)
{
    puts("Starts an OS/2 program in another session; /PMC keeps it in this console.");
    puts("START [\"title\"] [/F|/B] [/FS|/WIN|/PM|/PMC] [/MAX|/MIN] [/I] [/N] [/PGM] program [args]");
    puts("  /F    foreground session       /B    background session");
    puts("  /FS   full-screen VIO          /WIN  windowable VIO");
    puts("  /PM   Presentation Manager     /PMC  PM attached to this console");
    puts("  /MAX  initially maximized      /MIN  initially minimized");
    puts("  /I    inherit shell-start environment");
    puts("  /N    execute program directly /PGM  next token is program name");
    puts("Use /PGM before a quoted program pathname; a leading quoted token is the session title.");
}

static int eStart(struct ShellState *s, char *tail, int *want_exit)
{
    char *line;
    char *p;
    char *token;
    char *args;
    char program[MAX_PATH];
    char typed_program[MAX_PATH];
    char title[MAX_PATH];
    char object[MAX_PATH];
    char *env;
    unsigned long sid;
    unsigned long pid;
    unsigned long fgbg;
    unsigned long sessionType;
    unsigned long inheritOpt;
    unsigned long pgmControl;
    int quoted;
    int kind;
    int title_set;
    int pgm_next;
    int forced_type;
    int pm_console;
    CmdO2Rc rc;
    struct CmdO2StartOptions o;

    (void)s;
    (void)want_exit;
    p = skip_ws(tail);
    if (*p == '\0') {
        fprintf(stderr, "The syntax of the command is incorrect.\n");
        return 1;
    }
    if (strlen(p) + 1U > CMDLINE_MAX) {
        fprintf(stderr, "The command line is too long.\n");
        return 1;
    }

    line = (char *)malloc(strlen(p) + 1U);
    if (line == NULL) {
        fprintf(stderr, "Out of memory.\n");
        return 1;
    }
    strcpy(line, p);
    p = line;
    program[0] = '\0';
    title[0] = '\0';
    object[0] = '\0';
    args = NULL;
    title_set = 0;
    pgm_next = 0;
    forced_type = 0;
    pm_console = 0;

    /* Historical START defaults to a background session unless /F or an
     * explicit /FS, /WIN or /PM selection makes it foreground. */
    fgbg = CMDO2_SSF_FGBG_BACK;
    sessionType = CMDO2_SSF_TYPE_DEFAULT;
    inheritOpt = CMDO2_SSF_INHERTOPT_PARENT;
    pgmControl = CMDO2_SSF_CONTROL_VISIBLE;

    for (;;) {
        char *next;
        next = start_next_token(p, &token, &quoted);
        if (token == NULL)
            break;

        if (pgm_next) {
            if (strlen(token) + 1U > sizeof(program)) {
                free(line);
                fprintf(stderr, "Program name is too long.\n");
                return 1;
            }
            strcpy(program, token);
            args = skip_ws(next);
            p = next;
            break;
        }

        if (token[0] == '/' || token[0] == '-') {
            const char *opt;
            opt = token + 1;
            if (ci_cmp(opt, "?") == 0 || ci_cmp(opt, "HELP") == 0) {
                start_help();
                free(line);
                return 0;
            } else if (ci_cmp(opt, "F") == 0 || ci_cmp(opt, "FG") == 0) {
                fgbg = CMDO2_SSF_FGBG_FORE;
            } else if (ci_cmp(opt, "B") == 0 || ci_cmp(opt, "BG") == 0) {
                fgbg = CMDO2_SSF_FGBG_BACK;
            } else if (ci_cmp(opt, "FS") == 0) {
                sessionType = CMDO2_SSF_TYPE_FULLSCREEN;
                forced_type = 1;
                pm_console = 0;
            } else if (ci_cmp(opt, "WIN") == 0) {
                sessionType = CMDO2_SSF_TYPE_WINDOWABLEVIO;
                forced_type = 1;
                pm_console = 0;
            } else if (ci_cmp(opt, "PM") == 0) {
                sessionType = CMDO2_SSF_TYPE_PM;
                forced_type = 1;
                pm_console = 0;
            } else if (ci_cmp(opt, "PMC") == 0) {
                pm_console = 1;
            } else if (ci_cmp(opt, "MAX") == 0) {
                pgmControl &= ~CMDO2_SSF_CONTROL_MINIMIZE;
                pgmControl |= CMDO2_SSF_CONTROL_MAXIMIZE;
            } else if (ci_cmp(opt, "MIN") == 0) {
                pgmControl &= ~CMDO2_SSF_CONTROL_MAXIMIZE;
                pgmControl |= CMDO2_SSF_CONTROL_MINIMIZE;
            } else if (ci_cmp(opt, "I") == 0) {
                inheritOpt = CMDO2_SSF_INHERTOPT_SHELL;
            } else if (ci_cmp(opt, "N") == 0) {
                /* Direct execution is already the M29L path. */
            } else if (ci_cmp(opt, "PGM") == 0) {
                pgm_next = 1;
            } else if (ci_cmp(opt, "C") == 0 || ci_cmp(opt, "K") == 0) {
                fprintf(stderr, "START /%s through a secondary CMD session is not implemented yet.\n", opt);
                free(line);
                return 1;
            } else if (ci_cmp(opt, "DOS") == 0) {
                fprintf(stderr, "START /DOS requires a DOS-session personality not implemented yet.\n");
                free(line);
                return 1;
            } else {
                fprintf(stderr, "Invalid START option - %s\n", token);
                free(line);
                return 1;
            }
            p = next;
            continue;
        }

        if (quoted && !title_set) {
            if (strlen(token) + 1U > sizeof(title)) {
                free(line);
                fprintf(stderr, "Session title is too long.\n");
                return 1;
            }
            strcpy(title, token);
            title_set = 1;
            p = next;
            continue;
        }

        if (strlen(token) + 1U > sizeof(program)) {
            free(line);
            fprintf(stderr, "Program name is too long.\n");
            return 1;
        }
        strcpy(program, token);
        args = skip_ws(next);
        p = next;
        break;
    }

    if (pgm_next && program[0] == '\0') {
        fprintf(stderr, "START /PGM requires a program name.\n");
        free(line);
        return 1;
    }
    if (program[0] == '\0') {
        fprintf(stderr, "The syntax of the command is incorrect.\n");
        free(line);
        return 1;
    }
    if (forced_type)
        fgbg = CMDO2_SSF_FGBG_FORE;
    if (!title_set)
        strcpy(title, program);
    if (args == NULL)
        args = "";

    strcpy(typed_program, program);
    if (!resolve_command(program, sizeof(program), &kind)) {
        fprintf(stderr, "START: command not found: %s\n", typed_program);
        free(line);
        return 1;
    }
    if (kind == COMMAND_BATCH) {
        fprintf(stderr, "START of CMD/BAT files requires the /C or /K secondary-CMD path, not implemented yet.\n");
        free(line);
        return 1;
    }
    if (kind == COMMAND_DOS_COM) {
        fprintf(stderr, "START /DOS requires a DOS-session personality not implemented yet.\n");
        free(line);
        return 1;
    }

    /* /PMC is the explicit escape hatch from automatic PM session launch.
     * It deliberately uses the old synchronous DosExecPgm path so stdout and
     * stderr remain attached to this CMD32 console for debugging. */
    if (pm_console) {
        int direct_rc;
        direct_rc = exec_resolved_mode(typed_program, program, args,
                                       CMDO2_EXEC_SYNC);
        free(line);
        return direct_rc;
    }

    env = NULL;
    if (inheritOpt == CMDO2_SSF_INHERTOPT_PARENT) {
        env = snapshot_environment();
        if (env == NULL) {
            fprintf(stderr, "Out of memory.\n");
            free(line);
            return 1;
        }
    }

    memset(&o, 0, sizeof(o));
    o.title = title;
    o.program = program;
    o.inputs = args;
    o.environment = env;
    o.related = CMDO2_SSF_RELATED_CHILD;
    o.fgbg = fgbg;
    o.traceOpt = CMDO2_SSF_TRACEOPT_NONE;
    o.inheritOpt = inheritOpt;
    o.sessionType = sessionType;
    o.pgmControl = pgmControl;
    o.objectBuffer = object;
    o.objectBufferLen = (unsigned long)sizeof(object);

    sid = 0UL;
    pid = 0UL;
    rc = CmdO2StartSessionEx(&o, &sid, &pid);
    if (env != NULL)
        free(env);
    free(line);
    if (rc != 0UL) {
        fprintf(stderr, "cmd32os2: DosStartSession(%s) rc=%lu",
                program, (unsigned long)rc);
        if (object[0] != '\0')
            fprintf(stderr, " object=[%s]", object);
        fputc('\n', stderr);
        return 1;
    }
    (void)sid;
    (void)pid;
    return 0;
}

static int eDetach(struct ShellState *s, char *tail, int *want_exit)
{
    char line[CMDLINE_MAX];
    char *p;

    (void)want_exit;
    p = skip_ws(tail);
    if (*p == '\0') {
        fprintf(stderr, "The syntax of the command is incorrect.\n");
        return 1;
    }
    if (strlen(p) + 1 > sizeof(line)) {
        fprintf(stderr, "The command line is too long.\n");
        return 1;
    }
    strcpy(line, p);
    return exec_external_mode(s, line, want_exit, CMDO2_EXEC_BACKGROUND, 0);
}

static int parse_task_id(const char *text, unsigned long *value)
{
    unsigned long v;
    unsigned long maxv;
    unsigned digit;

    if (text == NULL || value == NULL || *text == '\0')
        return 0;
    v = 0UL;
    maxv = ~0UL;
    while (*text != '\0') {
        if (*text < '0' || *text > '9')
            return 0;
        digit = (unsigned)(*text - '0');
        if (v > (maxv - (unsigned long)digit) / 10UL)
            return 0;
        v = v * 10UL + (unsigned long)digit;
        ++text;
    }
    if (v == 0UL)
        return 0;
    *value = v;
    return 1;
}

static int ePs(struct ShellState *s, char *tail, int *want_exit)
{
    struct CmdO2SessionInfo *entries;
    const char *p;
    unsigned long count;
    unsigned long i;
    CmdO2Rc rc;

    (void)s;
    (void)want_exit;
    p = skip_ws(tail);
    if (*p != '\0') {
        if (ci_cmp(p, "/?") == 0 || ci_cmp(p, "-?") == 0) {
            puts("PS");
            puts("Lists related child sessions started by this CMD32 shell.");
            return 0;
        }
        fprintf(stderr, "PS: unexpected argument - %s\n", p);
        return 1;
    }

    entries = (struct CmdO2SessionInfo *)malloc(
        (size_t)CMDO2_SESSION_QUERY_MAX * sizeof(entries[0]));
    if (entries == NULL) {
        fprintf(stderr, "Out of memory.\n");
        return 1;
    }
    count = 0UL;
    rc = CmdO2QuerySessions(entries, CMDO2_SESSION_QUERY_MAX, &count);
    if (rc != 0UL) {
        fprintf(stderr, "PS: session query failed rc=%lu\n",
                (unsigned long)rc);
        free(entries);
        return 1;
    }

    puts("TASK  PID       EXECUTABLE  TITLE");
    if (count == 0UL) {
        puts("(no child sessions)");
        free(entries);
        return 0;
    }
    for (i = 0UL; i < count; ++i) {
        printf("%-5lu %-9lu %s", entries[i].taskId, entries[i].pid,
               entries[i].program);
        if (entries[i].title[0] != '\0')
            printf("  %s", entries[i].title);
        putchar('\n');
    }
    free(entries);
    return 0;
}

static int eKill(struct ShellState *s, char *tail, int *want_exit)
{
    const char *p;
    char arg[64];
    char extra[8];
    unsigned long taskId;
    CmdO2Rc rc;
    int ar;

    (void)s;
    (void)want_exit;
    p = tail;
    ar = next_builtin_arg(&p, arg, sizeof(arg));
    if (ar == 1 && (ci_cmp(arg, "/?") == 0 || ci_cmp(arg, "-?") == 0)) {
        puts("KILL task-id");
        puts("Stops a related child session listed by PS.");
        return 0;
    }
    if (ar != 1 || !parse_task_id(arg, &taskId) ||
        next_builtin_arg(&p, extra, sizeof(extra)) != 0) {
        fprintf(stderr, "Usage: KILL task-id\n");
        return 1;
    }

    rc = CmdO2StopSession(taskId);
    if (rc == 0UL)
        return 0;
    if (rc == 369UL)
        fprintf(stderr, "KILL: no such child task - %lu\n", taskId);
    else if (rc == 460UL)
        fprintf(stderr, "KILL: task %lu is not owned by this shell.\n", taskId);
    else
        fprintf(stderr, "KILL: unable to stop task %lu, rc=%lu\n",
                taskId, (unsigned long)rc);
    return 1;
}

static int eNotYet(struct ShellState *s, char *tail, int *want_exit)
{
    (void)s;
    (void)tail;
    (void)want_exit;
    fprintf(stderr, "This CMD built-in is not implemented yet in M29P.\n");
    return 1;
}

/* Command names and e* symbol/module names are recovered from the Dec-1991
 * NT CMD COFF table and the OS/2 6.64 command-handler table. */
static const struct Builtin g_builtins[] = {
    { "DIR",      eDirectory, "cinfo.c",  "_eDirectory", 0U },
    { "TYPE",     eType,      "cinfo.c",  "_eType", 0U },
    { "VER",      eVersion,   "cinfo.c",  "_eVersion", 0U },
    { "CD",       eChdir,     "cpath.c",  "_eChdir", 0U },
    { "CHDIR",    eChdir,     "cpath.c",  "_eChdir", 0U },
    { "MD",       eMkdir,     "cpath.c",  "_eMkdir", 0U },
    { "MKDIR",    eMkdir,     "cpath.c",  "_eMkdir", 0U },
    { "RD",       eRmdir,     "cpath.c",  "_eRmdir", 0U },
    { "RMDIR",    eRmdir,     "cpath.c",  "_eRmdir", 0U },
    { "SET",      eSet,       "cenv.c",   "_eSet", 0U },
    { "ECHO",     eEcho,      "cbatch.c", "_eEcho", BUILTIN_KEEP_ERRORLEVEL },
    { "PAUSE",    ePause,     "cbatch.c", "_ePause", 0U },
    { "REM",      eRem,       "cparse.c", "_ParseRem", BUILTIN_KEEP_ERRORLEVEL },
    { "CLS",      eCls,       "cother.c", "_eCls", 0U },
    { "EXIT",     eExit,      "cother.c", "_eExit", 0U },

    { "ERASE",    eDelete,    "cfile.c",  "_eDelete", 0U },
    { "DEL",      eDelete,    "cfile.c",  "_eDelete", 0U },
    { "COPY",     eCopy,      "cfile.c",  "_eCopy", 0U },
    { "RENAME",   eRename,    "cfile.c",  "_eRename", 0U },
    { "REN",      eRename,    "cfile.c",  "_eRename", 0U },
    { "MOVE",     eMove,      "cfile.c",  "_eMove", 0U },
    { "DATE",     eNotYet,    "cclock.c", "_eDate", 0U },
    { "TIME",     eNotYet,    "cclock.c", "_eTime", 0U },
    { "PROMPT",   eNotYet,    "cenv.c",   "_ePrompt", 0U },
    { "PATH",     ePath,      "cenv.c",   "_ePath", 0U },
    { "GOTO",     eGoto,      "cbatch.c", "_eGoto", 0U },
    { "SHIFT",    eShift,     "cbatch.c", "_eShift", 0U },
    { "CALL",     eCall,      "cbatch.c", "_eCall", BUILTIN_KEEP_ERRORLEVEL },
    { "VERIFY",   eNotYet,    "cother.c", "_eVerify", 0U },
    { "VOL",      eNotYet,    "cinfo.c",  "_eVolume", 0U },
    { "SETLOCAL", eSetlocal,  "cbatch.c", "_eSetlocal", 0U },
    { "ENDLOCAL", eEndlocal,  "cbatch.c", "_eEndlocal", 0U },
    { "CHCP",     eNotYet,    "cfile.c",  "_eChcp", 0U },
    { "START",    eStart,     "cfile.c",  "_eStart", 0U },
    { "PS",       ePs,        "sesmgr.c", "_ePs", 0U },
    { "KILL",     eKill,      "sesmgr.c", "_eKill", 0U },
    { "DPATH",    eNotYet,    "cenv.c",   "_eAppend", 0U },
    { "KEYS",     eNotYet,    "ckeys.c",  "_eKeys", 0U },
    { "FOR",      eFor,       "cbatch.c", "_eFor", BUILTIN_KEEP_ERRORLEVEL },
    { "IF",       eIf,        "cbatch.c", "_eIf", BUILTIN_KEEP_ERRORLEVEL },
    { "DETACH",   eDetach,    "cop.c",    "_eDetach", 0U },
    { "EXTPROC",  eNotYet,    "cbatch.c", "_eExtproc", 0U }
};

static const struct Builtin *find_builtin(const char *name)
{
    size_t i;
    for (i = 0; i < sizeof(g_builtins) / sizeof(g_builtins[0]); ++i) {
        if (ci_cmp(name, g_builtins[i].name) == 0)
            return &g_builtins[i];
    }
    return NULL;
}

static int run_simple_line(struct ShellState *s, char *line, int *want_exit,
                           int *keep_errorlevel)
{
    char command[64];
    char *tail;
    const struct Builtin *b;

    if (keep_errorlevel != NULL)
        *keep_errorlevel = 0;

    trim_end(line);
    line = skip_ws(line);
    if (*line == '\0')
        return 0;

    /* Historical CMD accepts ECHO. as a blank-line idiom. */
    if (ci_cmp(line, "ECHO.") == 0) {
        fputc('\n', stdout);
        return 0;
    }
    if (ci_prefix(line, "REM ") || ci_cmp(line, "REM") == 0)
        return 0;

    /* OS/2/DOS drive selector is a command in its own right. */
    if (((line[0] >= 'A' && line[0] <= 'Z') ||
         (line[0] >= 'a' && line[0] <= 'z')) &&
        line[1] == ':' && line[2] == '\0') {
        unsigned long disk;
        CmdO2Rc drc;
        disk = (unsigned long)(toupper((unsigned char)line[0]) - 'A' + 1);
        drc = CmdO2SetDefaultDisk(disk);
        if (drc != 0) {
            fprintf(stderr, "The system cannot find the drive specified.\n");
            return 1;
        }
        return 0;
    }

    tail = split_program(line, command, sizeof(command));
    if (tail == NULL)
        return 1;

    b = find_builtin(command);
    if (b != NULL) {
        if (keep_errorlevel != NULL &&
            (b->flags & BUILTIN_KEEP_ERRORLEVEL) != 0U)
            *keep_errorlevel = 1;
        return b->fn(s, tail, want_exit);
    }

    return exec_external(s, line, want_exit);
}

static int execute_node(struct ShellState *s, const struct CmdNode *node,
                        int *want_exit);

static int execute_node_core(struct ShellState *s, const struct CmdNode *node,
                             int *want_exit)
{
    int left_rc;
    int right_rc;

    if (node == NULL)
        return 0;

    switch (node->type) {
    case CMD_NODE_SIMPLE:
        {
            char *line;
            size_t n;

            if (node->text == NULL)
                return 0;
            n = strlen(node->text) + 1U;
            if (n > CMDLINE_MAX) {
                fprintf(stderr, "The command line is too long.\n");
                return 1;
            }
            line = (char *)malloc(n);
            if (line == NULL)
                return 1;
            memcpy(line, node->text, n);
            {
                int keep_errorlevel;
                keep_errorlevel = 0;
                left_rc = run_simple_line(s, line, want_exit,
                                          &keep_errorlevel);
                free(line);
                if (!keep_errorlevel)
                    s->last_rc = left_rc;
            }
            return left_rc;
        }

    case CMD_NODE_GROUP:
        return execute_node(s, node->left, want_exit);

    case CMD_NODE_SEQUENCE:
        left_rc = execute_node(s, node->left, want_exit);
        if (*want_exit)
            return left_rc;
        right_rc = execute_node(s, node->right, want_exit);
        return right_rc;

    case CMD_NODE_AND:
        left_rc = execute_node(s, node->left, want_exit);
        if (*want_exit || left_rc != 0)
            return left_rc;
        return execute_node(s, node->right, want_exit);

    case CMD_NODE_OR:
        left_rc = execute_node(s, node->left, want_exit);
        if (*want_exit || left_rc == 0)
            return left_rc;
        return execute_node(s, node->right, want_exit);

    case CMD_NODE_PIPE:
        left_rc = execute_pipeline_node(node);
        s->last_rc = left_rc;
        return left_rc;
    default:
        fprintf(stderr, "cmd32os2: unknown parser node type %d\n", node->type);
        return 1;
    }
}

static int execute_node(struct ShellState *s, const struct CmdNode *node,
                        int *want_exit)
{
    struct RedirectState st;
    int redirected;
    int rc;

    if (node == NULL)
        return 0;

    redirected = 0;
    if (node->redirs != NULL) {
        if (!apply_redirections(node->redirs, &st))
            return 1;
        redirected = 1;
    }

    rc = execute_node_core(s, node, want_exit);

    if (redirected)
        restore_redirections(&st);
    return rc;
}

static int shell_env_lookup(void *ctx, const char *name,
                            char *dst, size_t cap)
{
    struct ShellState *s;
    CmdO2Rc rc;
    char number[32];
    size_t n;

    if (cap == 0)
        return 0;
    s = (struct ShellState *)ctx;
    if (s != NULL && ci_cmp(name, "ERRORLEVEL") == 0) {
        sprintf(number, "%d", s->last_rc);
        n = strlen(number);
        if (n + 1 > cap)
            return 0;
        memcpy(dst, number, n + 1);
        return 1;
    }
    rc = CmdO2EnvGet(name, dst, (unsigned long)cap);
    if (rc == CMDO2_ERROR_ENVVAR_NOT_FOUND) {
        dst[0] = '\0';
        return 1;
    }
    return rc == 0;
}

static int run_line(struct ShellState *s, char *line, int *want_exit)
{
    struct CmdParseResult parsed;
    char *expanded;
    char *parse_line;
    int rc;

    trim_end(line);
    if (*skip_ws(line) == '\0')
        return 0;

    expanded = NULL;
    parse_line = line;
    if (CmdBatchActive(&s->batch)) {
        expanded = (char *)malloc(CMDLINE_MAX);
        if (expanded == NULL) {
            s->last_rc = 1;
            return 1;
        }
        if (!CmdBatchExpandLine(&s->batch, line, expanded, CMDLINE_MAX)) {
            fprintf(stderr, "The command line is too long after batch expansion.\n");
            free(expanded);
            s->last_rc = 1;
            return 1;
        }
        parse_line = expanded;
    }

    if (!CmdParserEx(parse_line, &parsed, shell_env_lookup, s)) {
        fprintf(stderr, "cmd32os2: parse error at column %d: %s\n",
                parsed.error_offset + 1, parsed.error);
        if (expanded != NULL)
            free(expanded);
        s->last_rc = 1;
        return 1;
    }
    if (expanded != NULL)
        free(expanded);
    rc = execute_node(s, parsed.root, want_exit);
    CmdFreeTree(parsed.root);

    /* M29P1: command output must be visible before the next prompt or batch
     * line.  The native bootstrap mirrors OS/2 HFILE 0/1/2 into CRT fd
     * 0/1/2 for redirection.  After a redirected ECHO the CRT may retain
     * later console output in stdout's buffer even though the prompt itself
     * is emitted directly through VioWrtTTY.  Redirection teardown already
     * flushes while the redirected handle is still installed, so this
     * command-boundary flush is safe and restores deterministic ordering. */
    fflush(stdout);
    fflush(stderr);
    return rc;
}

static int read_console_line_basic(char *line, size_t cap)
{
    struct CmdO2KbdKeyInfo key;
    size_t used;
    CmdO2Rc rc;
    char ch;
    static const char erase_seq[] = "\b \b";
    static const char newline[] = "\r\n";

    if (line == NULL || cap < 2U)
        return 0;
    used = 0;
    line[0] = '\0';

    for (;;) {
        rc = CmdO2KbdCharIn(&key, CMDO2_IO_WAIT);
        if (rc != 0)
            return 0;
        ch = (char)key.chChar;

        if ((unsigned char)ch == 0x03U) {
            static const char break_seq[] = "^C\r\n";
            line[0] = '\0';
            (void)CmdO2VioWrtTTY(break_seq,
                                 (unsigned long)(sizeof(break_seq) - 1U));
            return -1;
        }
        if (ch == '\0')
            continue;
        if ((unsigned char)ch == 0x1aU && used == 0U)
            return 0;
        if (ch == '\r' || ch == '\n') {
            line[used] = '\0';
            (void)CmdO2VioWrtTTY(newline, 2UL);
            return 1;
        }
        if (ch == '\b' || (unsigned char)ch == 0x7fU) {
            if (used != 0U) {
                --used;
                line[used] = '\0';
                (void)CmdO2VioWrtTTY(erase_seq,
                                     (unsigned long)(sizeof(erase_seq) - 1U));
            }
            continue;
        }
        if ((unsigned char)ch < 0x20U)
            continue;
        if (used + 1U >= cap)
            continue;
        line[used++] = ch;
        line[used] = '\0';
        (void)CmdO2VioWrtTTY(&ch, 1UL);
    }
}

static void editor_copy_bounded(char *dst, size_t cap, const char *src)
{
    size_t n;
    if (cap == 0U)
        return;
    n = strlen(src);
    if (n >= cap)
        n = cap - 1U;
    if (n != 0U)
        memcpy(dst, src, n);
    dst[n] = '\0';
}

static void editor_move(char *dst, const char *src, size_t count)
{
    size_t i;
    if (dst == src || count == 0U)
        return;
    if (dst < src) {
        for (i = 0U; i < count; ++i)
            dst[i] = src[i];
    } else {
        i = count;
        while (i != 0U) {
            --i;
            dst[i] = src[i];
        }
    }
}

static const char *history_line(const struct CmdHistory *history, unsigned index)
{
    unsigned slot;
    if (history == NULL || index >= history->count)
        return "";
    slot = (history->first + index) % CMD_HISTORY_MAX;
    return history->lines[slot];
}

static int line_has_text(const char *line)
{
    while (*line != '\0') {
        if (!isspace((unsigned char)*line))
            return 1;
        ++line;
    }
    return 0;
}

static void history_add(struct CmdHistory *history, const char *line)
{
    unsigned slot;
    if (history == NULL || line == NULL || !line_has_text(line))
        return;

    if (history->count < CMD_HISTORY_MAX) {
        slot = (history->first + history->count) % CMD_HISTORY_MAX;
        ++history->count;
    } else {
        slot = history->first;
        history->first = (history->first + 1U) % CMD_HISTORY_MAX;
    }
    editor_copy_bounded(history->lines[slot], CMDLINE_MAX, line);
}

static void editor_write_spaces(size_t count)
{
    static const char spaces[] =
        "                                                                ";
    while (count != 0U) {
        size_t chunk;
        chunk = count;
        if (chunk > sizeof(spaces) - 1U)
            chunk = sizeof(spaces) - 1U;
        (void)CmdO2VioWrtTTY(spaces, (unsigned long)chunk);
        count -= chunk;
    }
}

/*
 * Redraw only the input field to the right of the prompt.  The editor uses a
 * horizontal viewport rather than allowing the editable text itself to wrap;
 * this keeps cursor addressing deterministic even when the prompt is already
 * on the last VIO row.  The full command remains in the 4096-byte buffer.
 */
static void editor_render(const char *line, size_t used, size_t cursor,
                          unsigned short row, unsigned short col,
                          unsigned short screen_cols, size_t *view_start)
{
    size_t width;
    size_t visible;
    size_t cursor_in_view;

    if (screen_cols <= col + 1U)
        return;
    width = (size_t)(screen_cols - col - 1U);
    if (width == 0U)
        return;

    if (*view_start > used)
        *view_start = used;
    if (cursor < *view_start)
        *view_start = cursor;
    else if (cursor >= *view_start + width) {
        if (width > 1U)
            *view_start = cursor - width + 1U;
        else
            *view_start = cursor;
    }

    visible = used - *view_start;
    if (visible > width)
        visible = width;

    (void)CmdO2VioSetCurPos(row, col);
    if (visible != 0U)
        (void)CmdO2VioWrtTTY(line + *view_start, (unsigned long)visible);
    if (visible < width)
        editor_write_spaces(width - visible);

    cursor_in_view = cursor - *view_start;
    if (cursor_in_view >= width)
        cursor_in_view = width - 1U;
    (void)CmdO2VioSetCurPos(row,
                            (unsigned short)(col + (unsigned short)cursor_in_view));
}

static void editor_replace_line(char *line, size_t cap, size_t *used,
                                size_t *cursor, const char *replacement)
{
    size_t n;
    n = strlen(replacement);
    if (n >= cap)
        n = cap - 1U;
    memcpy(line, replacement, n);
    line[n] = '\0';
    *used = n;
    *cursor = n;
}

/*
 * Interactive filename completion.  Keep all filesystem scratch in static
 * storage: CMD32's C/386 build deliberately runs with a small guest stack.
 * Completion is case-insensitive (like OS/2 paths) but preserves the spelling
 * returned by DosFindFirst/Next.  R1 completes only filesystem names; it does
 * not try to complete builtins, environment variables, or switches.
 */
struct CmdCompletionState {
    char token[MAX_PATH];
    char dir_prefix[MAX_PATH];
    char base_prefix[MAX_PATH];
    char pattern[MAX_PATH + 3];
    char common[MAX_PATH];
    char replacement[MAX_PATH + 3];
    unsigned long count;
    unsigned long unique_attr;
};

static struct CmdCompletionState g_completion;

static int completion_separator(unsigned char ch)
{
    return ch == ' ' || ch == '\t' || ch == '|' || ch == '<' ||
           ch == '>' || ch == '&';
}

static void editor_bell(void)
{
    static const char bell[] = "\a";
    (void)CmdO2VioWrtTTY(bell, 1UL);
}

/* Locate the filesystem token prefix which ends at the edit cursor.  An open
 * quote is treated as the token delimiter, so: TYPE "MY D<Tab> completes the
 * text inside the quote without including the quote in the filesystem path. */
static int completion_token_prefix(const char *line, size_t cursor,
                                   size_t *start, int *quoted)
{
    size_t i;
    size_t s;
    int in_quote;
    int token_quoted;

    s = 0U;
    in_quote = 0;
    token_quoted = 0;
    for (i = 0U; i < cursor; ++i) {
        unsigned char ch;
        ch = (unsigned char)line[i];
        if (ch == '"') {
            if (!in_quote) {
                in_quote = 1;
                token_quoted = 1;
                s = i + 1U;
            } else {
                in_quote = 0;
                token_quoted = 0;
                s = i + 1U;
            }
        } else if (!in_quote && completion_separator(ch)) {
            s = i + 1U;
            token_quoted = 0;
        }
    }

    /* A Tab immediately after a closed quote is not a request to enumerate
     * the whole current directory. */
    if (!in_quote && cursor != 0U && line[cursor - 1U] == '"')
        return 0;
    if (cursor < s || cursor - s >= sizeof(g_completion.token))
        return 0;

    *start = s;
    *quoted = token_quoted && in_quote;
    memcpy(g_completion.token, line + s, cursor - s);
    g_completion.token[cursor - s] = '\0';
    return 1;
}

static void completion_common_update(char *common, const char *name)
{
    size_t i;
    i = 0U;
    while (common[i] != '\0' && name[i] != '\0' &&
           toupper((unsigned char)common[i]) ==
               toupper((unsigned char)name[i]))
        ++i;
    common[i] = '\0';
}

static int completion_prepare_pattern(void)
{
    const char *slash;
    const char *slash2;
    const char *last;
    size_t dir_len;
    size_t token_len;

    if (strchr(g_completion.token, '*') != NULL ||
        strchr(g_completion.token, '?') != NULL)
        return 0;

    slash = strrchr(g_completion.token, '\\');
    slash2 = strrchr(g_completion.token, '/');
    last = slash;
    if (slash2 != NULL && (last == NULL || slash2 > last))
        last = slash2;

    dir_len = 0U;
    if (last != NULL)
        dir_len = (size_t)(last - g_completion.token) + 1U;
    if (dir_len >= sizeof(g_completion.dir_prefix))
        return 0;
    if (dir_len != 0U)
        memcpy(g_completion.dir_prefix, g_completion.token, dir_len);
    g_completion.dir_prefix[dir_len] = '\0';

    token_len = strlen(g_completion.token);
    if (token_len - dir_len >= sizeof(g_completion.base_prefix))
        return 0;
    strcpy(g_completion.base_prefix, g_completion.token + dir_len);

    if (token_len + 2U > sizeof(g_completion.pattern))
        return 0;
    strcpy(g_completion.pattern, g_completion.token);
    strcat(g_completion.pattern, "*");
    return 1;
}

static int completion_scan_matches(void)
{
    struct CmdO2FindData fd;
    CmdO2FindHandle h;
    CmdO2Rc rc;

    g_completion.count = 0UL;
    g_completion.common[0] = '\0';
    g_completion.unique_attr = 0UL;

    rc = CmdO2FindFirst(g_completion.pattern, &h, &fd);
    if (rc != 0)
        return 0;

    for (;;) {
        if (strcmp(fd.name, ".") != 0 && strcmp(fd.name, "..") != 0) {
            if (g_completion.count == 0UL) {
                editor_copy_bounded(g_completion.common,
                                    sizeof(g_completion.common), fd.name);
                g_completion.unique_attr = fd.attr;
            } else {
                completion_common_update(g_completion.common, fd.name);
            }
            ++g_completion.count;
        }
        rc = CmdO2FindNext(h, &fd);
        if (rc != 0)
            break;
    }
    (void)CmdO2FindClose(h);

    if (rc != 0 && rc != CMDO2_ERROR_NO_MORE_FILES)
        return 0;
    return g_completion.count != 0UL;
}

static int editor_replace_span(char *line, size_t cap, size_t *used,
                               size_t *cursor, size_t start, size_t end,
                               const char *replacement)
{
    size_t old_len;
    size_t new_len;
    size_t replacement_len;

    if (end < start || end > *used)
        return 0;
    old_len = end - start;
    replacement_len = strlen(replacement);
    new_len = *used - old_len + replacement_len;
    if (new_len + 1U > cap)
        return 0;

    editor_move(line + start + replacement_len, line + end,
                *used - end + 1U);
    if (replacement_len != 0U)
        memcpy(line + start, replacement, replacement_len);
    *used = new_len;
    *cursor = start + replacement_len;
    return 1;
}

static void completion_write_name(const struct CmdO2FindData *fd)
{
    static const char slash[] = "\\";
    static const char newline[] = "\r\n";
    size_t n;

    n = strlen(fd->name);
    if (n != 0U)
        (void)CmdO2VioWrtTTY(fd->name, (unsigned long)n);
    if (fd->attr & CMDO2_ATTR_DIRECTORY)
        (void)CmdO2VioWrtTTY(slash, 1UL);
    (void)CmdO2VioWrtTTY(newline, 2UL);
}

static void completion_list_matches(void)
{
    struct CmdO2FindData fd;
    CmdO2FindHandle h;
    CmdO2Rc rc;

    rc = CmdO2FindFirst(g_completion.pattern, &h, &fd);
    if (rc != 0)
        return;
    for (;;) {
        if (strcmp(fd.name, ".") != 0 && strcmp(fd.name, "..") != 0)
            completion_write_name(&fd);
        rc = CmdO2FindNext(h, &fd);
        if (rc != 0)
            break;
    }
    (void)CmdO2FindClose(h);
}

static int completion_build_replacement(void)
{
    size_t n;

    n = strlen(g_completion.dir_prefix) + strlen(g_completion.common);
    if (n + 2U > sizeof(g_completion.replacement))
        return 0;
    strcpy(g_completion.replacement, g_completion.dir_prefix);
    strcat(g_completion.replacement, g_completion.common);

    if (g_completion.count == 1UL &&
        (g_completion.unique_attr & CMDO2_ATTR_DIRECTORY) != 0UL) {
        n = strlen(g_completion.replacement);
        if (n == 0U || (g_completion.replacement[n - 1U] != '\\' &&
                        g_completion.replacement[n - 1U] != '/')) {
            g_completion.replacement[n++] = '\\';
            g_completion.replacement[n] = '\0';
        }
    }
    return 1;
}

/* Complete the token prefix under the cursor.  Ambiguous matches deliberately
 * list immediately on the first Tab (rather than requiring a second Tab), as
 * this is the requested CMD32 behavior. */
static void editor_complete_filename(char *line, size_t cap, size_t *used,
                                     size_t *cursor, unsigned short *row,
                                     unsigned short *col,
                                     unsigned short screen_cols,
                                     size_t *view_start)
{
    size_t start;
    int quoted;
    static const char newline[] = "\r\n";

    if (!completion_token_prefix(line, *cursor, &start, &quoted) ||
        !completion_prepare_pattern() || !completion_scan_matches()) {
        editor_bell();
        return;
    }
    (void)quoted; /* R1 preserves an existing quote but does not synthesize one. */

    if (!completion_build_replacement() ||
        !editor_replace_span(line, cap, used, cursor, start, *cursor,
                             g_completion.replacement)) {
        editor_bell();
        return;
    }

    if (g_completion.count == 1UL) {
        editor_render(line, *used, *cursor, *row, *col, screen_cols,
                      view_start);
        return;
    }

    /* Ambiguous: extend as far as possible, ring, list candidates, then print
     * a fresh prompt and restore the still-editable command line. */
    editor_bell();
    (void)CmdO2VioSetCurPos(*row, *col);
    if (screen_cols > *col + 1U)
        editor_write_spaces((size_t)(screen_cols - *col - 1U));
    (void)CmdO2VioSetCurPos(*row, *col);
    (void)CmdO2VioWrtTTY(newline, 2UL);
    completion_list_matches();
    print_prompt();
    if (CmdO2VioGetCurPos(row, col) != 0)
        return;
    *view_start = 0U;
    editor_render(line, *used, *cursor, *row, *col, screen_cols, view_start);
}

static int read_console_line(struct CmdHistory *history, char *line, size_t cap)
{
    struct CmdO2KbdKeyInfo key;
    CmdO2Rc rc;
    size_t used;
    size_t cursor;
    size_t view_start;
    unsigned short row;
    unsigned short col;
    unsigned short rows;
    unsigned short cols;
    int history_pos;
    static const char newline[] = "\r\n";

    if (line == NULL || cap < 2U)
        return 0;

    if (CmdO2VioGetCurPos(&row, &col) != 0 ||
        CmdO2VioGetScreenSize(&rows, &cols) != 0 || cols < 4U)
        return read_console_line_basic(line, cap);
    (void)rows;

    /* If a very long prompt consumes the row, move the editor to a fresh one. */
    if (col + 2U >= cols) {
        (void)CmdO2VioWrtTTY(newline, 2UL);
        if (CmdO2VioGetCurPos(&row, &col) != 0)
            return read_console_line_basic(line, cap);
    }

    used = 0U;
    cursor = 0U;
    view_start = 0U;
    history_pos = -1;
    line[0] = '\0';
    if (history != NULL)
        history->draft[0] = '\0';
    editor_render(line, used, cursor, row, col, cols, &view_start);

    for (;;) {
        unsigned char ch;
        unsigned char scan;

        rc = CmdO2KbdCharIn(&key, CMDO2_IO_WAIT);
        if (rc != 0)
            return 0;
        ch = key.chChar;
        scan = key.chScan;

        if (ch == 0x03U) {
            static const char break_seq[] = "^C\r\n";
            line[0] = '\0';
            (void)CmdO2VioSetCurPos(row, col);
            editor_write_spaces((size_t)(cols - col - 1U));
            (void)CmdO2VioSetCurPos(row, col);
            (void)CmdO2VioWrtTTY(break_seq,
                                 (unsigned long)(sizeof(break_seq) - 1U));
            return -1;
        }

        if (ch == 0U || ch == 0xe0U) {
            if (scan == CMD_SCAN_UP) {
                if (history != NULL && history->count != 0U) {
                    if (history_pos < 0) {
                        editor_copy_bounded(history->draft,
                                            sizeof(history->draft), line);
                        history_pos = (int)history->count - 1;
                    } else if (history_pos > 0) {
                        --history_pos;
                    }
                    editor_replace_line(line, cap, &used, &cursor,
                                        history_line(history,
                                                     (unsigned)history_pos));
                }
            } else if (scan == CMD_SCAN_DOWN) {
                if (history_pos >= 0) {
                    if ((unsigned)(history_pos + 1) < history->count) {
                        ++history_pos;
                        editor_replace_line(line, cap, &used, &cursor,
                                            history_line(history,
                                                         (unsigned)history_pos));
                    } else {
                        history_pos = -1;
                        editor_replace_line(line, cap, &used, &cursor,
                                            history->draft);
                    }
                }
            } else if (scan == CMD_SCAN_LEFT) {
                if (cursor != 0U)
                    --cursor;
            } else if (scan == CMD_SCAN_RIGHT) {
                if (cursor < used)
                    ++cursor;
            } else if (scan == CMD_SCAN_HOME) {
                cursor = 0U;
            } else if (scan == CMD_SCAN_END) {
                cursor = used;
            } else if (scan == CMD_SCAN_DELETE) {
                if (cursor < used) {
                    editor_move(line + cursor, line + cursor + 1U,
                                used - cursor);
                    --used;
                }
            }
            editor_render(line, used, cursor, row, col, cols, &view_start);
            continue;
        }

        if (ch == 0x1aU && used == 0U)
            return 0;
        if (ch == '\r' || ch == '\n') {
            line[used] = '\0';
            /* Finish visually at the end of the command even if the user
             * pressed Enter while editing in the middle of a recalled line. */
            cursor = used;
            editor_render(line, used, cursor, row, col, cols, &view_start);
            (void)CmdO2VioWrtTTY(newline, 2UL);
            return 1;
        }
        if (ch == '\b' || ch == 0x7fU) {
            if (cursor != 0U) {
                editor_move(line + cursor - 1U, line + cursor,
                            used - cursor + 1U);
                --cursor;
                --used;
            }
            editor_render(line, used, cursor, row, col, cols, &view_start);
            continue;
        }
        if (ch == '\t') {
            editor_complete_filename(line, cap, &used, &cursor, &row, &col,
                                     cols, &view_start);
            continue;
        }
        if (ch < 0x20U)
            continue;
        if (used + 1U >= cap)
            continue;

        editor_move(line + cursor + 1U, line + cursor,
                    used - cursor + 1U);
        line[cursor] = (char)ch;
        ++cursor;
        ++used;
        editor_render(line, used, cursor, row, col, cols, &view_start);
    }
}

static int run_startup_cmd(struct ShellState *s, int *want_exit)
{
    /* Optional per-shell initialization.  Keep this on the interactive
     * command-processor path only: pipeline workers also invoke CMD32OS2 with
     * -c and must not recursively rerun the user's startup file.  Ordinary
     * SET commands persist because run_batch_file() only unwinds SETLOCAL
     * frames when the batch invocation returns. */
    if (!path_is_file(CMD32_STARTUP_CMD))
        return 0;
    return run_batch_file(s, CMD32_STARTUP_CMD, "", want_exit);
}

static void print_prompt(void)
{
    char cwd[MAX_PATH];
    char prompt[MAX_PATH + 4];
    size_t n;
    if (CmdO2QueryCurrentDir(cwd, (unsigned long)sizeof(cwd)) == 0) {
        prompt[0] = '[';
        prompt[1] = '\0';
        strncat(prompt, cwd, sizeof(prompt) - 4U);
        n = strlen(prompt);
        if (n + 2U < sizeof(prompt)) {
            prompt[n++] = '>';
            prompt[n++] = ']';
            prompt[n] = '\0';
        }
    } else {
        strcpy(prompt, "[C:\\>]");
    }
    (void)CmdO2VioWrtTTY(prompt, (unsigned long)strlen(prompt));
}

int main(int argc, char **argv)
{
    struct ShellState shell;
    static struct CmdHistory history;
    char line[CMDLINE_MAX];
    int want_exit;
    int rc;

    /* Parser-only archaeology/debug mode does not require DOSCALLS.dll. */
    if (argc >= 3 && ci_cmp(argv[1], "--parse") == 0) {
        struct CmdParseResult parsed;
        size_t i;
        size_t used;
        line[0] = '\0';
        used = 0;
        for (i = 2; i < (size_t)argc; ++i) {
            size_t add;
            add = strlen(argv[i]) + (i == 2 ? 0 : 1);
            if (used + add + 1 >= sizeof(line)) {
                return 1;
            }
            if (i != 2) {
                strcat(line, " ");
                ++used;
            }
            strcat(line, argv[i]);
            used += strlen(argv[i]);
        }
        if (!CmdParser(line, &parsed)) {
            fprintf(stderr, "parse error at column %d: %s\n",
                    parsed.error_offset + 1, parsed.error);
            return 1;
        }
        CmdDumpTree(parsed.root, 0);
        CmdFreeTree(parsed.root);
        return 0;
    }

    memset(&history, 0, sizeof(history));

    if (!shell_init(&shell))
        return 2;

    /* Useful scripted regression: CMD32OS2 -c "args.exe one two three" */
    if (argc >= 3 && ci_cmp(argv[1], "-c") == 0) {
        size_t i;
        size_t used;
        line[0] = '\0';
        used = 0;
        for (i = 2; i < (size_t)argc; ++i) {
            size_t add;
            add = strlen(argv[i]) + (i == 2 ? 0 : 1);
            if (used + add + 1 >= sizeof(line)) {
                shell_done(&shell);
                return 1;
            }
            if (i != 2) {
                strcat(line, " ");
                ++used;
            }
            strcat(line, argv[i]);
            used += strlen(argv[i]);
        }
        want_exit = 0;
        rc = run_line(&shell, line, &want_exit);
        shell_done(&shell);
        return rc;
    }

    puts("OS/2 CMD32 personality bootstrap (M31G QoL TABCOMP R3)");
    puts("QoL: STARTUP + history + TAB completion + COPY + PS/KILL + PM auto-session routing.");
    puts("External .EXE execution is routed through DOSCALLS.283 -> OS2HOST32.");
    want_exit = 0;
    rc = run_startup_cmd(&shell, &want_exit);
    if (want_exit) {
        shell_done(&shell);
        return rc;
    }
    while (!want_exit) {
        int input_rc;
        print_prompt();
        input_rc = read_console_line(&history, line, sizeof(line));
        if (input_rc == 0)
            break;
        if (input_rc < 0)
            continue;
        history_add(&history, line);
        rc = run_line(&shell, line, &want_exit);
    }

    shell_done(&shell);
    return rc;
}
