#ifndef OS2SS_LE4C_CONSOLE_H
#define OS2SS_LE4C_CONSOLE_H

#include "reactos_canonical.h"

/*
 * LE4C TEMPORARY NATIVE CONSOLE BACKEND protocol.
 * This is deliberately separate from OS2_PROTOCOL_VERSION and is not VIO.
 */
#define LE4C_CONSOLE_PORT_NAME       L"\\OS2SS\\Le4cConsolePort"
#define LE4C_CONSOLE_VERSION         1UL
#define LE4C_CONSOLE_ROLE_LAUNCHER   1UL
#define LE4C_CONSOLE_MAX_BYTES       192UL
#define LE4C_CONSOLE_STDOUT          1UL
#define LE4C_CONSOLE_STDERR          2UL

typedef struct _LE4C_CONSOLE_CONNECT_INFO
{
    ULONG Version;
    ULONG Role;
} LE4C_CONSOLE_CONNECT_INFO, *PLE4C_CONSOLE_CONNECT_INFO;

typedef enum _LE4C_CONSOLE_OPERATION
{
    Le4cConsoleWait = 1,
    Le4cConsoleWrite,
    Le4cConsoleRead,
    Le4cConsoleExit,
    Le4cConsoleAck,
    Le4cConsoleAckReply
} LE4C_CONSOLE_OPERATION;

typedef struct _LE4C_CONSOLE_MESSAGE
{
    PORT_MESSAGE Header;
    ULONG Version;
    ULONG Operation;
    ULONG Sequence;
    NTSTATUS Status;
    ULONG FileHandle;
    ULONG Length;
    ULONG BytesWritten;
    UCHAR Data[LE4C_CONSOLE_MAX_BYTES];
} LE4C_CONSOLE_MESSAGE, *PLE4C_CONSOLE_MESSAGE;

C_ASSERT(sizeof(LE4C_CONSOLE_CONNECT_INFO) == 0x08);
C_ASSERT(sizeof(LE4C_CONSOLE_MESSAGE) == 0xF8);
C_ASSERT(FIELD_OFFSET(LE4C_CONSOLE_MESSAGE, Version) == 0x18);
C_ASSERT(FIELD_OFFSET(LE4C_CONSOLE_MESSAGE, Data) == 0x34);

static __inline VOID
Le4cInitConsoleMessage(PLE4C_CONSOLE_MESSAGE Message, ULONG Operation)
{
    memset(Message, 0, sizeof(*Message));
    Message->Header.u2.ZeroInit = 0;
    Message->Header.u1.s1.DataLength =
        (CSHORT)(sizeof(*Message) - sizeof(Message->Header));
    Message->Header.u1.s1.TotalLength = (CSHORT)sizeof(*Message);
    Message->Version = LE4C_CONSOLE_VERSION;
    Message->Operation = Operation;
    Message->Status = STATUS_SUCCESS;
}

#endif
