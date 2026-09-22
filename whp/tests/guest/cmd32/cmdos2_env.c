/*
 * cmdos2_env.c - portable CMD-owned OS/2 environment block.
 *
 * OS/2's process environment is not a convenient mutable shell data store.
 * In particular, historical CRT putenv()/getenv() state and the environment
 * seen by DosScanEnv/DosExecPgm can diverge.  CMD therefore keeps the block it
 * will pass to children explicitly.  Both the native bootstrap and direct LX
 * backend seed this store from their process' initial environment.
 */

#include "cmdos2_env.h"

static char env_block[CMDO2_ENV_MAX];
static char env_scratch[CMDO2_ENV_MAX];
static char env_path_seed[CMDO2_ENV_MAX];
static unsigned long env_bytes;
static int env_ready;

static unsigned long c_len(const char *s)
{
    const char *p;
    p = s;
    while (*p != '\0')
        ++p;
    return (unsigned long)(p - s);
}

static void c_copy(void *vd, const void *vs, unsigned long n)
{
    unsigned char *d;
    const unsigned char *s;
    d = (unsigned char *)vd;
    s = (const unsigned char *)vs;
    while (n-- != 0UL)
        *d++ = *s++;
}

static unsigned char c_upper(unsigned char c)
{
    if (c >= (unsigned char)'a' && c <= (unsigned char)'z')
        return (unsigned char)(c - (unsigned char)'a' + (unsigned char)'A');
    return c;
}

static int name_eq(const char *entry, const char *name)
{
    const char *a;
    const char *b;
    a = entry;
    b = name;
    while (*b != '\0' && *a != '\0' && *a != '=') {
        if (c_upper((unsigned char)*a) != c_upper((unsigned char)*b))
            return 0;
        ++a;
        ++b;
    }
    return *b == '\0' && *a == '=';
}

static int valid_name(const char *name)
{
    const char *p;
    if (name == 0 || *name == '\0' || *name == '=')
        return 0;
    p = name;
    while (*p != '\0') {
        if (*p == '=')
            return 0;
        ++p;
    }
    return 1;
}

static unsigned long block_size(const char *block)
{
    const char *p;
    const char *start;

    if (block == 0 || *block == '\0')
        return 2UL;
    start = block;
    p = block;
    while (*p != '\0') {
        p += c_len(p) + 1UL;
        if ((unsigned long)(p - start) >= CMDO2_ENV_MAX)
            return 0UL;
    }
    ++p; /* second NUL */
    return (unsigned long)(p - start);
}

int CmdEnvStoreInit(const char *block)
{
    unsigned long n;

    n = block_size(block);
    if (n == 0UL || n > CMDO2_ENV_MAX)
        return 0;
    if (block == 0) {
        env_block[0] = '\0';
        env_block[1] = '\0';
        env_bytes = 2UL;
    } else {
        c_copy(env_block, block, n);
        env_bytes = n;
    }
    env_ready = 1;
    return 1;
}

void CmdEnvStoreDone(void)
{
    env_block[0] = '\0';
    env_block[1] = '\0';
    env_bytes = 2UL;
    env_ready = 0;
}

/*
 * M29N1 OS/2 personality namespace.
 *
 * The native bootstrap necessarily begins life with a Win32 environment, but
 * the command processor must not inherit Windows' often enormous PATH as its
 * guest command namespace.  OS2PATH is the host-facing seed for the OS/2
 * personality.  Inside CMD and in environments passed to OS/2 children it is
 * mirrored to the historical PATH variable.
 *
 * If the host did not provide OS2PATH, start with the deliberately small
 * current-directory-only path.  env.cmd can then establish a more complete
 * OS/2 tree.
 */
int CmdEnvStoreApplyOs2Path(void)
{
    CmdO2Rc rc;

    if (!env_ready)
        (void)CmdEnvStoreInit((const char *)0);

    rc = CmdO2EnvGet("OS2PATH", env_path_seed,
                     (unsigned long)sizeof(env_path_seed));
    if (rc != CMDO2_NO_ERROR || env_path_seed[0] == '\0') {
        env_path_seed[0] = '.';
        env_path_seed[1] = '\0';
        if (CmdO2EnvSet("OS2PATH", env_path_seed) != CMDO2_NO_ERROR)
            return 0;
    }
    if (CmdO2EnvSet("PATH", env_path_seed) != CMDO2_NO_ERROR)
        return 0;
    return 1;
}

