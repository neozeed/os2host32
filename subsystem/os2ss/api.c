#include "os2ss.h"
#include <ndk/kefuncs.h>

static NTSTATUS
Os2HandleR3Ping(
    _In_ POS2_PROCESS Process,
    _Inout_ POS2_API_MESSAGE Message)
{
    if (Message->Data.Ping.Value != OS2_R3_PING_REQUEST_VALUE)
    {
        DbgPrint("OS2SS: LE4C invalid PING value %08lx personality_pid=%lu\n",
                 Message->Data.Ping.Value,
                 Process->PersonalityProcessId);
        Message->ReturnCode = 0;
        Message->HostStatus = STATUS_INVALID_PARAMETER;
        return Message->HostStatus;
    }

    DbgPrint("OS2SS: LE4C PING personality_pid=%lu\n",
             Process->PersonalityProcessId);
    Message->ReturnCode = OS2_R3_PING_REPLY_VALUE;
    Message->HostStatus = STATUS_SUCCESS;
    DbgPrint("OS2SS: LE4C PING reply CAFEBABE\n");
    return STATUS_SUCCESS;
}

static ULONG
Os2R4QuerySysInfoValue(_In_ ULONG Index)
{
    switch (Index)
    {
        case OS2_QSV_PAGE_SIZE:
            return OS2_R4_PAGE_SIZE_VALUE;
        case OS2_QSV_VERSION_MAJOR:
            return OS2_R4_VERSION_MAJOR_VALUE;
        case OS2_QSV_VERSION_MINOR:
            return OS2_R4_VERSION_MINOR_VALUE;
        default:
            return 0;
    }
}

NTSTATUS
Os2SrvDosQuerySysInfo(
    _In_ POS2_PROCESS Process,
    _Inout_ POS2_API_MESSAGE Message)
{
    ULONG Start = Message->Data.QuerySysInfoRequest.Start;
    ULONG Last = Message->Data.QuerySysInfoRequest.Last;
    ULONG BufferSize = Message->Data.QuerySysInfoRequest.BufferSize;
    ULONG Count;
    ULONG Index;
    POS2_QUERY_SYSINFO_REPLY Reply = &Message->Data.QuerySysInfoReply;

    DbgPrint("OS2SS: LE4C DosQuerySysInfo personality_pid=%lu start=%lu last=%lu\n",
             Process->PersonalityProcessId,
             Start,
             Last);

    /* Personality-level validation errors are returned as OS/2 APIRET. */
    Message->HostStatus = STATUS_SUCCESS;
    Message->ReturnCode = OS2_NO_ERROR;

    if (Start == 0 || Last < Start)
    {
        Message->ReturnCode = OS2_ERROR_INVALID_PARAMETER;
        DbgPrint("OS2SS: LE4C DosQuerySysInfo rc=%lu count=0\n",
                 Message->ReturnCode);
        return STATUS_SUCCESS;
    }

    Count = Last - Start + 1UL;
    if (Count == 0 || Count > OS2_QUERY_SYSINFO_MAX_VALUES)
    {
        Message->ReturnCode = OS2_ERROR_INVALID_PARAMETER;
        DbgPrint("OS2SS: LE4C DosQuerySysInfo rc=%lu count=0\n",
                 Message->ReturnCode);
        return STATUS_SUCCESS;
    }

    if (BufferSize < Count * sizeof(ULONG))
    {
        Message->ReturnCode = OS2_ERROR_BUFFER_OVERFLOW;
        DbgPrint("OS2SS: LE4C DosQuerySysInfo rc=%lu count=0\n",
                 Message->ReturnCode);
        return STATUS_SUCCESS;
    }

    if (Start < OS2_QSV_PAGE_SIZE || Last > OS2_QSV_VERSION_MINOR)
    {
        Message->ReturnCode = OS2_ERROR_INVALID_PARAMETER;
        DbgPrint("OS2SS: LE4C DosQuerySysInfo rc=%lu count=0\n",
                 Message->ReturnCode);
        return STATUS_SUCCESS;
    }

    memset(Reply, 0, sizeof(*Reply));
    Reply->Count = Count;
    for (Index = 0; Index < Count; ++Index)
        Reply->Values[Index] = Os2R4QuerySysInfoValue(Start + Index);

    Message->ReturnCode = OS2_NO_ERROR;
    Message->HostStatus = STATUS_SUCCESS;
    DbgPrint("OS2SS: LE4C DosQuerySysInfo rc=0 count=%lu\n", Count);
    return STATUS_SUCCESS;
}

