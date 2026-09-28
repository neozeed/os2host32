/*
 * doscalls.c - exported OS/2 DOSCALLS ABI veneer.
 *
 * DOSCALLS R2 keeps historical names/ordinals/signatures here while moving
 * OS/2 Control Program semantics into common/doscalls/os2_doscalls.c and
 * Win32 mechanics into common/win32/os2_doscalls_win32.c.
 *
 * Keep this file free of Win32 HANDLE/DWORD/API dependencies.
 */
#include "os2_doscalls.h"
#include "os2_doscalls_win32.h"

#ifndef __cdecl
#define __cdecl
#endif

static struct Os2DosSession *dos_session(void)
{
    return os2_doscalls_win32_session();
}

/*
 * C/386/EMX selector bridge helpers are register-ABI tokens, not normal C
 * calls.  Keep EAX untouched exactly as in the proven pre-R2 implementation.
 */
#if defined(__GNUC__) && defined(__i386__)
void __attribute__((naked)) __cdecl DosFlatToSel(void)
{
    __asm__ __volatile__("ret");
}
#elif defined(_MSC_VER) && defined(_M_IX86)
__declspec(naked) void __cdecl DosFlatToSel(void)
{
    __asm ret
}
#else
void __cdecl DosFlatToSel(void)
{
}
#endif

#if defined(__GNUC__) && defined(__i386__)
void __attribute__((naked)) __cdecl DosSelToFlat(void)
{
    __asm__ __volatile__("ret");
}
#elif defined(_MSC_VER) && defined(_M_IX86)
__declspec(naked) void __cdecl DosSelToFlat(void)
{
    __asm ret
}
#else
void __cdecl DosSelToFlat(void)
{
}
#endif

unsigned short __cdecl DosSetSigHandler(void *routine, void *prev_address,
                                         unsigned short *prev_action,
                                         unsigned short action,
                                         unsigned short sig_number)
{
    return os2_dos_DosSetSigHandler(dos_session(), routine, prev_address, prev_action, action, sig_number);
}

unsigned short __cdecl DosSetVec(unsigned short vector, void *routine,
                                  O2ULONG *prev_address)
{
    return os2_dos_DosSetVec(dos_session(), vector, routine, prev_address);
}

unsigned short __cdecl DosFlagProcess(unsigned short process_id,
                                      unsigned short action_code,
                                      unsigned short flag_number,
                                      unsigned short flag_argument)
{
    return os2_dos_DosFlagProcess(dos_session(), process_id, action_code, flag_number, flag_argument);
}

O2APIRET __cdecl DosDevIOCtl(O2HFILE hDevice,
                             O2ULONG category,
                             O2ULONG function,
                             void *pParmList,
                             O2ULONG cbParmLengthMax,
                             O2ULONG *pcbParmLengthInOut,
                             void *pDataArea,
                             O2ULONG cbDataLengthMax,
                             O2ULONG *pcbDataLengthInOut)
{
    return os2_dos_DosDevIOCtl(dos_session(), hDevice, category, function, pParmList, cbParmLengthMax, pcbParmLengthInOut, pDataArea, cbDataLengthMax, pcbDataLengthInOut);
}

O2APIRET __cdecl DosBeep(O2ULONG frequency, O2ULONG duration)
{
    return os2_dos_DosBeep(dos_session(), frequency, duration);
}

O2APIRET __cdecl Dos16Beep(O2ULONG frequency, O2ULONG duration)
{
    return os2_dos_Dos16Beep(dos_session(), frequency, duration);
}

O2APIRET __cdecl DosSleep(O2ULONG milliseconds)
{
    return os2_dos_DosSleep(dos_session(), milliseconds);
}

O2APIRET __cdecl Dos16Sleep(O2ULONG milliseconds)
{
    return os2_dos_Dos16Sleep(dos_session(), milliseconds);
}

O2APIRET __cdecl DosFindClose(O2ULONG hdir)
{
    return os2_dos_DosFindClose(dos_session(), hdir);
}

