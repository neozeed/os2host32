#include "os2ss.h"
#include <ndk/exfuncs.h>

static HANDLE gSbListenPort;
static HANDLE gSbCommPort;
static HANDLE gSmApiPort;
static HANDLE gSbThread;
static UNICODE_STRING gSbPortName;

static HANDLE gObjectDirectory;
static UNICODE_STRING gObjectDirectoryName;

static HANDLE gApiListenPort;
static HANDLE gApiThread;
static UNICODE_STRING gApiPortName;

static HANDLE gConsoleListenPort;
static HANDLE gConsoleCommPort;
static HANDLE gConsoleThread;
static HANDLE gConsoleAckEvent;
static UNICODE_STRING gConsolePortName;
static RTL_CRITICAL_SECTION gConsoleLock;
static BOOLEAN gConsoleWaitPending;
static LE4C_CONSOLE_MESSAGE gConsoleWaitMessage;
static BOOLEAN gConsoleActionQueued;
static ULONG gConsoleActionOperation;
static ULONG gConsoleActionSequence;
static ULONG gConsoleActionFileHandle;
static ULONG gConsoleActionLength;
static UCHAR gConsoleActionData[LE4C_CONSOLE_MAX_BYTES];
static ULONG gConsoleNextSequence = 1;
static BOOLEAN gConsoleAwaitingAck;
static ULONG gConsoleAwaitSequence;
static NTSTATUS gConsoleAckStatus;
static ULONG gConsoleAckBytes;
static ULONG gConsoleAckDataLength;
static UCHAR gConsoleAckData[LE4C_CONSOLE_MAX_BYTES];

static VOID
Le4cFillConsoleActionReply(
    _Inout_ PLE4C_CONSOLE_MESSAGE Message,
    _In_ ULONG Operation,
    _In_ ULONG Sequence,
    _In_ ULONG FileHandle,
    _In_reads_bytes_opt_(Length) const UCHAR *Bytes,
    _In_ ULONG Length)
{
    ULONG Index;
    Message->Version = LE4C_CONSOLE_VERSION;
    Message->Operation = Operation;
    Message->Sequence = Sequence;
    Message->Status = STATUS_SUCCESS;
    Message->FileHandle = FileHandle;
    Message->Length = Length;
    Message->BytesWritten = 0;
    memset(Message->Data, 0, sizeof(Message->Data));
    for (Index = 0; Index < Length && Index < LE4C_CONSOLE_MAX_BYTES; ++Index)
        Message->Data[Index] = Bytes != NULL ? Bytes[Index] : 0;
}

static NTSTATUS
Le4cDispatchConsoleAction(
    _In_ ULONG Operation,
    _In_ ULONG FileHandle,
    _In_reads_bytes_opt_(Length) const UCHAR *Bytes,
    _In_ ULONG Length,
    _Out_opt_ PULONG BytesWritten)
{
    NTSTATUS Status = STATUS_SUCCESS;
    LE4C_CONSOLE_MESSAGE Reply;
    HANDLE CommPort = NULL;
    BOOLEAN ReplyPending = FALSE;
    ULONG Sequence;

    if (Length > LE4C_CONSOLE_MAX_BYTES)
        return STATUS_BUFFER_OVERFLOW;

    if (BytesWritten != NULL)
        *BytesWritten = 0;

    (void)NtResetEvent(gConsoleAckEvent, NULL);
    (void)RtlEnterCriticalSection(&gConsoleLock);
    if (gConsoleCommPort == NULL || gConsoleActionQueued || gConsoleAwaitingAck)
    {
        (void)RtlLeaveCriticalSection(&gConsoleLock);
        return STATUS_PORT_DISCONNECTED;
    }

    Sequence = gConsoleNextSequence++;
    if (gConsoleNextSequence == 0)
        gConsoleNextSequence = 1;
    gConsoleAwaitingAck = TRUE;
    gConsoleAwaitSequence = Sequence;
    gConsoleAckStatus = STATUS_UNSUCCESSFUL;
    gConsoleAckBytes = 0;
    gConsoleAckDataLength = 0;
    memset(gConsoleAckData, 0, sizeof(gConsoleAckData));

    if (gConsoleWaitPending)
    {
        Reply = gConsoleWaitMessage;
        Le4cFillConsoleActionReply(&Reply,
                                   Operation,
                                   Sequence,
                                   FileHandle,
                                   Bytes,
                                   Length);
        gConsoleWaitPending = FALSE;
        ReplyPending = TRUE;
        CommPort = gConsoleCommPort;
    }
    else
    {
        ULONG Index;
        gConsoleActionQueued = TRUE;
        gConsoleActionOperation = Operation;
        gConsoleActionSequence = Sequence;
        gConsoleActionFileHandle = FileHandle;
        gConsoleActionLength = Length;
        memset(gConsoleActionData, 0, sizeof(gConsoleActionData));
        for (Index = 0; Index < Length; ++Index)
            gConsoleActionData[Index] = Bytes != NULL ? Bytes[Index] : 0;
    }
    (void)RtlLeaveCriticalSection(&gConsoleLock);

    if (ReplyPending)
    {
        Status = NtReplyPort(CommPort, &Reply.Header);
        if (!NT_SUCCESS(Status))
        {
            (void)RtlEnterCriticalSection(&gConsoleLock);
            gConsoleAwaitingAck = FALSE;
            (void)RtlLeaveCriticalSection(&gConsoleLock);
            return Status;
        }
    }

    Status = NtWaitForSingleObject(gConsoleAckEvent, FALSE, NULL);
    if (!NT_SUCCESS(Status))
        return Status;

    (void)RtlEnterCriticalSection(&gConsoleLock);
    Status = gConsoleAckStatus;
    if (BytesWritten != NULL)
        *BytesWritten = gConsoleAckBytes;
    gConsoleAwaitingAck = FALSE;
    (void)RtlLeaveCriticalSection(&gConsoleLock);
    return Status;
}

