# Milestone 28C5 — relocatable flat-32 LX mapping

M28C5 follows the successful M28C4 native redirection regression and fixes the
remaining direct-loader issue seen on modern Windows/WOW64.

## What the M28C4 diagnostic proved

On the failing machine `VirtualQuery(0x00010000)` reported:

    state=00001000 type=00040000 allocbase=00010000

That is committed mapped memory (`MEM_COMMIT` + `MEM_MAPPED`) already occupying
the preferred C/386 LX address.  Reserving the low range in OS2HOST32 therefore
cannot make that address available.

## New loader behaviour

The current direct execution subset already requires proper LX fixups:

* internal `OFF32` fixups;
* external ordinal `REL32` fixups.

That means a flat 32-bit LX image does not actually need to run at its preferred
object bases.  M28C5 now first checks whether every preferred LX object range is
free.  If so, the historical exact-address mapping is retained.

If any preferred range is occupied, OS2HOST32 reserves a private relocation
arena (preferring 0x01000000), preserves the relative spacing between LX
objects, and maps the complete image there.  Internal OFF32 fixups are written
using each object's actual mapped address; REL32 import fixups are calculated
from the actual source address; and the entry point and stack use the relocated
object addresses.

Typical relocated output is expected to look like:

    Preferred LX addresses are occupied; relocating image span 00010000..0003ADD0 ...
    mapped object 1 preferred=00010000 actual=01000000 ...
    mapped object 2 preferred=00020000 actual=01010000 ...

The exact relocation arena may differ if 0x01000000 is unavailable.

## Why this is preferable

This is closer to a real LX loader than trying to force modern Windows to leave
1990s preferred linear addresses unused.  The current direct subset is only
accepted when its fixup inventory is sufficient for this relocation model.

## Regression

The M28C4 native redirection regression remains the baseline:

    cd examples
    ..\cmd32os2.exe
    m28c-redir-test.cmd

Expected tail:

    delete-one=OK
    delete-two=OK
    M28C4 redirection regression complete

Then run the genuine C/386 LX smoke program:

    os2host32.exe --run cmdos2_os2_smoke.exe

Expected tail remains:

    pipe child exact payload=OK (20 bytes)
    stdout redirection/append through DosDupHandle=OK
    M28C_OS2_BACKEND_OK
