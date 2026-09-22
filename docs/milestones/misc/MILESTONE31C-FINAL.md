# M31C FINAL - HANOI

Frozen from the exact M31C R1 implementation proven on Windows with the untouched Microsoft/IBM OS/2 2.0 Beta 2 SDK HANOI.EXE.

Proven behavior:
- original LE icon/menu/dialog/accelerator resources
- GpiSetCurrentPosition/GpiSetColor/GpiBox animated Hanoi drawing
- Options -> Set and Alt+S change disk count
- About dialog
- worker-thread animation through tens of thousands of moves
- WinPostMsg completion back to the PM thread
- DosExit(EXIT_THREAD) terminates only the worker
- Stop interrupts an in-progress 16-disk solve
- clean WM_CLOSE/WM_DESTROY/WM_QUIT shutdown

M31A and M31B remain regression gates.
