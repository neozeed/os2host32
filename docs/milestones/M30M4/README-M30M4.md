# M30M4 — EMX relocation crash diagnostics

M30M4 is a diagnostic checkpoint built directly on M30M3.  It does **not**
change the inferred-relocation classifier.  Its purpose is to tell us whether
a later application fault is close to bytes that the bound-EXE recovery pass
actually rewrote.

## Why this checkpoint exists

M30M3 made genuine progress with the lean Infocom build:

* the sidecarless `hi.exe` regression still runs;
* `infocom.exe -v nosuch.dat` reaches the application, calls DosOpen, prints
  the expected open failure, and terminates through EMX;
* a real Planetfall story is opened, seeked, and its first 64-byte Z-machine
  header is read before a new fault at guest TEXT+0x0ACD.

The same logs also showed that the optional fixed DATA/BSS island was **not**
actually active: object 2 was still relocated and `deltaD` remained non-zero.
M30M4 therefore adds explicit diagnostics rather than assuming that idea helped.

## New diagnostics

When `OS2_EMX_FIXED_DATA=1` is requested, the mapper now reports whether the
preferred `0x00020000` DATA/BSS slot is free.  If it is already occupied,
`VirtualQuery` details for that address are printed and normal relocated DATA
is used exactly as before.

The M30M inference pass now retains a byte map of accepted TEXT relocation
sites for diagnostics only.  If the guest later faults inside the inferred
main TEXT object, the exception report prints:

* the TEXT-relative and original preferred fault address;
* bytes around EIP; and
* every inferred relocation patch site in that window, including the current
  patched dword value and whether it came from operand or table inference.

This does not alter guest bytes beyond the same M30M3 inference pass.

## Test

Build normally:

    make clean
    make tools compat

Regression:

    m30m-emx-infer-run.cmd hi.exe

Infocom / Planetfall:

    m30m-emx-infer-run.cmd infocom.exe -v PLANETFA

On a fault, preserve the new `M30M4 TEXT`, `M30M4 bytes`, and
`M30M4 nearby inferred patch` lines.  Those should tell us whether the crash
instruction itself or an adjacent operand was modified by relocation recovery.
