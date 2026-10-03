#ifndef OS2_DOSCALLS_BACKEND_H
#define OS2_DOSCALLS_BACKEND_H

#include "os2_doscalls.h"

enum Os2DosCallId {
    OS2_DOS_CALL_DOSSETSIGHANDLER = 1,
    OS2_DOS_CALL_DOSFLAGPROCESS = 2,
    OS2_DOS_CALL_DOSDEVIOCTL = 3,
    OS2_DOS_CALL_DOSBEEP = 4,
    OS2_DOS_CALL_DOS16BEEP = 5,
    OS2_DOS_CALL_DOSSLEEP = 6,
    OS2_DOS_CALL_DOS16SLEEP = 7,
    OS2_DOS_CALL_DOSFINDCLOSE = 8,
    OS2_DOS_CALL_DOSFINDFIRST = 9,
    OS2_DOS_CALL_DOSFINDNEXT = 10,
    OS2_DOS_CALL_DOSCREATETHREAD = 11,
    OS2_DOS_CALL_DOSCREATEEVENTSEM = 12,
    OS2_DOS_CALL_DOSOPENEVENTSEM = 13,
    OS2_DOS_CALL_DOSCLOSEEVENTSEM = 14,
    OS2_DOS_CALL_DOSRESETEVENTSEM = 15,
    OS2_DOS_CALL_DOSPOSTEVENTSEM = 16,
    OS2_DOS_CALL_DOSWAITEVENTSEM = 17,
    OS2_DOS_CALL_DOSQUERYEVENTSEM = 18,
    OS2_DOS_CALL_DOSCREATEMUTEXSEM = 19,
    OS2_DOS_CALL_DOSOPENMUTEXSEM = 20,
    OS2_DOS_CALL_DOSCLOSEMUTEXSEM = 21,
    OS2_DOS_CALL_DOSREQUESTMUTEXSEM = 22,
    OS2_DOS_CALL_DOSRELEASEMUTEXSEM = 23,
    OS2_DOS_CALL_DOSGETDATETIME = 24,
    OS2_DOS_CALL_DOSSETDEFAULTDISK = 25,
    OS2_DOS_CALL_DOSSCANENV = 26,
    OS2_DOS_CALL_DOSSEARCHPATH = 27,
    OS2_DOS_CALL_DOSGETINFOBLOCKS = 28,
    OS2_DOS_CALL_DOSERROR = 29,
    OS2_DOS_CALL_DOSSETFILEINFO = 30,
    OS2_DOS_CALL_DOSSETPATHINFO = 31,
    OS2_DOS_CALL_DOSQAPPTYPE = 32,
    OS2_DOS_CALL_DOSQUERYAPPTYPE = 33,
    OS2_DOS_CALL_DOSQUERYPATHINFO = 34,
    OS2_DOS_CALL_DOSQUERYHTYPE = 35,
    OS2_DOS_CALL_DOSEXITLIST = 36,
    OS2_DOS_CALL_DOSENTERCRITSEC = 37,
    OS2_DOS_CALL_DOSEXIT = 38,
    OS2_DOS_CALL_DOSDELETEDIR = 39,
    OS2_DOS_CALL_DOSSETCURRENTDIR = 40,
    OS2_DOS_CALL_DOSSETFILEPTR = 41,
    OS2_DOS_CALL_DOSCLOSE = 42,
    OS2_DOS_CALL_DOSCREATEPIPE = 43,
    OS2_DOS_CALL_DOSDUPHANDLE = 44,
    OS2_DOS_CALL_DOSDELETE = 45,
    OS2_DOS_CALL_DOSCREATEDIR = 46,
    OS2_DOS_CALL_DOSMOVE = 47,
    OS2_DOS_CALL_DOSSETFILESIZE = 48,
    OS2_DOS_CALL_DOSOPEN = 49,
    OS2_DOS_CALL_DOSQUERYCURRENTDIR = 50,
    OS2_DOS_CALL_DOSQUERYCURRENTDISK = 51,
    OS2_DOS_CALL_DOSQUERYFILEINFO = 52,
    OS2_DOS_CALL_DOSREAD = 53,
    OS2_DOS_CALL_DOSWRITE = 54,
    OS2_DOS_CALL_DOSEXECPGM = 55,
    OS2_DOS_CALL_DOSWAITCHILD = 56,
    OS2_DOS_CALL_DOSSUBSETMEM = 57,
    OS2_DOS_CALL_DOSSUBALLOCMEM = 58,
    OS2_DOS_CALL_DOSSUBFREEMEM = 59,
    OS2_DOS_CALL_DOSSUBUNSETMEM = 60,
    OS2_DOS_CALL_DOSALLOCMEM = 61,
    OS2_DOS_CALL_DOSFREEMEM = 62,
    OS2_DOS_CALL_DOSSETMEM = 63,
    OS2_DOS_CALL_DOSQUERYMEM = 64,
    OS2_DOS_CALL_DOSLOADMODULE = 65,
    OS2_DOS_CALL_DOSQUERYMODULEHANDLE = 66,
    OS2_DOS_CALL_DOSQUERYMODULENAME = 67,
    OS2_DOS_CALL_DOSQUERYPROCADDR = 68,
    OS2_DOS_CALL_DOSFREEMODULE = 69,
    OS2_DOS_CALL_DOSSETEXCEPTIONHANDLER = 70,
    OS2_DOS_CALL_DOSUNSETEXCEPTIONHANDLER = 71,
    OS2_DOS_CALL_DOSSETSIGNALEXCEPTIONFOCUS = 72,
    OS2_DOS_CALL_DOSSETRELMAXFH = 73,
    OS2_DOS_CALL_DOSACKNOWLEDGESIGNALEXCEPTION = 74,
    OS2_DOS_CALL_DOSQUERYSYSINFO = 75,
    OS2_DOS_CALL_DOSSETPROCESSCP = 76,
    OS2_DOS_CALL_DOSQUERYCP = 77,
    OS2_DOS_CALL_DOSQUERYCTRYINFO = 78,
    OS2_DOS_CALL_DOSQUERYDBCSENV = 79,
    OS2_DOS_CALL_DOSMAPCASE = 80,
    OS2_DOS_CALL_DOSSETPRIORITY = 81,
    OS2_DOS_CALL_DOSGETRESOURCE = 82,
    OS2_DOS_CALL_DOSFREERESOURCE = 83,
    OS2_DOS_CALL_COUNT = 84
};

