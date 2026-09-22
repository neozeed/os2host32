# Dec-1991 NT CMD function index

Recovered from PE COFF FILE records and named symbols.

## `cmd.c`

| RVA | Function |
|---:|---|
| `0x00010000` | `_main` |
| `0x00010184` | `_Dispatch` |
| `0x00010308` | `_SetRedir` |
| `0x0001067C` | `_AddRedir` |
| `0x0001087C` | `_ResetRedir` |
| `0x000108D8` | `_FindFixAndRun` |
| `0x00010B30` | `_FindAndFix` |
| `0x00010DB0` | `_UnParse` |
| `0x00010E4C` | `_UnBuild` |
| `0x00011140` | `_UnDuRd` |
| `0x000111B8` | `_SPutC` |

## `csig.c`

| RVA | Function |
|---:|---|
| `0x000112B0` | `_Abort` |
| `0x000112D0` | `_ExitAbort` |
| `0x000112F0` | `_SigCleanUp` |

## `cop.c`

| RVA | Function |
|---:|---|
| `0x00011410` | `_eDetach` |
| `0x00011428` | `_eComSep` |
| `0x0001144C` | `_eOr` |
| `0x00011474` | `_eAnd` |
| `0x000114A0` | `_ePipe` |
| `0x00011758` | `_PipeErr` |
| `0x00011770` | `_PipeWait` |
| `0x000117F4` | `_BreakPipes` |
| `0x00011934` | `_eParen` |

## `ctools2.c`

| RVA | Function |
|---:|---|
| `0x00011950` | `_OnOffCheck` |
| `0x00011A14` | `_ChangeDrive` |
| `0x00011AC8` | `_HandleFix` |
| `0x00011AD0` | `_PutStdOut` |
| `0x00011AE8` | `_PutStdErr` |
| `0x00011B00` | `_PutMsg` |
| `0x00011BD4` | `_argstr1` |
| `0x00011BF0` | `_argstr2` |
| `0x00011C0C` | `_Copen_Work` |
| `0x00011D28` | `_Copen` |
| `0x00011D3C` | `_Copen_Copy` |
| `0x00011D50` | `_InSetList` |
| `0x00011D90` | `_Cdup` |
| `0x00011DC4` | `_Cdup2` |
| `0x00011E30` | `_Cclose` |
| `0x00011E88` | `_SetList` |
| `0x00011EC0` | `_GetFuncPtr` |
| `0x00011ED4` | `_FindCmd` |
| `0x00011F34` | `_KillProc` |
| `0x00011F6C` | `_WaitProc` |
| `0x00011FE4` | `_ParseLabel` |
| `0x000120BC` | `_EatWS` |
| `0x00012128` | `_IsValidDrv` |
| `0x00012158` | `_IsDriveLocked` |
| `0x000121CC` | `_PrtErr` |
| `0x00012220` | `_GetMsg` |
| `0x00012270` | `_dayptr` |
| `0x00012344` | `_Copen_Work2` |
| `0x0001242C` | `_Copen2` |
| `0x00012444` | `_Copen_Copy2` |
| `0x00012458` | `_stripit` |

## `cinit.c`

| RVA | Function |
|---:|---|
| `0x000124E6` | `_Handler` |
| `0x00012526` | `_Init` |
| `0x00012672` | `_CheckSwitches` |
| `0x0001283E` | `_SetUpEnvironment` |

## `ctools3.c`

| RVA | Function |
|---:|---|
| `0x00012A16` | `_FullPath` |
| `0x00012B76` | `_FileIsDevice` |
| `0x00012C42` | `_FileIsPipe` |
| `0x00012C72` | `_GetDir` |
| `0x00012D3A` | `_ChangeDir` |
| `0x00012F4E` | `_exists` |
| `0x00012F8E` | `_exists_ex` |
| `0x00013092` | `_FixPChar` |
| `0x000130C6` | `_FlushKB` |
| `0x0001310A` | `_DriveIsFixed` |
| `0x0001312E` | `_cmd_printf` |

## `cenv.c`

| RVA | Function |
|---:|---|
| `0x000131D6` | `_ePath` |
| `0x000131EA` | `_eAppend` |
| `0x000131FE` | `_PathWork` |
| `0x000132D2` | `_ePrompt` |
| `0x000132FE` | `_eSet` |
| `0x00013312` | `_SetWork` |
| `0x000134B2` | `_DisplayEnv` |
| `0x00013576` | `_SetEnvVar` |
| `0x000135A6` | `_GetEnvVar` |
| `0x000135CE` | `_CopyEnv` |
| `0x0001365A` | `_ResetEnv` |
| `0x0001369E` | `_MoveEnv` |
| `0x000136F2` | `_GetEnvCb` |