O2APIRET __cdecl DosFindFirst(const char *filespec, O2ULONG *phdir,
                              O2ULONG attributes, void *findbuf,
                              O2ULONG cbBuf, O2ULONG *pCount,
                              O2ULONG infoLevel)
{
    return os2_dos_DosFindFirst(dos_session(), filespec, phdir, attributes, findbuf, cbBuf, pCount, infoLevel);
}

O2APIRET __cdecl DosFindNext(O2ULONG hdir, void *findbuf,
                             O2ULONG cbBuf, O2ULONG *pCount)
{
    return os2_dos_DosFindNext(dos_session(), hdir, findbuf, cbBuf, pCount);
}

O2APIRET __cdecl DosCreateThread(O2ULONG *ptid, O2THREADFN fn,
                                 O2ULONG param, O2ULONG flags,
                                 O2ULONG stackSize)
{
    return os2_dos_DosCreateThread(dos_session(), ptid, fn, param, flags, stackSize);
}

O2APIRET __cdecl DosCreateEventSem(const char *name, O2ULONG *phev,
                                   O2ULONG flags, O2ULONG initialState)
{
    return os2_dos_DosCreateEventSem(dos_session(), name, phev, flags, initialState);
}

O2APIRET __cdecl DosOpenEventSem(const char *name, O2ULONG *phev)
{
    return os2_dos_DosOpenEventSem(dos_session(), name, phev);
}

O2APIRET __cdecl DosCloseEventSem(O2ULONG hev)
{
    return os2_dos_DosCloseEventSem(dos_session(), hev);
}

O2APIRET __cdecl DosResetEventSem(O2ULONG hev, O2ULONG *postCount)
{
    return os2_dos_DosResetEventSem(dos_session(), hev, postCount);
}

O2APIRET __cdecl DosPostEventSem(O2ULONG hev)
{
    return os2_dos_DosPostEventSem(dos_session(), hev);
}

O2APIRET __cdecl DosWaitEventSem(O2ULONG hev, O2ULONG timeout)
{
    return os2_dos_DosWaitEventSem(dos_session(), hev, timeout);
}

O2APIRET __cdecl DosQueryEventSem(O2ULONG hev, O2ULONG *postCount)
{
    return os2_dos_DosQueryEventSem(dos_session(), hev, postCount);
}

O2APIRET __cdecl DosCreateMutexSem(const char *name, O2ULONG *phmtx,
                                   O2ULONG flags, O2ULONG initialOwner)
{
    return os2_dos_DosCreateMutexSem(dos_session(), name, phmtx, flags, initialOwner);
}

O2APIRET __cdecl DosOpenMutexSem(const char *name, O2ULONG *phmtx)
{
    return os2_dos_DosOpenMutexSem(dos_session(), name, phmtx);
}

O2APIRET __cdecl DosCloseMutexSem(O2ULONG hmtx)
{
    return os2_dos_DosCloseMutexSem(dos_session(), hmtx);
}

O2APIRET __cdecl DosRequestMutexSem(O2ULONG hmtx, O2ULONG timeout)
{
    return os2_dos_DosRequestMutexSem(dos_session(), hmtx, timeout);
}

O2APIRET __cdecl DosReleaseMutexSem(O2ULONG hmtx)
{
    return os2_dos_DosReleaseMutexSem(dos_session(), hmtx);
}

O2APIRET __cdecl DosGetDateTime(void *buffer)
{
    return os2_dos_DosGetDateTime(dos_session(), buffer);
}

O2APIRET __cdecl DosSetDefaultDisk(O2ULONG diskNum)
{
    return os2_dos_DosSetDefaultDisk(dos_session(), diskNum);
}

O2APIRET __cdecl DosScanEnv(const char *name, char **value)
{
    return os2_dos_DosScanEnv(dos_session(), name, value);
}

O2APIRET __cdecl DosSearchPath(O2ULONG flags, const char *pathOrName,
                               const char *filename, char *buffer,
                               O2ULONG cbBuffer)
{
    return os2_dos_DosSearchPath(dos_session(), flags, pathOrName, filename, buffer, cbBuffer);
}

O2APIRET __cdecl DosGetInfoBlocks(void **pptib, void **pppib)
{
    return os2_dos_DosGetInfoBlocks(dos_session(), pptib, pppib);
}

