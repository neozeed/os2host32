# DOS memory sample for failed Oorx reception

DOS's `754C` generates noise in the terrain segment. Its first `55A2` blit
changes BP to 8 (the 40-byte EGA row pitch minus its 32-byte window width).
The next 23 iterations therefore read segment `0008`, not the terrain segment.
Their colour/pattern comes from that DOS environment's low memory.

R32 preserves this source change. Without a supplied sample, low memory is
generated afresh each static frame using an independent PRNG. This is a
synthetic missing-data fallback, **not a reconstruction of your DOS memory**.
A supplied sample is never randomized; original source and offset playback
remain intact.

For a closer pattern, run the included `DOSLOW.COM` in the DOS environment you
use for the original game. It writes 16512 bytes to `DOSLOW.BIN`. Copy that
file into R32's `r20/assets` folder before starting the SDL game. The sampler
reads memory and writes its output file; it does not patch the game or install
a TSR. Run it with the same DOS and driver setup. A sample taken at the DOS
prompt may still differ from memory while the game is active.

For an exact comparison, use the emulator debugger to capture physical
`00000` through `0407F` while the original game is running, save those bytes as
`DOSLOW.BIN`, and copy that into `r20/assets`. A larger flat dump starting at
physical zero is also accepted; R32 reads its first 16512 bytes.

GNU binutils rebuild (not required to use the prebuilt 70-byte COM file):

```sh
as --32 r32/dos-tools/doslow.s -o doslow.o
ld -m elf_i386 -Ttext 0x100 --oformat binary doslow.o -o DOSLOW.COM
```

The sampler's instruction stream and DOS-call arguments are checked by the
bundled test-only CPU harness. It was not run in a real DOS installation here.
