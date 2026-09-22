WHP OS/2 V2 R10 - minimal Presentation Manager window
====================================================
Cumulative source release based on R9, whose callback1 and callbackdll tests
passed on the user's Windows WHP host with Microsoft C/386 6.00.081 and the
IBM 2.01.005 LX linker. This release adds a deliberately small PMWIN bridge.
No rebuilt Windows host or PM guest executable is included.

BUILD AND RUN
  build-v2.cmd                  (x64 MSVC developer prompt)
  build-c386-pm.cmd              (your C/386 environment)
  run-pmhello.cmd                (Windows WHP)

Copy the resulting pmhello.exe beside whp_os2_v2_hi.exe if building separately.
The guest build uses the original SDK declarations, os2386.lib and libc.lib;
C386LIB defaults to \c386\lib. No resource compiler or new SDK is required.
The host now links User32.lib and Gdi32.lib as well as WinHvPlatform.lib.

You should see a resizable window titled:
  WHP OS/2 R10 - guest PM window
It paints black text on white. Type a printable key: the displayed text should
change to "Last character: X". Resize or uncover it and check that it repaints.
Close the window (or press Escape) after seeing the text. Expected console:
  pmhello PASS (<n> paints, <n> character messages)
Native close/resize/keyboard behavior needs this interactive Windows check.

For a short automated guest-path smoke test:
  run-pmhello.cmd --auto
This paints, yields within the guest paint callback while a worker runs,
posts a synthetic OS/2 WM_CHAR, repaints with 'A', then posts WM_CLOSE.
It checks at least two paints and a character message before printing PASS.
It is not a substitute for checking real keyboard input and the displayed
window. Diagnostics go to pmhello-trace.txt (overwritten each run).
No long torture batch is required. Suggested focused regressions:
  run-callback.cmd
  run-sync.cmd

DESIGN
One guest thread owns one HAB, one message queue, one registered class and
one standard frame/client pair. Guest handles are fixed 32-bit tokens;
Windows HWND/HDC and runtime pointers remain on the 64-bit host.

The native Windows window procedure only translates and queues messages.
It never calls a guest code address. WinGetMsg parks the guest thread when
its queue is empty. Other runnable guest threads continue normally; when
all guests are blocked, the scheduler waits for native input or the nearest
guest deadline. WinDispatchMsg and WinSendMsg enter the guest window procedure
using R9's saved-context continuation frames. Nested calls and guest sleeps
inside the procedure therefore use the existing scheduler, without recursive
host run loops. Dispatch/Send return the guest procedure's MRESULT in EAX.

WinCreateStdWindow calls guest WM_CREATE before returning the frame handle;
a nonzero result rejects creation and clears the client output handle.
The native window becomes visible after successful WM_CREATE. WinDestroyWindow
calls guest WM_DESTROY before releasing native resources. The test checks
both callbacks and a nested WinSendMsg from WM_CREATE returning 17+25=42.

The supplied legacy SDK QMSG layout is explicitly marshalled as 28 bytes:
HWND at 0; USHORT message at 4; zero padding at 6; mp1 at 8; mp2 at 12;
time at 16; POINTL at 20. No native Windows MSG layout leaks into guest RAM.
RECTL uses 32-bit coordinates and OS/2 bottom-left coordinates; drawing
converts these to the native top-left coordinate system.

IMPLEMENTED PMWIN ORDINALS
  703 WinBeginPaint         716 WinCreateMsgQueue
  726 WinDestroyMsgQueue    728 WinDestroyWindow
  738 WinEndPaint           743 WinFillRect
  763 WinInitialize        765 WinInvalidateRect
  840 WinQueryWindowRect   888 WinTerminate
  902 WinPostQueueMsg      908 WinCreateStdWindow
  911 WinDefWindowProc     912 WinDispatchMsg
  913 WinDrawText          915 WinGetMsg
  919 WinPostMsg           920 WinSendMsg
  926 WinRegisterClass
Ordinals match the existing V1 PMWIN export definitions. This fixture uses
only SDK calls; there are no synthetic WHPTEST imports in pmhello.

CURRENT BOUNDARIES
- One PM owner thread, queue, class and frame/client pair per guest process.
  PM calls from another guest thread fail. No cross-thread Send/Post support.
- Frame and client tokens map to one native top-level window. No child-window
  tree, arbitrary controls, dialogs, menus, resources or window extra bytes.
  Supported frame flags are TITLEBAR, SYSMENU, SIZEBORDER, MINBUTTON,
  MAXBUTTON, SHELLPOSITION and TASKLIST. Default native position/size is used.
  Test passes zero client style, resource module and resource ID.
- WinGetMsg supports only an unfiltered queue. Queue capacity is 128; pending
  paint/size/close messages are coalesced. A full explicit Post returns FALSE;
  overflow from native input stops execution with a diagnostic.
- Translated input is WM_CREATE, WM_DESTROY, WM_SIZE, WM_PAINT, WM_CLOSE and
  basic character WM_CHAR. No mouse, virtual-key/key-up/modifier tracking,
  accelerators, focus protocol, timers or accurate QMSG pointer position.
- WM_CREATE parameters are zero. WM_SIZE supplies the new size in mp2;
  previous-size mp1 is zero. WinDefWindowProc handles close by posting quit;
  other defaults return zero.
- Painting covers the full client area with a temporary native DC. Native
  paint damage is validated when queued; guest BeginPaint reports the full
  rectangle. No PM update-region/clip-region emulation or persistent HPS.
- WinDrawText uses the supplied Beta2 SHORT length, including -1 for a NUL
  terminated string. Maximum 1023 bytes. Basic palette colors, horizontal/
  vertical alignment and DT_ERASERECT only; a native default font is used.
- Existing cooperative scheduling and integer-only callback context remain.
  Compute-only guest code does not pump Windows messages until a WHP exit.
- PMGPI remains explicitly unsupported. This is a PM bridge test, not yet
  enough to run SarienPM. Unsupported PMWIN calls log and return zero.

VALIDATION PERFORMED HERE
- pmhello.c passes GCC -m32 C89 syntax/type checking against the supplied
  Beta2 headers with legacy compiler keywords removed for that check.
  This does not claim Microsoft C/386 compilation or linking.
- Production PM, callback and scheduler helpers compile and execute under
  AddressSanitizer/UndefinedBehaviorSanitizer with Windows/WHP test doubles.
  Tests check callback results/nesting, creation rejection, QMSG packing and
  bounds, native-to-guest character translation, paint coalescing, full queue,
  blocking GetMsg/native wakeup, sleeping peer scheduling, rectangle mapping,
  paint-handle lifetime, invalid inputs, destruction and cleanup.
- Existing R7/R8/R9 native scheduler/info/queue/mutex/callback checks pass in
  the same run, including thread recycling. Logs: validation/r10-native-check.*
  Reproduce on a Linux development host with GCC:
    ASAN_OPTIONS=detect_leaks=0 python3 tests/run-sync-check.py
  LeakSanitizer is disabled for the ptrace-based execution environment.
- No live Windows build, C/386 link or WHP GUI execution was available here.
  Earlier release notes and validation files are retained as historical records.
