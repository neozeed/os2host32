#include "../common/os2msg.h"
#include "../common/os2loader.h"
#include "../common/os2image.h"
#include "../common/os2veneer.h"
#include "../common/os2startup.h"
#include "../common/os2sha256.h"
#include <ndk/iofuncs.h>

#define LE4C_MAX_IMAGE_SIZE      (16UL * 1024UL * 1024UL)
#define LE4C_OBJECT_COUNT        2u
#define LE4C_OBJECT1_BASE        0x00010000UL
#define LE4C_OBJECT2_BASE        0x00020000UL
#define LE4C_ARENA_START         0x01000000UL
#define LE4C_ARENA_END           0x08000000UL
#define LE4C_ALLOC_GRANULARITY   0x00010000UL
#define LE4C_MAX_EXTERNAL_SITES  32u
#define LE4C_VENEER_REGION_SIZE  0x00001000UL
#define LE4C_STARTUP_REGION_SIZE 0x00020000UL
#define LE4C_MAX_ENV_BYTES       32768u
#define LE4C_MAX_PATH_BYTES      512u
#define LE4C_MAX_ALLOCS          32u
#define LE4IO_MAX_FILES           32u
#define LE4IO_MAX_PATH            1024u
#define LE4IO_MAX_IMPORTS         32u

static const ULONG gSupportedOrdinals[] = {
    223u,224u,230u,234u,256u,257u,259u,272u,273u,281u,282u,299u,304u,305u,348u
};

static HANDLE gApiPort;
static HANDLE gSharedSection;
static PUCHAR gClientViewBase;
static ULONG gClientViewSize;
static PVOID gRelocatedBases[LE4C_OBJECT_COUNT];
static PVOID gVeneerBase;
static PVOID gStartupBase;

/* Single-thread LE4C gateway state. */
#if 0
msvc.. sorry
static volatile ULONG gLe4cNativeEsp;
static volatile ULONG gLe4cOs2Esp;
static volatile ULONG gLe4cSavedEbx;
static volatile ULONG gLe4cSavedEsi;
static volatile ULONG gLe4cSavedEdi;
static volatile ULONG gLe4cSavedEbp;
static volatile ULONG gLe4cLastOrdinal;
static volatile ULONG gLe4cLastReturnCode;
static volatile ULONG gLe4cEntryVa;
#else
ULONG gLe4cEntryVa;
ULONG gLe4cLastOrdinal;
ULONG gLe4cLastReturnCode;
ULONG gLe4cNativeEsp;
ULONG gLe4cOs2Esp;
ULONG gLe4cSavedEbp;
ULONG gLe4cSavedEbx;
ULONG gLe4cSavedEdi;
ULONG gLe4cSavedEsi;
#endif

static CHAR gAnsiEnvironment[LE4C_MAX_ENV_BYTES];
static CHAR gProgramPath[LE4C_MAX_PATH_BYTES];
static CHAR gArg0[LE4C_MAX_PATH_BYTES];

typedef struct _LE4C_NATIVE_GUARD
{
    PVOID Environment;
    PVOID ProcessParameters;
    PVOID DeallocationStack;
    PVOID StackBase;
    PVOID StackLimit;
} LE4C_NATIVE_GUARD;

typedef struct _LE4C_ALLOCATION
{
    PVOID Base;
    SIZE_T Size;
    ULONG Flags;
    BOOLEAN Used;
} LE4C_ALLOCATION;

static LE4C_ALLOCATION gAllocations[LE4C_MAX_ALLOCS];

typedef struct _LE4IO_FILE
{
    HANDLE Handle;
    BOOLEAN Used;
} LE4IO_FILE;

static LE4IO_FILE gFiles[LE4IO_MAX_FILES];
static CHAR gIoAnsiPath[LE4IO_MAX_PATH];
static WCHAR gIoWidePath[LE4IO_MAX_PATH];
static WCHAR gIoFullPath[LE4IO_MAX_PATH];


static BOOLEAN
IsSupportedOrdinal(ULONG Ordinal)
{
    ULONG Index;
    for (Index = 0; Index < (ULONG)(sizeof(gSupportedOrdinals) / sizeof(gSupportedOrdinals[0])); ++Index)
        if (gSupportedOrdinals[Index] == Ordinal) return TRUE;
    return FALSE;
}

static APIRET
StatusToOs2Error(NTSTATUS Status)
{
    if (NT_SUCCESS(Status)) return OS2_NO_ERROR;
    if (Status == STATUS_OBJECT_NAME_NOT_FOUND || Status == STATUS_NO_SUCH_FILE)
        return OS2_ERROR_FILE_NOT_FOUND;
    if (Status == STATUS_OBJECT_PATH_NOT_FOUND)
        return OS2_ERROR_PATH_NOT_FOUND;
    if (Status == STATUS_ACCESS_DENIED || Status == STATUS_SHARING_VIOLATION)
        return OS2_ERROR_ACCESS_DENIED;
    if (Status == STATUS_NO_MEMORY || Status == STATUS_INSUFFICIENT_RESOURCES)
        return OS2_ERROR_NOT_ENOUGH_MEMORY;
    if (Status == STATUS_INVALID_HANDLE)
        return OS2_ERROR_INVALID_HANDLE;
    return OS2_ERROR_GEN_FAILURE;
}

static BOOLEAN
CopyOs2String(ULONG Address, CHAR *Buffer, ULONG Capacity)
{
    ULONG Index;
    const CHAR *Source;
    if (Address == 0u || Buffer == NULL || Capacity < 2u) return FALSE;
    Source = (const CHAR *)(ULONG_PTR)Address;
    for (Index = 0; Index + 1u < Capacity; ++Index)
    {
        CHAR C = Source[Index];
        Buffer[Index] = C;
        if (C == 0) return TRUE;
    }
    Buffer[Capacity - 1u] = 0;
    return FALSE;
}

static BOOLEAN
AsciiPathToUnicode(const CHAR *Ansi, WCHAR *Wide, ULONG CapacityChars)
{
    ULONG Index;
    if (Ansi == NULL || Wide == NULL || CapacityChars < 2u) return FALSE;
    for (Index = 0; Index + 1u < CapacityChars; ++Index)
    {
        UCHAR C = (UCHAR)Ansi[Index];
        Wide[Index] = (WCHAR)C;
        if (C == 0) return TRUE;
    }
    Wide[CapacityChars - 1u] = 0;
    return FALSE;
}

static APIRET
MakeNtPathFromGuest(ULONG PathAddress, UNICODE_STRING *NtPath)
{
    if (!CopyOs2String(PathAddress, gIoAnsiPath, sizeof(gIoAnsiPath)) ||
        !AsciiPathToUnicode(gIoAnsiPath, gIoWidePath, LE4IO_MAX_PATH))
        return OS2_ERROR_INVALID_PARAMETER;
    if (!RtlDosPathNameToNtPathName_U(gIoWidePath, NtPath, NULL, NULL))
        return OS2_ERROR_PATH_NOT_FOUND;
    return OS2_NO_ERROR;
}

static HANDLE
NativeFileFromOs2(ULONG FileHandle)
{
    if (FileHandle < 3u || FileHandle >= LE4IO_MAX_FILES || !gFiles[FileHandle].Used)
        return NULL;
    return gFiles[FileHandle].Handle;
}

static VOID
CloseAllNativeFiles(VOID)
{
    ULONG Index;
    for (Index = 3u; Index < LE4IO_MAX_FILES; ++Index)
    {
        if (gFiles[Index].Used)
        {
            NtClose(gFiles[Index].Handle);
            gFiles[Index].Handle = NULL;
            gFiles[Index].Used = FALSE;
        }
    }
}

static USHORT
PackOs2Date(const LARGE_INTEGER *SystemTime)
{
    LARGE_INTEGER Local;
    TIME_FIELDS Fields;
    ULONG Year;
    if (!NT_SUCCESS(RtlSystemTimeToLocalTime((PLARGE_INTEGER)SystemTime, &Local))) return 0;
    RtlTimeToTimeFields(&Local, &Fields);
    Year = Fields.Year >= 1980 ? (ULONG)Fields.Year - 1980u : 0u;
    if (Year > 127u) Year = 127u;
    return (USHORT)((Fields.Day & 31u) | ((Fields.Month & 15u) << 5) | (Year << 9));
}

static USHORT
PackOs2Time(const LARGE_INTEGER *SystemTime)
{
    LARGE_INTEGER Local;
    TIME_FIELDS Fields;
    if (!NT_SUCCESS(RtlSystemTimeToLocalTime((PLARGE_INTEGER)SystemTime, &Local))) return 0;
    RtlTimeToTimeFields(&Local, &Fields);
    return (USHORT)(((Fields.Second / 2) & 31u) | ((Fields.Minute & 63u) << 5) | ((Fields.Hour & 31u) << 11));
}