O2APIRET __cdecl DosError(O2ULONG flags)
{
    return os2_dos_DosError(dos_session(), flags);
}

O2APIRET __cdecl DosSetFileInfo(O2HFILE hFile, O2ULONG level,
                                const void *buffer, O2ULONG cb)
{
    return os2_dos_DosSetFileInfo(dos_session(), hFile, level, buffer, cb);
}

O2APIRET __cdecl DosSetPathInfo(const char *path, O2ULONG level,
                                const void *buffer, O2ULONG cb,
                                O2ULONG options)
{
    return os2_dos_DosSetPathInfo(dos_session(), path, level, buffer, cb, options);
}

O2APIRET __cdecl DosQAppType(const char *path, O2ULONG *appType)
{
    return os2_dos_DosQAppType(dos_session(), path, appType);
}

O2APIRET __cdecl DosQueryAppType(const char *path, O2ULONG *appType)
{
    return os2_dos_DosQueryAppType(dos_session(), path, appType);
}

O2APIRET __cdecl DosQueryPathInfo(const char *path, O2ULONG level,
                                  void *buffer, O2ULONG cb)
{
    return os2_dos_DosQueryPathInfo(dos_session(), path, level, buffer, cb);
}

O2APIRET __cdecl DosQueryHType(O2HFILE hFile, O2ULONG *pType, O2ULONG *pAttr)
{
    return os2_dos_DosQueryHType(dos_session(), hFile, pType, pAttr);
}

O2APIRET __cdecl DosExitList(O2ULONG orderCode, void (__cdecl *routine)(O2ULONG))
{
    return os2_dos_DosExitList(dos_session(), orderCode, routine);
}

O2APIRET __cdecl DosEnterCritSec(void)
{
    return os2_dos_DosEnterCritSec(dos_session());
}

void __cdecl DosExit(O2ULONG action, O2ULONG result)
{
    os2_dos_DosExit(dos_session(), action, result);
}

O2APIRET __cdecl DosDeleteDir(const char *path)
{
    return os2_dos_DosDeleteDir(dos_session(), path);
}

O2APIRET __cdecl DosSetCurrentDir(const char *path)
{
    return os2_dos_DosSetCurrentDir(dos_session(), path);
}

O2APIRET __cdecl DosSetFilePtr(O2HFILE hFile, O2LONG distance,
                              O2ULONG method, O2ULONG *newpos)
{
    return os2_dos_DosSetFilePtr(dos_session(), hFile, distance, method, newpos);
}

O2APIRET __cdecl DosClose(O2HFILE hFile)
{
    return os2_dos_DosClose(dos_session(), hFile);
}

O2APIRET __cdecl DosCreatePipe(O2HFILE *pread, O2HFILE *pwrite, O2ULONG size)
{
    return os2_dos_DosCreatePipe(dos_session(), pread, pwrite, size);
}

O2APIRET __cdecl DosDupHandle(O2HFILE oldFile, O2HFILE *pnewFile)
{
    return os2_dos_DosDupHandle(dos_session(), oldFile, pnewFile);
}

O2APIRET __cdecl DosDelete(const char *path, O2ULONG reserved)
{
    return os2_dos_DosDelete(dos_session(), path, reserved);
}

O2APIRET __cdecl DosCreateDir(const char *path, void *pEA, O2ULONG reserved)
{
    return os2_dos_DosCreateDir(dos_session(), path, pEA, reserved);
}

O2APIRET __cdecl DosMove(const char *oldPath, const char *newPath)
{
    return os2_dos_DosMove(dos_session(), oldPath, newPath);
}

O2APIRET __cdecl DosSetFileSize(O2HFILE hFile, O2ULONG size)
{
    return os2_dos_DosSetFileSize(dos_session(), hFile, size);
}

O2APIRET __cdecl DosOpen(const char *path, O2HFILE *phFile, O2ULONG *pAction,
                         O2ULONG cbFile, O2ULONG attr, O2ULONG openFlags,
                         O2ULONG openMode, void *pEA, O2ULONG reserved)
{
    return os2_dos_DosOpen(dos_session(), path, phFile, pAction, cbFile, attr, openFlags, openMode, pEA, reserved);
}

