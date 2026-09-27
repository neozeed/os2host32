#ifndef OS2SS_SERVER_H
#define OS2SS_SERVER_H

#include "../common/os2msg.h"
#include "../common/le4c_console.h"

typedef enum _OS2_PROCESS_STATE
{
    Os2ProcessStateCreated = 1,
    Os2ProcessStateApiConnected,
    Os2ProcessStateClosing
} OS2_PROCESS_STATE;

typedef struct _OS2_PROCESS
{
    LIST_ENTRY Entry;
    HANDLE ProcessHandle;
    CLIENT_ID ClientId;
    ULONG SessionId;
    ULONG PersonalityProcessId;
    HANDLE ApiPort;
    PVOID SharedServerBase;
    ULONG SharedDataSize;
    OS2_PROCESS_STATE State;
} OS2_PROCESS, *POS2_PROCESS;

C_ASSERT(sizeof(OS2_PROCESS_STATE) == sizeof(ULONG));
C_ASSERT(FIELD_OFFSET(OS2_PROCESS, Entry) == 0x00);
C_ASSERT(FIELD_OFFSET(OS2_PROCESS, ProcessHandle) == 0x08);
C_ASSERT(FIELD_OFFSET(OS2_PROCESS, ClientId) == 0x0C);
C_ASSERT(FIELD_OFFSET(OS2_PROCESS, SessionId) == 0x14);
C_ASSERT(FIELD_OFFSET(OS2_PROCESS, PersonalityProcessId) == 0x18);
C_ASSERT(FIELD_OFFSET(OS2_PROCESS, ApiPort) == 0x1C);
C_ASSERT(FIELD_OFFSET(OS2_PROCESS, SharedServerBase) == 0x20);
C_ASSERT(FIELD_OFFSET(OS2_PROCESS, SharedDataSize) == 0x24);
C_ASSERT(FIELD_OFFSET(OS2_PROCESS, State) == 0x28);
C_ASSERT(sizeof(OS2_PROCESS) == 0x2C);

extern LIST_ENTRY g_Os2ProcessList;
extern RTL_CRITICAL_SECTION g_Os2ProcessLock;

typedef NTSTATUS
(*POS2_API_HANDLER)(
    _In_ POS2_PROCESS Process,
    _Inout_ POS2_API_MESSAGE Message);

extern POS2_API_HANDLER g_Os2ApiHandlers[Os2ApiMax];

NTSTATUS
Os2InitializeProcessTable(VOID);

POS2_PROCESS
Os2CreateProcessRecord(
    _In_ HANDLE ProcessHandle,
    _In_ PCLIENT_ID ClientId,
    _In_ ULONG SessionId);

POS2_PROCESS
Os2FindProcessByClientId(
    _In_ PCLIENT_ID ClientId);

POS2_PROCESS
Os2FindProcessByProcessId(
    _In_ HANDLE ProcessId);

POS2_PROCESS
Os2FindProcessByPersonalityId(
    _In_ ULONG PersonalityProcessId);

VOID
Os2DestroyProcessRecord(
    _Inout_ POS2_PROCESS Process);

NTSTATUS
Os2Le4cConsoleWrite(
    _In_ ULONG FileHandle,
    _In_reads_bytes_(Length) const UCHAR *Bytes,
    _In_ ULONG Length,
    _Out_ PULONG BytesWritten);

NTSTATUS
Os2Le4cConsoleRead(
    _Out_writes_bytes_to_(Capacity, *BytesRead) UCHAR *Bytes,
    _In_ ULONG Capacity,
    _Out_ PULONG BytesRead);

NTSTATUS
Os2Le4cConsoleNotifyExit(
    _In_ ULONG Result);

NTSTATUS
Os2DispatchApi(
    _In_ POS2_PROCESS Process,
    _Inout_ POS2_API_MESSAGE Message);

#endif
