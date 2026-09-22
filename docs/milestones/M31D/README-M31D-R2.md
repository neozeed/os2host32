# M31D R2 - BIO build fix

R2 is a compile-only correction to M31D R1.

`pm_dialog_wndproc()` introduced mouse and vertical-scroll translation for BIO but
R1 accidentally used `mp1` and `mp2` without declarations.

R2 declares those values locally in their C89 blocks:

- `O2MPARAM mp1` in the `WM_LBUTTONDOWN/WM_LBUTTONUP` block
- `O2MPARAM mp2` in the `WM_VSCROLL` block

No intended runtime behavior changed from R1.

Build/test:

    make clean
    make tools compat
    make m31d-bio-check
    m31d-bio-r2-test.cmd

Then run BIO with `OS2_PM_TRACE=1` for the first runtime pass.