NTSTATUS
Os2Le4cConsoleWrite(
    _In_ ULONG FileHandle,
    _In_reads_bytes_(Length) const UCHAR *Bytes,
    _In_ ULONG Length,
    _Out_ PULONG BytesWritten)
{
    ULONG Total = 0;
    NTSTATUS Status = STATUS_SUCCESS;

    if (BytesWritten != NULL)
        *BytesWritten = 0;

    /*
     * TEMPORARY NATIVE CONSOLE BACKEND transport only.  ReactOS x86 LPC has
     * a 256-byte maximum message length, while the frozen R5 DosWrite shared
     * channel permits up to 32 KiB.  Stream one semantic DosWrite as a series
     * of acknowledged console actions rather than imposing an artificial
     * application-visible write-size limit.
     */
    while (Total < Length)
    {
        ULONG Chunk = Length - Total;
        ULONG ChunkWritten = 0;
        if (Chunk > LE4C_CONSOLE_MAX_BYTES)
            Chunk = LE4C_CONSOLE_MAX_BYTES;

        Status = Le4cDispatchConsoleAction(Le4cConsoleWrite,
                                           FileHandle,
                                           Bytes + Total,
                                           Chunk,
                                           &ChunkWritten);
        Total += ChunkWritten;
        if (!NT_SUCCESS(Status) || ChunkWritten != Chunk)
            break;
    }

    if (BytesWritten != NULL)
        *BytesWritten = Total;
    return Status;
}


NTSTATUS
Os2Le4cConsoleRead(
    _Out_writes_bytes_to_(Capacity, *BytesRead) UCHAR *Bytes,
    _In_ ULONG Capacity,
    _Out_ PULONG BytesRead)
{
    NTSTATUS Status;
    ULONG Actual = 0;
    ULONG Index;

    if (BytesRead != NULL) *BytesRead = 0;
    if (Bytes == NULL || BytesRead == NULL || Capacity == 0)
        return STATUS_INVALID_PARAMETER;
    if (Capacity > LE4C_CONSOLE_MAX_BYTES)
        Capacity = LE4C_CONSOLE_MAX_BYTES;

    Status = Le4cDispatchConsoleAction(Le4cConsoleRead, 0u, NULL, Capacity, &Actual);
    if (!NT_SUCCESS(Status)) return Status;

    (void)RtlEnterCriticalSection(&gConsoleLock);
    if (Actual > gConsoleAckDataLength) Actual = gConsoleAckDataLength;
    if (Actual > Capacity) Actual = Capacity;
    for (Index = 0; Index < Actual; ++Index) Bytes[Index] = gConsoleAckData[Index];
    (void)RtlLeaveCriticalSection(&gConsoleLock);
    *BytesRead = Actual;
    return STATUS_SUCCESS;
}

NTSTATUS
Os2Le4cConsoleNotifyExit(
    _In_ ULONG Result)
{
    ULONG Ignored = 0;
    return Le4cDispatchConsoleAction(Le4cConsoleExit,
                                     Result,
                                     NULL,
                                     0,
                                     &Ignored);
}

