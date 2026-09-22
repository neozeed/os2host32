# Milestone 30M - first bound-EXE EMX relocation recovery pass

M30M abandons the M30K..M30L fixed-address research branch and resumes from the
known-good M30J loader/runtime.  See `RESEARCH-M30K-M30L.md` for the experiments
and why they are not carried forward.

The goal is the preservation case: an old bound EMX `.exe` should eventually be
sufficient by itself.  The original unbound a.out may no longer exist.

## What emxbind lost

The retained hello-world oracle contains 620 a.out relocation records:

* 597 TEXT records
* 23 DATA records

Only 372 are absolute internal TEXT/DATA/BSS relocations that must change when
the bound LX is moved.  The PC-relative relocations continue to work when the
image moves as a unit.

The bound LX itself reports zero internal LX fixup sites, so M30J had to read
those 372 locations from the unbound a.out sidecar.

## M30M inference rules

Set:

```
OS2_EMX_INFER_RELOCS=audit
```

for a non-destructive scan, or:

```
OS2_EMX_INFER_RELOCS=1
```

for the experimental patch-and-run path.

M30M considers only dwords whose values point into the original EMX TEXT or
DATA/BSS ranges.  It then selects candidates in three classes:

1. recognizable i386 `imm32` / `disp32` address operands in TEXT;
2. aligned runs of at least two pointer-looking dwords embedded in TEXT (jump
   tables / function-pointer tables);
3. aligned pointer-looking dwords in DATA/BSS.

A literal equal to the old TEXT base is deliberately left alone when it occurs
as an isolated immediate.  Old EMX code also uses `0x10000` as a size constant,
and without relocation metadata that value is ambiguous.

## Oracle score on the retained hello

`hi.aout-oracle` is deliberately named so `os2host32` cannot accidentally use
it as the old automatic `hi` sidecar.

Run:

```
python tools\emx_reloc_oracle.py hi.aout-oracle
```

The current first-pass classifier reports:

```
truth: text=349 data=23 total=372
infer: text=351 data=23 ambiguous=3
score: TP=372 FP=2 FN=0
false-positive TEXT sites: 00000EAC 00003777
```

That is intentionally documented rather than hidden: the first classifier finds
all known required relocation sites for the hello, but still has two extra
TEXT candidates caused by instruction-like byte patterns in embedded data.
Those two ambiguities are the next thing to eliminate without sacrificing real
sites.

The oracle is development evidence only.  Runtime inference does not open it.

## Tests

Build normally:

```
make clean
make tools compat
```

Place `hi.exe` and `emx.dll` beside the built files.

First run the safe audit:

```
m30m-emx-infer-audit.cmd hi.exe
```

This performs mapping/import/fixup work and prints the recovered relocation
counts, but does not alter the inferred sites or execute the guest.

The expected proof line begins:

```
M30M EMX infer   : mode=audit ...
M30M EMX infer   : bound LX only; no a.out sidecar was consulted
M30M EMX infer   : AUDIT ONLY - guest bytes were not modified
```

For the actual experimental sidecarless run:

```
m30m-emx-infer-run.cmd hi.exe
```

That enables the same inferred set in patch mode and executes the guest.
Because the current oracle still reports two false-positive TEXT candidates,
this run is explicitly experimental; a crash is useful evidence rather than a
regression of the stable M30J sidecar path.

## M30J sidecar path retained

Nothing removes the proven M30J path.  If `OS2_EMX_INFER_RELOCS` is unset and
`OS2_EMX_AOUT_SIDECAR=1` is set, the original sidecar relocation mechanism is
still available for development and comparison.

If inference is enabled, M30M deliberately ignores `OS2_EMX_AOUT_SIDECAR` so a
sidecar can never silently hide whether bound-only recovery is working.
