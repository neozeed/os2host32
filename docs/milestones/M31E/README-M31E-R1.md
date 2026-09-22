# M31E R1 - OPENDLG real guest PM DLL

Target: the untouched Microsoft/IBM OS/2 2.0 Beta 2 SDK OPENDLG.DLL and its HELLO.EXE demonstrator.

This milestone deliberately does **not** replace OPENDLG with a Win32 common-dialog shim.  The historical 32-bit LE DLL is loaded as guest code by the existing M29N2 guest-module loader; HELLO calls OPENDLG ordinal 26 (`SetupDLF`) and ordinal 27 (`DlgFile`) directly.  OPENDLG then calls the host DOSCALLS/PMWIN/PMGPI personalities just like an OS/2 DLL would.

## What R1 adds

- guest-DLL-owned PM resources are registered under the DLL's synthetic HMODULE before the DLL INIT routine runs;
- per-module PM resource namespaces, so publishing main-EXE resources does not erase resources belonging to loaded guest DLLs;
- correct OS/2 RT_STRING bundle decoding, including a real string ID 0;
- native list-box controls plus the OPENDLG `LM_*` message subset and `LN_SELECT` / `LN_ENTER` -> `WM_CONTROL` bridge;
- `WinQueryWindowULong` / `WinSetWindowULong` index 0 storage used by dialog procedures;
- `WinSetWindowText`, `WinEnableWindowUpdate`, and pointer/hourglass APIs used during directory scans;
- `DosSearchPath` ordinal 228;
- Beta-2 279-byte `FILEFINDBUF` support for `DosFindFirst` / `DosFindNext` without removing the newer FILEFINDBUF3 path;
- `GpiSetAttrs` ordinal 588 for HELLO's character-color bundle.

## Historical binary facts

OPENDLG.DLL exports:

    21 ALERTBOX
    26 SETUPDLF
    27 DLGFILE
    28 OPENFILE
    29 FILEINPATH

Its resources include the Open dialog (id 100), Save As dialog (id 101), a CP850 string table, and DLGINCLUDE data.  HELLO.EXE imports OPENDLG.26 and OPENDLG.27 and carries its own icon/menu/About dialog.

The SDK README itself notes that OPENDLG predates the OS/2 2.0 standard system file dialogs.  That makes it especially useful here: it is a real PM DLL with its own code, data, dialogs, strings, list boxes and DOS file enumeration rather than a thin call into a system common-dialog API.

## Build / regression

On the Win32 build machine:

    make clean
    make tools compat
    make m31e-opendlg-check

Host-independent checks only:

    make m31e-opendlg-static-check

Run:

    m31e-opendlg-r1-test.cmd

The script sets:

    OS2LIBPATH=examples\m31e-opendlg;.
    OS2_PM_TRACE=1
    OS2_TRACE_MODULES=1

Expected loader trace includes a real guest module load/resource/init/export sequence for OPENDLG.

## Runtime test plan

1. HELLO should show its icon/menu and paint a dark-blue client with red `Hello World`.
2. File -> Open should enter OPENDLG.DLL and show its original Open dialog.
3. Directory/file list boxes should populate; selection and double-click should update/navigate normally.
4. File -> Save As should show the original Save As dialog, initially containing `foo.bar` from HELLO.
5. HELLO's own About dialog should continue to work.
6. Closing HELLO should unload/terminate the guest DLL without disturbing PMWIN resources belonging to the main EXE.

This R1 has not been Win32-compiled or executed in the Linux build environment; the i3 is the first compiler/runtime gate.
