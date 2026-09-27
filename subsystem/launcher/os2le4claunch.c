#include "../common/reactos_canonical.h"
#include "../common/le4c_console.h"
#include <ndk/exfuncs.h>
#include <ndk/kefuncs.h>

__declspec(dllimport)
BOOL WINAPI
ReadConsoleA(
    HANDLE hConsoleInput,
    PVOID lpBuffer,
    ULONG nNumberOfCharsToRead,
    PULONG lpNumberOfCharsRead,
    PVOID pInputControl);

__declspec(dllimport)
BOOL WINAPI
WriteConsoleA(
    HANDLE hConsoleOutput,
    const VOID *lpBuffer,
    ULONG nNumberOfCharsToWrite,
    PULONG lpNumberOfCharsWritten,
    PVOID lpReserved);

static HANDLE gConsolePort;
static WCHAR gTargetImage[512];


static PCWSTR
GetTargetImageArgument(VOID)
{
    PRTL_USER_PROCESS_PARAMETERS Params = NtCurrentPeb()->ProcessParameters;
    ULONG Chars, I = 0, Out = 0;
    BOOLEAN Quoted = FALSE;
    static const WCHAR DefaultName[] = L"hi.exe";
    if (Params == NULL || Params->CommandLine.Buffer == NULL)
        return DefaultName;
    Chars = Params->CommandLine.Length / sizeof(WCHAR);
    if (Chars == 0u) return DefaultName;

    while (I < Chars && (Params->CommandLine.Buffer[I] == L' ' || Params->CommandLine.Buffer[I] == L'\t')) ++I;
    if (I < Chars && Params->CommandLine.Buffer[I] == L'"') { Quoted = TRUE; ++I; }
    while (I < Chars)
    {
        WCHAR C = Params->CommandLine.Buffer[I++];
        if ((Quoted && C == L'"') || (!Quoted && (C == L' ' || C == L'\t'))) break;
    }
    while (I < Chars && (Params->CommandLine.Buffer[I] == L' ' || Params->CommandLine.Buffer[I] == L'\t')) ++I;
    if (I >= Chars) return DefaultName;

    Quoted = FALSE;
    if (Params->CommandLine.Buffer[I] == L'"') { Quoted = TRUE; ++I; }
    while (I < Chars && Out + 1u < (ULONG)(sizeof(gTargetImage)/sizeof(gTargetImage[0])))
    {
        WCHAR C = Params->CommandLine.Buffer[I++];
        if ((Quoted && C == L'"') || (!Quoted && (C == L' ' || C == L'\t'))) break;
        gTargetImage[Out++] = C;
    }
    gTargetImage[Out] = 0;
    return Out != 0u ? gTargetImage : DefaultName;
}

static NTSTATUS
CheckOs2SubsystemRunning(
    _Out_ PBOOLEAN Running)
{
    NTSTATUS Status;
    HANDLE DirectoryHandle = NULL;
    UNICODE_STRING DirectoryName;
    OBJECT_ATTRIBUTES ObjectAttributes;

    if (Running == NULL)
        return STATUS_INVALID_PARAMETER;

    *Running = FALSE;
    RtlInitUnicodeString(&DirectoryName, L"\\OS2SS");
    InitializeObjectAttributes(&ObjectAttributes,
                               &DirectoryName,
                               OBJ_CASE_INSENSITIVE,
                               NULL,
                               NULL);

    Status = NtOpenDirectoryObject(&DirectoryHandle,
                                   DIRECTORY_TRAVERSE,
                                   &ObjectAttributes);
    if (NT_SUCCESS(Status))
    {
        NtClose(DirectoryHandle);
        *Running = TRUE;
        return STATUS_SUCCESS;
    }

    if (Status == STATUS_OBJECT_NAME_NOT_FOUND ||
        Status == STATUS_OBJECT_PATH_NOT_FOUND)
    {
        return STATUS_SUCCESS;
    }

    return Status;
}

