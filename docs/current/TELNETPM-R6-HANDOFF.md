# TELNETPM R6 handoff: first live session on native Win32

Recorded: 2026-10-03. Track: V1/native Win32. State: R6 live-tested on
Windows; first remote BBS exchange achieved; terminal rendering remains
incorrect. Preserve this point before the next rendering changes.

The companion [milestone](../milestones/telnetpm/TELNETPM-R6-LIVE.md)
records the observed result and its limits. This document describes source
integration and the next engineering work. It supersedes the status claims
in the earlier TELNETPM phase-2/R5 notes, which remain useful design history.

## What is now proven

The original 273,544-byte TELNETPM.EXE starts through the existing exact-image
native bridge, creates its PM UI, connects to `vert.synchro.net`, receives
the BBS banner, sends typed login text, and displays the server's response.
The final screenshot shows an unsuccessful username attempt and another
login prompt. Successful account authentication is not claimed.

The supplied R6 Windows smoke prints 39 R6 checks PASS and 131 main API
checks PASS. Its font query returns 777 records, 75 regular fixed-pitch
fonts, with a 228-byte stride. The R6 session log has 23 send entries and
27 receive entries, with no UNIMPLEMENTED call or exception marker. These
are call-entry counts, not packet counts or proof of each return value.

R5's separate native Winsock smoke already passed 95 checks, including
cross-thread close during recv, accept, recvfrom and infinite select.
The R6 networking DLLs and socket smoke are byte-identical to those R5
files. R6 extends PM initialization; it does not revise that transport.

## Exact application and execution boundary

* Input size: 273,544 bytes, seven LX objects, eighteen resources.
* SHA-256: `16f34d712cdefb8b3f4fa93b7956fc97429697419deccd80b62b8d838d4960e2`.
* Launch with `os2host32.exe --telnetpm-probe path\to\telnetpm.exe`.
* The packaged loader is named `os2host32-phase2-r6.exe`; it is the unchanged
  R5 loader with a package-specific filename.
* The on-disk OS/2 EXE is unchanged. Earlier phase-2 work performs guarded
  in-memory fixups, exception-chain redirection and known mixed-mode bridge
  replacements. This is not general execution of arbitrary 16-bit LX code.
* The newer 276,480-byte TELNETPM is a different specimen and is not admitted.
* Continue using the existing ETC/profile environment. The live R6 log
  reports `ETC=C:\OS2\etc`; R6 supplies no replacement INI.

## Source integration

The handoff archive contains **all 15 complete source/build/test files
added or changed by R6**, plus this handoff and the milestone. `source/`
is an overlay at repository-relative paths. It requires the existing
TELNETPM R5 source baseline; it is not a complete standalone checkout.

The working tree originally came from the uploaded `os2host32-ebb2009rd.zip`
and has earlier TELNETPM work applied. Its recorded Git HEAD,
`ebb2009716073aef0eb4b30d11b8d0111b758d7f`, is not an R5 commit identifier.
Do not interpret the R6 patch as a patch against that bare commit or against
an uninspected current GitHub HEAD. The eight modified files have saved R5
base hashes in `integration/source-manifest.json` in the handoff archive.

Required earlier work includes the probe loader and its bridge/intake
headers, PM resource registration, the R3 PM/profile helpers, R4 native
socket adapters, and R5 switch-list/close fixes. In particular, the API
smoke already depends on `dlls/pmshapi/switchlist.h`; the host-check target
also uses the earlier profile and switch-list test sources. Retain the
existing `dlls/pm-common/pmcompat.h` shared presentation-space definition.

From an R5 tree, apply the combined source-and-documentation patch:

```sh
git apply --check /path/to/patches/telnetpm-r6-integration.patch
git apply /path/to/patches/telnetpm-r6-integration.patch
git diff --check
make MINGW=i686-w64-mingw32-gcc PMWIN.dll PMGPI.dll telnetpm-api-smoke.exe
```

If R6 source is already installed, apply only
`patches/telnetpm-r6-documentation.patch`. The original code-only
`patches/telnetpm-r6-pm-init.patch` is also included unchanged. Choose one
applicable route; do not apply both the combined and code-only patches.

If the check reports conflicts, merge the relevant changes into the newer
files. The complete files under `source/` are reference snapshots, not an
instruction to overwrite newer PM changes. The archive's patches contain
only the explicit R6 file set and the two new documents; unrelated working
tree differences, including subsystem/WHP material, are not part of them.

Suggested commit subject:

```text
Add TELNETPM R6 PM APIs and record first live BBS session
```

Suggested preservation tag after integration and verification:
`telnetpm-phase2-r6-live`. No commit, tag or push was performed by this
handoff preparation.

## Complete R6 source inventory

