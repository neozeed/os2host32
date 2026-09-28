#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "windows.h"
#include "os2_queue.h"
#include "os2_queue_win32.h"

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved);

static int failures;
static void check(int ok, const char *message)
{
    if (!ok) {
        fprintf(stderr, "FAIL: %s\n", message);
        ++failures;
    }
}

int main(void)
{
    struct Os2QueueSession *session;
    Os2QueueHandle hq;
    struct Os2QueueRequestData request;
    Os2QueueU32 length;
    Os2QueueU32 data;
    unsigned char priority;

    check(DllMain(NULL, DLL_PROCESS_ATTACH, NULL) == TRUE,
          "Win32 backend DLL attach");
    session = os2_queue_win32_session();
    check(session != NULL, "Win32 session published");
    hq = 0UL;
    check(os2_queue_DosCreateQueue(session, &hq, OS2_QUEUE_FIFO,
                                   "\\QUEUES\\shim") == 0UL,
          "Win32 backend create");
    check(os2_queue_DosWriteQueue(session, hq, 12UL, 3UL,
                                  0xabcdef01UL, 4UL) == 0UL,
          "Win32 backend set-event path");
    memset(&request, 0, sizeof(request));
    length = data = 0UL;
    priority = 0U;
    check(os2_queue_DosReadQueue(session, hq, &request, &length, &data,
                                 0UL, OS2_QUEUE_DCWW_WAIT, &priority, 0UL) == 0UL,
          "Win32 backend read available entry");
    check(request.pid == 4242UL && request.data == 12UL &&
          length == 3UL && data == 0xabcdef01UL && priority == 4U,
          "Win32 backend result fields");
    check(DllMain(NULL, DLL_PROCESS_DETACH, NULL) == TRUE,
          "Win32 backend DLL detach");

    if (failures != 0) {
        fprintf(stderr, "queue-win32-shim-check: %d failure(s)\n", failures);
        return 1;
    }
    printf("queue-win32-shim-check: PASS\n");
    return 0;
}
