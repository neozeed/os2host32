# Soft386 NE-H4H-NLS — RC.EXE NLS.1 DosCaseMap

Date: 2026-10-09
Exact baseline: `os2host32-SOFT386-NE-H4G-RC-source.zip` SHA256 `ac9a4d1a54fbd3490781bd538928dff11810b8bb79dbc372086fb6b3227e2d1c`.
This is a strictly additive 16-bit NLS NE-service milestone, not another DOSCALLS file ABI rewrite.

## Windows evidence

The user live-verified CL386 and LINK386 on H4G. Real RC 2.01.02 with SimCity still produced a zero-byte `simcity.RES` but the H4G `--trace-hc` log shows processing *beyond* the formerly broken RCPP child stdout redirection. It opened the RCPP output temporary file and successfully deleted an intermediate path, then ended with:

```
soft386: NLS16.1 SS:SP=0110:6224
soft386: unsupported NLS16.1
soft386: NE missing audited Pascal cleanup for NLS.1
soft386: NE far return failed
soft386: termination=NE runtime failure rc=1
```

This is the first explicit H4G fatal boundary; the previously corrected DosDelete is **not** the immediate cause. No claim is made that `simcity.RES` will be nonempty until the user tests on Windows.

## ABI and implementation

`NLS.1` is `DosCaseMap`, confirmed by `sdk/os2h/bseord.h` (`ORD_DOSCASEMAP 1`) and `sdk/os2h/bsedos16.h`:

```
APIRET APIENTRY DosCaseMap(USHORT usLen, PCOUNTRYCODE pctryc, PCHAR pchStr);
```

10-byte **16-bit Pascal** frame: at SS:SP+4 string FAR pointer, +8 COUNTRYCODE FAR pointer, +12 USHORT length; AX APIRET; callee drops exactly 10 argument bytes plus 4 return bytes. 16-bit COUNTRYCODE holds two USHORT values, not the 32-bit two-ULONG form. The handler validates the 16-bit segment-offset span and RAM mapping, reads COUNTRYCODE, then calls the already-shared `os2_nls_map_case()` on the guest string in place. That core uses explicit CP437/CP850 tables and maintains OS/2-visible NLS state, avoiding host locale-dependent changes. The existing NLS.4 handler remains supported and gains service-specific return tracing.

H4G's conservative unknown-import cleanup behavior is preserved; only the supported NLS.1 is added to the module-scoped whitelist. The 34 DOSCALLS cleanup signatures are unchanged. No file, process, CPU, or native personality implementation changed.

## Files changed

- `soft386/src/soft386_os2.c`: NLS.1 dispatch, exact 10-byte cleanup, validation, tracing, version banner H4H.
- `soft386/tests/check-ne-pascal-abi.py`: NLS.1 width checked against sdk header.
- `soft386/tests/check-ne-nls-casemap.py` (new): constructed NE with real far import relocations; asserts AX, SP, ASCII+OEM uppercase, invalid country, invalid selector, zero-length.
- `soft386/Makefile`: H4H fixture included under `make check-ne`.
- `soft386/docs/H4H-NLS-CASEMAP-HANDOFF.md` (new): this handoff.

## Verification

Local POSIX `-O0` diagnostic build passed. The constructed NLS.1 guest fixture passed. `make check-ne` passed including inherited Microsoft NE CRT memory models, LINK386, LIB, KBDCALLS and RC file/EA fixtures. `make check-quick` passed including LE and personality bridge tests. The package is source-only; no native Windows runtime is claimed here. Real CL386/LINK386 on **H4G** were user-live-proven; H4H must preserve them when built on Windows.

## Windows acceptance

Build with your existing i686/RosBE toolchain, from unpacked package `soft386/`:

```
make WINCC=gcc win32
copy /y soft386_os2.exe C:\OS2\soft386_os2.exe
```

Expect `Soft386 OS/2 NE-H4H-NLS`. In the SimCity source directory:

```
copy /y simcity.exe simcity.before-h4h.exe
C:\OS2\soft386_os2.exe --trace-hc RC.EXE -i include -i include\os2h simcity.rc simcity.exe >rc-h4h.out 2>rc-h4h.err
echo %ERRORLEVEL%
findstr /i "NLS16.1 unsupported missing termination SetFileInfo Delete" rc-h4h.err
dir simcity.RES simcity.exe simcity.$$$
```

The specific prior boundary should now show `NLS16.1 DosCaseMap ... rc=0` or an explicit NLS APIRET rather than `unsupported NLS16.1`. Do not assume that RC succeeds in the same run: a new later API or semantic discrepancy may emerge. Re-run genuine `CL386.EXE -c void.c` and LINK386 as no-regression acceptance checks.