## `cbatch.c`

| RVA | Function |
|---:|---|
| `0x00013716` | `_BatProc` |
| `0x00013806` | `_BatLoop` |
| `0x00013A36` | `_SetBat` |
| `0x00013D56` | `_DisplayStatement` |
| `0x00013FC6` | `_DisplayOperator` |
| `0x00014002` | `_DisplayRedirection` |
| `0x00014056` | `_OpenPosBat` |
| `0x000140EE` | `_eEcho` |
| `0x00014196` | `_FvarRestore` |
| `0x000141BE` | `_eFor` |
| `0x00014522` | `_FWork` |
| `0x00014582` | `_SubFor` |
| `0x0001478E` | `_SFWork` |
| `0x0001495A` | `_ForFree` |
| `0x00014982` | `_eGoto` |
| `0x00014BA6` | `_eIf` |
| `0x00014C3E` | `_eErrorLevel` |
| `0x00014C5A` | `_eExist` |
| `0x00014C6A` | `_eNot` |
| `0x00014C92` | `_eStrCmp` |
| `0x00014CD2` | `_ePause` |
| `0x00014D92` | `_eShift` |
| `0x00014E0E` | `_eSetlocal` |
| `0x00014EE6` | `_eEndlocal` |
| `0x00014F1E` | `_ElclWork` |
| `0x00014FA6` | `_eCall` |
| `0x00014FBE` | `_CallWork` |
| `0x000151F2` | `_eExtproc` |
| `0x00015206` | `_ExtPWork` |

## `cmem.c`

| RVA | Function |
|---:|---|
| `0x00015406` | `_FreeBigBuf` |
| `0x00015432` | `_FreeStack` |
| `0x00015476` | `_FreeStr` |
| `0x000154E6` | `_GetBigBuf` |
| `0x0001556E` | `_mknode` |
| `0x0001557A` | `_mkstr` |
| `0x000155D2` | `_gmkstr` |
| `0x000155F2` | `_resize` |

## `string.c`

| RVA | Function |
|---:|---|
| `0x00015676` | `_InitializeDbcsLeadCharTable` |
| `0x0001569A` | `_mystrchr` |
| `0x00015716` | `_mystrrchr` |
| `0x00015792` | `_mystrcspn` |
| `0x000157EA` | `_lastc` |
| `0x00015822` | `_penulc` |
| `0x00015862` | `_prevc` |
| `0x000158B2` | `_FarZScan` |

## `cext.c`

| RVA | Function |
|---:|---|
| `0x0001594C` | `_ExtCom` |
| `0x000159AC` | `_ECWork` |
| `0x00015B7C` | `_ExecPgm` |
| `0x00015C80` | `_SearchForExecutable` |
| `0x000164CC` | `_DoFind` |
| `0x00016524` | `_ExecError` |
| `0x0001674C` | `_tokshrink` |

## `clex.c`

| RVA | Function |
|---:|---|
| `0x0001680C` | `_InitLex` |
| `0x00016838` | `_Lex` |
| `0x00016A60` | `_TextCheck` |
| `0x00016BE0` | `_GetByte` |
| `0x00016C88` | `_UnGetByte` |
| `0x00016C9C` | `_FillBuf` |
| `0x000170C8` | `_LexCopy` |
| `0x00017154` | `_PrintPrompt` |
| `0x0001734C` | `_IsData` |
| `0x00017358` | `_SubVar` |
| `0x00017634` | `_MSEnvVar` |

## `ckeys.c`

| RVA | Function |
|---:|---|
| `0x000176BC` | `_EditLine` |
| `0x000176E4` | `_eKeys` |

## `ffirst.c`

| RVA | Function |
|---:|---|
| `0x000177EC` | `_IsDosAttribMatch` |
| `0x00017808` | `_IsNtAttribMatch` |
| `0x00017810` | `_FindFirst` |
| `0x00017908` | `_FindFirstNt` |
| `0x00017924` | `_ffirst` |
| `0x00017940` | `_FindNextNt` |
| `0x00017958` | `_fnext` |
| `0x00017970` | `_FindNext` |
| `0x000179B8` | `_findclose` |

## `cpwork.c`

