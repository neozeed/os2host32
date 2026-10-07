# Soft386 R5A — native-service routing and NEKO intake

Date: 2026-10-04. Base: the R5 source/PM/DLL package from this conversation.
This is a corrective milestone prompted by the user's live Windows traces.
HANOI was reported working with R5; BIO and NEKO GUI execution under R5A has
not been verified on Windows here.

## Root cause of the misleading header error

Both uploaded BIO files are identical: 72,298 bytes, SHA256
`816c96f22f913531c782df91677aa7c05cdbf8ed81ee6c67cfe42ac8c2954b2d`.
They have a valid 32-bit LE header at file offset 0x400.

R5 did not classify PMSHAPI as a native system service. BIO imports it, so the
guest DLL loader searched OS2LIBPATH. Without a matching file it reported a
missing guest DLL. With a native PE PMSHAPI.DLL present, it instead tried to
parse that DLL as LE/LX and emitted the generic "input is not an LE/LX
executable" message. That message did not identify which file failed.
The same failure was reproduced locally with a PE DLL named PMSHAPI.DLL on
OS2LIBPATH. It was not caused by BIO's header or by a bad environment script.
The earlier suggestion that the BIO copies differed was incorrect.

R5A adds explicit system routes for PMSHAPI, HELPMGR and PMWP. Header failures
now include the file being loaded, file size, new-header offset and signature
bytes. A PE image is identified as a native image requiring an explicit bridge.
Other unbridged services can still fail; the diagnostic now identifies the
actual dependency instead of implicating the main EXE. WMCHAR and the Sarien
binaries were not supplied in this turn, so their exact dependency is unverified.

## New service boundary

All existing DLL sources remain unchanged. Only soft386/ sources are changed.

| Service | R5A behavior |
| --- | --- |
| PMSHAPI.101/.102/.103 | Query size, open and close profiles; typed HINI tokens |
| PMSHAPI.114/.116 | Query integer and write string; host copies of strings; native SHORT result normalized |
| PMSHAPI.117/.118 | Bounded binary profile reads/writes, including size-query copy-back |
| PMSHAPI.120/.129 | Add/remove switch entry; 96-byte flat SWCNTRL copy with translated window handles; typed HSWITCH |
| HELPMGR.51/.52/.54 | Create, destroy and associate typed help instances; HELPINIT strings copied, return code copied back |
| PMWP.203 | Delegate to the jar's DosLoadModule operation, preserving guest module/resource ownership |

PMSHAPI's predefined profile handles remain constants; opened profiles get
private jar tokens. A stale or wrong-kind token is rejected. Switch entries
with nonzero hprog are not supported. HELPMGR retains the existing backend's
limited instance-lifetime behavior; it does not implement IBM IPF rendering.
HELPINIT supports null or resource-ID help tables, not guest in-memory tables.
The new native DLL path overrides are --pmshapi-dll and --helpmgr-dll, with
SOFT386_PMSHAPI_DLL and SOFT386_HELPMGR_DLL environment equivalents. They are
propagated to child jars in the same way as the existing PM paths.

PMWP.203 must not call native DOSCALLS to load NEKO.DLL: its LE/LX objects and
HMODULE belong to the jar. This route works without loading native PMWP.dll.

## NEKO page and resource fix

NEKO.EXE is LX. Pages 5 and 6 use iterated records (page-map flag 1), expanding
to 4,096 and 3,244 bytes. NEKO.DLL has 12 pages: 11 iterated and one ordinary.
It has no code entry point and contains 36 resources in three objects whose
preferred addresses are all zero.

R5A ports the native loader's bounded repeat-count/pattern-length decoder to
the jar. It uses the file-relative iterated-data section and LX page shift,
checks encoded bounds and expansion size, and rejects trailing data. Unsupported
page encodings now report the page number and flag. LE iterated pages and LX
compressed/range pages remain unsupported.

Each DLL resource object receives a distinct guest allocation. This avoids
overwriting earlier resource objects that share preferred address zero. Normal
DLL objects retain their relative layout; overlapping normal objects fail.
--check now accepts a resource-only DLL and reports its resource count.

## Validation

- Full GCC host `make -C soft386 check`: R2C, R3, R4, R5 and R5A pass.
- New synthetic CPU test: PMWP.203 loads an iterated resource DLL; DosGetResource
  reads different marker data from two independently mapped zero-base objects.
- Tests reject zero-repeat, over-expansion, truncated and out-of-file streams.
- Native-service routing is tested with PE names shadowing OS2LIBPATH; an unknown
  PE guest dependency must fail with its actual filename and PE diagnosis.
- Real supplied BIO, HANOI, NEKO and NEKO.DLL pass loader checks. NEKO.DLL yields
  36 validated resource copies. These checks do not execute their GUI code.
- PM boundary tests cover profile strings/data/size outputs, signed short returns,
  stale HINI/HSWITCH/help handles, HELPINIT pointer copying and buffer bounds.
- AddressSanitizer/UndefinedBehaviorSanitizer boundary run passes; leak checking
  is disabled because the environment cannot inspect the process task list.
- i686 MinGW cross-build passes. The vessel imports only KERNEL32 and msvcrt.
  PMSHAPI.dll and HELPMGR.dll in the kit are built from unchanged source.

Evidence is under validation/r5a/. Historical R5 evidence remains under r5/.
No Wine or display-backed Windows execution was available here.

## Windows use

Extract soft386-R5A-win32-smoke.zip into a fresh directory. Run RUN-R5A.cmd to
check the generated resource-loader fixture and the previous R5 smoke tests.
The kit requires no Python or compiler. VIEW-R5.cmd retains its five-second
PM display test.

For the existing C:\OS2 installation, replace C:\OS2\soft386_os2.exe with
soft386\soft386_os2.exe from the kit. Keep the original native system DLLs and
the existing env.cmd. PATH locates native DLLs; OS2LIBPATH locates LE/LX user
DLLs after system-module classification. NEKO.DLL must retain that filename
and be beside NEKO.EXE or on OS2LIBPATH.

From C:\OS2\demos:

```bat
..\soft386_os2.exe --check BIO.EXE
..\soft386_os2.exe --check NEKO.EXE
..\soft386_os2.exe --check NEKO.DLL
..\soft386_os2.exe --max-cycles 0 --trace-hc --trace-native BIO.EXE 2>bio-r5a.trace
..\soft386_os2.exe --max-cycles 0 --trace-hc --trace-native NEKO.EXE 2>neko-r5a.trace
```

Use NEKO.DLL's actual path if it lives in an OS2LIBPATH directory. The normal
run banner identifies R5A. --check performs loading/fixups without native GUI
execution and now marks imports with no PM marshaller. A successful check is
not proof of runtime API completeness.

## Remaining application work

R5A fixes intake and selected service routes. WinSubclassWindow, WinDlgBox,
clipboard operations and several drawing/dialog/control helpers are still
unbridged. BIO and NEKO import some of these, so their live behavior remains
an acceptance target. HANOI's DosEnterCritSec (.232) warning remains; guest
critical-section scheduling needs a separate implementation. One PM queue owner
per jar, retained DLL mappings, and the R5 callback lifecycle limits still apply.
WHP remains frozen.