static ULONG NTAPI
ConsoleListener(PVOID Parameter)
{
    NTSTATUS Status;
    LE4C_CONSOLE_MESSAGE ReceiveMsg;
    LE4C_CONSOLE_MESSAGE ReplyMsg;
    BOOLEAN HaveReply = FALSE;
    PVOID PortContext = NULL;
    ULONG MessageType;
    (void)Parameter;

    memset(&ReplyMsg, 0, sizeof(ReplyMsg));
    for (;;)
    {
        memset(&ReceiveMsg, 0, sizeof(ReceiveMsg));
        Status = NtReplyWaitReceivePort(gConsoleListenPort,
                                        &PortContext,
                                        HaveReply ? &ReplyMsg.Header : NULL,
                                        &ReceiveMsg.Header);
        HaveReply = FALSE;
        if (!NT_SUCCESS(Status))
        {
            DbgPrint("OS2SS: LE4C console NtReplyWaitReceivePort failed %08lx\n", (ULONG)Status);
            continue;
        }

        MessageType = (ULONG)ReceiveMsg.Header.u2.s2.Type;
        if (MessageType == LPC_CONNECTION_REQUEST)
        {
            HANDLE CommPort = NULL;
            BOOLEAN Accept = FALSE;
            ULONG DataLength = (ULONG)(USHORT)ReceiveMsg.Header.u1.s1.DataLength;
            PLE4C_CONSOLE_CONNECT_INFO Info =
                (PLE4C_CONSOLE_CONNECT_INFO)((PUCHAR)&ReceiveMsg + sizeof(PORT_MESSAGE));

            if (DataLength >= sizeof(*Info) &&
                Info->Version == LE4C_CONSOLE_VERSION &&
                Info->Role == LE4C_CONSOLE_ROLE_LAUNCHER)
            {
                (void)RtlEnterCriticalSection(&gConsoleLock);
                Accept = (gConsoleCommPort == NULL);
                (void)RtlLeaveCriticalSection(&gConsoleLock);
            }

            Status = NtAcceptConnectPort(&CommPort,
                                         NULL,
                                         &ReceiveMsg.Header,
                                         Accept,
                                         NULL,
                                         NULL);
            if (NT_SUCCESS(Status) && Accept)
                Status = NtCompleteConnectPort(CommPort);

            if (NT_SUCCESS(Status) && Accept)
            {
                (void)RtlEnterCriticalSection(&gConsoleLock);
                gConsoleCommPort = CommPort;
                (void)RtlLeaveCriticalSection(&gConsoleLock);
                DbgPrint("OS2SS: LE4C TEMPORARY NATIVE CONSOLE BACKEND launcher connected\n");
            }
            else if (CommPort != NULL)
            {
                NtClose(CommPort);
            }
            continue;
        }

        if (MessageType == LPC_PORT_CLOSED || MessageType == LPC_CLIENT_DIED)
        {
            (void)RtlEnterCriticalSection(&gConsoleLock);
            if (gConsoleCommPort != NULL)
            {
                NtClose(gConsoleCommPort);
                gConsoleCommPort = NULL;
            }
            gConsoleWaitPending = FALSE;
            gConsoleActionQueued = FALSE;
            if (gConsoleAwaitingAck)
            {
                gConsoleAckStatus = STATUS_PORT_DISCONNECTED;
                gConsoleAckBytes = 0;
                gConsoleAckDataLength = 0;
                (void)NtSetEvent(gConsoleAckEvent, NULL);
            }
            (void)RtlLeaveCriticalSection(&gConsoleLock);
            DbgPrint("OS2SS: LE4C console launcher closed/died type=%lu\n", MessageType);
            continue;
        }

        if (ReceiveMsg.Version != LE4C_CONSOLE_VERSION ||
            ReceiveMsg.Header.u1.s1.TotalLength != (CSHORT)sizeof(ReceiveMsg))
        {
            ReplyMsg = ReceiveMsg;
            ReplyMsg.Status = STATUS_INVALID_PARAMETER;
            HaveReply = TRUE;
            continue;
        }

        if (ReceiveMsg.Operation == Le4cConsoleWait)
        {
            (void)RtlEnterCriticalSection(&gConsoleLock);
            if (gConsoleActionQueued)
            {
                ReplyMsg = ReceiveMsg;
                Le4cFillConsoleActionReply(&ReplyMsg,
                                           gConsoleActionOperation,
                                           gConsoleActionSequence,
                                           gConsoleActionFileHandle,
                                           gConsoleActionData,
                                           gConsoleActionLength);
                gConsoleActionQueued = FALSE;
                HaveReply = TRUE;
            }
            else if (!gConsoleWaitPending)
            {
                gConsoleWaitMessage = ReceiveMsg;
                gConsoleWaitPending = TRUE;
            }
            else
            {
                ReplyMsg = ReceiveMsg;
                ReplyMsg.Status = STATUS_DEVICE_BUSY;
                HaveReply = TRUE;
            }
            (void)RtlLeaveCriticalSection(&gConsoleLock);
            continue;
        }

        if (ReceiveMsg.Operation == Le4cConsoleAck)
        {
            (void)RtlEnterCriticalSection(&gConsoleLock);
            if (gConsoleAwaitingAck && ReceiveMsg.Sequence == gConsoleAwaitSequence)
            {
                ULONG CopyLength = ReceiveMsg.Length;
                ULONG Index;
                if (CopyLength > LE4C_CONSOLE_MAX_BYTES) CopyLength = LE4C_CONSOLE_MAX_BYTES;
                gConsoleAckStatus = ReceiveMsg.Status;
                gConsoleAckBytes = ReceiveMsg.BytesWritten;
                gConsoleAckDataLength = CopyLength;
                memset(gConsoleAckData, 0, sizeof(gConsoleAckData));
                for (Index = 0; Index < CopyLength; ++Index)
                    gConsoleAckData[Index] = ReceiveMsg.Data[Index];
                (void)NtSetEvent(gConsoleAckEvent, NULL);
            }
            ReplyMsg = ReceiveMsg;
            ReplyMsg.Operation = Le4cConsoleAckReply;
            ReplyMsg.Status = STATUS_SUCCESS;
            HaveReply = TRUE;
            (void)RtlLeaveCriticalSection(&gConsoleLock);
            continue;
        }

        ReplyMsg = ReceiveMsg;
        ReplyMsg.Status = STATUS_NOT_IMPLEMENTED;
        HaveReply = TRUE;
    }
}