O2APIRET __cdecl DosQueryCurrentDir(O2ULONG diskNum, char *buffer,
                                    O2ULONG *pcb)
{
    return os2_dos_DosQueryCurrentDir(dos_session(), diskNum, buffer, pcb);
}

O2APIRET __cdecl DosQueryCurrentDisk(O2ULONG *pdisk, O2ULONG *plogical)
{
    return os2_dos_DosQueryCurrentDisk(dos_session(), pdisk, plogical);
}

O2APIRET __cdecl DosQueryFileInfo(O2HFILE hFile, O2ULONG level,
                                  void *buffer, O2ULONG cb)
{
    return os2_dos_DosQueryFileInfo(dos_session(), hFile, level, buffer, cb);
}

O2APIRET __cdecl DosRead(O2HFILE hFile, void *buffer,
                         O2ULONG count, O2ULONG *actual)
{
    return os2_dos_DosRead(dos_session(), hFile, buffer, count, actual);
}

O2APIRET __cdecl DosWrite(O2HFILE hFile, const void *buffer,
                          O2ULONG count, O2ULONG *actual)
{
    return os2_dos_DosWrite(dos_session(), hFile, buffer, count, actual);
}

O2APIRET __cdecl DosExecPgm(char *objectName, O2LONG objectNameLen,
                            O2ULONG execFlag, const char *args,
                            const char *env,
                            struct O2ResultCodes *results,
                            const char *program)
{
    return os2_dos_DosExecPgm(dos_session(), objectName, objectNameLen, execFlag, args, env, results, program);
}

O2APIRET __cdecl DosWaitChild(O2ULONG action, O2ULONG option,
                              struct O2ResultCodes *results,
                              O2ULONG *ppid, O2ULONG pid)
{
    return os2_dos_DosWaitChild(dos_session(), action, option, results, ppid, pid);
}

O2APIRET __cdecl DosSubSetMem(void *base, O2ULONG flags, O2ULONG size)
{
    return os2_dos_DosSubSetMem(dos_session(), base, flags, size);
}

O2APIRET __cdecl DosSubAllocMem(void *base, void **ppBlock, O2ULONG size)
{
    return os2_dos_DosSubAllocMem(dos_session(), base, ppBlock, size);
}

O2APIRET __cdecl DosSubFreeMem(void *base, void *block, O2ULONG size)
{
    return os2_dos_DosSubFreeMem(dos_session(), base, block, size);
}

O2APIRET __cdecl DosSubUnsetMem(void *base)
{
    return os2_dos_DosSubUnsetMem(dos_session(), base);
}

O2APIRET __cdecl DosAllocMem(void **ppBase, O2ULONG size,
                             O2ULONG flags, O2ULONG reserved)
{
    return os2_dos_DosAllocMem(dos_session(), ppBase, size, flags, reserved);
}

O2APIRET __cdecl DosFreeMem(void *base)
{
    return os2_dos_DosFreeMem(dos_session(), base);
}

O2APIRET __cdecl DosSetMem(void *base, O2ULONG size, O2ULONG flags)
{
    return os2_dos_DosSetMem(dos_session(), base, size, flags);
}

O2APIRET __cdecl DosQueryMem(void *base, O2ULONG *pcb, O2ULONG *pflags)
{
    return os2_dos_DosQueryMem(dos_session(), base, pcb, pflags);
}

O2APIRET __cdecl DosLoadModule(char *objectName, O2ULONG objectNameLen,
                               const char *moduleName, O2ULONG *moduleHandle)
{
    return os2_dos_DosLoadModule(dos_session(), objectName, objectNameLen, moduleName, moduleHandle);
}

O2APIRET __cdecl DosQueryModuleHandle(const char *moduleName,
                                      O2ULONG *moduleHandle)
{
    return os2_dos_DosQueryModuleHandle(dos_session(), moduleName, moduleHandle);
}

