# DOSCALLS R2 API Routing Matrix

The export table is frozen. “Typed Win32 backend dispatch” means the public ABI still enters `os2_dos_*` first, but API-specific host mechanics have not yet been decomposed into smaller primitive backend operations.

| Export | Ordinal | R2 route |
|---|---:|---|
| `DosQAppType` | 163 | typed Win32 backend dispatch |
| `DosQueryAppType` | 323 | common semantics/state |
| `DosError` | 212 | common semantics/state |
| `DosSetFileInfo` | 218 | typed Win32 backend dispatch; OS/2 visible handle/process state is common |
| `DosSetSigHandler` | 14 | common semantics/state |
| `DosFlagProcess` | 15 | common semantics/state |
| `DosSetPathInfo` | 219 | typed Win32 backend dispatch |
| `DosSetDefaultDisk` | 220 | typed Win32 backend dispatch |
| `DosSetPriority` | 236 | typed Win32 backend dispatch |
| `DosCreatePipe` | 239 | typed Win32 backend dispatch; OS/2 visible handle/process state is common |
| `Dos16Sleep` | 32 | common semantics/state |
| `Dos16Beep` | 50 | common semantics/state |
| `DosQueryPathInfo` | 223 | typed Win32 backend dispatch |
| `DosQueryHType` | 224 | typed Win32 backend dispatch; shared os2_doscalls_core preserved in backend; OS/2 visible handle/process state is common |
| `DosDeleteDir` | 226 | typed Win32 backend dispatch |
| `DosScanEnv` | 227 | typed Win32 backend dispatch |
| `DosSearchPath` | 228 | typed Win32 backend dispatch |
| `DosSleep` | 229 | common semantics/state |
| `DosGetDateTime` | 230 | typed Win32 backend dispatch; shared os2_doscalls_core preserved in backend |
| `DosEnterCritSec` | 232 | common semantics/state |
| `DosExit` | 234 | typed Win32 backend dispatch |
| `DosSetCurrentDir` | 255 | typed Win32 backend dispatch |
| `DosSetFilePtr` | 256 | typed Win32 backend dispatch; shared os2_doscalls_core preserved in backend; OS/2 visible handle/process state is common |
| `DosClose` | 257 | typed Win32 backend dispatch; OS/2 visible handle/process state is common |
| `DosDelete` | 259 | typed Win32 backend dispatch |
| `DosDupHandle` | 260 | typed Win32 backend dispatch; OS/2 visible handle/process state is common |
| `DosFindClose` | 263 | typed Win32 backend dispatch; OS/2 visible handle/process state is common |
| `DosFindFirst` | 264 | typed Win32 backend dispatch; OS/2 visible handle/process state is common |
| `DosFindNext` | 265 | typed Win32 backend dispatch; OS/2 visible handle/process state is common |
| `DosCreateDir` | 270 | typed Win32 backend dispatch |
| `DosMove` | 271 | typed Win32 backend dispatch |
| `DosSetFileSize` | 272 | typed Win32 backend dispatch; OS/2 visible handle/process state is common |
| `DosOpen` | 273 | typed Win32 backend dispatch; OS/2 visible handle/process state is common |
| `DosQueryCurrentDir` | 274 | typed Win32 backend dispatch |
| `DosQueryCurrentDisk` | 275 | typed Win32 backend dispatch |
| `DosQueryFileInfo` | 279 | typed Win32 backend dispatch; OS/2 visible handle/process state is common |
| `DosWaitChild` | 280 | typed Win32 backend dispatch; OS/2 visible handle/process state is common |
| `DosRead` | 281 | typed Win32 backend dispatch; OS/2 visible handle/process state is common |
| `DosWrite` | 282 | typed Win32 backend dispatch; shared os2_doscalls_core preserved in backend; OS/2 visible handle/process state is common |
| `DosExecPgm` | 283 | typed Win32 backend dispatch; OS/2 visible handle/process state is common |
| `DosDevIOCtl` | 284 | typed Win32 backend dispatch |
| `DosBeep` | 286 | common validation + typed Win32 audio dispatch |
| `DosSetProcessCp` | 289 | typed Win32 backend dispatch |
| `DosQueryCp` | 291 | typed Win32 backend dispatch |
| `DosExitList` | 296 | common semantics/state |
| `DosAllocMem` | 299 | typed Win32 backend dispatch; shared os2_doscalls_core preserved in backend |
| `DosFreeMem` | 304 | typed Win32 backend dispatch; shared os2_doscalls_core preserved in backend |
| `DosSetMem` | 305 | typed Win32 backend dispatch; shared os2_doscalls_core preserved in backend |
| `DosQueryMem` | 306 | typed Win32 backend dispatch |
| `DosCreateThread` | 311 | typed Win32 backend dispatch |
| `DosGetInfoBlocks` | 312 | typed Win32 backend dispatch |
| `DosLoadModule` | 318 | typed Win32 backend dispatch |
| `DosQueryModuleHandle` | 319 | typed Win32 backend dispatch |
| `DosQueryModuleName` | 320 | typed Win32 backend dispatch |
| `DosQueryProcAddr` | 321 | typed Win32 backend dispatch |
| `DosFreeModule` | 322 | typed Win32 backend dispatch |
| `DosCreateEventSem` | 324 | common semantics/state |
| `DosOpenEventSem` | 325 | common semantics/state |
| `DosCloseEventSem` | 326 | common semantics/state |
| `DosResetEventSem` | 327 | common semantics/state |
| `DosPostEventSem` | 328 | common semantics/state |
| `DosWaitEventSem` | 329 | common semantics/state |
| `DosQueryEventSem` | 330 | common semantics/state |
| `DosCreateMutexSem` | 331 | typed Win32 backend dispatch |
| `DosOpenMutexSem` | 332 | typed Win32 backend dispatch |
| `DosCloseMutexSem` | 333 | typed Win32 backend dispatch |
| `DosRequestMutexSem` | 334 | typed Win32 backend dispatch |
| `DosReleaseMutexSem` | 335 | typed Win32 backend dispatch |
| `DosSubSetMem` | 344 | common semantics/state |
| `DosSubAllocMem` | 345 | common semantics/state |
| `DosSubFreeMem` | 346 | common semantics/state |
| `DosSubUnsetMem` | 347 | common semantics/state |
| `DosQuerySysInfo` | 348 | typed Win32 backend dispatch; shared os2_doscalls_core preserved in backend |
| `DosQueryCtryInfo` | 395 | typed Win32 backend dispatch |
| `DosQueryDBCSEnv` | 396 | typed Win32 backend dispatch |
| `DosMapCase` | 397 | typed Win32 backend dispatch |
| `DosSetExceptionHandler` | 354 | common semantics/state |
| `DosUnsetExceptionHandler` | 355 | common semantics/state |
| `DosSetSignalExceptionFocus` | 378 | common semantics/state |
| `DosSetRelMaxFH` | 382 | common semantics/state |
| `DosAcknowledgeSignalException` | 418 | common semantics/state |
| `DosFlatToSel` | 425 | register-ABI veneer (EAX token preserved) |
| `DosSelToFlat` | 426 | register-ABI veneer (EAX token preserved) |
| `O2NlsQueryCtryInfo` | named | named helper -> common DOSCALLS NLS route |
| `O2NlsQueryDBCSEnv` | named | named helper -> common DOSCALLS NLS route |
| `O2NlsMapCase` | named | named helper -> common DOSCALLS NLS route |