static NTSTATUS
BuildR5ObjectDirectorySecurity(PSECURITY_DESCRIPTOR Sd, PACL Acl, ULONG AclBytes)
{
    NTSTATUS Status;
    SID SystemSid = { SID_REVISION, 1, {{0,0,0,0,0,5}}, { SECURITY_LOCAL_SYSTEM_RID } };
    SID WorldSid = { SID_REVISION, 1, {{0,0,0,0,0,1}}, { SECURITY_WORLD_RID } };

    Status = RtlCreateSecurityDescriptor(Sd, SECURITY_DESCRIPTOR_REVISION);
    if (!NT_SUCCESS(Status)) return Status;
    Status = RtlCreateAcl(Acl, AclBytes, ACL_REVISION);
    if (!NT_SUCCESS(Status)) return Status;

    /*
     * The parent object is a Directory object, so its DACL must contain
     * DIRECTORY_* rights rather than PORT_* rights.  OS2SS (LocalSystem)
     * needs create-object access for SbPort/ApiPort; ordinary clients need
     * only traversal to reach the separately secured ApiPort child.
     */
    Status = RtlAddAccessAllowedAce(Acl, ACL_REVISION, DIRECTORY_ALL_ACCESS, &SystemSid);
    if (!NT_SUCCESS(Status)) return Status;
    Status = RtlAddAccessAllowedAce(Acl, ACL_REVISION, DIRECTORY_TRAVERSE, &WorldSid);
    if (!NT_SUCCESS(Status)) return Status;
    return RtlSetDaclSecurityDescriptor(Sd, TRUE, Acl, FALSE);
}

static NTSTATUS
BuildLocalSystemPortSecurity(PSECURITY_DESCRIPTOR Sd, PACL Acl, ULONG AclBytes)
{
    NTSTATUS Status;
    SID SystemSid = { SID_REVISION, 1, {{0,0,0,0,0,5}}, { SECURITY_LOCAL_SYSTEM_RID } };

    Status = RtlCreateSecurityDescriptor(Sd, SECURITY_DESCRIPTOR_REVISION);
    if (!NT_SUCCESS(Status)) return Status;
    Status = RtlCreateAcl(Acl, AclBytes, ACL_REVISION);
    if (!NT_SUCCESS(Status)) return Status;
    Status = RtlAddAccessAllowedAce(Acl, ACL_REVISION, PORT_ALL_ACCESS, &SystemSid);
    if (!NT_SUCCESS(Status)) return Status;
    return RtlSetDaclSecurityDescriptor(Sd, TRUE, Acl, FALSE);
}

/*
 * R5 preserves the corrected R4 proof-only application-port ACL.  Security policy and
 * client identity validation are explicitly deferred; this is not a proposed
 * production OS/2 personality-server ACL.
 */
static NTSTATUS
BuildR5ApplicationPortSecurity(PSECURITY_DESCRIPTOR Sd, PACL Acl, ULONG AclBytes)
{
    NTSTATUS Status;
    SID WorldSid = { SID_REVISION, 1, {{0,0,0,0,0,1}}, { SECURITY_WORLD_RID } };

    Status = RtlCreateSecurityDescriptor(Sd, SECURITY_DESCRIPTOR_REVISION);
    if (!NT_SUCCESS(Status)) return Status;
    Status = RtlCreateAcl(Acl, AclBytes, ACL_REVISION);
    if (!NT_SUCCESS(Status)) return Status;
    Status = RtlAddAccessAllowedAce(Acl, ACL_REVISION, PORT_ALL_ACCESS, &WorldSid);
    if (!NT_SUCCESS(Status)) return Status;
    return RtlSetDaclSecurityDescriptor(Sd, TRUE, Acl, FALSE);
}