| RVA | Function |
|---:|---|
| `0x00017AF4` | `_copy` |
| `0x00017C00` | `_get_full_name` |
| `0x00017D4C` | `_do_normal_copy` |
| `0x00018474` | `_source_eq_dest` |
| `0x00018560` | `_do_combine_copy` |
| `0x000189A0` | `_initialize_struct` |
| `0x000189C0` | `_close_dest` |
| `0x00018A7C` | `_get_dest_name` |
| `0x00018CB8` | `_wildcard_rename` |
| `0x00018E58` | `_scan_bytes` |
| `0x00018EA4` | `_read_bytes` |
| `0x00018F84` | `_write_bytes` |
| `0x0001905C` | `_same_fcpy` |
| `0x000190A8` | `_same_file` |
| `0x0001910C` | `_copy_error` |
| `0x000191A0` | `_copy_was_ascii` |
| `0x000191D0` | `_open_for_append` |
| `0x000192B8` | `_WinEditName` |

## `cclock.c`

| RVA | Function |
|---:|---|
| `0x000193F4` | `_eDate` |
| `0x00019458` | `_eTime` |
| `0x000194BC` | `_PrintDate` |
| `0x00019558` | `_PrintTime` |
| `0x000195EC` | `_GetVerSetDateTime` |
| `0x000197A0` | `_VerifyDateString` |
| `0x000198D0` | `_VerifyTimeString` |
| `0x000199EC` | `_ConvertTimeToFileTime` |
| `0x00019A68` | `_ConvertFileTimeToTime` |

## `cother.c`

| RVA | Function |
|---:|---|
| `0x00019ADA` | `_eCls` |
| `0x00019B9A` | `_eExit` |
| `0x00019BAE` | `_eVerify` |
| `0x00019BC2` | `_VerifyWork` |
| `0x00019C1A` | `_GetSetVerMode` |

## `cparse.c`

| RVA | Function |
|---:|---|
| `0x00019C3C` | `_Parser` |
| `0x00019CD0` | `_ParseStatement` |
| `0x00019D0C` | `_ParseFor` |
| `0x00019E18` | `_ParseIf` |
| `0x00019EE4` | `_ParseDetach` |
| `0x00019F50` | `_ParseRem` |
| `0x00019FDC` | `_ParseS0` |
| `0x0001A010` | `_ParseS1` |
| `0x0001A02C` | `_ParseS2` |
| `0x0001A048` | `_ParseS3` |
| `0x0001A064` | `_ParseS4` |
| `0x0001A1F8` | `_ParseS5` |
| `0x0001A34C` | `_ParseCond` |
| `0x0001A440` | `_ParseArgEqArg` |
| `0x0001A5DC` | `_ParseCmd` |
| `0x0001A718` | `_ParseRedir` |
| `0x0001A8EC` | `_BinaryOperator` |
| `0x0001A9F0` | `_BuildArgList` |
| `0x0001AAD4` | `_GetCheckStr` |
| `0x0001AAFC` | `_GeTexTok` |
| `0x0001AB68` | `_GeToken` |
| `0x0001ABCC` | `_LoadNodeTC` |
| `0x0001AC2C` | `_PError` |
| `0x0001AC48` | `_PSError` |
| `0x0001ACD4` | `_SpaceCat` |

## `ctools1.c`

| RVA | Function |
|---:|---|
| `0x0001AD92` | `_TokStr` |
| `0x0001B03E` | `_LoopThroughArgs` |
| `0x0001B386` | `_IsDriveNameOnly` |
| `0x0001B3D6` | `_ScanFSpec` |
| `0x0001B76A` | `_SetFsSetSaveDir` |

## `cfile.c`

| RVA | Function |
|---:|---|
| `0x0001E5CA` | `_eCopy` |
| `0x0001E5E2` | `_eDelete` |
| `0x0001E5FA` | `_DelWork` |
| `0x0001ECF6` | `_PromptUser` |
| `0x0001EE02` | `_eRename` |
| `0x0001EE16` | `_RenWork` |
| `0x0001F42E` | `_RenError` |
| `0x0001F4A6` | `_eMove` |
| `0x0001F516` | `_MoveParse` |
| `0x0001F91E` | `_MoveError` |
| `0x0001F996` | `_Move` |
| `0x0001FDA6` | `_eChcp` |
| `0x0001FDBE` | `_eStart` |

## `cchcp.c`

| RVA | Function |
|---:|---|
| `0x0001FDDA` | `_Chcp` |