/*
 * R5 stdout backend is intentionally proof-level: OS2SS has no durable OS/2
 * HFILE/console routing layer yet, so accepted stdout bytes are emitted via
 * DbgPrint solely to make the transport observable.  APIRET/pcbActual remain
 * OS/2-facing; general HFILE semantics are deferred.
 */
NTSTATUS
Os2SrvDosWrite(
    _In_ POS2_PROCESS Process,
    _Inout_ POS2_API_MESSAGE Message)
{
    ULONG FileHandle = Message->Data.DosWriteRequest.FileHandle;
    ULONG Offset = Message->Data.DosWriteRequest.SharedOffset;
    ULONG Length = Message->Data.DosWriteRequest.Length;
    PUCHAR Bytes;
    ULONG Index;
    ULONG Actual = 0;
    NTSTATUS ConsoleStatus;

    Message->HostStatus = STATUS_SUCCESS;
    Message->ReturnCode = OS2_NO_ERROR;
    Message->Data.DosWriteReply.BytesWritten = 0;

    DbgPrint("OS2SS: LE4C DosWrite personality_pid=%lu handle=%lu length=%lu\n",
             Process != NULL ? Process->PersonalityProcessId : 0,
             FileHandle,
             Length);

    if (Process == NULL || Process->SharedServerBase == NULL ||
        Process->SharedDataSize != OS2_SHARED_SECTION_SIZE)
    {
        Message->ReturnCode = OS2_ERROR_GEN_FAILURE;
        return STATUS_SUCCESS;
    }

    if (FileHandle != LE4C_CONSOLE_STDOUT && FileHandle != LE4C_CONSOLE_STDERR)
    {
        Message->ReturnCode = OS2_ERROR_INVALID_HANDLE;
        return STATUS_SUCCESS;
    }

    if (Length > OS2_R5_MAX_WRITE ||
        Offset > Process->SharedDataSize ||
        Length > (Process->SharedDataSize - Offset))
    {
        Message->ReturnCode = OS2_ERROR_INVALID_PARAMETER;
        return STATUS_SUCCESS;
    }

    Bytes = (PUCHAR)Process->SharedServerBase + Offset;

    /* Secondary evidence only: this is SERIAL DEBUG OUTPUT, not console semantics. */
    DbgPrint("OS2SS: LE4C SERIAL DEBUG OUTPUT begin\n");
    for (Index = 0; Index < Length; ++Index)
        DbgPrint("%c", (ULONG)Bytes[Index]);
    DbgPrint("OS2SS: LE4C SERIAL DEBUG OUTPUT end\n");

    /*
     * TEMPORARY NATIVE CONSOLE BACKEND: OS2SS remains semantic authority.
     * The console-attached subsystem-3 launcher performs the actual ConSrv
     * client write and acknowledges the byte count before DosWrite succeeds.
     */
    ConsoleStatus = Os2Le4cConsoleWrite(FileHandle, Bytes, Length, &Actual);
    if (!NT_SUCCESS(ConsoleStatus) || Actual != Length)
    {
        DbgPrint("OS2SS: LE4C visible console write failed status=%08lx actual=%lu\n",
                 (ULONG)ConsoleStatus, Actual);
        Message->ReturnCode = OS2_ERROR_GEN_FAILURE;
        Message->HostStatus = ConsoleStatus;
        Message->Data.DosWriteReply.BytesWritten = Actual;
        return STATUS_SUCCESS;
    }

    Message->Data.DosWriteReply.BytesWritten = Actual;
    Message->ReturnCode = OS2_NO_ERROR;
    Message->HostStatus = STATUS_SUCCESS;
    DbgPrint("OS2SS: LE4C visible console write rc=0 actual=%lu\n", Actual);
    return STATUS_SUCCESS;
}

