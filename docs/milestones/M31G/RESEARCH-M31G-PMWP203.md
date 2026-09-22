# M31G research - OS/2 2.0 GA PMWP ordinal 203

This note records the evidence used for M31G R5.  It does **not** make the
historical IBM PMWP.DLL a V1 runtime dependency.

## Historical specimen

User-supplied OS/2 2.0 GA `PMWP.DLL`:

- size: 1,146,133 bytes
- SHA-256: `d4991c438ab8fb13a6df4a0186d4934a7eecc718f4f5d9e4f98cab924a399bb1`
- 11 LX objects
- object 2 is 16-bit; the other relevant code/data objects are 32-bit
- fixups include SEL16, PTR16:16, PTR16:32, OFF32, REL32 and aliases
- imports include PMSHAPI, PMWIN, PMCTLS, PMVIOP, DOSCALLS, PMGPI, NLS,
  SESMGR, QUECALLS, PMDRAG, MSG and SOM

Therefore the complete Workplace Shell DLL is not a direct-host V1 module.
Executing it would require selector/far-pointer and wider WPS/SOM machinery.

## Export 203

The GA LX entry table contains ordinal 203 as a 32-bit entry in object 1 at
object offset `0x00060DDC`.

Disassembly of the export establishes the calling shape without requiring a
name:

- standard 32-bit stack frame
- reads four arguments at `[ebp+08]`, `[ebp+0c]`, `[ebp+10]`, `[ebp+14]`
- returns a local 16-bit result after explicitly zero-extending it into EAX
- plain `ret`, consistent with the cdecl-style caller cleanup observed in NEKO

No trustworthy public historical name for GA PMWP ordinal 203 has been
established. Later WPS API mappings identify nearby public ordinals such as
200/201/205 but do not identify 203. Do not invent a symbolic API name.

## NEKO call site

Untouched NEKO.EXE SHA-256:
`3315d6ccbb8ec2f2a1ffb53c3444257bf798f7af4cad9527738cb54cb1d45b0b`

NEKO has one REL32 external fixup for PMWP.203 at object-1 source offset
`0x49`, the displacement of the call beginning at object offset `0x48`.
The startup sequence is structurally:

    WinInitialize(...)
    WinCreateMsgQueue(...)
    push <pointer/path>
    push <pointer/"NEKO">
    push <data pointer>
    push 1
    call PMWP.203
    add  esp, 16
    or   eax, eax
    je   success

Thus NEKO independently proves:

- four 32-bit arguments
- caller stack cleanup
- full EAX return is tested
- zero means success

The argument values look like an early shell/application registration or
initialisation request, but that semantic name remains an inference rather
than a proven API identity.

## R5 compatibility decision

Route `PMWP` as a native host personality and export only ordinal 203 as
`PMWPOrdinal203 @203 NONAME`. Preserve the proven four-DWORD ABI, return
full-EAX zero success, and trace only raw values. This models the shell request
as advisory in a native-Win32 process without importing the Workplace Shell or
adding application-specific behavior.

## R14 correction from stronger GA evidence

The R13 import-complete runtime invalidated R5's advisory-success model.  NEKO
continued with HMODULE zero and every animation WinLoadPointer missed.

A second pass over the original GA PMWP.DLL reconstructed ordinal 203's external
call on the successful path.  The REL32 fixup at that call targets
DOSCALLS ordinal 318, `DosLoadModule`.  The argument setup is structurally:

    push [ebp+0x0c]   ; ordinal-203 arg2 = PHMODULE
    push [ebp+0x10]   ; ordinal-203 arg3 = module name
    push 0x105        ; failure-buffer length
    lea  local_fail_buffer
    push local_fail_buffer
    call DOSCALLS.318

Untouched NEKO passes arg3 pointing at `"NEKO"` and arg2 pointing at its global
HMODULE slot.  Later WinLoadPointer call sites load that slot and use it as the
resource-module handle.  This establishes a materially stronger semantic fact
than was available at R5.

R14 keeps ordinal 203 unnamed but replaces the unconditional zero return with a
call through the native DOSCALLS.318 compatibility personality.  The normal
OS2HostModuleLoad graph and OS2LIBPATH rules therefore remain authoritative.
