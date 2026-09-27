#include "os2ss.h"

LIST_ENTRY g_Os2ProcessList;
RTL_CRITICAL_SECTION g_Os2ProcessLock;

static ULONG g_NextPersonalityProcessId = 1;

NTSTATUS
Os2InitializeProcessTable(VOID)
{
    InitializeListHead(&g_Os2ProcessList);
    g_NextPersonalityProcessId = 1;
    return RtlInitializeCriticalSection(&g_Os2ProcessLock);
}

POS2_PROCESS
Os2CreateProcessRecord(
    _In_ HANDLE ProcessHandle,
    _In_ PCLIENT_ID ClientId,
    _In_ ULONG SessionId)
{
    POS2_PROCESS Process;

    Process = (POS2_PROCESS)RtlAllocateHeap(RtlGetProcessHeap(),
                                            HEAP_ZERO_MEMORY,
                                            sizeof(*Process));
    if (Process == NULL)
        return NULL;

    Process->ProcessHandle = ProcessHandle;
    Process->ClientId = *ClientId;
    Process->SessionId = SessionId;
    Process->ApiPort = NULL;
    Process->SharedServerBase = NULL;
    Process->SharedDataSize = 0;
    Process->State = Os2ProcessStateCreated;

    (void)RtlEnterCriticalSection(&g_Os2ProcessLock);
    Process->PersonalityProcessId = g_NextPersonalityProcessId++;
    if (g_NextPersonalityProcessId == 0)
        g_NextPersonalityProcessId = 1;
    InsertTailList(&g_Os2ProcessList, &Process->Entry);
    (void)RtlLeaveCriticalSection(&g_Os2ProcessLock);

    return Process;
}

POS2_PROCESS
Os2FindProcessByClientId(
    _In_ PCLIENT_ID ClientId)
{
    PLIST_ENTRY Entry;
    POS2_PROCESS Process;
    POS2_PROCESS Found = NULL;

    (void)RtlEnterCriticalSection(&g_Os2ProcessLock);
    for (Entry = g_Os2ProcessList.Flink;
         Entry != &g_Os2ProcessList;
         Entry = Entry->Flink)
    {
        Process = CONTAINING_RECORD(Entry, OS2_PROCESS, Entry);
        if (Process->ClientId.UniqueProcess == ClientId->UniqueProcess &&
            Process->ClientId.UniqueThread == ClientId->UniqueThread)
        {
            Found = Process;
            break;
        }
    }
    (void)RtlLeaveCriticalSection(&g_Os2ProcessLock);
    return Found;
}

POS2_PROCESS
Os2FindProcessByProcessId(
    _In_ HANDLE ProcessId)
{
    PLIST_ENTRY Entry;
    POS2_PROCESS Process;
    POS2_PROCESS Found = NULL;

    (void)RtlEnterCriticalSection(&g_Os2ProcessLock);
    for (Entry = g_Os2ProcessList.Flink;
         Entry != &g_Os2ProcessList;
         Entry = Entry->Flink)
    {
        Process = CONTAINING_RECORD(Entry, OS2_PROCESS, Entry);
        if (Process->ClientId.UniqueProcess == ProcessId)
        {
            Found = Process;
            break;
        }
    }
    (void)RtlLeaveCriticalSection(&g_Os2ProcessLock);
    return Found;
}

POS2_PROCESS
Os2FindProcessByPersonalityId(
    _In_ ULONG PersonalityProcessId)
{
    PLIST_ENTRY Entry;
    POS2_PROCESS Process;
    POS2_PROCESS Found = NULL;

    (void)RtlEnterCriticalSection(&g_Os2ProcessLock);
    for (Entry = g_Os2ProcessList.Flink;
         Entry != &g_Os2ProcessList;
         Entry = Entry->Flink)
    {
        Process = CONTAINING_RECORD(Entry, OS2_PROCESS, Entry);
        if (Process->PersonalityProcessId == PersonalityProcessId)
        {
            Found = Process;
            break;
        }
    }
    (void)RtlLeaveCriticalSection(&g_Os2ProcessLock);
    return Found;
}

VOID
Os2DestroyProcessRecord(
    _Inout_ POS2_PROCESS Process)
{
    if (Process == NULL)
        return;

    (void)RtlEnterCriticalSection(&g_Os2ProcessLock);
    Process->State = Os2ProcessStateClosing;
    RemoveEntryList(&Process->Entry);
    (void)RtlLeaveCriticalSection(&g_Os2ProcessLock);

    /* R4 preserves ownership of SMSS-supplied process handles in the process record. */
    (void)RtlFreeHeap(RtlGetProcessHeap(), 0, Process);
}
