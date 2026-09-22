# M31G R16 - relocatable LX DLL object layout

R15 keeps the NEKO runtime work from R14 and adds the separate e.exe boundary
PMWIN.833 `WinQueryVersion(HAB)`.  The next real NEKO runtime boundary is not a
missing API: the external resource module `NEKO.DLL` is a flat 32-bit LX DLL
with 36 resources, zero imports, zero fixups, and three objects whose preferred
relocation bases are all zero.

The old mapper assumed every LX object's preferred base was a unique linear
slot.  If all bases are zero it can incorrectly attempt `VirtualAlloc` at
address zero (where NULL actually means "choose an address"), or relocate
multiple objects to the same arena offset.

R16 fixes this generically:

- literal object base 0 is always treated as relocatable rather than a direct
  preferred Win32 address;
- overlapping/duplicate preferred object ranges are detected;
- such modules receive one reserved relocation arena with distinct 64-KB
  aligned object slots, preserving the per-object `mapped` addresses consumed
  by fixups, exports, protection and PM resource publication;
- ordinary non-overlapping EXE layouts retain the existing relative relocation
  scheme;
- no NEKO/module-name special case is present.

For the observed NEKO.DLL object sizes the selected packed offsets are:

    object 1  +00000000   size 000027C8
    object 2  +00010000   size 00007538
    object 3  +00020000   size 00000050

The existing per-object resource shadows are retained; they are harmless with
unique slots and continue to protect older LE resource layouts.

## Build / test

Build the host and compatibility personalities as usual.  `NEKO.DLL` should be
on `OS2LIBPATH` (or in the current directory):

    make os2host32.exe PMWP.dll PMWIN.dll
    set OS2LIBPATH=C:\cl386-research\os2_2.0\x\OS2\APPS;.
    set OS2_PM_TRACE=1
    os2host32 --run C:\cl386-research\os2_2.0\x\OS2\APPS\NEKO.EXE

The expected new loader evidence is a packed relocation-arena line followed by
all 36 guest resources being published.  If that succeeds, the next evidence
should come from the R14 real `WinSubclassWindow` / timer path rather than the
module loader.

The independent e.exe track should be run from this same tree; R15's
PMWIN.833 `WinQueryVersion` remains included.