const char *CmdEnvStoreBlock(void)
{
    if (!env_ready)
        (void)CmdEnvStoreInit((const char *)0);
    return env_block;
}

unsigned long CmdEnvStoreSize(void)
{
    if (!env_ready)
        (void)CmdEnvStoreInit((const char *)0);
    return env_bytes;
}

unsigned long CmdO2EnvSize(void)
{
    return CmdEnvStoreSize();
}

CmdO2Rc CmdO2EnvGet(const char *name, char *buffer, unsigned long cap)
{
    const char *p;
    const char *v;
    unsigned long n;

    if (!valid_name(name) || buffer == 0 || cap == 0UL)
        return CMDO2_ERROR_INVALID_PARAMETER;
    if (!env_ready)
        (void)CmdEnvStoreInit((const char *)0);

    p = env_block;
    while (*p != '\0') {
        if (*p != '=' && name_eq(p, name)) {
            v = p;
            while (*v != '\0' && *v != '=')
                ++v;
            if (*v == '=')
                ++v;
            n = c_len(v) + 1UL;
            if (n > cap)
                return CMDO2_ERROR_BUFFER_OVERFLOW;
            c_copy(buffer, v, n);
            return CMDO2_NO_ERROR;
        }
        p += c_len(p) + 1UL;
    }
    buffer[0] = '\0';
    return CMDO2_ERROR_ENVVAR_NOT_FOUND;
}

CmdO2Rc CmdO2EnvSet(const char *name, const char *value)
{
    const char *p;
    unsigned long out;
    unsigned long l;
    unsigned long nn;
    unsigned long vn;
    int deleting;

    if (!valid_name(name))
        return CMDO2_ERROR_INVALID_PARAMETER;
    if (!env_ready)
        (void)CmdEnvStoreInit((const char *)0);

    deleting = (value == 0 || *value == '\0');
    out = 0UL;
    p = env_block;

    while (*p != '\0') {
        l = c_len(p) + 1UL;
        if (!name_eq(p, name)) {
            if (out + l + 1UL > CMDO2_ENV_MAX)
                return CMDO2_ERROR_NOT_ENOUGH_MEMORY;
            c_copy(env_scratch + out, p, l);
            out += l;
        }
        p += l;
    }

    if (!deleting) {
        nn = c_len(name);
        vn = c_len(value);
        if (out + nn + 1UL + vn + 1UL + 1UL > CMDO2_ENV_MAX)
            return CMDO2_ERROR_NOT_ENOUGH_MEMORY;
        c_copy(env_scratch + out, name, nn);
        out += nn;
        env_scratch[out++] = '=';
        c_copy(env_scratch + out, value, vn);
        out += vn;
        env_scratch[out++] = '\0';
    }

    env_scratch[out++] = '\0';
    if (out == 1UL)
        env_scratch[out++] = '\0';
    c_copy(env_block, env_scratch, out);
    env_bytes = out;
    return CMDO2_NO_ERROR;
}

CmdO2Rc CmdO2EnvExport(char *buffer, unsigned long cap, unsigned long *actual)
{
    if (!env_ready)
        (void)CmdEnvStoreInit((const char *)0);
    if (actual != 0)
        *actual = env_bytes;
    if (buffer == 0 || cap < env_bytes)
        return CMDO2_ERROR_BUFFER_OVERFLOW;
    c_copy(buffer, env_block, env_bytes);
    return CMDO2_NO_ERROR;
}

CmdO2Rc CmdO2EnvImport(const char *block)
{
    unsigned long n;

    if (block == 0)
        return CMDO2_ERROR_INVALID_PARAMETER;
    n = block_size(block);
    if (n == 0UL || n > CMDO2_ENV_MAX)
        return CMDO2_ERROR_INVALID_PARAMETER;
    c_copy(env_block, block, n);
    env_bytes = n;
    env_ready = 1;
    return CMDO2_NO_ERROR;
}