struct Os2DosArgs_DosSetSigHandler {
    void *routine;
    void *prev_address;
    unsigned short *prev_action;
    unsigned short action;
    unsigned short sig_number;
};

struct Os2DosArgs_DosFlagProcess {
    unsigned short process_id;
    unsigned short action_code;
    unsigned short flag_number;
    unsigned short flag_argument;
};

struct Os2DosArgs_DosDevIOCtl {
    O2HFILE hDevice;
    O2ULONG category;
    O2ULONG function;
    void *pParmList;
    O2ULONG cbParmLengthMax;
    O2ULONG *pcbParmLengthInOut;
    void *pDataArea;
    O2ULONG cbDataLengthMax;
    O2ULONG *pcbDataLengthInOut;
};

struct Os2DosArgs_DosBeep {
    O2ULONG frequency;
    O2ULONG duration;
};

struct Os2DosArgs_Dos16Beep {
    O2ULONG frequency;
    O2ULONG duration;
};

struct Os2DosArgs_DosSleep {
    O2ULONG milliseconds;
};

struct Os2DosArgs_Dos16Sleep {
    O2ULONG milliseconds;
};

struct Os2DosArgs_DosFindClose {
    O2ULONG hdir;
};

struct Os2DosArgs_DosFindFirst {
    const char *filespec;
    O2ULONG *phdir;
    O2ULONG attributes;
    void *findbuf;
    O2ULONG cbBuf;
    O2ULONG *pCount;
    O2ULONG infoLevel;
};

struct Os2DosArgs_DosFindNext {
    O2ULONG hdir;
    void *findbuf;
    O2ULONG cbBuf;
    O2ULONG *pCount;
};

struct Os2DosArgs_DosCreateThread {
    O2ULONG *ptid;
    O2THREADFN fn;
    O2ULONG param;
    O2ULONG flags;
    O2ULONG stackSize;
};

struct Os2DosArgs_DosCreateEventSem {
    const char *name;
    O2ULONG *phev;
    O2ULONG flags;
    O2ULONG initialState;
};

struct Os2DosArgs_DosOpenEventSem {
    const char *name;
    O2ULONG *phev;
};

struct Os2DosArgs_DosCloseEventSem {
    O2ULONG hev;
};

struct Os2DosArgs_DosResetEventSem {
    O2ULONG hev;
    O2ULONG *postCount;
};

struct Os2DosArgs_DosPostEventSem {
    O2ULONG hev;
};

struct Os2DosArgs_DosWaitEventSem {
    O2ULONG hev;
    O2ULONG timeout;
};

struct Os2DosArgs_DosQueryEventSem {
    O2ULONG hev;
    O2ULONG *postCount;
};

struct Os2DosArgs_DosCreateMutexSem {
    const char *name;
    O2ULONG *phmtx;
    O2ULONG flags;
    O2ULONG initialOwner;
};

struct Os2DosArgs_DosOpenMutexSem {
    const char *name;
    O2ULONG *phmtx;
};

