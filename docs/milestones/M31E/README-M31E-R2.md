# M31E R2 — OPENDLG build fix

R2 is a compile-only correction to M31E R1.

The new OPENDLG titlebar path in `WinWindowFromID()` referenced
`O2_FID_TITLEBAR`, but R1 omitted the constant definition.

The Microsoft/IBM OS/2 2.0 Beta-2 (6.78) `pmwin.h` defines:

    FID_TITLEBAR  0x8003
    FID_MENU      0x8005
    FID_CLIENT    0x8008

R2 therefore adds:

    #define O2_FID_TITLEBAR 0x8003U

No runtime behavior is otherwise changed from R1.
