# Soft386 OS/2 R4 handoff

R4 is an additive 80387/x87 milestone on top of live-proven R3 process vessels.

Implemented:

1. Supplied Tiny386 `fpu.c` / `fpu.h` integrated into the jar.
2. `I386_ENABLE_FPU` enabled in normal and Win32 Soft386 builds.
3. FPU attached before protected-mode reset in every process vessel.
4. Opaque FPU bytes included in `CPUI386_State` save/restore.
5. Per-thread x87 state isolation verified with a synthetic two-TID guest.
6. Arithmetic/transcendental fixture covers FSQRT, FSIN and FCOS.
7. Full inherited R2C/R3 regression remains green under GCC and Clang.

Pending live Windows acceptance:

* real Phoon execution under RosBE/i386-built Soft386;
* any additional x87 opcode/fidelity issue exposed by that binary.

No native DOSCALLS/VIOCALLS/KBDCALLS/SESMGR DLL source or ABI changes are part
of R4.
