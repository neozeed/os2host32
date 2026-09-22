# WHP OS/2 V2 — Milestone 2 checkpoint

Frozen 2026-09-21 at Jason's request after a clean Windows build and successful interactive CMD32 child execution. Runtime source is byte-for-byte identical to the tested M2 process R2 warning-fixed release. No new runtime changes are introduced by this checkpoint; the runtime banner remains M2 process R2 deliberately.

## Achieved

Milestone1 established the Sarien Presentation Manager path, including working Space Quest 2 gameplay, save/restore, beeps, and corrected client geometry. Milestone2 adds a working CMD32 console path and guest child-process execution on WHP.

User-confirmed Windows results:
- CMD32 command execution: `echo WHP_CMD_OK`.
- Interactive console tests reported working; prior traces retained.
- `run-process.cmd`: execchild arguments/environment PASS; process1 PASS (sync, arguments/environment, async wait, reap, stdout).
- `run-cmd32.cmd -c "hi.exe"`: hi!.
- `run-cmd32.cmd -c "execchild.exe 37"`: execchild running inside WHP.
- Interactive CMD32: change to C:\os2\demos\phoon, run phoon.exe, display the moon, return to the prompt.
- Final warning-fixed host build reported clean, with hello, execchild and interactive phoon still running.

Evidence is user-reported Windows output plus retained native harness logs. A separate CMD `echo %ERRORLEVEL%` result was not supplied; the process fixture itself verified exit-code propagation. No full post-fix rerun of every historical suite is claimed.

## Contents and build

Complete cumulative source, recovered CMD32 source, supplied CMD32/hello guest binaries, old C/386 test sources and scripts, historical notes, patches and validation logs are included. The newly built Windows host, process1.exe, execchild.exe and phoon.exe from Jason's machine are not included: rebuild the host/fixtures and use the existing phoon executable.

Use an x64 Visual Studio Developer Command Prompt for `build-v2.cmd`.
Use the configured Microsoft C/386 6.00.081 / LINK386 setup for `build-c386-process.cmd`.
Then run `run-process.cmd`, followed by `run-cmd32.cmd` from Windows.
Read NEXT-CHAT-HANDOFF.md before continuing development.

## Checkpoint policy

Keep this ZIP unchanged as the known-working baseline. Start the next development release in a separate directory. SHA256SUMS-MILESTONE2.txt covers every packaged file other than itself. Older checksum files and patches belong to their named historical releases, not this checkpoint.