static VOID
LogCreateSession(PSB_API_MSG Msg)
{
    PSB_CREATE_SESSION_MSG Cs = &Msg->u.CreateSession;
    RTL_USER_PROCESS_INFORMATION *Pi = &Cs->ProcessInfo;
    POS2_PROCESS Process;
    UCHAR NameBuffer[1024];
    ULONG ReturnLength = 0;
    NTSTATUS Status;

    DbgPrint("OS2SS: received SbpCreateSession\n");
    DbgPrint("OS2SS: SessionId=%lu PID=%p TID=%p\n",
             Cs->SessionId,
             Pi->ClientId.UniqueProcess,
             Pi->ClientId.UniqueThread);
    DbgPrint("OS2SS: ProcessHandle=%p ThreadHandle=%p\n",
             Pi->ProcessHandle,
             Pi->ThreadHandle);
    DbgPrint("OS2SS: subsystem=%lu TransferAddress=%p\n",
             Pi->ImageInformation.SubSystemType,
             Pi->ImageInformation.TransferAddress);

    Status = NtQueryInformationProcess(Pi->ProcessHandle,
                                       ProcessImageFileName,
                                       NameBuffer,
                                       sizeof(NameBuffer),
                                       &ReturnLength);
    if (NT_SUCCESS(Status))
    {
        PUNICODE_STRING ImageName = (PUNICODE_STRING)NameBuffer;
        DbgPrint("OS2SS: image=%wZ\n", ImageName);
    }
    else
    {
        DbgPrint("OS2SS: image query failed status=%08lx\n", (ULONG)Status);
    }

    if (Pi->ImageInformation.SubSystemType != IMAGE_SUBSYSTEM_OS2_CUI)
    {
        DbgPrint("OS2SS: ERROR expected subsystem=5, got %lu\n",
                 Pi->ImageInformation.SubSystemType);
        Msg->ReturnValue = STATUS_INVALID_PARAMETER;
        return;
    }

    Process = Os2FindProcessByClientId(&Pi->ClientId);
    if (Process == NULL)
    {
        Process = Os2CreateProcessRecord(Pi->ProcessHandle,
                                         &Pi->ClientId,
                                         Cs->SessionId);
        if (Process == NULL)
        {
            DbgPrint("OS2SS: LE4C process-record allocation failed\n");
            Msg->ReturnValue = STATUS_NO_MEMORY;
            return;
        }

        DbgPrint("OS2SS: LE4C process record created personality_pid=%lu\n",
                 Process->PersonalityProcessId);
    }

    {
        ULONG PreviousSuspendCount = 0;

        DbgPrint("OS2SS: LE4C resuming type-5 initial thread\n");
        Status = NtResumeThread(Pi->ThreadHandle, &PreviousSuspendCount);
        DbgPrint("OS2SS: NtResumeThread returned %08lx\n", (ULONG)Status);
        DbgPrint("OS2SS: previous suspend count=%lu\n", PreviousSuspendCount);

        Msg->ReturnValue = NT_SUCCESS(Status) ? STATUS_SUCCESS : Status;
    }
}

static ULONG NTAPI
SbListener(PVOID Parameter)
{
    NTSTATUS Status;
    SB_API_MSG ReceiveMsg;
    SB_API_MSG ReplyMsg;
    BOOLEAN HaveReply = FALSE;
    PVOID PortContext = NULL;
    ULONG MessageType;
    (void)Parameter;

    memset(&ReplyMsg, 0, sizeof(ReplyMsg));

    for (;;)
    {
        memset(&ReceiveMsg, 0, sizeof(ReceiveMsg));
        Status = NtReplyWaitReceivePort(gSbListenPort,
                                        &PortContext,
                                        HaveReply ? &ReplyMsg.h : NULL,
                                        &ReceiveMsg.h);
        HaveReply = FALSE;

        if (!NT_SUCCESS(Status))
        {
            DbgPrint("OS2SS: NtReplyWaitReceivePort(SB) failed %08lx\n", (ULONG)Status);
            continue;
        }

        MessageType = (ULONG)ReceiveMsg.h.u2.s2.Type;
        if (MessageType == LPC_CONNECTION_REQUEST)
        {
            REMOTE_PORT_VIEW RemoteView;
            HANDLE CommPort = NULL;
            memset(&RemoteView, 0, sizeof(RemoteView));
            RemoteView.Length = sizeof(RemoteView);

            Status = NtAcceptConnectPort(&CommPort,
                                         NULL,
                                         &ReceiveMsg.h,
                                         TRUE,
                                         NULL,
                                         &RemoteView);
            if (NT_SUCCESS(Status))
                Status = NtCompleteConnectPort(CommPort);

            if (NT_SUCCESS(Status))
            {
                gSbCommPort = CommPort;
                DbgPrint("OS2SS: SMSS callback connection accepted\n");
            }
            else
            {
                DbgPrint("OS2SS: callback connection failed %08lx\n", (ULONG)Status);
                if (CommPort) NtClose(CommPort);
            }
            continue;
        }

        if (MessageType == LPC_PORT_CLOSED || MessageType == LPC_CLIENT_DIED)
        {
            DbgPrint("OS2SS: SB peer closed/died type=%lu\n", MessageType);
            continue;
        }

        if ((ULONG)ReceiveMsg.ApiNumber >= (ULONG)SbpMaxApiNumber)
        {
            DbgPrint("OS2SS: invalid SB API %lu\n", (ULONG)ReceiveMsg.ApiNumber);
            ReceiveMsg.ApiNumber = SbpMaxApiNumber;
            ReceiveMsg.ReturnValue = STATUS_NOT_IMPLEMENTED;
        }
        else if (ReceiveMsg.ApiNumber == SbpCreateSession)
        {
            LogCreateSession(&ReceiveMsg);
        }
        else
        {
            DbgPrint("OS2SS: unimplemented SB API %lu\n", (ULONG)ReceiveMsg.ApiNumber);
            ReceiveMsg.ReturnValue = STATUS_NOT_IMPLEMENTED;
        }

        memcpy(&ReplyMsg, &ReceiveMsg, sizeof(ReplyMsg));
        HaveReply = TRUE;
    }
}

