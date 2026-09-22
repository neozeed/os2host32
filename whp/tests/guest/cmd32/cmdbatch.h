#ifndef CMDBATCH_H
#define CMDBATCH_H

#include <stddef.h>

/*
 * cmdbatch.h - M24 semantic reconstruction of the Dec-1991 NT CMD cbatch.c
 * boundary.  The names and responsibilities are derived from the retained
 * COFF symbols in CMD.EXE; the implementation is clean-room C89.
 */

#define CMDBATCH_LINE_MAX 4096
#define CMDBATCH_MAX_ARGS 64

struct CmdBatchFrame;

typedef int (*CmdBatchRunLineFn)(void *ctx, char *line, int *want_exit);
typedef int (*CmdBatchExistsFn)(void *ctx, const char *path);

struct CmdBatchState {
    struct CmdBatchFrame *current;
    void *host_ctx;
    CmdBatchRunLineFn run_line;
    CmdBatchExistsFn path_exists;
};

void CmdBatchInit(struct CmdBatchState *state,
                  void *host_ctx,
                  CmdBatchRunLineFn run_line,
                  CmdBatchExistsFn path_exists);
void CmdBatchDone(struct CmdBatchState *state);
int CmdBatchActive(const struct CmdBatchState *state);
const char *CmdBatchCurrentFile(const struct CmdBatchState *state);
/* Replace/chaining semantics: a batch file invoked without CALL ends the
 * current batch frame before control transfers to the new file. */
void CmdBatchEndCurrent(struct CmdBatchState *state);

/* BatProc/BatLoop/SetBat-shaped file execution boundary. */
int CmdBatchRunFile(struct CmdBatchState *state,
                    const char *path,
                    const char *arg_tail,
                    int *want_exit);

/* Batch positional expansion (%0..%9, %*, and %% -> %). */
int CmdBatchExpandLine(struct CmdBatchState *state,
                       const char *src,
                       char *dst,
                       size_t cap);

/* Recovered cbatch.c e* command boundaries. */
int CmdBatchGoto(struct CmdBatchState *state, const char *tail);
int CmdBatchShift(struct CmdBatchState *state);
int CmdBatchCallLabel(struct CmdBatchState *state,
                      const char *tail,
                      int *want_exit);
int CmdBatchIf(struct CmdBatchState *state,
               const char *tail,
               int last_rc,
               int *want_exit);
int CmdBatchFor(struct CmdBatchState *state,
                const char *tail,
                int *want_exit);

#endif
