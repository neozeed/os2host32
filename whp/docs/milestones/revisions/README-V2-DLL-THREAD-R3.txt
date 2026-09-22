WHP OS/2 V2 - guest DLL + threaded DLL milestone R3
====================================================

Purpose
-------
R3 keeps the proven R2 WHP execution, virtual-thread scheduler and event
semaphores, then adds the first real OS/2 user-DLL loader to the Win64 V2
runtime.

The main architectural rule is now explicit:

  * one OS/2 process owns one WHP guest address space;
  * all virtual OS/2 threads execute in that same address space;
  * user DLL code and process-instance DLL data are mapped once into that
    address space and are therefore naturally visible to every guest thread;
  * only per-thread registers/stacks/scheduler state are private.

What R3 adds
------------
1. Flat 32-bit OS/2 LE DLL detection (module type 0x00008000).
2. A reserved guest DLL arena:

       03000000 .. 03EFFFFF   guest DLL images

   DosAllocMem / virtual-thread stacks remain below 03000000.

3. DLL rebasing. Preferred LE object addresses are preserved relative to one
   another but moved as a group into the DLL arena. Internal OFF32 fixups use
   the actual mapped guest object addresses.

4. Guest-module registry. A DLL is loaded once per V2 process. Later imports
   of the same module resolve to the already loaded image.

5. Export resolution from the LE entry table:
   - 32-bit exported entry points by ordinal
   - resident/non-resident export names by name

6. Import resolution:
   - DOSCALLS ordinal imports -> shared OUT 0F0h synthetic veneers
   - user-DLL ordinal imports -> direct guest export address
   - user-DLL named imports -> direct guest export address
   - a guest DLL may recursively import another guest DLL

7. Host output is now unbuffered so diagnostic printf output and guest
   DosWrite output should no longer produce the confusing byte-level
   interleaving seen in the R2 torture logs.

What R3 deliberately does NOT do yet
------------------------------------
* DosLoadModule / DosFreeModule / DosGetProcAddr dynamic runtime loading.
* DLL initialization/termination entry routines. The supplied V2DLL.DLL is
  intentionally CRT-free and has no initialization dependency.
* 16-bit DLL objects or 16-bit/call-gate exports.
* DLL forwarder exports.
* PMWIN/PMGPI host personalities in WHP V2.
* General non-flat fixup forms. This milestone remains on the proven flat
  internal OFF32 + external REL32 subset.

Files
-----
whp_os2_v2_hi.c             R3 Win64 WHP runtime
build-v2.cmd                 build the Win64 runtime

v2dll.c / v2dll.def          CRT-free C/386 test DLL
dlltest.c / dlltest.def      simple EXE -> DLL test
dllthread.c / dllthread.def  16/30-thread shared-DLL torture test
build-c386-dll-tests.cmd     builds the DLL and both DLL tests

The proven R2 regression sources are also included:
thread1.c
threadtort.c/.def
sem1.c/.def
semtort.c/.def
build-c386-regressions.cmd
hi.exe

Compiler note
-------------
The recovered 1991 NT C/386 tree contains C1_386, not C1L_386. All R3 build
scripts therefore explicitly use:

    -B1 C1_386

C1L was the alternate larger pass-1 compiler in other Microsoft C packages;
it is not required for these C/386 fixtures.

Build
-----
From an x64 Visual Studio Developer Command Prompt:

    build-v2.cmd

From the C/386 environment with CL386, C1_386 and LINK386 on PATH:

    build-c386-dll-tests.cmd

If your libraries are not in \c386\lib:

    set C386LIB=C:\wherever\the\libs\are
    build-c386-dll-tests.cmd

The DLL is found first in the current directory. It can alternatively be
placed on:

    set OS2LIBPATH=C:\some\dll\directory;C:\another

Recommended test order
----------------------
First keep the old paths green:

    whp_os2_v2_hi.exe hi.exe
    whp_os2_v2_hi.exe thread1.exe
    whp_os2_v2_hi.exe threadtort.exe 30
    whp_os2_v2_hi.exe sem1.exe
    whp_os2_v2_hi.exe semtort.exe 24

Then the new DLL tests:

    whp_os2_v2_hi.exe dlltest.exe
    whp_os2_v2_hi.exe dllthread.exe
    whp_os2_v2_hi.exe dllthread.exe 30

What dlltest proves
-------------------
The EXE imports V2DllAdd by name and V2DllNext/V2DllSay by ordinal.
V2DLL.DLL contains writable process-instance data and imports DosWrite from
DOSCALLS. V2DllSay therefore gives this execution path:

    untouched EXE
       -> direct CALL into mapped guest V2DLL.DLL
       -> DLL calls its DOSCALLS.282 import veneer
       -> OUT 0F0h
       -> Win64 DosWrite personality
       -> resume V2DLL.DLL
       -> return directly to EXE

Expected guest output includes:

    Hello from guest V2DLL.DLL!
    dlltest PASS

What dllthread proves
---------------------
Default 16, optional up to 30 workers. Every worker calls V2DllNext and
V2DllAdd in the same mapped V2DLL.DLL. V2DllNext increments a DLL global.
The test verifies the values are 1..N, proving that all virtual guest threads
see the same process-local DLL data instance. Even workers call
DosExit(EXIT_THREAD); odd workers return through the private V2 thread-return
veneer.

Expected output ends with:

    dllthread: message emitted inside V2DLL.DLL
    dllthread PASS

Useful R3 trace lines
---------------------
A successful DLL load should include lines like:

    v2: GUESTDLL LOAD   V2DLL -> V2DLL.DLL handle=00010000
    v2: GUESTDLL object 1 ... pref=........ guest=030.....
    v2: resolve DOSCALLS     .282  -> 000F.... [V2DLL]
    v2: GUESTDLL READY  V2DLL ...
    v2: resolve V2DLL       .V2DllAdd -> 030..... [dlltest.exe]

If the first live test fails, please capture the entire console output plus
V2DLL.MAP and DLLTEST.MAP. The loader now prints enough preferred/mapped/export
information to distinguish a bad LE export parse from a bad relocation.