static POS2_CONNECT_INFO
ValidateR5ConnectionInfo(POS2_API_MESSAGE Message)
{
    POS2_CONNECT_INFO ConnectInfo;
    ULONG DataLength = (ULONG)(USHORT)Message->Header.u1.s1.DataLength;

    if (DataLength < sizeof(OS2_CONNECT_INFO))
    {
        DbgPrint("OS2SS: LE4C API connection info too short: %lu\n", DataLength);
        return NULL;
    }

    ConnectInfo = (POS2_CONNECT_INFO)((PUCHAR)Message + sizeof(PORT_MESSAGE));
    if (ConnectInfo->ConnectionType != (ULONG)Os2ConnectionProcess ||
        ConnectInfo->Version != OS2_PROTOCOL_VERSION)
    {
        DbgPrint("OS2SS: LE4C API connection protocol mismatch type=%lu version=%lu\n",
                 ConnectInfo->ConnectionType,
                 ConnectInfo->Version);
        return NULL;
    }

    return ConnectInfo;
}

static VOID
UnbindR5ApiPort(_In_opt_ POS2_PROCESS Process)
{
    if (Process == NULL)
        return;

    if (Process->ApiPort != NULL)
    {
        /* Closing the LPC communication port releases its mapped port view. */
        NtClose(Process->ApiPort);
        Process->ApiPort = NULL;
    }

    Process->SharedServerBase = NULL;
    Process->SharedDataSize = 0;

    if (Process->State != Os2ProcessStateClosing)
        Process->State = Os2ProcessStateCreated;

    DbgPrint("OS2SS: LE4C API client unbound personality_pid=%lu\n",
             Process->PersonalityProcessId);
}

static ULONG NTAPI
ApiListener(PVOID Parameter)
{
    NTSTATUS Status;
    OS2_API_MESSAGE ReceiveMsg;
    OS2_API_MESSAGE ReplyMsg;
    BOOLEAN HaveReply = FALSE;
    PVOID PortContext = NULL;
    ULONG MessageType;
    (void)Parameter;

    memset(&ReplyMsg, 0, sizeof(ReplyMsg));

    for (;;)
    {
        memset(&ReceiveMsg, 0, sizeof(ReceiveMsg));
        PortContext = NULL;
        Status = NtReplyWaitReceivePort(gApiListenPort,
                                        &PortContext,
                                        HaveReply ? &ReplyMsg.Header : NULL,
                                        &ReceiveMsg.Header);
        HaveReply = FALSE;

        if (!NT_SUCCESS(Status))
        {
            DbgPrint("OS2SS: LE4C API receive failed %08lx\n", (ULONG)Status);
            continue;
        }

        MessageType = (ULONG)ReceiveMsg.Header.u2.s2.Type;
        if (MessageType == LPC_CONNECTION_REQUEST)
        {
            REMOTE_PORT_VIEW RemoteView;
            HANDLE CommPort = NULL;
            POS2_CONNECT_INFO ConnectInfo;
            POS2_PROCESS Process = NULL;
            BOOLEAN Accept = FALSE;

            DbgPrint("OS2SS: LE4C API connection request PID=%p TID=%p\n",
                     ReceiveMsg.Header.ClientId.UniqueProcess,
                     ReceiveMsg.Header.ClientId.UniqueThread);

            ConnectInfo = ValidateR5ConnectionInfo(&ReceiveMsg);
            if (ConnectInfo != NULL)
            {
                /* Kernel-supplied LPC PID is authoritative; client data is not identity. */
                Process = Os2FindProcessByProcessId(
                    ReceiveMsg.Header.ClientId.UniqueProcess);
                if (Process == NULL)
                {
                    DbgPrint("OS2SS: LE4C API connection rejected: unknown process\n");
                }
                else if (Process->ApiPort != NULL)
                {
                    DbgPrint("OS2SS: LE4C API connection rejected: process already bound\n");
                    Process = NULL;
                }
                else if ((ULONG)ReceiveMsg.Header.ClientViewSize !=
                         OS2_SHARED_SECTION_SIZE)
                {
                    DbgPrint("OS2SS: LE4C API connection rejected: shared view size=%lu\n",
                             (ULONG)ReceiveMsg.Header.ClientViewSize);
                    Process = NULL;
                }
                else
                {
                    ConnectInfo->PersonalityProcessId =
                        Process->PersonalityProcessId;
                    Accept = TRUE;
                    DbgPrint("OS2SS: LE4C matched personality_pid=%lu\n",
                             Process->PersonalityProcessId);
                }
            }

            memset(&RemoteView, 0, sizeof(RemoteView));
            RemoteView.Length = sizeof(RemoteView);

            Status = NtAcceptConnectPort(&CommPort,
                                         Process,
                                         &ReceiveMsg.Header,
                                         Accept,
                                         NULL,
                                         &RemoteView);
            if (NT_SUCCESS(Status) && Accept)
                Status = NtCompleteConnectPort(CommPort);

            if (NT_SUCCESS(Status) && Accept)
            {
                if (RemoteView.ViewBase == NULL ||
                    (ULONG)RemoteView.ViewSize != OS2_SHARED_SECTION_SIZE)
                {
                    DbgPrint("OS2SS: LE4C shared view mapping invalid base=%p size=%lu\n",
                             RemoteView.ViewBase, (ULONG)RemoteView.ViewSize);
                    NtClose(CommPort);
                    CommPort = NULL;
                    Status = STATUS_INVALID_VIEW_SIZE;
                }
                else
                {
                    Process->ApiPort = CommPort;
                    Process->SharedServerBase = RemoteView.ViewBase;
                    Process->SharedDataSize = (ULONG)RemoteView.ViewSize;
                    Process->State = Os2ProcessStateApiConnected;
                    DbgPrint("OS2SS: LE4C API client bound personality_pid=%lu\n",
                             Process->PersonalityProcessId);
                    DbgPrint("OS2SS: LE4C shared section established personality_pid=%lu size=%lu\n",
                             Process->PersonalityProcessId,
                             Process->SharedDataSize);
                }
            }
            else
            {
                if (Accept)
                {
                    DbgPrint("OS2SS: LE4C API connection failed %08lx\n",
                             (ULONG)Status);
                }
                if (CommPort != NULL)
                    NtClose(CommPort);
            }
            continue;
        }

        if (MessageType == LPC_PORT_CLOSED || MessageType == LPC_CLIENT_DIED)
        {
            DbgPrint("OS2SS: LE4C API client closed/died type=%lu\n", MessageType);
            UnbindR5ApiPort((POS2_PROCESS)PortContext);
            continue;
        }

        if (PortContext == NULL)
        {
            DbgPrint("OS2SS: LE4C API request has no process context\n");
            ReceiveMsg.ReturnCode = 0;
            ReceiveMsg.HostStatus = STATUS_INVALID_CID;
        }
        else
        {
            (void)Os2DispatchApi((POS2_PROCESS)PortContext, &ReceiveMsg);
        }
        memcpy(&ReplyMsg, &ReceiveMsg, sizeof(ReplyMsg));
        HaveReply = TRUE;
    }
}