| Path | Change | Purpose |
|---|---|---|
| `Makefile` | Modified | Link font enumerator; add helper/test dependencies and host checks |
| `common/api/os2_api_catalog.inc` | Modified | Register eight ordinal APIs and argument sizes |
| `dlls/pmwin/pmwin.c` | Modified | Accelerator ownership/dispatch, message resources, bounded string decoding |
| `dlls/pmwin/pmwin.def` | Modified | Seven new ordinal exports |
| `dlls/pmwin/pm_accel.h` | Added | Owned tables, queue/window associations, matching and lifecycle |
| `dlls/pmwin/pm_text.h` | Added | Bounded RT_STRING/RT_MESSAGE bundle decoder |
| `dlls/pmgpi/pmgpi.c` | Modified | GpiQueryFonts wrapper and extended FONTMETRICS definition |
| `dlls/pmgpi/pmgpi.def` | Modified | Export ordinal 586 |
| `dlls/pmgpi/font_query.h` | Added | 228-byte guest metrics layout and enumerator interface |
| `dlls/pmgpi/font_query.c` | Added | GDI family/style/size enumeration and metric conversion |
| `tests/pmcompat/telnetpm-api-smoke.c` | Modified | Run new native R6 smoke section |
| `tests/pmcompat/telnetpm-r6-smoke.h` | Added | Native ordinal, keyboard, resource and installed-font checks |
| `tests/pmcompat/accel-text-host-check.c` | Added | Production-helper accelerator/text regressions |
| `tests/pmcompat/font-query-host-check.c` | Added | Production enumerator with controlled GDI fixtures |
| `tests/pmcompat/win32-stub/windows.h` | Modified | Host-test Win32 declarations |

The two new repository documents are this file and
`docs/milestones/telnetpm/TELNETPM-R6-LIVE.md`. Runtime code in the handoff is
byte-identical to the shipped R6 source snapshot; this handoff adds no fix.

## Added API surface and invariants

| Import | API | Flat argument bytes |
|---|---|---:|
| PMWIN.709 | WinCopyAccelTable | 12 |
| PMWIN.713 | WinCreateAccelTable | 8 |
| PMWIN.723 | WinDestroyAccelTable | 4 |
| PMWIN.776 | WinLoadAccelTable | 12 |
| PMWIN.779 | WinLoadMessage | 20 |
| PMWIN.798 | WinQueryAccelTable | 8 |
| PMWIN.850 | WinSetAccelTable | 12 |
| PMGPI.586 | GpiQueryFonts | 24 |

These are 32-bit cdecl adapters exported by ordinal with NONAME. Five were
previously deferred TELNETPM imports; create/copy/destroy complete the table
lifecycle without being new imports of this particular application.

Accelerator tables are a count/codepage WORD pair followed by six-byte
entries. Resources are copied into owned storage; handles are distinct from
Windows HACCEL values. The registry supports 128 live tables and serial
handles. Queue association is in PM thread state; window association uses
a property on an own-process HWND. A zero table detaches the association.

FCF_ACCELTABLE is `0x8000`. Standard windows load an automatic table and
destroy it on WM_NCDESTROY. Dispatch handles virtual keys before native
TranslateMessage, so a handled key does not generate a duplicate character.
Queue matches take precedence, followed by window/ancestor tables. Command
ID zero is valid; it is used by a TELNETPM help entry. Modal dispatch and
native-child-to-PM-parent routing are covered by the implementation.

Keep `PMCompatWindow.proc` as the first field: existing callback dispatch
also relies on the first-field layout of PMCompatDialog. R6 adds auto_accel
at the end. Do not reinterpret arbitrary native control GWL_USERDATA as
one of these PM structures.

RT_MESSAGE and RT_STRING decoding uses sixteen-entry bundles: a codepage
WORD followed by byte lengths that include each string's terminator.
Output is bounded and reserves a NUL. All four RT_MESSAGE bundles in the
target EXE were checked against this representation.

GpiQueryFonts returns the number of records **not copied** and writes the
number copied through the count pointer. A count-only query permits zero
capacity, zero stride and a null output buffer. It enumerates families and
then named family styles/raster sizes. The same enumeration is used for
counting and copying; allocation is owned by the caller.

The guest FONTMETRICS is 228 bytes: a 208-byte base, two LONG name atoms and
twelve PANOSE bytes. TELNETPM inspects average width at offset 100, baseline
extent at 112, fsType at 144 and fsSelection at 148. Its preferred startup
font is regular fixed-pitch, width 8, baseline extent 12. R6 fills the
metrics used by this selection, writes the caller's stride, and preserves
the older prefix layout. Availability of the preferred font is host-specific.

## Validation and reproduction

Build environment: i686 MinGW GCC 13.2.0, C89, GNU make. Production DLLs
and native smoke programs need no Python. There is no Windows runtime in
the build worker; Windows results below came from the user's supplied logs.

| Check | Result and environment |
|---|---|
| PMWIN, PMGPI, API smoke build | PE32/i386 cross-build succeeds |
| Accelerator/text helper | 290 PASS, production helper with mocked Win32 state |
| Font-query helper | 23 PASS, production enumerator with controlled GDI fixtures |
| Existing profile/switch helpers | 81 / 281 PASS |
| Existing bridge | 285 PASS, production x86 bytes under Unicorn |
| API catalogue | 347 entries cover 345 ordinal exports |
| R6 native section | 39 PASS on Windows |
| Main API smoke | 131 PASS on Windows; includes the nested R6 section as one check |
| R5 native sockets, unchanged in R6 | 95 PASS on Windows |
| Live application | Remote banner, typed login, server response; rendering defects |