struct Os2DosArgs_DosCloseMutexSem {
    O2ULONG hmtx;
};

struct Os2DosArgs_DosRequestMutexSem {
    O2ULONG hmtx;
    O2ULONG timeout;
};

struct Os2DosArgs_DosReleaseMutexSem {
    O2ULONG hmtx;
};

struct Os2DosArgs_DosGetDateTime {
    void *buffer;
};

struct Os2DosArgs_DosSetDefaultDisk {
    O2ULONG diskNum;
};

struct Os2DosArgs_DosScanEnv {
    const char *name;
    char **value;
};

struct Os2DosArgs_DosSearchPath {
    O2ULONG flags;
    const char *pathOrName;
    const char *filename;
    char *buffer;
    O2ULONG cbBuffer;
};

struct Os2DosArgs_DosGetInfoBlocks {
    void **pptib;
    void **pppib;
};

struct Os2DosArgs_DosError {
    O2ULONG flags;
};

struct Os2DosArgs_DosSetFileInfo {
    O2HFILE hFile;
    O2ULONG level;
    const void *buffer;
    O2ULONG cb;
};

struct Os2DosArgs_DosSetPathInfo {
    const char *path;
    O2ULONG level;
    const void *buffer;
    O2ULONG cb;
    O2ULONG options;
};

struct Os2DosArgs_DosQAppType {
    const char *path;
    O2ULONG *appType;
};

struct Os2DosArgs_DosQueryAppType {
    const char *path;
    O2ULONG *appType;
};

struct Os2DosArgs_DosQueryPathInfo {
    const char *path;
    O2ULONG level;
    void *buffer;
    O2ULONG cb;
};

struct Os2DosArgs_DosQueryHType {
    O2HFILE hFile;
    O2ULONG *pType;
    O2ULONG *pAttr;
};

struct Os2DosArgs_DosExitList {
    O2ULONG orderCode;
    void (__cdecl *routine)(O2ULONG);
};

struct Os2DosArgs_DosEnterCritSec {
    int unused;
};

struct Os2DosArgs_DosExit {
    O2ULONG action;
    O2ULONG result;
};

struct Os2DosArgs_DosDeleteDir {
    const char *path;
};

struct Os2DosArgs_DosSetCurrentDir {
    const char *path;
};

struct Os2DosArgs_DosSetFilePtr {
    O2HFILE hFile;
    O2LONG distance;
    O2ULONG method;
    O2ULONG *newpos;
};

struct Os2DosArgs_DosClose {
    O2HFILE hFile;
};

struct Os2DosArgs_DosCreatePipe {
    O2HFILE *pread;
    O2HFILE *pwrite;
    O2ULONG size;
};

struct Os2DosArgs_DosDupHandle {
    O2HFILE oldFile;
    O2HFILE *pnewFile;
};

struct Os2DosArgs_DosDelete {
    const char *path;
    O2ULONG reserved;
};

struct Os2DosArgs_DosCreateDir {
    const char *path;
    void *pEA;
    O2ULONG reserved;
};

struct Os2DosArgs_DosMove {
    const char *oldPath;
    const char *newPath;
};

struct Os2DosArgs_DosSetFileSize {
    O2HFILE hFile;
    O2ULONG size;
};

struct Os2DosArgs_DosOpen {
    const char *path;
    O2HFILE *phFile;
    O2ULONG *pAction;
    O2ULONG cbFile;
    O2ULONG attr;
    O2ULONG openFlags;
    O2ULONG openMode;
    void *pEA;
    O2ULONG reserved;
};

struct Os2DosArgs_DosQueryCurrentDir {
    O2ULONG diskNum;
    char *buffer;
    O2ULONG *pcb;
};

struct Os2DosArgs_DosQueryCurrentDisk {
    O2ULONG *pdisk;
    O2ULONG *plogical;
};

struct Os2DosArgs_DosQueryFileInfo {
    O2HFILE hFile;
    O2ULONG level;
    void *buffer;
    O2ULONG cb;
};

struct Os2DosArgs_DosRead {
    O2HFILE hFile;
    void *buffer;
    O2ULONG count;
    O2ULONG *actual;
};

struct Os2DosArgs_DosWrite {
    O2HFILE hFile;
    const void *buffer;
    O2ULONG count;
    O2ULONG *actual;
};

struct Os2DosArgs_DosExecPgm {
    char *objectName;
    O2LONG objectNameLen;
    O2ULONG execFlag;
    const char *args;
    const char *env;
    struct O2ResultCodes *results;
    const char *program;
};

struct Os2DosArgs_DosWaitChild {
    O2ULONG action;
    O2ULONG option;
    struct O2ResultCodes *results;
    O2ULONG *ppid;
    O2ULONG pid;
};

