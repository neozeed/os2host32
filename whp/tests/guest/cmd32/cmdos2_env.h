#ifndef CMDOS2_ENV_H
#define CMDOS2_ENV_H

/*
 * Internal shared environment store used by both cmdos2 backends.
 *
 * The command processor owns a mutable environment block.  This is important
 * on OS/2 because the kernel process environment exposed by DosScanEnv/
 * the OS/2/C runtime startup environment is the original process environment;
 * SET semantics belong
 * to CMD, and child inheritance is made explicit through DosExecPgm's env
 * argument.
 *
 * ANSI C89; deliberately avoids CRT string/memory helpers so the direct
 * C/386 backend remains independent of the currently mismatched CRT calling
 * convention decoration.
 */

#include "cmdos2.h"

#define CMDO2_ENV_MAX 32768UL

int CmdEnvStoreInit(const char *block);
void CmdEnvStoreDone(void);
const char *CmdEnvStoreBlock(void);
unsigned long CmdEnvStoreSize(void);
int CmdEnvStoreApplyOs2Path(void);

#endif
