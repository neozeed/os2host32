#ifndef OS2SS_OS2MSG_H
#define OS2SS_OS2MSG_H

#include "reactos_canonical.h"

/* LE4X preserves the version-1 wire envelope and adds only hi.exe DOSCALLS semantics. */
#define OS2_PROTOCOL_VERSION        1UL
#define OS2_OBJECT_DIRECTORY_NAME   L"\\OS2SS"
#define OS2_SB_PORT_NAME            L"\\OS2SS\\SbPort"
#define OS2_API_PORT_NAME           L"\\OS2SS\\ApiPort"
#define OS2_R3_PING_REQUEST_VALUE   0x13579BDFUL
#define OS2_R3_PING_REPLY_VALUE     0xCAFEBABEUL
#define OS2_API_PAYLOAD_SIZE        16UL
#define OS2_SHARED_SECTION_SIZE     (32UL * 1024UL)
#define OS2_R5_MAX_WRITE            OS2_SHARED_SECTION_SIZE
#define OS2_HFILE_STDOUT            1UL

/* OS/2 APIRET values used by the R4 DosQuerySysInfo veneer/server. */
typedef ULONG APIRET;
typedef ULONG OS2_HFILE;
#define OS2_NO_ERROR                0UL
#define OS2_ERROR_INVALID_FUNCTION  1UL
#define OS2_ERROR_FILE_NOT_FOUND     2UL
#define OS2_ERROR_PATH_NOT_FOUND     3UL
#define OS2_ERROR_TOO_MANY_OPEN_FILES 4UL
#define OS2_ERROR_ACCESS_DENIED      5UL
#define OS2_ERROR_INVALID_HANDLE    6UL
#define OS2_ERROR_NOT_ENOUGH_MEMORY 8UL
#define OS2_ERROR_INVALID_ACCESS     12UL
#define OS2_ERROR_GEN_FAILURE       31UL
#define OS2_ERROR_INVALID_PARAMETER 87UL
#define OS2_ERROR_BUFFER_OVERFLOW   111UL

/*
 * Authoritative OS/2 QSV numbers: BSEDOS.H / OS/2 programming references.
 * R4 deliberately supports only the contiguous 10..12 range.
 */
#define OS2_QSV_PAGE_SIZE           10UL
#define OS2_QSV_VERSION_MAJOR       11UL
#define OS2_QSV_VERSION_MINOR       12UL
#define OS2_QUERY_SYSINFO_MAX_VALUES 3UL

/* R4 personality values, matching the existing V2 proof behavior. */
#define OS2_R4_PAGE_SIZE_VALUE      4096UL
#define OS2_R4_VERSION_MAJOR_VALUE  20UL
#define OS2_R4_VERSION_MINOR_VALUE  0UL


#define OS2_EXIT_THREAD             0UL
#define OS2_EXIT_PROCESS            1UL
#define OS2_PAG_READ                0x00000001UL
#define OS2_PAG_WRITE               0x00000002UL
#define OS2_PAG_EXECUTE             0x00000004UL
#define OS2_PAG_GUARD               0x00000008UL
#define OS2_PAG_COMMIT              0x00000010UL
#define OS2_PAG_DECOMMIT            0x00000020UL
#define OS2_OBJ_TILE                0x00000040UL

typedef enum _OS2_CONNECTION_TYPE
{
    Os2ConnectionProcess = 1
} OS2_CONNECTION_TYPE;

typedef struct _OS2_CONNECT_INFO
{
    ULONG ConnectionType;
    ULONG Version;
    ULONG PersonalityProcessId;
} OS2_CONNECT_INFO, *POS2_CONNECT_INFO;

typedef enum _OS2_API_NUMBER
{
    Os2ApiR3Ping = 0,
    Os2ApiDosQuerySysInfo,
    Os2ApiDosWrite,
    Os2ApiDosQueryHType,
    Os2ApiDosExit,
    Os2ApiDosSetFilePtr,
    Os2ApiDosAllocMem,
    Os2ApiDosFreeMem,
    Os2ApiDosSetMem,
    Os2ApiDosGetDateTime,
    Os2ApiDosRead,
    Os2ApiMax
} OS2_API_NUMBER;

typedef struct _OS2_QUERY_SYSINFO_REQUEST
{
    ULONG Start;
    ULONG Last;
    ULONG BufferSize;
} OS2_QUERY_SYSINFO_REQUEST, *POS2_QUERY_SYSINFO_REQUEST;