O2APIRET __cdecl DosQueryModuleName(O2ULONG moduleHandle, O2ULONG cb,
                                    char *buffer)
{
    return os2_dos_DosQueryModuleName(dos_session(), moduleHandle, cb, buffer);
}

O2APIRET __cdecl DosQueryProcAddr(O2ULONG moduleHandle, O2ULONG ordinal,
                                  const char *procName, void **procAddress)
{
    return os2_dos_DosQueryProcAddr(dos_session(), moduleHandle, ordinal, procName, procAddress);
}

O2APIRET __cdecl DosFreeModule(O2ULONG moduleHandle)
{
    return os2_dos_DosFreeModule(dos_session(), moduleHandle);
}

O2APIRET __cdecl DosSetExceptionHandler(struct O2ExceptionRegistrationRecord *rec)
{
    return os2_dos_DosSetExceptionHandler(dos_session(), rec);
}

O2APIRET __cdecl DosUnsetExceptionHandler(struct O2ExceptionRegistrationRecord *rec)
{
    return os2_dos_DosUnsetExceptionHandler(dos_session(), rec);
}

O2APIRET __cdecl DosSetSignalExceptionFocus(O2ULONG enable, O2ULONG *pulTimes)
{
    return os2_dos_DosSetSignalExceptionFocus(dos_session(), enable, pulTimes);
}

O2APIRET __cdecl DosSetRelMaxFH(O2LONG *pcbReqCount, O2ULONG *pcbCurMaxFH)
{
    return os2_dos_DosSetRelMaxFH(dos_session(), pcbReqCount, pcbCurMaxFH);
}

O2APIRET __cdecl DosAcknowledgeSignalException(O2ULONG signalNum)
{
    return os2_dos_DosAcknowledgeSignalException(dos_session(), signalNum);
}

O2APIRET __cdecl DosQuerySysInfo(O2ULONG first, O2ULONG last,
                                  void *buffer, O2ULONG cb)
{
    return os2_dos_DosQuerySysInfo(dos_session(), first, last, buffer, cb);
}

O2APIRET __cdecl DosSetProcessCp(O2ULONG codepage)
{
    return os2_dos_DosSetProcessCp(dos_session(), codepage);
}

O2APIRET __cdecl DosQueryCp(O2ULONG cb, O2ULONG *codepages, O2ULONG *actual)
{
    return os2_dos_DosQueryCp(dos_session(), cb, codepages, actual);
}

O2APIRET __cdecl O2NlsQueryCtryInfo(O2ULONG cb, const void *countrycode,
                                     void *countryinfo, O2ULONG *actual)
{
    return os2_dos_DosQueryCtryInfo(dos_session(), cb, countrycode, countryinfo, actual);
}

O2APIRET __cdecl O2NlsQueryDBCSEnv(O2ULONG cb, const void *countrycode,
                                    void *buffer)
{
    return os2_dos_DosQueryDBCSEnv(dos_session(), cb, countrycode, buffer);
}

O2APIRET __cdecl O2NlsMapCase(O2ULONG cb, const void *countrycode, void *buffer)
{
    return os2_dos_DosMapCase(dos_session(), cb, countrycode, buffer);
}

O2APIRET __cdecl DosQueryCtryInfo(O2ULONG cb, const void *countrycode,
                                   void *countryinfo, O2ULONG *actual)
{
    return os2_dos_DosQueryCtryInfo(dos_session(), cb, countrycode, countryinfo, actual);
}

O2APIRET __cdecl DosQueryDBCSEnv(O2ULONG cb, const void *countrycode,
                                  void *buffer)
{
    return os2_dos_DosQueryDBCSEnv(dos_session(), cb, countrycode, buffer);
}

O2APIRET __cdecl DosMapCase(O2ULONG cb, const void *countrycode, void *buffer)
{
    return os2_dos_DosMapCase(dos_session(), cb, countrycode, buffer);
}

O2APIRET __cdecl DosSetPriority(O2ULONG scope, O2ULONG prtyClass,
                                O2LONG delta, O2ULONG porTid)
{
    return os2_dos_DosSetPriority(dos_session(), scope, prtyClass, delta, porTid);
}