static VOID
FillOs2FileStatus(PUCHAR Dest, const FILE_NETWORK_OPEN_INFORMATION *Info)
{
    ULONG FileSize = Info->EndOfFile.HighPart ? 0xffffffffu : Info->EndOfFile.LowPart;
    ULONG Allocation = Info->AllocationSize.HighPart ? 0xffffffffu : Info->AllocationSize.LowPart;
    USHORT Date, Time;
    Date = PackOs2Date(&Info->CreationTime); Time = PackOs2Time(&Info->CreationTime);
    Dest[0]=(UCHAR)Date; Dest[1]=(UCHAR)(Date>>8); Dest[2]=(UCHAR)Time; Dest[3]=(UCHAR)(Time>>8);
    Date = PackOs2Date(&Info->LastAccessTime); Time = PackOs2Time(&Info->LastAccessTime);
    Dest[4]=(UCHAR)Date; Dest[5]=(UCHAR)(Date>>8); Dest[6]=(UCHAR)Time; Dest[7]=(UCHAR)(Time>>8);
    Date = PackOs2Date(&Info->LastWriteTime); Time = PackOs2Time(&Info->LastWriteTime);
    Dest[8]=(UCHAR)Date; Dest[9]=(UCHAR)(Date>>8); Dest[10]=(UCHAR)Time; Dest[11]=(UCHAR)(Time>>8);
    *(PULONG)(Dest + 12) = FileSize;
    *(PULONG)(Dest + 16) = Allocation;
    *(PULONG)(Dest + 20) = Info->FileAttributes & 0x37u;
}

static VOID
CaptureNativeGuard(LE4C_NATIVE_GUARD *Guard)
{
    PPEB Peb = NtCurrentPeb();
    PTEB Teb = NtCurrentTeb();
    PRTL_USER_PROCESS_PARAMETERS Params = Peb ? Peb->ProcessParameters : NULL;
    Guard->Environment = Params ? Params->Environment : NULL;
    Guard->ProcessParameters = Params;
    Guard->DeallocationStack = Teb ? Teb->DeallocationStack : NULL;
    Guard->StackBase = Teb ? Teb->NtTib.StackBase : NULL;
    Guard->StackLimit = Teb ? Teb->NtTib.StackLimit : NULL;
}

static BOOLEAN
NativeGuardUnchanged(const LE4C_NATIVE_GUARD *Before)
{
    LE4C_NATIVE_GUARD Now;
    CaptureNativeGuard(&Now);
    return Now.Environment == Before->Environment &&
           Now.ProcessParameters == Before->ProcessParameters &&
           Now.DeallocationStack == Before->DeallocationStack &&
           Now.StackBase == Before->StackBase && Now.StackLimit == Before->StackLimit;
}

static VOID
ReleaseRegion(PVOID *Region, const char *Name)
{
    if (*Region != NULL)
    {
        PVOID Base = *Region;
        SIZE_T Size = 0;
        NTSTATUS Status = NtFreeVirtualMemory(NtCurrentProcess(), &Base, &Size, MEM_RELEASE);
        DbgPrint("OS2BOOT: LE4C cleanup %s base=%p status=%08lx\n", Name, *Region, (ULONG)Status);
        *Region = NULL;
    }
}

static VOID
ReleaseOwnedMemory(VOID)
{
    ULONG Index;
    CloseAllNativeFiles();
    for (Index = 0; Index < LE4C_MAX_ALLOCS; ++Index)
    {
        if (gAllocations[Index].Used)
        {
            PVOID Base = gAllocations[Index].Base;
            SIZE_T Zero = 0;
            (void)NtFreeVirtualMemory(NtCurrentProcess(), &Base, &Zero, MEM_RELEASE);
            gAllocations[Index].Used = FALSE;
        }
    }
    ReleaseRegion(&gStartupBase, "startup");
    ReleaseRegion(&gVeneerBase, "veneer");
    for (Index = 0; Index < LE4C_OBJECT_COUNT; ++Index)
    {
        if (gRelocatedBases[Index] != NULL)
        {
            CHAR Name[16];
            Name[0] = 'o'; Name[1] = 'b'; Name[2] = 'j'; Name[3] = (CHAR)('1' + Index); Name[4] = 0;
            ReleaseRegion(&gRelocatedBases[Index], Name);
        }
    }
}

static VOID
FailAndExit(const char *Stage, NTSTATUS Status)
{
    DbgPrint("OS2BOOT: LE4C %s failed status=%08lx last_ordinal=%lu last_rc=%lu\n",
             Stage, (ULONG)Status, gLe4cLastOrdinal, gLe4cLastReturnCode);
    ReleaseOwnedMemory();
    if (gApiPort != NULL) NtClose(gApiPort);
    if (gSharedSection != NULL) NtClose(gSharedSection);
    NtTerminateProcess(NtCurrentProcess(), Status);
    for (;;) { }
}

static VOID
FailImageStage(const char *Stage, OS2I_STATUS Status)
{
    DbgPrint("OS2BOOT: LE4C %s failed image_status=%lu(%s)\n",
             Stage, (ULONG)Status, os2i_status_name(Status));
    FailAndExit(Stage, STATUS_INVALID_IMAGE_FORMAT);
}

static NTSTATUS
ReadExactFixture(PUCHAR *FileBuffer, PULONG FileSize)
{
    NTSTATUS Status;
    HANDLE FileHandle = NULL;
    OBJECT_ATTRIBUTES ObjectAttributes;
    IO_STATUS_BLOCK IoStatus;
    FILE_STANDARD_INFORMATION StandardInfo;
    PRTL_USER_PROCESS_PARAMETERS Parameters = NtCurrentPeb()->ProcessParameters;
    PVOID Buffer = NULL;
    ULONG Size;
    LARGE_INTEGER Offset;

    if (Parameters == NULL || Parameters->CommandLine.Buffer == NULL || Parameters->CommandLine.Length == 0)
        return STATUS_INVALID_PARAMETER;
    InitializeObjectAttributes(&ObjectAttributes, &Parameters->CommandLine, OBJ_CASE_INSENSITIVE, NULL, NULL);
    Status = NtOpenFile(&FileHandle, FILE_GENERIC_READ | SYNCHRONIZE, &ObjectAttributes, &IoStatus,
                        FILE_SHARE_READ, FILE_SYNCHRONOUS_IO_NONALERT | FILE_NON_DIRECTORY_FILE);
    if (!NT_SUCCESS(Status)) return Status;
    memset(&StandardInfo, 0, sizeof(StandardInfo));
    Status = NtQueryInformationFile(FileHandle, &IoStatus, &StandardInfo, sizeof(StandardInfo), FileStandardInformation);
    if (!NT_SUCCESS(Status)) { NtClose(FileHandle); return Status; }
    if (StandardInfo.EndOfFile.HighPart != 0 || StandardInfo.EndOfFile.LowPart == 0 ||
        StandardInfo.EndOfFile.LowPart > LE4C_MAX_IMAGE_SIZE)
    { NtClose(FileHandle); return STATUS_INVALID_IMAGE_FORMAT; }
    Size = StandardInfo.EndOfFile.LowPart;
    Buffer = RtlAllocateHeap(RtlGetProcessHeap(), 0, Size);
    if (Buffer == NULL) { NtClose(FileHandle); return STATUS_NO_MEMORY; }
    Offset.QuadPart = 0;
    Status = NtReadFile(FileHandle, NULL, NULL, NULL, &IoStatus, Buffer, Size, &Offset, NULL);
    NtClose(FileHandle);
    if (!NT_SUCCESS(Status) || (ULONG)IoStatus.Information != Size)
    { RtlFreeHeap(RtlGetProcessHeap(), 0, Buffer); return NT_SUCCESS(Status) ? STATUS_END_OF_FILE : Status; }
    *FileBuffer = (PUCHAR)Buffer; *FileSize = Size; return STATUS_SUCCESS;
}


static NTSTATUS
VerifyCompatibleLePlan(const OS2L_PLAN *Plan)
{
    size_t Index;
    if (Plan == NULL || Plan->format != OS2L_FORMAT_LE || Plan->object_count != LE4C_OBJECT_COUNT ||
        Plan->objects == NULL || Plan->objects[0].preferred_va != LE4C_OBJECT1_BASE ||
        Plan->objects[1].preferred_va != LE4C_OBJECT2_BASE ||
        Plan->objects[0].virtual_size == 0u || Plan->objects[1].virtual_size == 0u ||
        Plan->entry_object != 1u || Plan->entry_offset >= Plan->objects[0].virtual_size ||
        Plan->stack_object != 2u || Plan->stack_offset == 0u ||
        Plan->stack_offset > Plan->objects[1].virtual_size ||
        Plan->named_import_count != 0u || Plan->unsupported_fixup_records != 0u ||
        Plan->external_fixup_sites > LE4C_MAX_EXTERNAL_SITES ||
        Plan->ordinal_import_count == 0u || Plan->ordinal_import_count > LE4IO_MAX_IMPORTS)
        return STATUS_INVALID_IMAGE_FORMAT;

    for (Index = 0; Index < Plan->fixup_count; ++Index)
    {
        const OS2L_FIXUP_SITE *Fixup = &Plan->fixups[Index];
        if (Fixup->target_kind == OS2L_TARGET_INTERNAL)
        {
            if (Fixup->source_type != OS2L_SRC_OFF32) return STATUS_INVALID_IMAGE_FORMAT;
        }
        else if (Fixup->target_kind == OS2L_TARGET_IMPORT_ORDINAL)
        {
            if (Fixup->source_type != OS2L_SRC_REL32 || !IsSupportedOrdinal(Fixup->target_value))
                return STATUS_NOT_IMPLEMENTED;
        }
        else return STATUS_INVALID_IMAGE_FORMAT;
    }
    for (Index = 0; Index < Plan->ordinal_import_count; ++Index)
        if (!IsSupportedOrdinal(Plan->ordinal_imports[Index].ordinal)) return STATUS_NOT_IMPLEMENTED;
    return STATUS_SUCCESS;
}

