#include <stdio.h>
#include <string.h>

#include "windows.h"
#include "os2_kbd.h"
#include "os2_kbd_win32.h"

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
    struct Os2KbdSession *session;
    struct Os2KbdKeyInfo key;
    struct Os2KbdStringInBuf sib;
    char buffer[8];

    kbd_stub_reset();
    session = os2_kbd_win32_session();
    check(session != NULL, "Win32 keyboard session published");

    memset(&key, 0, sizeof(key));
    check(os2_kbd_KbdCharIn(session, &key, OS2_KBD_IO_NOWAIT, 0U) == 0U &&
          key.fbStatus == 0U,
          "Win32 NOWAIT empty returns immediately");

    kbd_stub_push_nonkey();
    kbd_stub_push_key(0U, 72U, LEFT_CTRL_PRESSED | SHIFT_PRESSED);
    check(os2_kbd_KbdCharIn(session, &key, OS2_KBD_IO_NOWAIT, 0U) == 0U &&
          key.chChar == 0U && key.chScan == 72U && key.fbStatus == 0x40U &&
          (key.fsState & 0x0007U) == 0x0007U,
          "Win32 backend skips non-key records and maps scan/modifier state");

    kbd_stub_push_key('P', 25U, 0UL);
    check(os2_kbd_KbdPeek(session, &key, 0U) == 0U && key.chChar == 'P',
          "Win32 peek returns key");
    check(os2_kbd_KbdCharIn(session, &key, OS2_KBD_IO_NOWAIT, 0U) == 0U &&
          key.chChar == 'P',
          "Win32 peek did not consume key");

    kbd_stub_push_key('h', 0U, 0UL);
    kbd_stub_push_key('i', 0U, 0UL);
    kbd_stub_push_key('\r', 0U, 0UL);
    memset(buffer, 0, sizeof(buffer));
    sib.cb = (unsigned short)sizeof(buffer);
    sib.cchIn = 0U;
    check(os2_kbd_KbdStringIn(session, buffer, &sib,
                              OS2_KBD_IO_WAIT, 0U) == 0U &&
          sib.cchIn == 2U && buffer[0] == 'h' && buffer[1] == 'i',
          "Win32 console KbdStringIn through common semantics");
    check(kbd_stub_echo_len() == 4U &&
          memcmp(kbd_stub_echo(), "hi\r\n", 4U) == 0,
          "Win32 console echo backend exercised");

    kbd_stub_set_input_type(FILE_TYPE_PIPE);
    kbd_stub_set_pipe_bytes("xy\n");
    memset(buffer, 0, sizeof(buffer));
    sib.cb = (unsigned short)sizeof(buffer);
    sib.cchIn = 0U;
    check(os2_kbd_KbdStringIn(session, buffer, &sib,
                              OS2_KBD_IO_WAIT, 0U) == 0U &&
          sib.cchIn == 2U && buffer[0] == 'x' && buffer[1] == 'y',
          "redirected pipe input remains byte-stream based");
    check(kbd_stub_echo_len() == 4U,
          "redirected input does not acquire console echo semantics");

    kbd_stub_set_input_type(FILE_TYPE_CHAR);
    kbd_stub_push_key('z', 0U, 0UL);
    check(os2_kbd_KbdFlushBuffer(session, 0U) == 0U &&
          kbd_stub_flush_count() == 1U,
          "Win32 FlushConsoleInputBuffer isolated in backend");
    check(os2_kbd_KbdCharIn(session, &key, OS2_KBD_IO_NOWAIT, 0U) == 0U &&
          key.fbStatus == 0U,
          "flush removed pending key");

    if (failures != 0) {
        fprintf(stderr, "kbd-win32-shim-check: %d failure(s)\n", failures);
        return 1;
    }
    printf("kbd-win32-shim-check: PASS\n");
    return 0;
}