typedef struct _OS2_QUERY_SYSINFO_REPLY
{
    ULONG Count;
    ULONG Values[OS2_QUERY_SYSINFO_MAX_VALUES];
} OS2_QUERY_SYSINFO_REPLY, *POS2_QUERY_SYSINFO_REPLY;


typedef struct _OS2_DOSWRITE_REQUEST
{
    ULONG FileHandle;
    ULONG SharedOffset;
    ULONG Length;
} OS2_DOSWRITE_REQUEST, *POS2_DOSWRITE_REQUEST;

typedef struct _OS2_DOSWRITE_REPLY
{
    ULONG BytesWritten;
} OS2_DOSWRITE_REPLY, *POS2_DOSWRITE_REPLY;


typedef struct _OS2_QUERY_HTYPE_REQUEST
{
    ULONG FileHandle;
} OS2_QUERY_HTYPE_REQUEST, *POS2_QUERY_HTYPE_REQUEST;

typedef struct _OS2_QUERY_HTYPE_REPLY
{
    ULONG Type;
    ULONG Attributes;
} OS2_QUERY_HTYPE_REPLY, *POS2_QUERY_HTYPE_REPLY;

typedef struct _OS2_EXIT_REQUEST
{
    ULONG Action;
    ULONG Result;
} OS2_EXIT_REQUEST, *POS2_EXIT_REQUEST;

typedef struct _OS2_SETFILEPTR_REQUEST
{
    ULONG FileHandle;
    LONG Distance;
    ULONG Method;
} OS2_SETFILEPTR_REQUEST, *POS2_SETFILEPTR_REQUEST;

typedef struct _OS2_SETFILEPTR_REPLY
{
    ULONG Position;
} OS2_SETFILEPTR_REPLY, *POS2_SETFILEPTR_REPLY;

typedef struct _OS2_ALLOCMEM_REQUEST
{
    ULONG Size;
    ULONG Flags;
    ULONG Reserved;
} OS2_ALLOCMEM_REQUEST, *POS2_ALLOCMEM_REQUEST;

typedef struct _OS2_FREEMEM_REQUEST
{
    ULONG Base;
} OS2_FREEMEM_REQUEST, *POS2_FREEMEM_REQUEST;

typedef struct _OS2_SETMEM_REQUEST
{
    ULONG Base;
    ULONG Size;
    ULONG Flags;
} OS2_SETMEM_REQUEST, *POS2_SETMEM_REQUEST;

typedef struct _OS2_DATETIME_REPLY
{
    UCHAR Hours;
    UCHAR Minutes;
    UCHAR Seconds;
    UCHAR Hundredths;
    UCHAR Day;
    UCHAR Month;
    USHORT Year;
    SHORT Timezone;
    UCHAR Weekday;
    UCHAR Reserved;
} OS2_DATETIME_REPLY, *POS2_DATETIME_REPLY;

typedef struct _OS2_DOSREAD_REQUEST
{
    ULONG FileHandle;
    ULONG SharedOffset;
    ULONG Length;
} OS2_DOSREAD_REQUEST, *POS2_DOSREAD_REQUEST;

typedef struct _OS2_DOSREAD_REPLY
{
    ULONG BytesRead;
} OS2_DOSREAD_REPLY, *POS2_DOSREAD_REPLY;

typedef union _OS2_API_PAYLOAD
{
    struct
    {
        ULONG Value;
        ULONG Reserved[3];
    } Ping;
    OS2_QUERY_SYSINFO_REQUEST QuerySysInfoRequest;
    OS2_QUERY_SYSINFO_REPLY QuerySysInfoReply;
    OS2_DOSWRITE_REQUEST DosWriteRequest;
    OS2_DOSWRITE_REPLY DosWriteReply;
    OS2_QUERY_HTYPE_REQUEST QueryHTypeRequest;
    OS2_QUERY_HTYPE_REPLY QueryHTypeReply;
    OS2_EXIT_REQUEST ExitRequest;
    OS2_SETFILEPTR_REQUEST SetFilePtrRequest;
    OS2_SETFILEPTR_REPLY SetFilePtrReply;
    OS2_ALLOCMEM_REQUEST AllocMemRequest;
    OS2_FREEMEM_REQUEST FreeMemRequest;
    OS2_SETMEM_REQUEST SetMemRequest;
    OS2_DATETIME_REPLY DateTimeReply;
    OS2_DOSREAD_REQUEST DosReadRequest;
    OS2_DOSREAD_REPLY DosReadReply;
    ULONG Raw[OS2_API_PAYLOAD_SIZE / sizeof(ULONG)];
    UCHAR RawBytes[OS2_API_PAYLOAD_SIZE];
} OS2_API_PAYLOAD, *POS2_API_PAYLOAD;