void CDECL entry(void)
{
    NTSTATUS Status;
    OBJECT_ATTRIBUTES Oa;
    SECURITY_DESCRIPTOR DirectorySd, SbSd, ApiSd, ConsoleSd;
    UCHAR DirectoryAclBuffer[128];
    UCHAR SbAclBuffer[128];
    UCHAR ApiAclBuffer[128];
    UCHAR ConsoleAclBuffer[128];
    CLIENT_ID ListenerCid;

    DbgPrint("OS2SS: LE4C starting\n");

    Status = Os2InitializeProcessTable();
    if (!NT_SUCCESS(Status))
    {
        DbgPrint("OS2SS: LE4C process table initialization failed %08lx\n", (ULONG)Status);
        NtTerminateProcess(NtCurrentProcess(), Status);
    }

    /*
     * R5 preserves the corrected R4 OS2SS object directory.  Keep its handle open for
     * the server lifetime.  Directory security is intentionally distinct
     * from port security: LocalSystem may create child objects, while WORLD
     * receives traverse-only access so clients can reach the separately
     * secured ApiPort.
     */
    RtlInitUnicodeString(&gObjectDirectoryName, OS2_OBJECT_DIRECTORY_NAME);
    Status = BuildR5ObjectDirectorySecurity(&DirectorySd,
                                             (PACL)DirectoryAclBuffer,
                                             sizeof(DirectoryAclBuffer));
    if (!NT_SUCCESS(Status))
    {
        DbgPrint("OS2SS: LE4C object directory security failed %08lx\n",
                 (ULONG)Status);
        NtTerminateProcess(NtCurrentProcess(), Status);
    }

    R0InitObjectAttributes(&Oa, &gObjectDirectoryName, 0, NULL, &DirectorySd);
    Status = NtCreateDirectoryObject(&gObjectDirectory,
                                     DIRECTORY_ALL_ACCESS,
                                     &Oa);
    if (!NT_SUCCESS(Status))
    {
        DbgPrint("OS2SS: NtCreateDirectoryObject(\\OS2SS) failed %08lx\n",
                 (ULONG)Status);
        NtTerminateProcess(NtCurrentProcess(), Status);
    }
    DbgPrint("OS2SS: object directory ready: \\OS2SS\n");

    RtlInitUnicodeString(&gSbPortName, OS2_SB_PORT_NAME);
    Status = BuildLocalSystemPortSecurity(&SbSd, (PACL)SbAclBuffer, sizeof(SbAclBuffer));
    if (!NT_SUCCESS(Status))
    {
        DbgPrint("OS2SS: SB security descriptor failed %08lx\n", (ULONG)Status);
        NtTerminateProcess(NtCurrentProcess(), Status);
    }

    R0InitObjectAttributes(&Oa, &gSbPortName, 0, NULL, &SbSd);
    Status = NtCreatePort(&gSbListenPort,
                          &Oa,
                          sizeof(SB_CONNECTION_INFO),
                          sizeof(SB_API_MSG),
                          32 * sizeof(SB_API_MSG));
    if (!NT_SUCCESS(Status))
    {
        DbgPrint("OS2SS: NtCreatePort(SB) failed %08lx\n", (ULONG)Status);
        NtTerminateProcess(NtCurrentProcess(), Status);
    }
    DbgPrint("OS2SS: SB callback port created: %wZ\n", &gSbPortName);

    Status = RtlCreateUserThread(NtCurrentProcess(),
                                 NULL,
                                 FALSE,
                                 0,
                                 0,
                                 0,
                                 SbListener,
                                 NULL,
                                 &gSbThread,
                                 &ListenerCid);
    if (!NT_SUCCESS(Status))
    {
        DbgPrint("OS2SS: SB listener thread creation failed %08lx\n", (ULONG)Status);
        NtTerminateProcess(NtCurrentProcess(), Status);
    }

    RtlInitUnicodeString(&gApiPortName, OS2_API_PORT_NAME);
    Status = BuildR5ApplicationPortSecurity(&ApiSd, (PACL)ApiAclBuffer, sizeof(ApiAclBuffer));
    if (!NT_SUCCESS(Status))
    {
        DbgPrint("OS2SS: LE4C API security descriptor failed %08lx\n", (ULONG)Status);
        NtTerminateProcess(NtCurrentProcess(), Status);
    }

    R0InitObjectAttributes(&Oa, &gApiPortName, 0, NULL, &ApiSd);
    Status = NtCreatePort(&gApiListenPort,
                          &Oa,
                          sizeof(OS2_CONNECT_INFO),
                          sizeof(OS2_API_MESSAGE),
                          32 * sizeof(OS2_API_MESSAGE));
    if (!NT_SUCCESS(Status))
    {
        DbgPrint("OS2SS: NtCreatePort(LE4C API) failed %08lx\n", (ULONG)Status);
        NtTerminateProcess(NtCurrentProcess(), Status);
    }
    DbgPrint("OS2SS: LE4C API port created: %wZ\n", &gApiPortName);

    Status = RtlCreateUserThread(NtCurrentProcess(),
                                 NULL,
                                 FALSE,
                                 0,
                                 0,
                                 0,
                                 ApiListener,
                                 NULL,
                                 &gApiThread,
                                 &ListenerCid);
    if (!NT_SUCCESS(Status))
    {
        DbgPrint("OS2SS: LE4C API listener thread creation failed %08lx\n", (ULONG)Status);
        NtTerminateProcess(NtCurrentProcess(), Status);
    }

    Status = RtlInitializeCriticalSection(&gConsoleLock);
    if (!NT_SUCCESS(Status))
    {
        DbgPrint("OS2SS: LE4C console lock initialization failed %08lx\n", (ULONG)Status);
        NtTerminateProcess(NtCurrentProcess(), Status);
    }

    Status = NtCreateEvent(&gConsoleAckEvent,
                           (SYNCHRONIZE | 0x0002UL),
                           NULL,
                           SynchronizationEvent,
                           FALSE);
    if (!NT_SUCCESS(Status))
    {
        DbgPrint("OS2SS: LE4C console ack event creation failed %08lx\n", (ULONG)Status);
        NtTerminateProcess(NtCurrentProcess(), Status);
    }

    RtlInitUnicodeString(&gConsolePortName, LE4C_CONSOLE_PORT_NAME);
    Status = BuildR5ApplicationPortSecurity(&ConsoleSd,
                                             (PACL)ConsoleAclBuffer,
                                             sizeof(ConsoleAclBuffer));
    if (!NT_SUCCESS(Status))
    {
        DbgPrint("OS2SS: LE4C console port security failed %08lx\n", (ULONG)Status);
        NtTerminateProcess(NtCurrentProcess(), Status);
    }

    R0InitObjectAttributes(&Oa, &gConsolePortName, 0, NULL, &ConsoleSd);
    Status = NtCreatePort(&gConsoleListenPort,
                          &Oa,
                          sizeof(LE4C_CONSOLE_CONNECT_INFO),
                          sizeof(LE4C_CONSOLE_MESSAGE),
                          8 * sizeof(LE4C_CONSOLE_MESSAGE));
    if (!NT_SUCCESS(Status))
    {
        DbgPrint("OS2SS: NtCreatePort(LE4C console) failed %08lx\n", (ULONG)Status);
        NtTerminateProcess(NtCurrentProcess(), Status);
    }
    DbgPrint("OS2SS: LE4C TEMPORARY NATIVE CONSOLE BACKEND port created: %wZ\n",
             &gConsolePortName);

    Status = RtlCreateUserThread(NtCurrentProcess(),
                                 NULL,
                                 FALSE,
                                 0,
                                 0,
                                 0,
                                 ConsoleListener,
                                 NULL,
                                 &gConsoleThread,
                                 &ListenerCid);
    if (!NT_SUCCESS(Status))
    {
        DbgPrint("OS2SS: LE4C console listener thread creation failed %08lx\n", (ULONG)Status);
        NtTerminateProcess(NtCurrentProcess(), Status);
    }

    /* Preserve the frozen R0/R1/R2 registration path. */
    Status = SmConnectToSm(&gSbPortName,
                           gSbListenPort,
                           IMAGE_SUBSYSTEM_OS2_CUI,
                           &gSmApiPort);
    if (!NT_SUCCESS(Status))
    {
        DbgPrint("OS2SS: SmConnectToSm failed %08lx\n", (ULONG)Status);
        NtTerminateProcess(NtCurrentProcess(), Status);
    }

    DbgPrint("OS2SS: registered IMAGE_SUBSYSTEM_OS2_CUI (5)\n");

    NtWaitForSingleObject(gSbThread, FALSE, NULL);
    NtTerminateProcess(NtCurrentProcess(), STATUS_UNSUCCESSFUL);
}
