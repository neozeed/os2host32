#include <string.h>
#include "windows.h"

#define STUB_MAX_RECORDS 32
#define STUB_MAX_PIPE 128
#define STUB_MAX_ECHO 256

static int input_token;
static int output_token;
static DWORD input_type = FILE_TYPE_CHAR;
static INPUT_RECORD records[STUB_MAX_RECORDS];
static unsigned int rec_head;
static unsigned int rec_count;
static char pipe_bytes[STUB_MAX_PIPE];
static unsigned int pipe_len;
static unsigned int pipe_pos;
static char echo_bytes[STUB_MAX_ECHO + 1];
static unsigned int echo_len;
static unsigned int flush_count;
static DWORD last_error;
static DWORD tick = 1000UL;
static PHANDLER_ROUTINE ctrl_handler;

void kbd_stub_reset(void)
{
    input_type = FILE_TYPE_CHAR;
    rec_head = rec_count = 0U;
    pipe_len = pipe_pos = 0U;
    echo_len = 0U;
    echo_bytes[0] = '\0';
    flush_count = 0U;
    last_error = 0UL;
    tick = 1000UL;
    ctrl_handler = NULL;
}

void kbd_stub_push_key(unsigned char ch, unsigned char scan, DWORD state)
{
    unsigned int pos;
    pos = (rec_head + rec_count) % STUB_MAX_RECORDS;
    memset(&records[pos], 0, sizeof(records[pos]));
    records[pos].EventType = KEY_EVENT;
    records[pos].Event.KeyEvent.bKeyDown = TRUE;
    records[pos].Event.KeyEvent.wRepeatCount = 1U;
    records[pos].Event.KeyEvent.wVirtualScanCode = scan;
    records[pos].Event.KeyEvent.uChar.AsciiChar = (char)ch;
    records[pos].Event.KeyEvent.dwControlKeyState = state;
    ++rec_count;
}

void kbd_stub_push_nonkey(void)
{
    unsigned int pos;
    pos = (rec_head + rec_count) % STUB_MAX_RECORDS;
    memset(&records[pos], 0, sizeof(records[pos]));
    records[pos].EventType = 2U;
    ++rec_count;
}

void kbd_stub_set_input_type(DWORD type) { input_type = type; }
void kbd_stub_set_pipe_bytes(const char *text)
{
    size_t n;
    n = strlen(text);
    if (n > STUB_MAX_PIPE) n = STUB_MAX_PIPE;
    memcpy(pipe_bytes, text, n);
    pipe_len = (unsigned int)n;
    pipe_pos = 0U;
}
const char *kbd_stub_echo(void) { return echo_bytes; }
unsigned int kbd_stub_echo_len(void) { return echo_len; }
unsigned int kbd_stub_flush_count(void) { return flush_count; }

HANDLE GetStdHandle(DWORD which)
{
    if (which == STD_INPUT_HANDLE) return (HANDLE)&input_token;
    if (which == STD_OUTPUT_HANDLE) return (HANDLE)&output_token;
    return INVALID_HANDLE_VALUE;
}
DWORD GetFileType(HANDLE handle)
{
    if (handle == (HANDLE)&input_token) return input_type;
    if (handle == (HANDLE)&output_token) return FILE_TYPE_CHAR;
    return 0UL;
}
BOOL SetConsoleCtrlHandler(PHANDLER_ROUTINE routine, BOOL add)
{
    (void)add; ctrl_handler = routine; return TRUE;
}
LONG InterlockedCompareExchange(volatile LONG *target, LONG exchange, LONG comparand)
{
    LONG old;
    old = *target;
    if (old == comparand) *target = exchange;
    return old;
}
LONG InterlockedExchange(volatile LONG *target, LONG value)
{
    LONG old;
    old = *target; *target = value; return old;
}
BOOL WriteConsoleInputA(HANDLE handle, const INPUT_RECORD *in,
                        DWORD count, DWORD *written)
{
    DWORD i;
    (void)handle;
    *written = 0UL;
    for (i = 0UL; i < count && rec_count < STUB_MAX_RECORDS; ++i) {
        unsigned int pos;
        pos = (rec_head + rec_count) % STUB_MAX_RECORDS;
        records[pos] = in[i];
        ++rec_count; ++*written;
    }
    return TRUE;
}
BOOL GetNumberOfConsoleInputEvents(HANDLE handle, DWORD *count)
{
    (void)handle; *count = (DWORD)rec_count; return TRUE;
}
BOOL ReadConsoleInputA(HANDLE handle, INPUT_RECORD *out,
                       DWORD count, DWORD *read)
{
    (void)handle; (void)count;
    *read = 0UL;
    if (rec_count == 0U) return TRUE;
    *out = records[rec_head];
    rec_head = (rec_head + 1U) % STUB_MAX_RECORDS;
    --rec_count;
    *read = 1UL;
    return TRUE;
}
BOOL PeekConsoleInputA(HANDLE handle, INPUT_RECORD *out,
                       DWORD count, DWORD *read)
{
    (void)handle; (void)count;
    *read = 0UL;
    if (rec_count == 0U) return TRUE;
    *out = records[rec_head];
    *read = 1UL;
    return TRUE;
}
BOOL PeekNamedPipe(HANDLE handle, void *buffer, DWORD buffer_size,
                   DWORD *bytes_read, DWORD *total_avail, DWORD *left)
{
    unsigned int avail;
    (void)handle; (void)left;
    avail = pipe_len - pipe_pos;
    if (total_avail != NULL) *total_avail = (DWORD)avail;
    if (bytes_read != NULL) *bytes_read = 0UL;
    if (buffer != NULL && buffer_size != 0UL && avail != 0U) {
        ((char *)buffer)[0] = pipe_bytes[pipe_pos];
        if (bytes_read != NULL) *bytes_read = 1UL;
    }
    return TRUE;
}
BOOL ReadFile(HANDLE handle, void *buffer, DWORD count, DWORD *read,
              void *overlapped)
{
    (void)handle; (void)overlapped;
    *read = 0UL;
    if (count == 0UL || pipe_pos >= pipe_len) return TRUE;
    ((char *)buffer)[0] = pipe_bytes[pipe_pos++];
    *read = 1UL;
    return TRUE;
}
BOOL FlushConsoleInputBuffer(HANDLE handle)
{
    (void)handle; rec_head = rec_count = 0U; ++flush_count; return TRUE;
}
BOOL WriteConsoleA(HANDLE handle, const void *buffer, DWORD count,
                   DWORD *written, void *reserved)
{
    unsigned int n;
    (void)handle; (void)reserved;
    n = (unsigned int)count;
    if (echo_len + n > STUB_MAX_ECHO) n = STUB_MAX_ECHO - echo_len;
    memcpy(echo_bytes + echo_len, buffer, n);
    echo_len += n;
    echo_bytes[echo_len] = '\0';
    *written = (DWORD)n;
    return TRUE;
}
DWORD GetTickCount(void) { return tick++; }
DWORD GetLastError(void) { return last_error; }
