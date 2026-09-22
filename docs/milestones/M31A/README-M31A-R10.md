# M31A R10 - WMCHAR record-corruption proof

R9 proved the indexed `gachVK[]` displacement is correctly relocated to
`010202EC`.  The keypress fault nevertheless showed EAX=00005720 at the
`gachVK[pcm->usVK]` load even though the translated `mp2` for `h` was
`00000068`, so the next question is whether WMCHAR is reading the intended
`gacm[]` record and when that record changes.

R10 is diagnostic-only:

* traces every internal OFF32 relocation targeting `gacm` and its fields
  (`object 2 + 0BD8..0BDE`) as well as the `gachVK` table;
* adds the text pointer and first four bytes to `WinDrawText` tracing;
* expands the PMWIN vectored-exception report with EFLAGS and general
  registers;
* when the fault still has a readable guest C frame, dumps DrawMessage's
  four arguments, the complete 8-byte `CHMSG`, and its 16-byte local `sz`.

No PM behavior or guest executable is changed.