static NTSTATUS
Os2SrvDosQueryHType(
    _In_ POS2_PROCESS Process,
    _Inout_ POS2_API_MESSAGE Message)
{
    ULONG Handle = Message->Data.QueryHTypeRequest.FileHandle;
    POS2_QUERY_HTYPE_REPLY Reply = &Message->Data.QueryHTypeReply;
    Message->HostStatus = STATUS_SUCCESS;
    Message->ReturnCode = OS2_NO_ERROR;
    memset(Reply, 0, sizeof(*Reply));

    DbgPrint("OS2SS: LE4C DosQueryHType personality_pid=%lu handle=%lu\n",
             Process->PersonalityProcessId, Handle);
    if (Handle > 2u)
    {
        Message->ReturnCode = OS2_ERROR_INVALID_HANDLE;
        return STATUS_SUCCESS;
    }

    /* Exact hi.exe only needs standard handles; model them as character devices. */
    Reply->Type = 1u;
    Reply->Attributes = 0u;
    DbgPrint("OS2SS: LE4C DosQueryHType rc=0 type=1 attr=0\n");
    return STATUS_SUCCESS;
}

static NTSTATUS
Os2SrvDosExit(
    _In_ POS2_PROCESS Process,
    _Inout_ POS2_API_MESSAGE Message)
{
    ULONG Action = Message->Data.ExitRequest.Action;
    ULONG Result = Message->Data.ExitRequest.Result;
    Message->HostStatus = STATUS_SUCCESS;
    Message->ReturnCode = OS2_NO_ERROR;

    DbgPrint("OS2SS: LE4C DosExit personality_pid=%lu action=%lu result=%lu\n",
             Process->PersonalityProcessId, Action, Result);
    if (Action == OS2_EXIT_PROCESS)
    {
        NTSTATUS ConsoleStatus = Os2Le4cConsoleNotifyExit(Result);
        if (!NT_SUCCESS(ConsoleStatus))
            DbgPrint("OS2SS: LE4C console exit notification failed %08lx\n", (ULONG)ConsoleStatus);
        return STATUS_SUCCESS;
    }
    if (Action == OS2_EXIT_THREAD)
    {
        /* Native-thread OS/2 semantics are explicitly outside LE4C. */
        Message->ReturnCode = OS2_ERROR_INVALID_FUNCTION;
        return STATUS_SUCCESS;
    }
    Message->ReturnCode = OS2_ERROR_INVALID_PARAMETER;
    return STATUS_SUCCESS;
}

static NTSTATUS
Os2SrvDosSetFilePtr(
    _In_ POS2_PROCESS Process,
    _Inout_ POS2_API_MESSAGE Message)
{
    ULONG Handle = Message->Data.SetFilePtrRequest.FileHandle;
    ULONG Method = Message->Data.SetFilePtrRequest.Method;
    (void)Process;
    Message->HostStatus = STATUS_SUCCESS;
    Message->Data.SetFilePtrReply.Position = 0u;
    if (Method > 2u)
        Message->ReturnCode = OS2_ERROR_INVALID_PARAMETER;
    else
        Message->ReturnCode = OS2_ERROR_INVALID_HANDLE;
    DbgPrint("OS2SS: LE4C DosSetFilePtr handle=%lu method=%lu rc=%lu (no general HFILE backend)\n",
             Handle, Method, Message->ReturnCode);
    return STATUS_SUCCESS;
}

static NTSTATUS
Os2SrvDosAllocMem(
    _In_ POS2_PROCESS Process,
    _Inout_ POS2_API_MESSAGE Message)
{
    ULONG Size = Message->Data.AllocMemRequest.Size;
    ULONG Flags = Message->Data.AllocMemRequest.Flags;
    ULONG Reserved = Message->Data.AllocMemRequest.Reserved;
    const ULONG Allowed = OS2_PAG_READ | OS2_PAG_WRITE | OS2_PAG_EXECUTE |
                          OS2_PAG_GUARD | OS2_PAG_COMMIT | OS2_OBJ_TILE;
    Message->HostStatus = STATUS_SUCCESS;
    Message->ReturnCode = OS2_NO_ERROR;
    DbgPrint("OS2SS: LE4C DosAllocMem personality_pid=%lu size=%lu flags=%08lx reserved=%08lx\n",
             Process->PersonalityProcessId, Size, Flags, Reserved);
    if (Size == 0u || (Flags & ~Allowed) != 0u)
        Message->ReturnCode = OS2_ERROR_INVALID_PARAMETER;
    return STATUS_SUCCESS;
}

