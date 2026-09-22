/* C/386 Beta2-compatible test of the later OS/2 GA information-block ABI.
 * Beta2 bsetib.h defines a DIFFERENT TIB. Never use its PTIB for ordinal 312.
 * .DEF binds our private cdecl name to DOSCALLS.312 (caller removes arguments,
 * just as this compiler's _syscall does). All other APIs use SDK declarations.
 */
#define INCL_DOSPROCESS
#define INCL_DOSSEMAPHORES
#include <os2.h>
#include <stdio.h>
#include <string.h>

struct InfoTib2 {
    ULONG tid, priority, version;
    USHORT must_complete, force;
};
struct InfoTib {
    PVOID exception;
    PVOID low, high;
    struct InfoTib2 *tib2;
    ULONG version, ordinal;
};
struct InfoPib {
    ULONG pid, ppid, module;
    char *cmd, *env;
    ULONG status, type;
};
extern APIRET _cdecl InfoBlocks(struct InfoTib **, struct InfoPib **);

/* Tiny leaf probes avoid an assembler dependency and C/386 inline-asm dialect
 * differences. The current WHP runtime maps guest data executable.
 * cdecl ULONG probe(ULONG offset): reload FS from its selector, then read
 * FS:[offset]. Only caller-saved EAX/ECX are touched. Bytes disassembled in
 * validation/fs-probe.txt. This fixture targets this WHP runtime.
 */
static unsigned char fs_probe_code[] = {
    0x8c,0xe0,             /* mov eax,fs (selector in AX) */
    0x8e,0xe0,             /* mov fs,ax */
    0x8b,0x4c,0x24,0x04,   /* mov ecx,[esp+4] */
    0x64,0x8b,0x01,        /* mov eax,fs:[ecx] */
    0xc3                  /* ret */
};
static ULONG fs_read(ULONG offset)
{
    union { unsigned char *bytes; ULONG (_cdecl *call)(ULONG); } probe;
    probe.bytes = fs_probe_code;
    return probe.call(offset);
}

#define MAX_INFO_WORKERS 30
#define INFO_STACK 16384UL
#define WAIT_MS 10000UL
static TID tids[MAX_INFO_WORKERS];
static HEV gates[MAX_INFO_WORKERS], ack;
static struct InfoTib *seen[MAX_INFO_WORKERS];
static volatile ULONG completed[MAX_INFO_WORKERS];
static struct InfoTib *main_tib;
static struct InfoPib *shared_pib;
static ULONG rounds;
static ULONG main_chain[2];

static void fail(ULONG line)
{
    printf("%s FAIL line %lu\n", INFO_TEST_NAME, line);
    DosExit(EXIT_PROCESS, 1);
}
#define CHECK(x) do { if (!(x)) fail((ULONG)__LINE__); } while (0)

static void check_info(struct InfoTib *expected, ULONG tid, PVOID marker)
{
    struct InfoTib *t;
    struct InfoPib *p;
    ULONG local;
    CHECK(InfoBlocks(&t, &p) == 0);
    CHECK(t == expected && p == shared_pib);
    CHECK(t->tib2 != NULL && t->tib2->tid == tid);
    CHECK((ULONG)t->low <= (ULONG)&local && (ULONG)&local < (ULONG)t->high);
    CHECK(t->exception == marker);
    CHECK(fs_read(0) == (ULONG)marker);
    CHECK(fs_read(4) == (ULONG)t->low);
    CHECK(fs_read(8) == (ULONG)t->high);
    CHECK(fs_read(12) == (ULONG)t->tib2);
    CHECK(t->version == 20 && t->tib2->version == 20);
    CHECK(t->tib2->priority == 0x200UL);
    CHECK(t->tib2->must_complete == 0 && t->tib2->force == 0);
    CHECK(p->pid != 0 && p->module == 1);
    CHECK(p->cmd != NULL && p->cmd[0] != 0 && p->env != NULL);
}

static void info_worker(ULONG index)
{
    struct InfoTib *t;
    struct InfoPib *p;
    ULONG r, count, chain[2];
    CHECK(InfoBlocks(&t, &p) == 0);
    CHECK(t != main_tib && p == shared_pib);
    CHECK(t->exception == (PVOID)0xffffffffUL);
    CHECK((ULONG)t->high - (ULONG)t->low == INFO_STACK);
    chain[0] = 0xffffffffUL; chain[1] = 0;
    t->exception = chain;
    seen[index] = t;
    for (r = 0; r < rounds; ++r) {
        check_info(t, tids[index], chain);
        CHECK(DosWaitEventSem(gates[index], WAIT_MS) == 0);
        CHECK(DosResetEventSem(gates[index], &count) == 0 && count == 1);
        check_info(t, tids[index], chain);
        completed[index] = r + 1;
        CHECK(DosPostEventSem(ack) == 0);
    }
    t->exception = (PVOID)0xffffffffUL;
    if ((index & 1UL) == 0) DosExit(EXIT_THREAD, 0);
    /* Odd workers exercise the synthetic thread-return stub. */
}