static NTSTATUS
ConnectConsoleBridge(VOID)
{
    NTSTATUS Status;
    UNICODE_STRING PortName;
    SECURITY_QUALITY_OF_SERVICE SecurityQos;
    ULONG MaxMessageLength = 0;
    ULONG ConnectInfoLength;
    LE4C_CONSOLE_CONNECT_INFO ConnectInfo;

    RtlInitUnicodeString(&PortName, LE4C_CONSOLE_PORT_NAME);
    memset(&SecurityQos, 0, sizeof(SecurityQos));
    SecurityQos.Length = sizeof(SecurityQos);
    SecurityQos.ImpersonationLevel = SecurityIdentification;
    SecurityQos.ContextTrackingMode = SECURITY_STATIC_TRACKING;
    SecurityQos.EffectiveOnly = TRUE;

    ConnectInfo.Version = LE4C_CONSOLE_VERSION;
    ConnectInfo.Role = LE4C_CONSOLE_ROLE_LAUNCHER;
    ConnectInfoLength = sizeof(ConnectInfo);

    Status = NtConnectPort(&gConsolePort,
                           &PortName,
                           &SecurityQos,
                           NULL,
                           NULL,
                           &MaxMessageLength,
                           &ConnectInfo,
                           &ConnectInfoLength);
    if (NT_SUCCESS(Status) && ConnectInfoLength != sizeof(ConnectInfo))
    {
        NtClose(gConsolePort);
        gConsolePort = NULL;
        return STATUS_INVALID_PARAMETER;
    }
    return Status;
}


static BOOLEAN
IsRetryableConsoleConnectStatus(
    _In_ NTSTATUS Status)
{
    return (Status == STATUS_OBJECT_NAME_NOT_FOUND ||
            Status == STATUS_OBJECT_PATH_NOT_FOUND ||
            Status == STATUS_PORT_CONNECTION_REFUSED ||
            Status == STATUS_PORT_DISCONNECTED);
}

static NTSTATUS
ConnectConsoleBridgeWithRetry(VOID)
{
    NTSTATUS Status = STATUS_UNSUCCESSFUL;
    LARGE_INTEGER Delay;
    ULONG Attempt;

    /* 50 ms relative delay, bounded to about one second total. */
    Delay.QuadPart = -500000LL;

    for (Attempt = 0; Attempt < 20; ++Attempt)
    {
        Status = ConnectConsoleBridge();
        if (NT_SUCCESS(Status))
            return Status;
        if (!IsRetryableConsoleConnectStatus(Status))
            return Status;
        (void)NtDelayExecution(FALSE, &Delay);
    }

    return Status;
}

static NTSTATUS
SendConsoleAck(
    ULONG Sequence,
    NTSTATUS ActionStatus,
    ULONG BytesWritten,
    const UCHAR *Data,
    ULONG DataLength)
{
    LE4C_CONSOLE_MESSAGE Request;
    LE4C_CONSOLE_MESSAGE Reply;
    NTSTATUS Status;

    Le4cInitConsoleMessage(&Request, Le4cConsoleAck);
    Request.Sequence = Sequence;
    Request.Status = ActionStatus;
    Request.BytesWritten = BytesWritten;
    if (DataLength > LE4C_CONSOLE_MAX_BYTES) DataLength = LE4C_CONSOLE_MAX_BYTES;
    Request.Length = DataLength;
    if (Data != NULL && DataLength != 0u) memcpy(Request.Data, Data, DataLength);
    memset(&Reply, 0, sizeof(Reply));
    Status = NtRequestWaitReplyPort(gConsolePort, &Request.Header, &Reply.Header);
    if (!NT_SUCCESS(Status))
        return Status;
    if (Reply.Version != LE4C_CONSOLE_VERSION ||
        Reply.Operation != Le4cConsoleAckReply)
        return STATUS_INVALID_PARAMETER;
    return Reply.Status;
}