struct Os2DosArgs_DosSubSetMem {
    void *base;
    O2ULONG flags;
    O2ULONG size;
};

struct Os2DosArgs_DosSubAllocMem {
    void *base;
    void **ppBlock;
    O2ULONG size;
};

struct Os2DosArgs_DosSubFreeMem {
    void *base;
    void *block;
    O2ULONG size;
};

struct Os2DosArgs_DosSubUnsetMem {
    void *base;
};

struct Os2DosArgs_DosAllocMem {
    void **ppBase;
    O2ULONG size;
    O2ULONG flags;
    O2ULONG reserved;
};

struct Os2DosArgs_DosFreeMem {
    void *base;
};

struct Os2DosArgs_DosSetMem {
    void *base;
    O2ULONG size;
    O2ULONG flags;
};

struct Os2DosArgs_DosQueryMem {
    void *base;
    O2ULONG *pcb;
    O2ULONG *pflags;
};

struct Os2DosArgs_DosGetResource {
    O2ULONG module, type, id;
    void **buffer;
};
struct Os2DosArgs_DosFreeResource { void *buffer; };

struct Os2DosArgs_DosLoadModule {
    char *objectName;
    O2ULONG objectNameLen;
    const char *moduleName;
    O2ULONG *moduleHandle;
};

struct Os2DosArgs_DosQueryModuleHandle {
    const char *moduleName;
    O2ULONG *moduleHandle;
};

struct Os2DosArgs_DosQueryModuleName {
    O2ULONG moduleHandle;
    O2ULONG cb;
    char *buffer;
};

struct Os2DosArgs_DosQueryProcAddr {
    O2ULONG moduleHandle;
    O2ULONG ordinal;
    const char *procName;
    void **procAddress;
};

struct Os2DosArgs_DosFreeModule {
    O2ULONG moduleHandle;
};

struct Os2DosArgs_DosSetExceptionHandler {
    struct O2ExceptionRegistrationRecord *rec;
};

struct Os2DosArgs_DosUnsetExceptionHandler {
    struct O2ExceptionRegistrationRecord *rec;
};

struct Os2DosArgs_DosSetSignalExceptionFocus {
    O2ULONG enable;
    O2ULONG *pulTimes;
};

struct Os2DosArgs_DosSetRelMaxFH {
    O2LONG *pcbReqCount;
    O2ULONG *pcbCurMaxFH;
};

struct Os2DosArgs_DosAcknowledgeSignalException {
    O2ULONG signalNum;
};

struct Os2DosArgs_DosQuerySysInfo {
    O2ULONG first;
    O2ULONG last;
    void *buffer;
    O2ULONG cb;
};

struct Os2DosArgs_DosSetProcessCp {
    O2ULONG codepage;
};

struct Os2DosArgs_DosQueryCp {
    O2ULONG cb;
    O2ULONG *codepages;
    O2ULONG *actual;
};

struct Os2DosArgs_DosQueryCtryInfo {
    O2ULONG cb;
    const void *countrycode;
    void *countryinfo;
    O2ULONG *actual;
};

struct Os2DosArgs_DosQueryDBCSEnv {
    O2ULONG cb;
    const void *countrycode;
    void *buffer;
};

struct Os2DosArgs_DosMapCase {
    O2ULONG cb;
    const void *countrycode;
    void *buffer;
};

struct Os2DosArgs_DosSetPriority {
    O2ULONG scope;
    O2ULONG prtyClass;
    O2LONG delta;
    O2ULONG porTid;
};

struct Os2DosBackendOps {
    void (*state_lock)(void *opaque);
    void (*state_unlock)(void *opaque);
    O2APIRET (*dispatch)(void *opaque, unsigned int call_id, void *call_args);
    O2APIRET (*standard_handle)(void *opaque, O2HFILE hfile, O2NATIVE *native_handle);
    O2APIRET (*close_native)(void *opaque, O2NATIVE native_handle);
    O2APIRET (*close_find)(void *opaque, O2NATIVE native_handle);
    O2APIRET (*sleep_ms)(void *opaque, O2ULONG milliseconds);
    O2APIRET (*validate_native_range)(void *opaque, const void *base, O2ULONG size);
    O2APIRET (*event_create)(void *opaque, int initial_state, O2NATIVE *native_event);
    O2APIRET (*event_reset)(void *opaque, O2NATIVE native_event);
    O2APIRET (*event_post)(void *opaque, O2NATIVE native_event);
    O2APIRET (*event_wait)(void *opaque, O2NATIVE native_event, O2ULONG timeout);
    O2APIRET (*event_close)(void *opaque, O2NATIVE native_event);
    void (*exception_head_changed)(void *opaque, struct O2ExceptionRegistrationRecord *head);
};

#endif