static NTSTATUS
Os2SrvDosFreeMem(
    _In_ POS2_PROCESS Process,
    _Inout_ POS2_API_MESSAGE Message)
{
    ULONG Base = Message->Data.FreeMemRequest.Base;
    Message->HostStatus = STATUS_SUCCESS;
    Message->ReturnCode = Base != 0u ? OS2_NO_ERROR : OS2_ERROR_INVALID_PARAMETER;
    DbgPrint("OS2SS: LE4C DosFreeMem personality_pid=%lu base=%08lx rc=%lu\n",
             Process->PersonalityProcessId, Base, Message->ReturnCode);
    return STATUS_SUCCESS;
}

static NTSTATUS
Os2SrvDosSetMem(
    _In_ POS2_PROCESS Process,
    _Inout_ POS2_API_MESSAGE Message)
{
    ULONG Base = Message->Data.SetMemRequest.Base;
    ULONG Size = Message->Data.SetMemRequest.Size;
    ULONG Flags = Message->Data.SetMemRequest.Flags;
    const ULONG Allowed = OS2_PAG_READ | OS2_PAG_WRITE | OS2_PAG_EXECUTE |
                          OS2_PAG_GUARD | OS2_PAG_COMMIT | OS2_PAG_DECOMMIT;
    Message->HostStatus = STATUS_SUCCESS;
    Message->ReturnCode = OS2_NO_ERROR;
    DbgPrint("OS2SS: LE4C DosSetMem personality_pid=%lu base=%08lx size=%lu flags=%08lx\n",
             Process->PersonalityProcessId, Base, Size, Flags);
    if (Base == 0u || Size == 0u || (Flags & ~Allowed) != 0u ||
        ((Flags & OS2_PAG_COMMIT) && (Flags & OS2_PAG_DECOMMIT)))
        Message->ReturnCode = OS2_ERROR_INVALID_PARAMETER;
    return STATUS_SUCCESS;
}


static NTSTATUS
Os2SrvDosGetDateTime(
    _In_ POS2_PROCESS Process,
    _Inout_ POS2_API_MESSAGE Message)
{
    LARGE_INTEGER SystemTime, LocalTime;
    TIME_FIELDS Fields;
    POS2_DATETIME_REPLY Reply = &Message->Data.DateTimeReply;
    NTSTATUS Status;
    (void)Process;

    memset(Reply, 0, sizeof(*Reply));
    Status = NtQuerySystemTime(&SystemTime);
    if (!NT_SUCCESS(Status))
    {
        Message->HostStatus = Status;
        Message->ReturnCode = OS2_ERROR_GEN_FAILURE;
        return STATUS_SUCCESS;
    }
    Status = RtlSystemTimeToLocalTime(&SystemTime, &LocalTime);
    if (!NT_SUCCESS(Status))
    {
        Message->HostStatus = Status;
        Message->ReturnCode = OS2_ERROR_GEN_FAILURE;
        return STATUS_SUCCESS;
    }
    RtlTimeToTimeFields(&LocalTime, &Fields);
    Reply->Hours = (UCHAR)Fields.Hour;
    Reply->Minutes = (UCHAR)Fields.Minute;
    Reply->Seconds = (UCHAR)Fields.Second;
    Reply->Hundredths = (UCHAR)(Fields.Milliseconds / 10);
    Reply->Day = (UCHAR)Fields.Day;
    Reply->Month = (UCHAR)Fields.Month;
    Reply->Year = (USHORT)Fields.Year;
    /* Timezone refinement is deferred; date/time fields are native-local. */
    Reply->Timezone = 0;
    Reply->Weekday = (UCHAR)Fields.Weekday;
    Reply->Reserved = 0;
    Message->HostStatus = STATUS_SUCCESS;
    Message->ReturnCode = OS2_NO_ERROR;
    DbgPrint("OS2SS: LE4IO DosGetDateTime rc=0 %04u-%02u-%02u %02u:%02u:%02u.%02u\n",
             (ULONG)Reply->Year, (ULONG)Reply->Month, (ULONG)Reply->Day,
             (ULONG)Reply->Hours, (ULONG)Reply->Minutes, (ULONG)Reply->Seconds,
             (ULONG)Reply->Hundredths);
    return STATUS_SUCCESS;
}