static NTSTATUS
ConsoleBridgeLoop(VOID)
{
    PRTL_USER_PROCESS_PARAMETERS Params = NtCurrentPeb()->ProcessParameters;

    DbgPrint("OS2LE4CLAUNCH: TEMPORARY NATIVE CONSOLE BACKEND console=%p stdout=%p stderr=%p\n",
             Params != NULL ? Params->ConsoleHandle : NULL,
             Params != NULL ? Params->StandardOutput : NULL,
             Params != NULL ? Params->StandardError : NULL);

    for (;;)
    {
        LE4C_CONSOLE_MESSAGE Request;
        LE4C_CONSOLE_MESSAGE Reply;
        NTSTATUS Status;

        Le4cInitConsoleMessage(&Request, Le4cConsoleWait);
        memset(&Reply, 0, sizeof(Reply));
        Status = NtRequestWaitReplyPort(gConsolePort, &Request.Header, &Reply.Header);
        if (!NT_SUCCESS(Status))
        {
            DbgPrint("OS2LE4CLAUNCH: console wait ended status=%08lx\n", (ULONG)Status);
            return Status;
        }

        if (Reply.Version != LE4C_CONSOLE_VERSION ||
            Reply.Length > LE4C_CONSOLE_MAX_BYTES)
        {
            (void)SendConsoleAck(Reply.Sequence, STATUS_INVALID_PARAMETER, 0, NULL, 0);
            continue;
        }

        if (Reply.Operation == Le4cConsoleWrite)
        {
            HANDLE OutputHandle = NULL;
            ULONG Written = 0;
            BOOL Success = FALSE;
            NTSTATUS WriteStatus;

            if (Params != NULL)
            {
                if (Reply.FileHandle == LE4C_CONSOLE_STDOUT)
                    OutputHandle = Params->StandardOutput;
                else if (Reply.FileHandle == LE4C_CONSOLE_STDERR)
                    OutputHandle = Params->StandardError;
            }

            if (OutputHandle != NULL)
                Success = WriteConsoleA(OutputHandle,
                                        Reply.Data,
                                        Reply.Length,
                                        &Written,
                                        NULL);

            WriteStatus = (Success && Written == Reply.Length) ?
                          STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
            DbgPrint("OS2LE4CLAUNCH: VISIBLE REACTOS CONSOLE OUTPUT status=%08lx actual=%lu\n",
                     (ULONG)WriteStatus,
                     Written);
            Status = SendConsoleAck(Reply.Sequence, WriteStatus, Written, NULL, 0);
            if (!NT_SUCCESS(Status))
                return Status;
            continue;
        }


        if (Reply.Operation == Le4cConsoleRead)
        {
            HANDLE InputHandle = Params != NULL ? Params->StandardInput : NULL;
            ULONG Read = 0;
            BOOL Success = FALSE;
            NTSTATUS ReadStatus;
            UCHAR Data[LE4C_CONSOLE_MAX_BYTES];
            ULONG Capacity = Reply.Length;

            if (Capacity > LE4C_CONSOLE_MAX_BYTES) Capacity = LE4C_CONSOLE_MAX_BYTES;
            memset(Data, 0, sizeof(Data));
            if (InputHandle != NULL && Capacity != 0u)
                Success = ReadConsoleA(InputHandle, Data, Capacity, &Read, NULL);
            ReadStatus = Success ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
            DbgPrint("OS2LE4CLAUNCH: VISIBLE REACTOS CONSOLE INPUT status=%08lx actual=%lu\n",
                     (ULONG)ReadStatus, Read);
            Status = SendConsoleAck(Reply.Sequence, ReadStatus, Read, Data, Read);
            if (!NT_SUCCESS(Status)) return Status;
            continue;
        }

        if (Reply.Operation == Le4cConsoleExit)
        {
            Status = SendConsoleAck(Reply.Sequence, STATUS_SUCCESS, 0, NULL, 0);
            DbgPrint("OS2LE4CLAUNCH: console bridge exit result=%lu ack=%08lx\n",
                     Reply.FileHandle,
                     (ULONG)Status);
            return Status;
        }

        (void)SendConsoleAck(Reply.Sequence, STATUS_NOT_IMPLEMENTED, 0, NULL, 0);
    }
}