static ULONG parse_count(char *text, ULONG max)
{
    ULONG n;
    unsigned digit;
    CHECK(*text != 0);
    n = 0;
    while (*text) {
        CHECK(*text >= '0' && *text <= '9');
        digit = (unsigned)(*text++ - '0');
        CHECK(n <= (max - digit) / 10UL);
        n = n * 10UL + digit;
    }
    CHECK(n >= 1 && n <= max);
    return n;
}

int main(int argc, char **argv)
{
    ULONG n, i, j, r, count;
    TID tid;
    struct InfoTib *t;
    struct InfoPib *p;
    char *tail;
    n = INFO_DEFAULT_WORKERS;
    rounds = INFO_DEFAULT_ROUNDS;
    CHECK(argc <= 3);
    if (argc > 1) n = parse_count(argv[1], MAX_INFO_WORKERS);
    if (argc > 2) rounds = parse_count(argv[2], 1000000UL);
    CHECK(sizeof(struct InfoTib) == 24 && sizeof(struct InfoTib2) == 16);
    CHECK(sizeof(struct InfoPib) == 28);
    CHECK(InfoBlocks(&main_tib, &shared_pib) == 0);
    CHECK(main_tib->tib2->tid == 1);
    CHECK(main_tib->exception == (PVOID)0xffffffffUL);
    CHECK(InfoBlocks(NULL, NULL) == 0);
    t = NULL; p = NULL;
    CHECK(InfoBlocks(&t, NULL) == 0 && t == main_tib);
    CHECK(InfoBlocks(NULL, &p) == 0 && p == shared_pib);
    /* R7 returns 87 without partial output for out-of-RAM pointers. */
    t = (struct InfoTib *)0x12345678UL;
    CHECK(InfoBlocks(&t, (struct InfoPib **)0xfffffffeUL) == 87);
    CHECK(t == (struct InfoTib *)0x12345678UL);
    p = (struct InfoPib *)0x12345678UL;
    CHECK(InfoBlocks((struct InfoTib **)0xfffffffeUL, &p) == 87);
    CHECK(p == (struct InfoPib *)0x12345678UL);
    tail = shared_pib->cmd + strlen(shared_pib->cmd) + 1;
    if (argc > 1) CHECK(strstr(tail, argv[1]) != NULL);
    if (argc > 2) CHECK(strstr(tail, argv[2]) != NULL);
    main_chain[0] = 0xffffffffUL; main_chain[1] = 0;
    main_tib->exception = main_chain;
    check_info(main_tib, 1, main_chain);
    CHECK(DosCreateEventSem(NULL, &ack, 0, FALSE) == 0);
    for (i = 0; i < n; ++i) {
        CHECK(DosCreateEventSem(NULL, &gates[i], 0, FALSE) == 0);
        CHECK(DosCreateThread(&tids[i], (PFNTHREAD)info_worker, i, 0, INFO_STACK) == 0);
    }
    for (r = 0; r < rounds; ++r) {
        for (i = 0; i < n; ++i) {
            CHECK(DosPostEventSem(gates[i]) == 0);
            CHECK(DosWaitEventSem(ack, WAIT_MS) == 0);
            CHECK(DosResetEventSem(ack, &count) == 0 && count == 1);
            check_info(main_tib, 1, main_chain);
            CHECK(completed[i] == r + 1 && seen[i] != NULL);
            for (j = 0; j < i; ++j) {
                CHECK(seen[i] != seen[j] && tids[i] != tids[j]);
                CHECK(seen[i]->tib2 != seen[j]->tib2);
                CHECK((ULONG)seen[i]->high <= (ULONG)seen[j]->low ||
                      (ULONG)seen[j]->high <= (ULONG)seen[i]->low);
            }
        }
    }
    for (i = 0; i < n; ++i) {
        tid = tids[i];
        CHECK(DosWaitThread(&tid, DCWW_WAIT) == 0);
        CHECK(completed[i] == rounds);
        CHECK(DosCloseEventSem(gates[i]) == 0);
    }
    CHECK(DosCloseEventSem(ack) == 0);
    check_info(main_tib, 1, main_chain);
    main_tib->exception = (PVOID)0xffffffffUL;
    printf("%s PASS (%lu workers, %lu rounds)\n", INFO_TEST_NAME, n, rounds);
    return 0;
}