static NTSTATUS
Os2SrvDosRead(
    _In_ POS2_PROCESS Process,
    _Inout_ POS2_API_MESSAGE Message)
{
    ULONG FileHandle = Message->Data.DosReadRequest.FileHandle;
    ULONG Offset = Message->Data.DosReadRequest.SharedOffset;
    ULONG Length = Message->Data.DosReadRequest.Length;
    ULONG Actual = 0;
    NTSTATUS Status;

    Message->HostStatus = STATUS_SUCCESS;
    Message->ReturnCode = OS2_NO_ERROR;
    Message->Data.DosReadReply.BytesRead = 0;
    if (Process == NULL || Process->SharedServerBase == NULL ||
        Process->SharedDataSize != OS2_SHARED_SECTION_SIZE)
    {
        Message->ReturnCode = OS2_ERROR_GEN_FAILURE;
        return STATUS_SUCCESS;
    }
    if (FileHandle != 0u)
    {
        Message->ReturnCode = OS2_ERROR_INVALID_HANDLE;
        return STATUS_SUCCESS;
    }
    if (Length > OS2_R5_MAX_WRITE || Offset > Process->SharedDataSize ||
        Length > Process->SharedDataSize - Offset)
    {
        Message->ReturnCode = OS2_ERROR_INVALID_PARAMETER;
        return STATUS_SUCCESS;
    }
    if (Length == 0u) return STATUS_SUCCESS;

    {
        ULONG Ask = Length;
        if (Ask > LE4C_CONSOLE_MAX_BYTES) Ask = LE4C_CONSOLE_MAX_BYTES;
        Status = Os2Le4cConsoleRead((PUCHAR)Process->SharedServerBase + Offset,
                                    Ask,
                                    &Actual);
    }
    if (!NT_SUCCESS(Status))
    {
        Message->HostStatus = Status;
        Message->ReturnCode = OS2_ERROR_GEN_FAILURE;
        return STATUS_SUCCESS;
    }
    Message->Data.DosReadReply.BytesRead = Actual;
    DbgPrint("OS2SS: LE4IO DosRead stdin rc=0 actual=%lu\n", Actual);
    return STATUS_SUCCESS;
}

POS2_API_HANDLER g_Os2ApiHandlers[Os2ApiMax] =
{
    Os2HandleR3Ping,
    Os2SrvDosQuerySysInfo,
    Os2SrvDosWrite,
    Os2SrvDosQueryHType,
    Os2SrvDosExit,
    Os2SrvDosSetFilePtr,
    Os2SrvDosAllocMem,
    Os2SrvDosFreeMem,
    Os2SrvDosSetMem,
    Os2SrvDosGetDateTime,
    Os2SrvDosRead
};

C_ASSERT((sizeof(g_Os2ApiHandlers) / sizeof(g_Os2ApiHandlers[0])) == Os2ApiMax);

NTSTATUS
Os2DispatchApi(
    _In_ POS2_PROCESS Process,
    _Inout_ POS2_API_MESSAGE Message)
{
    POS2_API_HANDLER Handler;

    if (Process == NULL)
    {
        Message->ReturnCode = 0;
        Message->HostStatus = STATUS_INVALID_CID;
        return Message->HostStatus;
    }

    if (Message->Header.u1.s1.TotalLength != (CSHORT)sizeof(*Message) ||
        Message->Header.u1.s1.DataLength !=
            (CSHORT)(sizeof(*Message) - sizeof(Message->Header)))
    {
        DbgPrint("OS2SS: LE4C invalid message length total=%u data=%u\n",
                 (ULONG)(USHORT)Message->Header.u1.s1.TotalLength,
                 (ULONG)(USHORT)Message->Header.u1.s1.DataLength);
        Message->ReturnCode = 0;
        Message->HostStatus = STATUS_INVALID_PARAMETER;
        return Message->HostStatus;
    }

    if (Message->ApiNumber >= (ULONG)Os2ApiMax)
    {
        DbgPrint("OS2SS: LE4C API %lu not implemented\n", Message->ApiNumber);
        Message->ReturnCode = 0;
        Message->HostStatus = STATUS_NOT_IMPLEMENTED;
        return Message->HostStatus;
    }

    Handler = g_Os2ApiHandlers[Message->ApiNumber];
    if (Handler == NULL)
    {
        Message->ReturnCode = 0;
        Message->HostStatus = STATUS_NOT_IMPLEMENTED;
        return Message->HostStatus;
    }

    return Handler(Process, Message);
}
