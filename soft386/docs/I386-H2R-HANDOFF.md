# Soft386 I386-H2R — bounded PM string copyout

## Status

**SOURCE/REGRESSION COMPLETE — Windows runtime pending.**

H2R fixes a generic Soft386 PM ABI-marshalling bug discovered while diagnosing
untouched OS/2 2.0 GA `NEKO.EXE` under the Tiny386 jar.

No PMWIN rendering semantics, LX page decoding, scheduler policy, Tiny386 CPU
semantics, or application-specific behavior are changed.

## Root cause proven by the H2Q/H2P watchpoint trace

`PrfQueryProfileData(NEKO/NEKODATA)` may legitimately fail when `neko.ini` is
missing/empty.  Neko then installs compiled-in defaults itself:

- guest `0x000211E0`: `00 -> 46`
- guest `0x000211E2`: `00 -> C8` (200 ms timer interval)
- guest `0x000211E4`: `00 -> 14`

Later Neko calls:

    WinLoadString(hab, hmod, 0x194, 0x100, 0x000211C0)

The Soft386 bridge allocated a zero-filled 256-byte native staging buffer,
called PMWIN successfully, then generic `copyout()` copied all 256 bytes back
to guest memory.  PMWIN had only defined/written the returned string and NUL.
The untouched zero-filled tail therefore overwrote adjacent guest globals,
including the defaults at `0x211E0/2/4`.

The watchpoint captured the corruption at the WinLoadString import veneer:

    mem-watch hostcall EIP=000F005D addr=000211E0 old=46 new=00
    mem-watch hostcall EIP=000F005D addr=000211E2 old=C8 new=00
    mem-watch hostcall EIP=000F005D addr=000211E4 old=14 new=00

That caused the later Neko `WinStartTimer` call to use timeout 0 instead of the
known-good native os2host32 value 200 ms, and its animation state never entered
the normal movement path.

## H2R fix

For PMWIN ordinals:

- 779 `WinLoadMessage`
- 781 `WinLoadString`

Soft386 now shrinks the output-copy length after the provider call to:

    min(returned_character_count + 1, caller_capacity)

where `+1` preserves the terminating NUL.  For a zero-length result with a
nonzero capacity, one NUL byte is copied back.  The unused staging-buffer tail
is never written to guest memory.

This preserves the jar architecture: PMWIN still owns API semantics; Soft386
only fixes the representation/copy boundary.

## Regression added

`soft386/tests/pm-bridge-check.c` now provides WinLoadString/WinLoadMessage test
providers and surrounds guest output buffers with sentinel bytes.  It verifies
that only `"Help\\0"` / `"Msg\\0"` are copied back and every byte after the NUL
remains unchanged.

## Validation performed

Passed locally:

- `make -C soft386 check-quick`
- `make -C soft386 pm-bridge-check && soft386/pm-bridge-check`
- `make -C soft386 queue-bridge-check net-bridge-check`
- `soft386/queue-bridge-check`
- `soft386/net-bridge-check`

Tiny386 compiler warnings are pre-existing.

## Windows acceptance test

Build H2R and run untouched Neko normally first:

    soft386_os2.exe --max-cycles 0 --run demos\NEKO.EXE

Expected first-order change: the cat timer/default state must no longer be
zeroed by WinLoadString.  With tracing enabled, `WinStartTimer` should carry
`000000C8`, and the prior hostcall watch should no longer show `0x211E0/2/4`
being zeroed by EIP `000F005D`.

Useful verification command if needed:

    soft386_os2.exe --max-cycles 0 --trace-native ^
      --watch-mem 0x211D0:0x40 --run demos\NEKO.EXE 2>neko-h2r.txt

The desired trace sequence is defaults being installed (`46`, `C8`, `14`) and
remaining intact across `WinLoadString`.
