# Start here - M31E / OPENDLG

M31D/BIO is considered frozen at the proven R8 behavior.

M31E changes the kind of compatibility being exercised: instead of another standalone executable extending PM/GPI ordinals, use the SDK's real OPENDLG.DLL and HELLO.EXE to validate the guest LE DLL loader in a substantial Presentation Manager workload.

Key invariants:
- do not translate OPENDLG.DLL to PE and do not replace DlgFile with a host common dialog;
- HELLO must resolve OPENDLG.26/27 through the generic guest-module loader;
- DLL resources must remain associated with the DLL HMODULE and be available during DLL initialization;
- preserve WMCHAR, HANOI, BIO and Sarien regressions.