The main and nested smoke counts describe different counters; do not
present their sum as 170 independent top-level checks. The DLL cross-build
retains three pre-existing warnings: two FARPROC function-pointer casts
and unused make_bmi. No new warning sites were introduced by R6.

On a build host with native C compiler and Python 3:

```sh
make telnetpm-api-host-check catalog-check
make telnetpm-bridge-check TELNETPM=/path/to/old/telnetpm.exe
```

The bridge test additionally requires Unicorn. Use the existing regression
tests when changing shared PM behavior; the R6 evidence does not establish
that every earlier PM application was retested on Windows.

For the packaged runtime, open CMD beside the runners:

```bat
run-api-smoke.cmd
run-telnetpm.cmd C:\OS2\tmp\telnetpm-phase1\telnetpm.exe
```

The scripts prefer their bin directory, then C:\OS2, then inherited PATH.
Other baseline DLLs, including HELPMGR/NLS as needed, remain supplied by the
existing installation. Raw R5/R6 logs and five screenshots are in the
archive's evidence directory; validation contains the local build/test
logs and patch/ordinal checks. The original shipped runtime archive was
`telnetpm-phase2-r6-pm-init.zip`, SHA-256
`d7d49a9ab13d9fe8a84c73d10579ea3452d2ce2568f9b7b3121b6a13dc160be8`.

## Remaining boundaries

Seven imports remain stop-on-call on the observed installation:
`DOSCALLS.300`, `PMWIN.890`, `PMCTLS.4`, `DOSCALLS.111`, `DOSCALLS.110`,
`DOSCALLS.356`, `MSG.6`. None was called in the supplied R6 session log.
Their mere presence in the deferred list is not an execution failure.

The terminal display has overlapping text, stale pixels after repainting,
and incorrect characters in ANSI artwork. Resizing/scrolling behavior and
cell geometry need further work. The supplied log ends in the message
loop, without a normal process-exit record; clean shutdown is not proven.

Accelerator AF_LONEKEY, non-ASCII codepage conversion and custom
WM_QUERYACCELTABLE overrides are not implemented. Text-resource bytes are
returned without codepage conversion. Installed GDI fonts form the public
font set; private-only enumeration is empty. Some metrics are approximated;
unmapped fields, name atoms and PANOSE remain zero. External authentication
DLLs and general mixed 16/32-bit execution remain outside the exact bridge.

## Next session: rendering work

Keep this R6 source/binary set as the preservation point. Start a separate
change for rendering and retain the passing socket/loader behavior.

1. Trace the guest's font creation arguments and return values. The run
   contains 4,800 GpiCreateLogFont calls and 4,800 WinGetLastError calls,
   but no GpiSetCharSet entry. This needs call-site analysis; do not infer
   successful font creation or selection from import-entry tracing alone.
2. Correct background-mix semantics. The existing GpiSetBackMix ignores
   both arguments and returns 1, while GpiCharStringAt unconditionally uses
   TRANSPARENT. These are confirmed implementation gaps and likely relevant
   to stale terminal pixels, but not a complete diagnosis of the screenshots.
3. Verify font/cell selection, baseline placement and CP437 glyph conversion
   together. GpiCharStringAt currently forwards bytes to TextOutA. Inspect
   the selected GDI face/charset and guest codepage before changing it.
4. Inspect WinScrollWindow clipping/update handling and repaint after resize.
   Check the shared presentation-space structure in pmcompat.h before adding
   state so PMWIN and PMGPI continue to agree on its layout.
5. Add focused pixel/layout checks for text replacement, fixed cells and
   scrolling, then repeat the live BBS test and relevant existing PM samples.

For a short diagnostic run, the existing DLL tracing switch is
`set OS2_PM_TRACE=1`; unset it with `set OS2_PM_TRACE=` afterward. It can
produce a large log. No additional trace was collected for this handoff.

WHP/V2 and the ReactOS subsystem remain unchanged. Do not solve rendering
by disabling the exact-image guard or changing the networking ABI.

## References used for R6

* [IBM toolkit headers](https://github.com/bitwiseworks/os2tk45/tree/master/h)
* [IBM WinQueryAccelTable reference mirror](https://komh.github.io/os2books/os2tk45/pm2/1764_L2H_WinQueryAccelTableSy.html)
* [IBM WinSetAccelTable reference mirror](https://komh.github.io/os2books/os2tk45/pm2/2718_L2H_WinSetAccelTableSynt.html)
* [IBM WinLoadMessage reference mirror](https://komh.github.io/os2books/os2tk45/pm2/1431_L2H_WinLoadMessageSyntax.html)
* [IBM GpiQueryFonts reference mirror](https://komh.github.io/os2books/os2tk45/gpi2/1594_L2H_GpiQueryFontsSyntax.html)
* [Microsoft EnumFontFamiliesA](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-enumfontfamiliesa)
* [Microsoft TEXTMETRICA](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/ns-wingdi-textmetrica)