## `start.c`

| RVA | Function |
|---:|---|
| `0x0001FE00` | `_getparam` |
| `0x0001FF08` | `_Start` |

## `cpath.c`

| RVA | Function |
|---:|---|
| `0x000204E0` | `_eMkdir` |
| `0x000204FC` | `_MdWork` |
| `0x00020594` | `_eChdir` |
| `0x00020690` | `_ChdirWork` |
| `0x000206D8` | `_PushStr` |
| `0x00020724` | `_PopStr` |
| `0x00020774` | `_DumpStrStack` |
| `0x000207B0` | `_PushCurDir` |
| `0x00020840` | `_ePushDir` |
| `0x000208FC` | `_ePopDir` |
| `0x0002092C` | `_eRmdir` |
| `0x00020948` | `_RdWork` |

## `cinfo.c`

| RVA | Function |
|---:|---|
| `0x0002099C` | `_eDirectory` |
| `0x000209B4` | `_eType` |
| `0x000209D0` | `_TyWork` |
| `0x00020BF8` | `_eVersion` |
| `0x00020C78` | `_eVolume` |
| `0x00020C94` | `_VolWork` |

## `dir.c`

| RVA | Function |
|---:|---|
| `0x00020E3C` | `_Dir` |
| `0x00020FF0` | `_SetAttribs` |
| `0x00021120` | `_SetSortDesc` |
| `0x00021238` | `_ParseDirParms` |
| `0x000215A8` | `_SetWildCards` |
| `0x000216F4` | `_IsFATDrive` |
| `0x000217D8` | `_GetDrive` |
| `0x000218C4` | `_PrintPatterns` |
| `0x00021F94` | `_DirWalkAndPrint` |
| `0x0002237C` | `_GetFS` |
| `0x00022620` | `_CmpName` |
| `0x0002265C` | `_CmpExt` |
| `0x00022708` | `_CmpTime` |
| `0x00022724` | `_CmpSize` |
| `0x00022750` | `_CmpType` |
| `0x00022770` | `_SortCompare` |
| `0x000227D4` | `_SortFileList` |

## `display.c`

| RVA | Function |
|---:|---|
| `0x00022812` | `_DisplayFileListHeader` |
| `0x00022842` | `_DisplayFileList` |
| `0x00022A12` | `_DisplayFile` |
| `0x00022ACE` | `_DisplayBare` |
| `0x00022BCE` | `_SetDotForm` |
| `0x00022C1E` | `_DisplayDotForm` |
| `0x00022C82` | `_DisplaySpacedForm` |
| `0x00022DA6` | `_DisplayOldRest` |
| `0x00022DF6` | `_DisplayTimeDate` |
| `0x00022E72` | `_DisplayNewRest` |
| `0x00022EC6` | `_DisplayWide` |
| `0x00022F4E` | `_GetMaxCbFileSize` |
| `0x00022FCE` | `_DisplayFileSizes` |
| `0x00023016` | `_DisplayTotals` |
| `0x0002304A` | `_DisplayDiskFreeSpace` |
| `0x0002310E` | `_DisplayVolInfo` |

## `console.c`

| RVA | Function |
|---:|---|
| `0x00023272` | `_OpenScreen` |
| `0x000232DA` | `_WriteString` |
| `0x000232F2` | `_WriteMsgString` |
| `0x000233E2` | `_WriteFmtString` |
| `0x0002347A` | `_WriteErrString` |
| `0x0002349E` | `_WriteEol` |
| `0x00023536` | `_CheckPause` |
| `0x0002355E` | `_SetTab` |
| `0x00023586` | `_WriteTab` |
| `0x000235FE` | `_FillToCol` |
| `0x00023652` | `_WriteFlush` |
| `0x000236BA` | `_WriteFlushAndEol` |

## `uipriv.c`

| RVA | Function |
|---:|---|
| `0x00024690` | `_OpenAppropriateToken` |
| `0x00024708` | `_EnableAllPrivileges` |
| `0x000247B0` | `_ResetAllPrivileges` |
| `0x00024860` | `_DisableAllPrivileges` |
| `0x000248A0` | `_ePriv` |

## `cpparse.c`

| RVA | Function |
|---:|---|
| `0x00025CF4` | `_parse_args` |
| `0x00025EF0` | `_handle_switch` |
| `0x00025F88` | `_found_file` |
| `0x0002603C` | `_set_mode` |
| `0x000260C8` | `_add_filespec_to_struct` |

