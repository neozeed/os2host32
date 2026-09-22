# M30M3 — EMX fixed DATA/BSS island + sidecarless TEXT relocation recovery

M30M3 follows the M30J/M30M line.  It does **not** restore the abandoned
M30K/M30L whole-address-space experiments.

## Why this checkpoint exists

The sidecarless `hi.exe` proof worked with M30M/M30M2.  The larger Infocom
sample exposed a harder x86 case: GCC 2.x generated control flow which can
enter bytes that also belong to the address operand of another instruction.
Rewriting every absolute DATA operand can therefore change the alternate
instruction stream.

The concrete failure was the instruction at original TEXT +0x0B97:

    c6 05 88 00 02 00 00    mov byte ptr [00020088h],0

M30M relocated the operand to the moved DATA object.  A later short branch
lands at +0x0B9A, inside that operand, so the relocated operand bytes changed
what that alternate decode did.  The resulting fault was an attempted write
to address 1.

## Observation from the bound executable

`emxbind` left a complete a.out header at file offset 0xC00.  For the supplied
Infocom binary it reports:

    ZMAGIC / i386
    text  = 0000F000
    data  = 00002000
    bss   = 00000CC0
    entry = 00010000
    trsize = 0
    drsize = 0

The original relocation records are stripped, but the TEXT, DATA and symbol
records remain.  Most importantly, classic EMX a.out has only a small static
DATA+BSS image beginning at 0x00020000.  The huge LX object 3 is the EMX heap,
not the original a.out BSS.

## M30M3 mapping policy

When all of the following are true:

* the image imports EMX;
* inferred relocation mode is enabled;
* `OS2_EMX_FIXED_DATA=1`; and
* the original DATA/BSS slot is free,

M30M3 maps only LX object 2 at its historical address (normally 0x00020000).
TEXT, the EMX heap and stack remain freely relocatable in the normal M30J
arena.

This is deliberately *not* M30L: no attempt is made to reserve the entire
historical address space.

With DATA+BSS fixed:

* absolute references to DATA+BSS require no relocation and are left byte-for-byte unchanged;
* TEXT jump/function tables which point to TEXT are still recovered and patched;
* DATA pointer cells which point to TEXT are still recovered and patched;
* if 0x00020000 is unavailable, the loader falls back to ordinary relocated DATA rather than failing.

The inference engine now applies the delta belonging to the referenced object
instead of requiring one common image delta.

## Development oracle result for hi

Keeping DATA fixed means only TEXT-targeting relocations need modification.
Against the retained unbound `hi.aout-oracle`:

    genuine relocations that still need patching: 229
    inferred genuine sites found:                229
    false positives:                               2
    false negatives:                               0

The two existing false positives are unchanged from M30M; this checkpoint is
about eliminating unnecessary DATA operand rewriting, not claiming perfect
relocation recovery yet.

## Test

Build normally:

    make clean
    make tools compat

Then, with no unbound `infocom` sidecar present:

    m30m-emx-infer-run.cmd infocom.exe -v nosuch.dat

The important new line is:

    M30M3 DATA island: preserving EMX DATA/BSS at 00020000..00022CC0

and the inference line should show a zero DATA delta:

    M30M EMX infer   : mode=patch deltaT=... deltaD=00000000 ...

The existing `hi.exe` test should also remain a regression test.