void CDECL entry(void)
{
    NTSTATUS Status;
    HANDLE SmApiPort = NULL;
    HANDLE ChildWaitHandle = NULL;
    UNICODE_STRING Os2Name;
    UNICODE_STRING NtBootPath;
    UNICODE_STRING NtHiPath;
    PCWSTR TargetImage;
    PRTL_USER_PROCESS_PARAMETERS Params = NULL;
    RTL_USER_PROCESS_INFORMATION Pi;
    BOOLEAN SubsystemRunning = FALSE;

    DbgPrint("OS2LE4CLAUNCH: starting\n");

    Status = SmConnectToSm(NULL, NULL, IMAGE_SUBSYSTEM_UNKNOWN, &SmApiPort);
    if (!NT_SUCCESS(Status))
    {
        DbgPrint("OS2LE4CLAUNCH: SmConnectToSm failed %08lx\n", (ULONG)Status);
        NtTerminateProcess(NtCurrentProcess(), Status);
    }
    DbgPrint("OS2LE4CLAUNCH: connected to SMSS\n");

    Status = CheckOs2SubsystemRunning(&SubsystemRunning);
    if (!NT_SUCCESS(Status))
    {
        DbgPrint("OS2LE4CLAUNCH: OS2SS readiness check failed %08lx\n", (ULONG)Status);
        NtClose(SmApiPort);
        NtTerminateProcess(NtCurrentProcess(), Status);
    }

    if (SubsystemRunning)
    {
        DbgPrint("OS2LE4CLAUNCH: OS2SS already running; skipping deferred subsystem load\n");
    }
    else
    {
        RtlInitUnicodeString(&Os2Name, L"Os2");
        DbgPrint("OS2LE4CLAUNCH: OS2SS not running; requesting deferred subsystem \"Os2\"\n");
        Status = SmLoadDeferedSubsystem(SmApiPort, &Os2Name);
        if (!NT_SUCCESS(Status))
        {
            /*
             * A second launcher may race the first one during subsystem
             * startup.  Re-check the namespace before treating the SMSS
             * load failure as fatal; if OS2SS is now present, the other
             * launcher won the race and we can safely continue.
             */
            NTSTATUS CheckStatus = CheckOs2SubsystemRunning(&SubsystemRunning);
            if (!NT_SUCCESS(CheckStatus) || !SubsystemRunning)
            {
                DbgPrint("OS2LE4CLAUNCH: SmLoadDeferedSubsystem failed %08lx\n", (ULONG)Status);
                NtClose(SmApiPort);
                NtTerminateProcess(NtCurrentProcess(), Status);
            }
            DbgPrint("OS2LE4CLAUNCH: OS2SS became available during startup race; continuing\n");
        }
    }

    Status = ConnectConsoleBridgeWithRetry();
    if (!NT_SUCCESS(Status))
    {
        DbgPrint("OS2LE4CLAUNCH: console bridge connection failed %08lx\n", (ULONG)Status);
        NtClose(SmApiPort);
        NtTerminateProcess(NtCurrentProcess(), Status);
    }
    DbgPrint("OS2LE4CLAUNCH: TEMPORARY NATIVE CONSOLE BACKEND connected\n");

    memset(&NtBootPath, 0, sizeof(NtBootPath));
    if (!RtlDosPathNameToNtPathName_U(L"OS2BOOT.EXE", &NtBootPath, NULL, NULL))
    {
        DbgPrint("OS2LE4CLAUNCH: path conversion failed for OS2BOOT.EXE\n");
        NtClose(gConsolePort);
        NtClose(SmApiPort);
        NtTerminateProcess(NtCurrentProcess(), STATUS_INVALID_PARAMETER);
    }

    TargetImage = GetTargetImageArgument();
    memset(&NtHiPath, 0, sizeof(NtHiPath));
    if (!RtlDosPathNameToNtPathName_U(TargetImage, &NtHiPath, NULL, NULL))
    {
        DbgPrint("OS2LE4CLAUNCH: path conversion failed for target image\n");
        RtlFreeUnicodeString(&NtBootPath);
        NtClose(gConsolePort);
        NtClose(SmApiPort);
        NtTerminateProcess(NtCurrentProcess(), STATUS_INVALID_PARAMETER);
    }
    DbgPrint("OS2LE4CLAUNCH: target image=%wZ\n", &NtHiPath);

    Status = RtlCreateProcessParameters(&Params,
                                        &NtBootPath,
                                        NULL,
                                        NULL,
                                        &NtHiPath,
                                        NULL,
                                        NULL,
                                        NULL,
                                        NULL,
                                        NULL);
    if (!NT_SUCCESS(Status))
    {
        DbgPrint("OS2LE4CLAUNCH: RtlCreateProcessParameters failed %08lx\n", (ULONG)Status);
        RtlFreeUnicodeString(&NtHiPath);
        RtlFreeUnicodeString(&NtBootPath);
        NtClose(gConsolePort);
        NtClose(SmApiPort);
        NtTerminateProcess(NtCurrentProcess(), Status);
    }

    memset(&Pi, 0, sizeof(Pi));
    Pi.Size = sizeof(Pi);
    Status = RtlCreateUserProcess(&NtBootPath,
                                  OBJ_CASE_INSENSITIVE,
                                  Params,
                                  NULL,
                                  NULL,
                                  NULL,
                                  FALSE,
                                  NULL,
                                  NULL,
                                  &Pi);
    RtlDestroyProcessParameters(Params);
    Params = NULL;

    if (!NT_SUCCESS(Status))
    {
        DbgPrint("OS2LE4CLAUNCH: RtlCreateUserProcess failed %08lx\n", (ULONG)Status);
        RtlFreeUnicodeString(&NtHiPath);
        RtlFreeUnicodeString(&NtBootPath);
        NtClose(gConsolePort);
        NtClose(SmApiPort);
        NtTerminateProcess(NtCurrentProcess(), Status);
    }

    DbgPrint("OS2LE4CLAUNCH: OS2BOOT created suspended PID=%p TID=%p\n",
             Pi.ClientId.UniqueProcess,
             Pi.ClientId.UniqueThread);
    DbgPrint("OS2LE4CLAUNCH: SubSystemType=%lu entry=%p\n",
             Pi.ImageInformation.SubSystemType,
             Pi.ImageInformation.TransferAddress);

    if (Pi.ImageInformation.SubSystemType != IMAGE_SUBSYSTEM_OS2_CUI)
    {
        DbgPrint("OS2LE4CLAUNCH: ERROR expected SubSystemType=5\n");
        NtClose(Pi.ThreadHandle);
        NtClose(Pi.ProcessHandle);
        RtlFreeUnicodeString(&NtHiPath);
        RtlFreeUnicodeString(&NtBootPath);
        NtClose(gConsolePort);
        NtClose(SmApiPort);
        NtTerminateProcess(NtCurrentProcess(), STATUS_INVALID_IMAGE_FORMAT);
    }

    Status = NtDuplicateObject(NtCurrentProcess(),
                               Pi.ProcessHandle,
                               NtCurrentProcess(),
                               &ChildWaitHandle,
                               SYNCHRONIZE,
                               0,
                               0);
    if (!NT_SUCCESS(Status))
    {
        DbgPrint("OS2LE4CLAUNCH: duplicate child wait handle failed %08lx\n", (ULONG)Status);
        NtClose(Pi.ThreadHandle);
        NtClose(Pi.ProcessHandle);
        NtClose(gConsolePort);
        NtClose(SmApiPort);
        NtTerminateProcess(NtCurrentProcess(), Status);
    }

    DbgPrint("OS2LE4CLAUNCH: submitting through SmExecPgm\n");
    Status = SmExecPgm(SmApiPort, &Pi, FALSE);
    DbgPrint("OS2LE4CLAUNCH: SmExecPgm returned %08lx\n", (ULONG)Status);

    RtlFreeUnicodeString(&NtHiPath);
    RtlFreeUnicodeString(&NtBootPath);
    NtClose(SmApiPort);

    if (NT_SUCCESS(Status))
    {
        NTSTATUS BridgeStatus;

        /*
         * IMPORTANT: service the visible-console bridge on the original
         * launcher thread.  KERNEL32 registered this initial thread with CSR
         * during process initialization.  A raw RtlCreateUserThread bypasses
         * the normal BasepNotifyCsrOfThread/CSR thread-registration path, so
         * ConSrv rejects WriteConsoleA requests issued by such a helper thread.
         */
        DbgPrint("OS2LE4CLAUNCH: TEMPORARY NATIVE CONSOLE BACKEND using CSR-registered main thread\n");
        BridgeStatus = ConsoleBridgeLoop();
        if (!NT_SUCCESS(BridgeStatus))
        {
            DbgPrint("OS2LE4CLAUNCH: console bridge loop ended status=%08lx\n",
                     (ULONG)BridgeStatus);
            Status = BridgeStatus;
        }

        (void)NtWaitForSingleObject(ChildWaitHandle, FALSE, NULL);
    }
    NtClose(ChildWaitHandle);

    /*
     * The launcher has remained alive for the entire vessel lifetime.
     * The normal DosExit path delivers and acknowledges its explicit
     * Le4cConsoleExit action before OS2BOOT terminates.
     */
    NtTerminateProcess(NtCurrentProcess(), Status);
    for (;;) { }
}