static NTSTATUS
CreateSharedSection(VOID)
{
    LARGE_INTEGER SectionSize;
    if (gSharedSection != NULL) return STATUS_INVALID_PARAMETER;
    SectionSize.QuadPart = OS2_SHARED_SECTION_SIZE;
    return NtCreateSection(&gSharedSection, SECTION_ALL_ACCESS, NULL, &SectionSize,
                           PAGE_READWRITE, SEC_COMMIT, NULL);
}

static NTSTATUS
ConnectPersonality(PULONG PersonalityProcessId)
{
    NTSTATUS Status;
    UNICODE_STRING PortName;
    SECURITY_QUALITY_OF_SERVICE SecurityQos;
    ULONG MaxMessageLength = 0, ConnectInfoLength;
    OS2_CONNECT_INFO ConnectInfo;
    PORT_VIEW ClientView;
    REMOTE_PORT_VIEW ServerView;

    Status = CreateSharedSection();
    if (!NT_SUCCESS(Status)) return Status;
    RtlInitUnicodeString(&PortName, OS2_API_PORT_NAME);
    memset(&SecurityQos, 0, sizeof(SecurityQos));
    SecurityQos.Length = sizeof(SecurityQos);
    SecurityQos.ImpersonationLevel = SecurityIdentification;
    SecurityQos.ContextTrackingMode = SECURITY_STATIC_TRACKING;
    SecurityQos.EffectiveOnly = TRUE;
    memset(&ClientView, 0, sizeof(ClientView));
    ClientView.Length = sizeof(ClientView);
    ClientView.SectionHandle = gSharedSection;
    ClientView.ViewSize = OS2_SHARED_SECTION_SIZE;
    memset(&ServerView, 0, sizeof(ServerView)); ServerView.Length = sizeof(ServerView);
    Os2InitConnectInfo(&ConnectInfo); ConnectInfoLength = sizeof(ConnectInfo);
    Status = NtConnectPort(&gApiPort, &PortName, &SecurityQos, &ClientView, &ServerView,
                           &MaxMessageLength, &ConnectInfo, &ConnectInfoLength);
    if (!NT_SUCCESS(Status)) return Status;
    if (ConnectInfoLength < sizeof(ConnectInfo) || ConnectInfo.PersonalityProcessId == 0 ||
        ClientView.ViewBase == NULL || (ULONG)ClientView.ViewSize != OS2_SHARED_SECTION_SIZE)
        return STATUS_INVALID_PARAMETER;
    gClientViewBase = (PUCHAR)ClientView.ViewBase;
    gClientViewSize = (ULONG)ClientView.ViewSize;
    *PersonalityProcessId = ConnectInfo.PersonalityProcessId;
    return STATUS_SUCCESS;
}

static ULONG
Align64K(ULONG Value)
{
    if (Value > 0xffffffffUL - (LE4C_ALLOC_GRANULARITY - 1u)) return 0;
    return (Value + LE4C_ALLOC_GRANULARITY - 1u) & ~(LE4C_ALLOC_GRANULARITY - 1u);
}

