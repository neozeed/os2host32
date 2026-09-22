# M30K through M30L research notes - why fixed-address EMX was abandoned

These checkpoints were deliberately exploratory.  None of their low-address
or PEB experiments are carried forward into M30M; M30M starts again from the
M30J source tree.

## The problem being tested

Classic `emxbind` output in the specimens examined here contains the a.out
TEXT/DATA/BSS contents but no usable LX internal relocation sites for the
original a.out absolute relocations.  On real OS/2 that was acceptable because
the executable normally occupied its preferred low addresses.

For the small EMX hello the historical layout is:

```
object 1 TEXT       00010000 .. 00018fff
object 2 DATA/BSS   00020000 .. 0002215f
object 3 arena      00030000 .. 0202ffff
object 4 stack      02030000 .. 0282ffff
```

M30C through M30J worked around modern Win32 relocation by retaining the
unbound a.out as a sidecar and applying its original relocation table.
K/L asked whether we could eliminate that sidecar by reproducing the old
address space instead.

## M30K - move the host PE high

The M30J host was linked at `0x00400000`, which lies inside the large EMX
historical arena.  M30K moved the host PE to a high image base and tried to map
the EMX executable at its preferred addresses.

Result: the first 64 KiB at `0x00010000` was already occupied before the guest
was mapped.  The host reported a committed `MEM_MAPPED` region at exactly the
EMX TEXT base, so simply moving the PE did not solve the problem.

## M30K1 - suspended child / parent reservation

A launcher created the 32-bit host suspended and called `VirtualAllocEx` before
resuming its primary thread.

Result: even the suspended child already had low process state mapped.  The
reservation failed with error 487 (`ERROR_INVALID_ADDRESS`) and a committed
region starting at `0x00010000`.  `CREATE_SUSPENDED` is therefore too late to
obtain a blank historical address space.

## M30K2* - environment / PEB investigation

Several diagnostics tested whether the low allocation was the process
environment or `RTL_USER_PROCESS_PARAMETERS`.

Useful findings:

* the early M30K2 copy crash was our bug: `VirtualQuery().RegionSize` was
  incorrectly treated as "bytes valid starting at the environment pointer";
* `RtlSetCurrentEnvironment` on current Windows was not safe with an arbitrary
  `VirtualAlloc` block, so that path was abandoned;
* direct PEB diagnostics eventually showed the real 32-bit
  `ProcessParameters` and environment living together around `0x007c0000`, not
  at `0x00010000`;
* the `0x00010000` blocker remained a separate 64 KiB `MEM_MAPPED` section;
* `GetMappedFileNameW` and `NtQueryVirtualMemory(MemorySectionName)` returned no
  backing filename for it;
* its bytes include `FF EE FF EE`, which resembles an NT heap-segment
  signature, but the research did not prove the exact owner.  We therefore do
  **not** identify it more specifically in the compatibility code.

Most importantly, this proved that invasive PEB/environment surgery was aimed
at the wrong allocation.  All such changes are removed in M30M.

## M30L - relocate only TEXT, keep the tail fixed

The next idea was to move only object 1 TEXT and leave DATA/BSS/arena/stack at
the historical addresses.  This would have preserved most absolute references
without needing relocation metadata.

Result: modern Windows has other allocations inside the supposedly fixed
`0x00020000..0x02830000` tail as well (the PEB diagnostics alone showed process
state around `0x007c0000`).  The tail cannot be reserved as one historical
span, so the hybrid layout silently fell back to the normal relocated image.

## Conclusion

Trying to recreate a 1990s EMX process map inside a modern WOW64 process is not
robust enough for preservation use.  It depends on undocumented host address
layout and would remain vulnerable to Windows-version and process-startup
changes.

The durable direction is instead:

1. map the LX wherever Win32 permits, as M30J already does;
2. recover the missing a.out absolute relocation *sites* from the bound image;
3. patch only those recovered sites by the mapping delta;
4. retain the original a.out only as a development oracle, not a runtime
   dependency.

M30M is the first checkpoint on that path.