typedef struct _OS2_API_MESSAGE
{
    PORT_MESSAGE Header;
    ULONG ApiNumber;
    ULONG ReturnCode;
    NTSTATUS HostStatus;
    ULONG Flags;
    OS2_API_PAYLOAD Data;
} OS2_API_MESSAGE, *POS2_API_MESSAGE;

/* x86 wire-layout contract. */
C_ASSERT(sizeof(APIRET) == sizeof(ULONG));
C_ASSERT(sizeof(OS2_HFILE) == sizeof(ULONG));
C_ASSERT(sizeof(PORT_MESSAGE) == 0x18);
C_ASSERT(sizeof(OS2_CONNECT_INFO) == 0x0C);
C_ASSERT(FIELD_OFFSET(OS2_CONNECT_INFO, ConnectionType) == 0x00);
C_ASSERT(FIELD_OFFSET(OS2_CONNECT_INFO, Version) == 0x04);
C_ASSERT(FIELD_OFFSET(OS2_CONNECT_INFO, PersonalityProcessId) == 0x08);
C_ASSERT(sizeof(OS2_QUERY_SYSINFO_REQUEST) == 0x0C);
C_ASSERT(sizeof(OS2_QUERY_SYSINFO_REPLY) == 0x10);
C_ASSERT(sizeof(OS2_DOSWRITE_REQUEST) == 0x0C);
C_ASSERT(sizeof(OS2_DOSWRITE_REPLY) == 0x04);
C_ASSERT(sizeof(OS2_QUERY_HTYPE_REQUEST) == 0x04);
C_ASSERT(sizeof(OS2_QUERY_HTYPE_REPLY) == 0x08);
C_ASSERT(sizeof(OS2_EXIT_REQUEST) == 0x08);
C_ASSERT(sizeof(OS2_SETFILEPTR_REQUEST) == 0x0C);
C_ASSERT(sizeof(OS2_SETFILEPTR_REPLY) == 0x04);
C_ASSERT(sizeof(OS2_ALLOCMEM_REQUEST) == 0x0C);
C_ASSERT(sizeof(OS2_FREEMEM_REQUEST) == 0x04);
C_ASSERT(sizeof(OS2_SETMEM_REQUEST) == 0x0C);
C_ASSERT(sizeof(OS2_DATETIME_REPLY) == 0x0C);
C_ASSERT(sizeof(OS2_DOSREAD_REQUEST) == 0x0C);
C_ASSERT(sizeof(OS2_DOSREAD_REPLY) == 0x04);
C_ASSERT(sizeof(OS2_API_PAYLOAD) == 0x10);
C_ASSERT(sizeof(OS2_API_MESSAGE) == 0x38);
C_ASSERT(FIELD_OFFSET(OS2_API_MESSAGE, ApiNumber) == 0x18);
C_ASSERT(FIELD_OFFSET(OS2_API_MESSAGE, ReturnCode) == 0x1C);
C_ASSERT(FIELD_OFFSET(OS2_API_MESSAGE, HostStatus) == 0x20);
C_ASSERT(FIELD_OFFSET(OS2_API_MESSAGE, Flags) == 0x24);
C_ASSERT(FIELD_OFFSET(OS2_API_MESSAGE, Data) == 0x28);

static __inline VOID
Os2InitConnectInfo(POS2_CONNECT_INFO ConnectInfo)
{
    memset(ConnectInfo, 0, sizeof(*ConnectInfo));
    ConnectInfo->ConnectionType = (ULONG)Os2ConnectionProcess;
    ConnectInfo->Version = OS2_PROTOCOL_VERSION;
}

static __inline VOID
Os2InitApiMessage(POS2_API_MESSAGE Message, OS2_API_NUMBER ApiNumber)
{
    memset(Message, 0, sizeof(*Message));
    Message->Header.u2.ZeroInit = 0;
    Message->Header.u1.s1.DataLength =
        (CSHORT)(sizeof(*Message) - sizeof(Message->Header));
    Message->Header.u1.s1.TotalLength = (CSHORT)sizeof(*Message);
    Message->ApiNumber = (ULONG)ApiNumber;
    Message->HostStatus = STATUS_SUCCESS;
}

#endif