static NTSTATUS
FindAndAllocateRegion(ULONG SearchStart, SIZE_T Extent, PVOID *BaseOut)
{
    ULONG Candidate = Align64K(SearchStart);
    MEMORY_BASIC_INFORMATION Info;
    SIZE_T ResultLength;
    if (!Candidate || !BaseOut || Extent == 0) return STATUS_INVALID_PARAMETER;
    while (Candidate < LE4C_ARENA_END)
    {
        ULONG RegionBase, RegionEnd;
        NTSTATUS Status;
        PVOID Base;
        SIZE_T Size;
        ResultLength = 0;
        Status = NtQueryVirtualMemory(NtCurrentProcess(), (PVOID)(ULONG_PTR)Candidate,
                                      MemoryBasicInformation, &Info, sizeof(Info), &ResultLength);
        if (!NT_SUCCESS(Status)) return Status;
        RegionBase = (ULONG)(ULONG_PTR)Info.BaseAddress;
        if ((ULONG)Info.RegionSize > 0xffffffffUL - RegionBase) RegionEnd = 0xffffffffUL;
        else RegionEnd = RegionBase + (ULONG)Info.RegionSize;
        if (Info.State == MEM_FREE && Candidate >= RegionBase &&
            Extent <= 0xffffffffUL - Candidate && Candidate + (ULONG)Extent <= RegionEnd)
        {
            Base = (PVOID)(ULONG_PTR)Candidate;
            Size = Extent;
            Status = NtAllocateVirtualMemory(NtCurrentProcess(), &Base, 0, &Size,
                                             MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
            if (!NT_SUCCESS(Status)) return Status;
            if ((ULONG)(ULONG_PTR)Base != Candidate)
            {
                PVOID ReleaseBase = Base; SIZE_T Zero = 0;
                NtFreeVirtualMemory(NtCurrentProcess(), &ReleaseBase, &Zero, MEM_RELEASE);
                return STATUS_CONFLICTING_ADDRESSES;
            }
            *BaseOut = Base;
            return STATUS_SUCCESS;
        }
        if (RegionEnd <= Candidate) return STATUS_CONFLICTING_ADDRESSES;
        Candidate = Align64K(RegionEnd);
        if (!Candidate) return STATUS_CONFLICTING_ADDRESSES;
    }
    return STATUS_NO_MEMORY;
}

static NTSTATUS
SetAndVerifyProtection(PVOID Base, SIZE_T Extent, ULONG Protection)
{
    NTSTATUS Status;
    PVOID ProtectBase = Base;
    SIZE_T ProtectSize = Extent;
    ULONG OldProtect = 0;
    MEMORY_BASIC_INFORMATION Info;
    SIZE_T ResultLength = 0;
    Status = NtProtectVirtualMemory(NtCurrentProcess(), &ProtectBase, &ProtectSize, Protection, &OldProtect);
    if (!NT_SUCCESS(Status)) return Status;
    Status = NtQueryVirtualMemory(NtCurrentProcess(), Base, MemoryBasicInformation,
                                  &Info, sizeof(Info), &ResultLength);
    if (!NT_SUCCESS(Status)) return Status;
    return Info.Protect == Protection ? STATUS_SUCCESS : STATUS_INVALID_PAGE_PROTECTION;
}

static APIRET
RequestMessage(OS2_API_MESSAGE *Message)
{
    NTSTATUS Status = NtRequestWaitReplyPort(gApiPort, &Message->Header, &Message->Header);
    if (!NT_SUCCESS(Status)) { gLe4cLastReturnCode = OS2_ERROR_GEN_FAILURE; return OS2_ERROR_GEN_FAILURE; }
    if (!NT_SUCCESS(Message->HostStatus)) { gLe4cLastReturnCode = OS2_ERROR_GEN_FAILURE; return OS2_ERROR_GEN_FAILURE; }
    gLe4cLastReturnCode = Message->ReturnCode;
    return (APIRET)Message->ReturnCode;
}

static APIRET
ClientDosQuerySysInfo(ULONG First, ULONG Last, ULONG Buffer, ULONG BufferSize)
{
    OS2_API_MESSAGE Message;
    ULONG Count, Index;
    PULONG Values = (PULONG)(ULONG_PTR)Buffer;
    if (Buffer == 0u || First == 0u || Last < First) return OS2_ERROR_INVALID_PARAMETER;
    Count = Last - First + 1u;
    if (Count == 0u || Count > OS2_QUERY_SYSINFO_MAX_VALUES || BufferSize < Count * sizeof(ULONG))
        return OS2_ERROR_INVALID_PARAMETER;
    Os2InitApiMessage(&Message, Os2ApiDosQuerySysInfo);
    Message.Data.QuerySysInfoRequest.Start = First;
    Message.Data.QuerySysInfoRequest.Last = Last;
    Message.Data.QuerySysInfoRequest.BufferSize = BufferSize;
    if (RequestMessage(&Message) != OS2_NO_ERROR) return (APIRET)Message.ReturnCode;
    if (Message.Data.QuerySysInfoReply.Count != Count) return OS2_ERROR_GEN_FAILURE;
    for (Index = 0; Index < Count; ++Index) Values[Index] = Message.Data.QuerySysInfoReply.Values[Index];
    return OS2_NO_ERROR;
}

static APIRET
ClientDosWrite(ULONG Handle, ULONG Buffer, ULONG Length, ULONG ActualAddress)
{
    OS2_API_MESSAGE Message;
    PULONG Actual = (PULONG)(ULONG_PTR)ActualAddress;
    HANDLE Native;
    IO_STATUS_BLOCK IoStatus;
    NTSTATUS Status;
    if (ActualAddress == 0u) return OS2_ERROR_INVALID_PARAMETER;
    *Actual = 0u;
    if (Length != 0u && Buffer == 0u) return OS2_ERROR_INVALID_PARAMETER;

    if (Handle >= 3u)
    {
        Native = NativeFileFromOs2(Handle);
        if (Native == NULL) return OS2_ERROR_INVALID_HANDLE;
        memset(&IoStatus, 0, sizeof(IoStatus));
        Status = NtWriteFile(Native, NULL, NULL, NULL, &IoStatus,
                             (PVOID)(ULONG_PTR)Buffer, Length, NULL, NULL);
        if (!NT_SUCCESS(Status)) return StatusToOs2Error(Status);
        *Actual = (ULONG)IoStatus.Information;
        return OS2_NO_ERROR;
    }

    if (Handle != 1u && Handle != 2u) return OS2_ERROR_INVALID_HANDLE;
    if (Length > OS2_R5_MAX_WRITE || gClientViewBase == NULL || gClientViewSize != OS2_SHARED_SECTION_SIZE)
        return OS2_ERROR_INVALID_PARAMETER;
    if (Length != 0u) memcpy(gClientViewBase, (PVOID)(ULONG_PTR)Buffer, Length);
    Os2InitApiMessage(&Message, Os2ApiDosWrite);
    Message.Data.DosWriteRequest.FileHandle = Handle;
    Message.Data.DosWriteRequest.SharedOffset = 0u;
    Message.Data.DosWriteRequest.Length = Length;
    if (RequestMessage(&Message) != OS2_NO_ERROR) return (APIRET)Message.ReturnCode;
    if (Message.Data.DosWriteReply.BytesWritten > Length) return OS2_ERROR_GEN_FAILURE;
    *Actual = Message.Data.DosWriteReply.BytesWritten;
    return OS2_NO_ERROR;
}


static APIRET
ClientDosGetDateTime(ULONG DateTimeAddress)
{
    OS2_API_MESSAGE Message;
    if (DateTimeAddress == 0u) return OS2_ERROR_INVALID_PARAMETER;
    Os2InitApiMessage(&Message, Os2ApiDosGetDateTime);
    if (RequestMessage(&Message) != OS2_NO_ERROR) return (APIRET)Message.ReturnCode;
    memcpy((PVOID)(ULONG_PTR)DateTimeAddress, &Message.Data.DateTimeReply,
           sizeof(Message.Data.DateTimeReply));
    return OS2_NO_ERROR;
}

static APIRET
ClientDosRead(ULONG Handle, ULONG Buffer, ULONG Length, ULONG ActualAddress)
{
    PULONG Actual = (PULONG)(ULONG_PTR)ActualAddress;
    IO_STATUS_BLOCK IoStatus;
    NTSTATUS Status;
    HANDLE Native;
    if (ActualAddress == 0u) return OS2_ERROR_INVALID_PARAMETER;
    *Actual = 0u;
    if (Length != 0u && Buffer == 0u) return OS2_ERROR_INVALID_PARAMETER;

    if (Handle == 0u)
    {
        OS2_API_MESSAGE Message;
        ULONG Ask = Length;
        if (Ask > OS2_R5_MAX_WRITE) Ask = OS2_R5_MAX_WRITE;
        if (gClientViewBase == NULL || gClientViewSize != OS2_SHARED_SECTION_SIZE)
            return OS2_ERROR_GEN_FAILURE;
        Os2InitApiMessage(&Message, Os2ApiDosRead);
        Message.Data.DosReadRequest.FileHandle = 0u;
        Message.Data.DosReadRequest.SharedOffset = 0u;
        Message.Data.DosReadRequest.Length = Ask;
        if (RequestMessage(&Message) != OS2_NO_ERROR) return (APIRET)Message.ReturnCode;
        if (Message.Data.DosReadReply.BytesRead > Ask) return OS2_ERROR_GEN_FAILURE;
        if (Message.Data.DosReadReply.BytesRead != 0u)
            memcpy((PVOID)(ULONG_PTR)Buffer, gClientViewBase, Message.Data.DosReadReply.BytesRead);
        *Actual = Message.Data.DosReadReply.BytesRead;
        return OS2_NO_ERROR;
    }

    Native = NativeFileFromOs2(Handle);
    if (Native == NULL) return OS2_ERROR_INVALID_HANDLE;
    memset(&IoStatus, 0, sizeof(IoStatus));
    Status = NtReadFile(Native, NULL, NULL, NULL, &IoStatus,
                        (PVOID)(ULONG_PTR)Buffer, Length, NULL, NULL);
    if (Status == STATUS_END_OF_FILE)
    {
        *Actual = 0u;
        return OS2_NO_ERROR;
    }
    if (!NT_SUCCESS(Status)) return StatusToOs2Error(Status);
    *Actual = (ULONG)IoStatus.Information;
    return OS2_NO_ERROR;
}

static APIRET
ClientDosClose(ULONG Handle)
{
    HANDLE Native = NativeFileFromOs2(Handle);
    NTSTATUS Status;
    if (Native == NULL) return OS2_ERROR_INVALID_HANDLE;
    Status = NtClose(Native);
    if (!NT_SUCCESS(Status)) return StatusToOs2Error(Status);
    gFiles[Handle].Handle = NULL;
    gFiles[Handle].Used = FALSE;
    return OS2_NO_ERROR;
}

static APIRET
ClientDosOpen(ULONG PathAddress, ULONG HfileAddress, ULONG ActionAddress,
              ULONG InitialSize, ULONG Attributes, ULONG OpenFlags,
              ULONG OpenMode, ULONG EaAddress)
{
    UNICODE_STRING NtPath;
    OBJECT_ATTRIBUTES ObjectAttributes;
    IO_STATUS_BLOCK IoStatus;
    LARGE_INTEGER AllocationSize;
    PLARGE_INTEGER AllocationPtr = NULL;
    HANDLE FileHandle = NULL;
    ACCESS_MASK Access;
    ULONG Share, Disposition, CreateOptions, FileAttributes;
    ULONG Slot, Action;
    NTSTATUS Status;
    APIRET Rc;

    if (HfileAddress == 0u || ActionAddress == 0u || EaAddress != 0u)
        return OS2_ERROR_INVALID_PARAMETER;
    Rc = MakeNtPathFromGuest(PathAddress, &NtPath);
    if (Rc != OS2_NO_ERROR) return Rc;

    switch (OpenMode & 7u)
    {
        case 0u: Access = FILE_GENERIC_READ; break;
        case 1u: Access = FILE_GENERIC_WRITE; break;
        case 2u: Access = FILE_GENERIC_READ | FILE_GENERIC_WRITE; break;
        default: RtlFreeUnicodeString(&NtPath); return OS2_ERROR_INVALID_ACCESS;
    }
    switch (OpenMode & 0x70u)
    {
        case 0u: case 0x40u: Share = FILE_SHARE_READ | FILE_SHARE_WRITE; break;
        case 0x10u: Share = 0u; break;
        case 0x20u: Share = FILE_SHARE_READ; break;
        case 0x30u: Share = FILE_SHARE_WRITE; break;
        default: RtlFreeUnicodeString(&NtPath); return OS2_ERROR_INVALID_ACCESS;
    }
    switch (OpenFlags)
    {
        case 1u: Disposition = FILE_OPEN; break;
        case 2u: Disposition = FILE_OVERWRITE; break;
        case 0x10u: Disposition = FILE_CREATE; break;
        case 0x11u: Disposition = FILE_OPEN_IF; break;
        case 0x12u: Disposition = FILE_OVERWRITE_IF; break;
        default: RtlFreeUnicodeString(&NtPath); return OS2_ERROR_INVALID_PARAMETER;
    }
    for (Slot = 3u; Slot < LE4IO_MAX_FILES; ++Slot) if (!gFiles[Slot].Used) break;
    if (Slot == LE4IO_MAX_FILES) { RtlFreeUnicodeString(&NtPath); return OS2_ERROR_TOO_MANY_OPEN_FILES; }

    AllocationSize.QuadPart = InitialSize;
    if (InitialSize != 0u) AllocationPtr = &AllocationSize;
    FileAttributes = Attributes & 0x27u;
    if (FileAttributes == 0u) FileAttributes = FILE_ATTRIBUTE_NORMAL;
    CreateOptions = FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT;
    Access |= SYNCHRONIZE;
    InitializeObjectAttributes(&ObjectAttributes, &NtPath, OBJ_CASE_INSENSITIVE, NULL, NULL);
    memset(&IoStatus, 0, sizeof(IoStatus));
    Status = NtCreateFile(&FileHandle, Access, &ObjectAttributes, &IoStatus,
                          AllocationPtr, FileAttributes, Share, Disposition,
                          CreateOptions, NULL, 0u);
    RtlFreeUnicodeString(&NtPath);
    if (!NT_SUCCESS(Status)) return StatusToOs2Error(Status);

    gFiles[Slot].Handle = FileHandle;
    gFiles[Slot].Used = TRUE;
    if (IoStatus.Information == FILE_CREATED) Action = 2u;
    else if (IoStatus.Information == FILE_OVERWRITTEN || IoStatus.Information == FILE_SUPERSEDED) Action = 3u;
    else Action = 1u;
    *(PULONG)(ULONG_PTR)HfileAddress = Slot;
    *(PULONG)(ULONG_PTR)ActionAddress = Action;
    DbgPrint("OS2BOOT: LE4IO DosOpen -> hfile=%lu action=%lu\n", Slot, Action);
    return OS2_NO_ERROR;
}

static APIRET
ClientDosDelete(ULONG PathAddress)
{
    UNICODE_STRING NtPath;
    OBJECT_ATTRIBUTES ObjectAttributes;
    NTSTATUS Status;
    APIRET Rc = MakeNtPathFromGuest(PathAddress, &NtPath);
    if (Rc != OS2_NO_ERROR) return Rc;
    InitializeObjectAttributes(&ObjectAttributes, &NtPath, OBJ_CASE_INSENSITIVE, NULL, NULL);
    Status = NtDeleteFile(&ObjectAttributes);
    RtlFreeUnicodeString(&NtPath);
    return StatusToOs2Error(Status);
}

static APIRET
ClientDosSetFileSize(ULONG Handle, ULONG Size)
{
    HANDLE Native = NativeFileFromOs2(Handle);
    FILE_END_OF_FILE_INFORMATION EndInfo;
    IO_STATUS_BLOCK IoStatus;
    NTSTATUS Status;
    if (Native == NULL) return OS2_ERROR_INVALID_HANDLE;
    EndInfo.EndOfFile.QuadPart = Size;
    Status = NtSetInformationFile(Native, &IoStatus, &EndInfo, sizeof(EndInfo), FileEndOfFileInformation);
    return StatusToOs2Error(Status);
}

static APIRET
ClientDosQueryPathInfo(ULONG PathAddress, ULONG Level, ULONG BufferAddress, ULONG BufferSize)
{
    UNICODE_STRING NtPath;
    OBJECT_ATTRIBUTES ObjectAttributes;
    FILE_NETWORK_OPEN_INFORMATION Info;
    NTSTATUS Status;
    APIRET Rc;
    if (BufferAddress == 0u) return OS2_ERROR_INVALID_PARAMETER;

    if (Level == 5u)
    {
        ULONG Bytes, Index, Chars;
        if (!CopyOs2String(PathAddress, gIoAnsiPath, sizeof(gIoAnsiPath)) ||
            !AsciiPathToUnicode(gIoAnsiPath, gIoWidePath, LE4IO_MAX_PATH)) return OS2_ERROR_INVALID_PARAMETER;
        Bytes = RtlGetFullPathName_U(gIoWidePath, sizeof(gIoFullPath), gIoFullPath, NULL);
        if (Bytes == 0u) return OS2_ERROR_PATH_NOT_FOUND;
        Chars = Bytes / sizeof(WCHAR);
        if (Chars + 1u > BufferSize) return OS2_ERROR_BUFFER_OVERFLOW;
        for (Index = 0; Index < Chars; ++Index)
            ((CHAR *)(ULONG_PTR)BufferAddress)[Index] = gIoFullPath[Index] <= 0x7fu ? (CHAR)gIoFullPath[Index] : '?';
        ((CHAR *)(ULONG_PTR)BufferAddress)[Chars] = 0;
        return OS2_NO_ERROR;
    }

    if (Level != 1u || BufferSize < 24u) return OS2_ERROR_INVALID_PARAMETER;
    Rc = MakeNtPathFromGuest(PathAddress, &NtPath);
    if (Rc != OS2_NO_ERROR) return Rc;
    InitializeObjectAttributes(&ObjectAttributes, &NtPath, OBJ_CASE_INSENSITIVE, NULL, NULL);
    memset(&Info, 0, sizeof(Info));
    Status = NtQueryFullAttributesFile(&ObjectAttributes, &Info);
    RtlFreeUnicodeString(&NtPath);
    if (!NT_SUCCESS(Status)) return StatusToOs2Error(Status);
    FillOs2FileStatus((PUCHAR)(ULONG_PTR)BufferAddress, &Info);
    return OS2_NO_ERROR;
}


static APIRET
ClientDosQueryHType(ULONG Handle, ULONG TypeAddress, ULONG AttrAddress)
{
    OS2_API_MESSAGE Message;
    if (TypeAddress == 0u || AttrAddress == 0u) return OS2_ERROR_INVALID_PARAMETER;
    if (Handle >= 3u)
    {
        if (NativeFileFromOs2(Handle) == NULL) return OS2_ERROR_INVALID_HANDLE;
        *(PULONG)(ULONG_PTR)TypeAddress = 0u;       /* disk/file handle */
        *(PULONG)(ULONG_PTR)AttrAddress = 0u;
        return OS2_NO_ERROR;
    }
    Os2InitApiMessage(&Message, Os2ApiDosQueryHType);
    Message.Data.QueryHTypeRequest.FileHandle = Handle;
    if (RequestMessage(&Message) != OS2_NO_ERROR) return (APIRET)Message.ReturnCode;
    *(PULONG)(ULONG_PTR)TypeAddress = Message.Data.QueryHTypeReply.Type;
    *(PULONG)(ULONG_PTR)AttrAddress = Message.Data.QueryHTypeReply.Attributes;
    return OS2_NO_ERROR;
}

static ULONG
ProtectFromOs2Flags(ULONG Flags)
{
    BOOLEAN R = (Flags & OS2_PAG_READ) != 0;
    BOOLEAN W = (Flags & OS2_PAG_WRITE) != 0;
    BOOLEAN X = (Flags & OS2_PAG_EXECUTE) != 0;
    ULONG Protection;
    if (X && W) Protection = PAGE_EXECUTE_READWRITE;
    else if (X && R) Protection = PAGE_EXECUTE_READ;
    else if (X) Protection = PAGE_EXECUTE;
    else if (W) Protection = PAGE_READWRITE;
    else if (R) Protection = PAGE_READONLY;
    else Protection = PAGE_NOACCESS;
    if (Flags & OS2_PAG_GUARD) Protection |= PAGE_GUARD;
    return Protection;
}

static LE4C_ALLOCATION *
FindAllocation(ULONG Base)
{
    ULONG Index;
    for (Index = 0; Index < LE4C_MAX_ALLOCS; ++Index)
        if (gAllocations[Index].Used && (ULONG)(ULONG_PTR)gAllocations[Index].Base == Base)
            return &gAllocations[Index];
    return NULL;
}

static APIRET
ClientDosAllocMem(ULONG BaseAddressAddress, ULONG Size, ULONG Flags, ULONG Reserved)
{
    OS2_API_MESSAGE Message;
    ULONG Index, Protection;
    PVOID Base = NULL;
    SIZE_T NativeSize = Size;
    NTSTATUS Status;
    if (BaseAddressAddress == 0u || Size == 0u) return OS2_ERROR_INVALID_PARAMETER;
    Os2InitApiMessage(&Message, Os2ApiDosAllocMem);
    Message.Data.AllocMemRequest.Size = Size;
    Message.Data.AllocMemRequest.Flags = Flags;
    Message.Data.AllocMemRequest.Reserved = Reserved;
    if (RequestMessage(&Message) != OS2_NO_ERROR) return (APIRET)Message.ReturnCode;
    for (Index = 0; Index < LE4C_MAX_ALLOCS; ++Index) if (!gAllocations[Index].Used) break;
    if (Index == LE4C_MAX_ALLOCS) return OS2_ERROR_NOT_ENOUGH_MEMORY;
    Protection = ProtectFromOs2Flags(Flags);
    if ((Protection & ~PAGE_GUARD) == PAGE_NOACCESS) Protection = PAGE_READWRITE;
    Status = NtAllocateVirtualMemory(NtCurrentProcess(), &Base, 0, &NativeSize,
                                     MEM_RESERVE | MEM_COMMIT, Protection);
    if (!NT_SUCCESS(Status) || Base == NULL) return OS2_ERROR_NOT_ENOUGH_MEMORY;
    gAllocations[Index].Base = Base; gAllocations[Index].Size = NativeSize;
    gAllocations[Index].Flags = Flags; gAllocations[Index].Used = TRUE;
    *(PULONG)(ULONG_PTR)BaseAddressAddress = (ULONG)(ULONG_PTR)Base;
    DbgPrint("OS2BOOT: LE4C DosAllocMem local base=%p size=%lu flags=%08lx\n", Base, Size, Flags);
    return OS2_NO_ERROR;
}

static APIRET
ClientDosFreeMem(ULONG BaseValue)
{
    OS2_API_MESSAGE Message;
    LE4C_ALLOCATION *Allocation;
    PVOID Base;
    SIZE_T Zero = 0;
    NTSTATUS Status;
    if (BaseValue == 0u) return OS2_ERROR_INVALID_PARAMETER;
    Os2InitApiMessage(&Message, Os2ApiDosFreeMem);
    Message.Data.FreeMemRequest.Base = BaseValue;
    if (RequestMessage(&Message) != OS2_NO_ERROR) return (APIRET)Message.ReturnCode;
    Allocation = FindAllocation(BaseValue);
    if (Allocation == NULL) return OS2_ERROR_INVALID_PARAMETER;
    Base = Allocation->Base;
    Status = NtFreeVirtualMemory(NtCurrentProcess(), &Base, &Zero, MEM_RELEASE);
    if (!NT_SUCCESS(Status)) return OS2_ERROR_INVALID_PARAMETER;
    Allocation->Used = FALSE;
    return OS2_NO_ERROR;
}

static APIRET
ClientDosSetMem(ULONG BaseValue, ULONG Size, ULONG Flags)
{
    OS2_API_MESSAGE Message;
    LE4C_ALLOCATION *Allocation;
    PVOID Base;
    SIZE_T NativeSize;
    NTSTATUS Status;
    ULONG OldProtect = 0, Protection;
    if (BaseValue == 0u || Size == 0u) return OS2_ERROR_INVALID_PARAMETER;
    Os2InitApiMessage(&Message, Os2ApiDosSetMem);
    Message.Data.SetMemRequest.Base = BaseValue;
    Message.Data.SetMemRequest.Size = Size;
    Message.Data.SetMemRequest.Flags = Flags;
    if (RequestMessage(&Message) != OS2_NO_ERROR) return (APIRET)Message.ReturnCode;
    Allocation = FindAllocation(BaseValue);
    if (Allocation == NULL || Size > Allocation->Size) return OS2_ERROR_INVALID_PARAMETER;
    Base = Allocation->Base; NativeSize = Size;
    if (Flags & OS2_PAG_DECOMMIT)
    {
        Status = NtFreeVirtualMemory(NtCurrentProcess(), &Base, &NativeSize, MEM_DECOMMIT);
        return NT_SUCCESS(Status) ? OS2_NO_ERROR : OS2_ERROR_INVALID_PARAMETER;
    }
    Protection = ProtectFromOs2Flags(Flags);
    if ((Protection & ~PAGE_GUARD) == PAGE_NOACCESS) Protection = PAGE_READWRITE;
    Status = NtProtectVirtualMemory(NtCurrentProcess(), &Base, &NativeSize, Protection, &OldProtect);
    return NT_SUCCESS(Status) ? OS2_NO_ERROR : OS2_ERROR_INVALID_PARAMETER;
}

static APIRET
ClientDosSetFilePtr(ULONG Handle, LONG Distance, ULONG Method, ULONG PositionAddress)
{
    OS2_API_MESSAGE Message;
    HANDLE Native;
    IO_STATUS_BLOCK IoStatus;
    FILE_POSITION_INFORMATION PositionInfo;
    FILE_STANDARD_INFORMATION StandardInfo;
    LONGLONG Base, NewPosition;
    NTSTATUS Status;
    if (PositionAddress == 0u || Method > 2u) return OS2_ERROR_INVALID_PARAMETER;

    if (Handle >= 3u)
    {
        Native = NativeFileFromOs2(Handle);
        if (Native == NULL) return OS2_ERROR_INVALID_HANDLE;
        memset(&PositionInfo, 0, sizeof(PositionInfo));
        if (Method == 0u) Base = 0;
        else if (Method == 1u)
        {
            Status = NtQueryInformationFile(Native, &IoStatus, &PositionInfo, sizeof(PositionInfo), FilePositionInformation);
            if (!NT_SUCCESS(Status)) return StatusToOs2Error(Status);
            Base = PositionInfo.CurrentByteOffset.QuadPart;
        }
        else
        {
            memset(&StandardInfo, 0, sizeof(StandardInfo));
            Status = NtQueryInformationFile(Native, &IoStatus, &StandardInfo, sizeof(StandardInfo), FileStandardInformation);
            if (!NT_SUCCESS(Status)) return StatusToOs2Error(Status);
            Base = StandardInfo.EndOfFile.QuadPart;
        }
        NewPosition = Base + (LONGLONG)Distance;
        if (NewPosition < 0 || NewPosition > 0xffffffffLL) return OS2_ERROR_INVALID_PARAMETER;
        PositionInfo.CurrentByteOffset.QuadPart = NewPosition;
        Status = NtSetInformationFile(Native, &IoStatus, &PositionInfo, sizeof(PositionInfo), FilePositionInformation);
        if (!NT_SUCCESS(Status)) return StatusToOs2Error(Status);
        *(PULONG)(ULONG_PTR)PositionAddress = (ULONG)NewPosition;
        return OS2_NO_ERROR;
    }

    Os2InitApiMessage(&Message, Os2ApiDosSetFilePtr);
    Message.Data.SetFilePtrRequest.FileHandle = Handle;
    Message.Data.SetFilePtrRequest.Distance = Distance;
    Message.Data.SetFilePtrRequest.Method = Method;
    if (RequestMessage(&Message) != OS2_NO_ERROR) return (APIRET)Message.ReturnCode;
    *(PULONG)(ULONG_PTR)PositionAddress = Message.Data.SetFilePtrReply.Position;
    return OS2_NO_ERROR;
}

static APIRET
ClientDosExit(ULONG Action, ULONG Result)
{
    OS2_API_MESSAGE Message;
    APIRET Rc;
    Os2InitApiMessage(&Message, Os2ApiDosExit);
    Message.Data.ExitRequest.Action = Action;
    Message.Data.ExitRequest.Result = Result;
    Rc = RequestMessage(&Message);
    if (Rc == OS2_NO_ERROR && Action == OS2_EXIT_PROCESS)
    {
        DbgPrint("OS2BOOT: LE4C DosExit terminating native vessel result=%lu\n", Result);
        NtTerminateProcess(NtCurrentProcess(), (NTSTATUS)Result);
        for (;;) { }
    }
    return Rc;
}

static ULONG
Os2Arg(ULONG Os2Esp, ULONG Number)
{
    return *(PULONG)(ULONG_PTR)(Os2Esp + 4u * Number);
}

//static ULONG __cdecl
ULONG __cdecl
Le4cGatewayDispatch(ULONG Ordinal, ULONG Os2Esp)
{
    APIRET Rc;
    gLe4cLastOrdinal = Ordinal;
    DbgPrint("OS2BOOT: LE4C gateway ordinal=%lu os2_esp=%08lx\n", Ordinal, Os2Esp);
    switch (Ordinal)
    {
        case 223u: Rc = ClientDosQueryPathInfo(Os2Arg(Os2Esp,1), Os2Arg(Os2Esp,2), Os2Arg(Os2Esp,3), Os2Arg(Os2Esp,4)); break;
        case 224u: Rc = ClientDosQueryHType(Os2Arg(Os2Esp,1), Os2Arg(Os2Esp,2), Os2Arg(Os2Esp,3)); break;
        case 230u: Rc = ClientDosGetDateTime(Os2Arg(Os2Esp,1)); break;
        case 234u: Rc = ClientDosExit(Os2Arg(Os2Esp,1), Os2Arg(Os2Esp,2)); break;
        case 256u: Rc = ClientDosSetFilePtr(Os2Arg(Os2Esp,1), (LONG)Os2Arg(Os2Esp,2), Os2Arg(Os2Esp,3), Os2Arg(Os2Esp,4)); break;
        case 257u: Rc = ClientDosClose(Os2Arg(Os2Esp,1)); break;
        case 259u: Rc = ClientDosDelete(Os2Arg(Os2Esp,1)); break;
        case 272u: Rc = ClientDosSetFileSize(Os2Arg(Os2Esp,1), Os2Arg(Os2Esp,2)); break;
        case 273u: Rc = ClientDosOpen(Os2Arg(Os2Esp,1), Os2Arg(Os2Esp,2), Os2Arg(Os2Esp,3), Os2Arg(Os2Esp,4), Os2Arg(Os2Esp,5), Os2Arg(Os2Esp,6), Os2Arg(Os2Esp,7), Os2Arg(Os2Esp,8)); break;
        case 281u: Rc = ClientDosRead(Os2Arg(Os2Esp,1), Os2Arg(Os2Esp,2), Os2Arg(Os2Esp,3), Os2Arg(Os2Esp,4)); break;
        case 282u: Rc = ClientDosWrite(Os2Arg(Os2Esp,1), Os2Arg(Os2Esp,2), Os2Arg(Os2Esp,3), Os2Arg(Os2Esp,4)); break;
        case 299u: Rc = ClientDosAllocMem(Os2Arg(Os2Esp,1), Os2Arg(Os2Esp,2), Os2Arg(Os2Esp,3), Os2Arg(Os2Esp,4)); break;
        case 304u: Rc = ClientDosFreeMem(Os2Arg(Os2Esp,1)); break;
        case 305u: Rc = ClientDosSetMem(Os2Arg(Os2Esp,1), Os2Arg(Os2Esp,2), Os2Arg(Os2Esp,3)); break;
        case 348u: Rc = ClientDosQuerySysInfo(Os2Arg(Os2Esp,1), Os2Arg(Os2Esp,2), Os2Arg(Os2Esp,3), Os2Arg(Os2Esp,4)); break;
        default: Rc = OS2_ERROR_INVALID_FUNCTION; break;
    }
    gLe4cLastReturnCode = Rc;
    DbgPrint("OS2BOOT: LE4C gateway ordinal=%lu rc=%lu\n", Ordinal, Rc);
    return Rc;
}

#if 0
moved to external assmebly file as tyring to build with gcc :(

Sorry

/*
 * Entry from untouched LE code. EAX carries the historical DOSCALLS ordinal.
 * The original C/386 stack is never used for NTDLL/LPC calls: save it, switch
 * to the preserved native stack anchor, dispatch, restore callee-saved state,
 * then RET through the original LE CALL return address.
 */
__declspec(naked) static VOID
Le4cDoscallGatewayEntry(VOID)
{
    __asm {
        mov dword ptr [gLe4cOs2Esp], esp
        mov dword ptr [gLe4cSavedEbx], ebx
        mov dword ptr [gLe4cSavedEsi], esi
        mov dword ptr [gLe4cSavedEdi], edi
        mov dword ptr [gLe4cSavedEbp], ebp
        mov dword ptr [gLe4cLastOrdinal], eax
        mov edx, dword ptr [gLe4cNativeEsp]
        mov esp, edx
        push dword ptr [gLe4cOs2Esp]
        push eax
        call Le4cGatewayDispatch
        add esp, 8
        mov dword ptr [gLe4cLastReturnCode], eax
        mov edx, dword ptr [gLe4cOs2Esp]
        mov esp, edx
        mov ebx, dword ptr [gLe4cSavedEbx]
        mov esi, dword ptr [gLe4cSavedEsi]
        mov edi, dword ptr [gLe4cSavedEdi]
        mov ebp, dword ptr [gLe4cSavedEbp]
        mov eax, dword ptr [gLe4cLastReturnCode]
        ret
    }
}

/* Local one-way transition. Exact WHP initial state: EAX=entry, other GPRs 0. */
__declspec(naked) static VOID __cdecl
Le4cEnterLe(ULONG Entry, ULONG Os2Esp)
{
    __asm {
        mov eax, dword ptr [esp + 4]
        mov dword ptr [gLe4cEntryVa], eax
        mov edx, dword ptr [esp + 8]
        mov dword ptr [gLe4cNativeEsp], esp
        mov esp, edx
        xor ebx, ebx
        xor ecx, ecx
        xor edx, edx
        xor esi, esi
        xor edi, edi
        xor ebp, ebp
        mov eax, dword ptr [gLe4cEntryVa]
        jmp eax
    }
}
#else
VOID
Le4cDoscallGatewayEntry(VOID);

VOID __cdecl
Le4cEnterLe(ULONG Entry, ULONG Os2Esp);
#endif

static BOOLEAN
AsciiEqual(const char *A, const char *B)
{
    ULONG I = 0;
    while (A[I] != 0 && B[I] != 0)
    {
        char CA = A[I], CB = B[I];
        if (CA >= 'a' && CA <= 'z') CA = (char)(CA - 'a' + 'A');
        if (CB >= 'a' && CB <= 'z') CB = (char)(CB - 'a' + 'A');
        if (CA != CB) return FALSE;
        ++I;
    }
    return A[I] == B[I];
}

static NTSTATUS
GenerateVeneersAndResolutions(const OS2L_PLAN *Plan,
                              OS2L_IMPORT_RESOLUTION *Resolutions,
                              ULONG ResolutionCapacity,
                              ULONG *ResolutionCount,
                              ULONG *NextSearch)
{
    ULONG Index;
    NTSTATUS Status;
    ULONG GatewayVa = (ULONG)(ULONG_PTR)Le4cDoscallGatewayEntry;
    if (Plan == NULL || Resolutions == NULL || ResolutionCount == NULL || NextSearch == NULL ||
        Plan->ordinal_import_count > ResolutionCapacity) return STATUS_INVALID_PARAMETER;
    Status = FindAndAllocateRegion(*NextSearch, LE4C_VENEER_REGION_SIZE, &gVeneerBase);
    if (!NT_SUCCESS(Status)) return Status;
    *NextSearch = Align64K((ULONG)(ULONG_PTR)gVeneerBase + LE4C_VENEER_REGION_SIZE);
    DbgPrint("OS2BOOT: LE4IO veneer region base=%p size=%08lx gateway=%08lx\n",
             gVeneerBase, LE4C_VENEER_REGION_SIZE, GatewayVa);
    *ResolutionCount = 0u;
    for (Index = 0; Index < (ULONG)Plan->ordinal_import_count; ++Index)
    {
        ULONG Ordinal = Plan->ordinal_imports[Index].ordinal;
        ULONG ModuleIndex = Plan->ordinal_imports[Index].module_index;
        ULONG VeneerVa = (ULONG)(ULONG_PTR)gVeneerBase + Index * OS2X86_VENEER_STRIDE;
        if (!IsSupportedOrdinal(Ordinal) || ModuleIndex == 0u || ModuleIndex > Plan->module_count ||
            !AsciiEqual(Plan->modules[ModuleIndex - 1u].name, "DOSCALLS"))
            return STATUS_NOT_IMPLEMENTED;
        if (!os2x86_emit_ordinal_veneer((PUCHAR)gVeneerBase + Index * OS2X86_VENEER_STRIDE,
                                       LE4C_VENEER_REGION_SIZE - Index * OS2X86_VENEER_STRIDE,
                                       VeneerVa, GatewayVa, Ordinal) ||
            !os2x86_verify_ordinal_veneer((PUCHAR)gVeneerBase + Index * OS2X86_VENEER_STRIDE,
                                         LE4C_VENEER_REGION_SIZE - Index * OS2X86_VENEER_STRIDE,
                                         VeneerVa, GatewayVa, Ordinal))
            return STATUS_INVALID_IMAGE_FORMAT;
        Resolutions[Index].module_index = ModuleIndex;
        Resolutions[Index].ordinal = Ordinal;
        Resolutions[Index].address = VeneerVa;
        ++*ResolutionCount;
        DbgPrint("OS2BOOT: LE4IO DOSCALLS.%lu -> %08lx\n", Ordinal, VeneerVa);
    }
    return SetAndVerifyProtection(gVeneerBase, LE4C_VENEER_REGION_SIZE, PAGE_EXECUTE_READ);
}

static NTSTATUS
NarrowNativeStartupInputs(PSIZE_T EnvironmentSize)
{
    PRTL_USER_PROCESS_PARAMETERS Params = NtCurrentPeb()->ProcessParameters;
    const WCHAR *Env;
    ULONG Out = 0, Chars, I, Start;
    if (!Params || !Params->CommandLine.Buffer || !EnvironmentSize) return STATUS_INVALID_PARAMETER;

    Env = (const WCHAR *)Params->Environment;
    if (Env != NULL)
    {
        ULONG ZeroRun = 0;
        for (I = 0; I < LE4C_MAX_ENV_BYTES - 1u; ++I)
        {
            WCHAR W = Env[I];
            gAnsiEnvironment[Out++] = (W <= 0x7fu) ? (CHAR)W : '?';
            if (W == 0) { if (++ZeroRun == 2u) break; }
            else ZeroRun = 0;
        }
        if (Out < 2u || gAnsiEnvironment[Out - 1u] != 0 || gAnsiEnvironment[Out - 2u] != 0)
            return STATUS_BUFFER_TOO_SMALL;
    }
    else
    {
        gAnsiEnvironment[0] = 0; gAnsiEnvironment[1] = 0; Out = 2u;
    }
    *EnvironmentSize = Out;

    Chars = Params->CommandLine.Length / sizeof(WCHAR);
    if (Chars >= LE4C_MAX_PATH_BYTES) return STATUS_BUFFER_TOO_SMALL;
    for (I = 0; I < Chars; ++I)
        gProgramPath[I] = (Params->CommandLine.Buffer[I] <= 0x7fu) ? (CHAR)Params->CommandLine.Buffer[I] : '?';
    gProgramPath[Chars] = 0;
    Start = 0;
    for (I = 0; I < Chars; ++I)
        if (gProgramPath[I] == '\\' || gProgramPath[I] == '/') Start = I + 1u;
    if (Chars - Start >= LE4C_MAX_PATH_BYTES) return STATUS_BUFFER_TOO_SMALL;
    for (I = Start; I <= Chars; ++I) gArg0[I - Start] = gProgramPath[I];
    return STATUS_SUCCESS;
}

void CDECL entry(void)
{
    NTSTATUS Status;
    ULONG PersonalityProcessId = 0;
    PUCHAR FileBuffer = NULL;
    ULONG FileSize = 0;
    OS2L_PLAN Plan;
    OS2L_ERROR ParseError;
    OS2L_RUNTIME_OBJECT Objects[LE4C_OBJECT_COUNT];
    OS2L_EXTERNAL_SNAPSHOT ExternalOriginal[LE4C_MAX_EXTERNAL_SITES];
    size_t ExternalCount = 0;
    OS2L_FIXUP_STATS InternalStats;
    OS2L_EXTERNAL_STATS ExternalStats;
    OS2L_IMPORT_RESOLUTION Resolutions[LE4IO_MAX_IMPORTS];
    ULONG ResolutionCount = 0u;
    OS2I_STATUS ImageStatus;
    LE4C_NATIVE_GUARD NativeBefore;
    ULONG EntryVa, StackTop, NextSearch;
    SIZE_T EnvironmentSize;
    OS2C386_STARTUP_INFO Startup;
    UCHAR Digest[32];

    memset(Objects, 0, sizeof(Objects));
    memset(Resolutions, 0, sizeof(Resolutions));
    memset(&Startup, 0, sizeof(Startup));
    memset(gAllocations, 0, sizeof(gAllocations));
    memset(gFiles, 0, sizeof(gFiles));
    CaptureNativeGuard(&NativeBefore);
    DbgPrint("OS2BOOT: LE4C ENTRY EXECUTED\n");
    DbgPrint("OS2BOOT: LE4C native env=%p params=%p dealloc_stack=%p stack=%p..%p\n",
             NativeBefore.Environment, NativeBefore.ProcessParameters, NativeBefore.DeallocationStack,
             NativeBefore.StackLimit, NativeBefore.StackBase);

    Status = ConnectPersonality(&PersonalityProcessId);
    if (!NT_SUCCESS(Status)) FailAndExit("personality connection", Status);
    DbgPrint("OS2BOOT: LE4C connected personality_pid=%lu client_view=%p size=%08lx\n",
             PersonalityProcessId, gClientViewBase, gClientViewSize);

    Status = ReadExactFixture(&FileBuffer, &FileSize);
    if (!NT_SUCCESS(Status)) FailAndExit("hi.exe acquisition", Status);
    os2_sha256(FileBuffer, FileSize, Digest);
    DbgPrint("OS2BOOT: LE4C image SHA256 observed first32=%02x%02x%02x%02x size=%lu (identity is not a launch guard)\n",
             Digest[0], Digest[1], Digest[2], Digest[3], FileSize);

    memset(&Plan, 0, sizeof(Plan)); memset(&ParseError, 0, sizeof(ParseError));
    if (!os2l_plan_image(FileBuffer, FileSize, &Plan, &ParseError))
    {
        DbgPrint("OS2BOOT: LE4C parser rejected hi.exe: %s\n", ParseError.message);
        RtlFreeHeap(RtlGetProcessHeap(), 0, FileBuffer);
        FailAndExit("frozen LE1 plan", STATUS_INVALID_IMAGE_FORMAT);
    }
    Status = VerifyCompatibleLePlan(&Plan);
    if (!NT_SUCCESS(Status)) { os2l_free_plan(&Plan); RtlFreeHeap(RtlGetProcessHeap(),0,FileBuffer); FailAndExit("compatible LE plan/import surface", Status); }
    DbgPrint("OS2BOOT: LE4C compatible LE plan verified obj1=%08lx obj2=%08lx entry=%08lx stack_top=%08lx\n",
             Plan.objects[0].virtual_size, Plan.objects[1].virtual_size, Plan.entry_va, Plan.stack_top);

    Objects[0].extent_size = os2l_round_extent(Plan.objects[0].virtual_size, Plan.page_size);
    Objects[1].extent_size = os2l_round_extent(Plan.objects[1].virtual_size, Plan.page_size);
    Status = FindAndAllocateRegion(LE4C_ARENA_START, Objects[0].extent_size, &gRelocatedBases[0]);
    if (!NT_SUCCESS(Status)) FailAndExit("object 1 relocation allocation", Status);
    Objects[0].bytes = (uint8_t *)gRelocatedBases[0]; Objects[0].actual_base = (ULONG)(ULONG_PTR)gRelocatedBases[0];
    NextSearch = Align64K(Objects[0].actual_base + Objects[0].extent_size);
    Status = FindAndAllocateRegion(NextSearch, Objects[1].extent_size, &gRelocatedBases[1]);
    if (!NT_SUCCESS(Status)) FailAndExit("object 2 relocation allocation", Status);
    Objects[1].bytes = (uint8_t *)gRelocatedBases[1]; Objects[1].actual_base = (ULONG)(ULONG_PTR)gRelocatedBases[1];
    NextSearch = Align64K(Objects[1].actual_base + Objects[1].extent_size);

    DbgPrint("OS2BOOT: LE4C object 1 preferred=%08lx actual=%08lx extent=%08lx\n",
             Plan.objects[0].preferred_va, Objects[0].actual_base, Objects[0].extent_size);
    DbgPrint("OS2BOOT: LE4C object 2 preferred=%08lx actual=%08lx extent=%08lx\n",
             Plan.objects[1].preferred_va, Objects[1].actual_base, Objects[1].extent_size);

    ImageStatus = os2l_materialize_objects(FileBuffer, FileSize, &Plan, Objects, LE4C_OBJECT_COUNT);
    if (ImageStatus != OS2I_OK) FailImageStage("object materialization", ImageStatus);
    ImageStatus = os2l_verify_materialized_objects(FileBuffer, FileSize, &Plan, Objects, LE4C_OBJECT_COUNT);
    if (ImageStatus != OS2I_OK) FailImageStage("page/zero-fill verification", ImageStatus);
    ImageStatus = os2l_capture_external_sites(&Plan, Objects, LE4C_OBJECT_COUNT,
                                              ExternalOriginal, LE4C_MAX_EXTERNAL_SITES, &ExternalCount);
    if (ImageStatus != OS2I_OK || ExternalCount != Plan.external_fixup_sites) FailImageStage("external staging capture", ImageStatus);
    ImageStatus = os2l_apply_internal_fixups(&Plan, Objects, LE4C_OBJECT_COUNT, &InternalStats);
    if (ImageStatus != OS2I_OK) FailImageStage("internal fixup application", ImageStatus);
    ImageStatus = os2l_verify_internal_fixups(&Plan, Objects, LE4C_OBJECT_COUNT, &InternalStats);
    if (ImageStatus != OS2I_OK) FailImageStage("internal fixup verification", ImageStatus);
    DbgPrint("OS2BOOT: LE4C internal fixups planned=%lu applied=%lu verified=%lu mismatches=%lu\n",
             InternalStats.internal_planned, InternalStats.internal_applied,
             InternalStats.internal_verified, InternalStats.internal_mismatches);

    Status = GenerateVeneersAndResolutions(&Plan, Resolutions, LE4IO_MAX_IMPORTS, &ResolutionCount, &NextSearch);
    if (!NT_SUCCESS(Status)) FailAndExit("DOSCALLS veneer generation", Status);
    ImageStatus = os2l_apply_external_ordinal_fixups(&Plan, Objects, LE4C_OBJECT_COUNT,
                                                     Resolutions, ResolutionCount, &ExternalStats);
    if (ImageStatus != OS2I_OK) FailImageStage("external fixup resolution", ImageStatus);
    ImageStatus = os2l_verify_external_ordinal_fixups(&Plan, Objects, LE4C_OBJECT_COUNT,
                                                      Resolutions, ResolutionCount, &ExternalStats);
    if (ImageStatus != OS2I_OK) FailImageStage("external fixup verification", ImageStatus);
    DbgPrint("OS2BOOT: LE4C external fixups planned=%lu resolved=%lu verified=%lu mismatches=%lu\n",
             ExternalStats.planned, ExternalStats.resolved, ExternalStats.verified, ExternalStats.mismatches);

    EntryVa = Objects[Plan.entry_object - 1u].actual_base + Plan.entry_offset;
    StackTop = Objects[Plan.stack_object - 1u].actual_base + Plan.stack_offset;
    Status = FindAndAllocateRegion(NextSearch, LE4C_STARTUP_REGION_SIZE, &gStartupBase);
    if (!NT_SUCCESS(Status)) FailAndExit("C/386 startup area allocation", Status);
    Status = NarrowNativeStartupInputs(&EnvironmentSize);
    if (!NT_SUCCESS(Status)) FailAndExit("C/386 startup input conversion", Status);
    if (!os2c386_build_startup_area((PUCHAR)gStartupBase, LE4C_STARTUP_REGION_SIZE,
                                    (ULONG)(ULONG_PTR)gStartupBase,
                                    gAnsiEnvironment, EnvironmentSize, gProgramPath, gArg0, &Startup) ||
        !os2c386_build_initial_stack(Objects[Plan.stack_object - 1u].bytes,
                                    Objects[Plan.stack_object - 1u].extent_size,
                                    Objects[Plan.stack_object - 1u].actual_base,
                                    StackTop, &Startup) ||
        !os2c386_verify_initial_stack(Objects[Plan.stack_object - 1u].bytes,
                                     Objects[Plan.stack_object - 1u].extent_size,
                                     Objects[Plan.stack_object - 1u].actual_base, &Startup))
        FailAndExit("C/386 startup frame", STATUS_DATA_ERROR);

    DbgPrint("OS2BOOT: LE4C relocated entry=%08lx\n", EntryVa);
    DbgPrint("OS2BOOT: LE4C stack_top=%08lx initial_esp=%08lx env=%08lx arg=%08lx pgm=%08lx\n",
             Startup.stack_top, Startup.initial_esp, Startup.env_va, Startup.arg_va, Startup.pgm_va);

    Status = SetAndVerifyProtection(gRelocatedBases[0], Objects[0].extent_size, PAGE_EXECUTE_READ);
    if (!NT_SUCCESS(Status)) FailAndExit("object 1 final RX protection", Status);
    Status = SetAndVerifyProtection(gRelocatedBases[1], Objects[1].extent_size, PAGE_READWRITE);
    if (!NT_SUCCESS(Status)) FailAndExit("object 2 final RW protection", Status);
    if (!NativeGuardUnchanged(&NativeBefore)) FailAndExit("native bootstrap state preservation", STATUS_DATA_ERROR);
    DbgPrint("OS2BOOT: LE4C object 1 protection=RX\n");
    DbgPrint("OS2BOOT: LE4C object 2 protection=RW\n");
    DbgPrint("OS2BOOT: LE4C native bootstrap state preserved\n");

    if (InternalStats.internal_applied != Plan.internal_fixup_sites ||
        InternalStats.internal_verified != Plan.internal_fixup_sites ||
        InternalStats.internal_mismatches != 0u ||
        ExternalStats.resolved != Plan.external_fixup_sites ||
        ExternalStats.verified != Plan.external_fixup_sites || ExternalStats.mismatches != 0u ||
        Startup.initial_esp + 20u != Startup.stack_top)
        FailAndExit("execution guard", STATUS_DATA_ERROR);

    os2l_free_plan(&Plan);
    RtlFreeHeap(RtlGetProcessHeap(), 0, FileBuffer);
    DbgPrint("OS2BOOT: LE4C EXECUTION ARMED\n");
    DbgPrint("OS2BOOT: LE4C entering LE entry=%08lx esp=%08lx native_fs_preserved\n",
             EntryVa, Startup.initial_esp);

    /* FIRST and only local transfer into untouched relocated LE code. */
    Le4cEnterLe(EntryVa, Startup.initial_esp);
    FailAndExit("LE entry unexpectedly returned", STATUS_UNSUCCESSFUL);
}
