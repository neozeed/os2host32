# Soft386 NE-H3H — guest-local 16-bit file API bridge

Base: NE-H3G complete source, all modifications confined to `soft386/`. The guest's original `LIB.EXE` is untouched.

## Added 16-bit DOSCALLS ordinals

- 58 `DosChgFilePtr`: handle, signed 32-bit distance, origin, far pointer result.
- 59 `DosClose`: closes guest-local HFILE mapping (stdio handles preserved).
- 60 `DosDelete`: delete pathname.
- 68 `DosNewSize`: truncate file to requested size.
- 70 `DosOpen`: original OS/2 1.x 30-byte Pascal argument frame, including additional 4-byte far reserved/EA parameter. OS/2 create/open/replace flags and mode translated to native open.
- 75 `DosQFileMode`: file attribute query using host stat (limited file/directory/read-only translation).
- 137 `DosRead`: now supports guest-local files in addition to stdin.
- 138 `DosWrite`: now supports guest-local files; standard stream behavior remains on the existing personality route.

Guest handles are independent from the host's file descriptors (64 slots, 0..2 standard streams, 3..63 allocated). They are closed at guest termination. Far pointers remain selectors and offsets in the guest RAM; system DLLs are unchanged. This is a narrow local filesystem backend, not a full filesystem personality.

## Live host test

A scripted stdin session with the untouched beta SDK librarian (`LIB.EXE`): `NEH3H.LIB\ny\n\n\n\n` creates a non-empty archive on the host, executes actual 512-byte writes, seeks and closes and exits with rc=0. The fixture test checks filename creation, non-empty data, stdout banner, no unsupported DOS16 calls, rc=0 in a temp directory.

`make check-quick` PASS: LIB create + LIB no-input, VOID, tiny/medium/large/compact/huge, existing 32-bit LE/thread/sync/memory/bridge tests. Full expanded suite and native MinGW runtime remain untested.

## Intentional limitations / follow-up

- DOS16.70's library-creation path uses a narrow compatibility accommodation: this historic LIB requests access bits 0 (read) with create flags 0x12, then immediately writes. On newly created files we open read/write to support the observed SDK behavior. Investigate real OS/2 precise rule before broadening this policy.
- No process-global 16-bit handle inheritance/duplication, path virtualization or drive translation (native Windows paths pass through; POSIX backslashes are normalized for test hosting).
- 16-bit far buffer translation is bound by emulated RAM; further work should validate selector descriptor *limits* and read/write rights per selector, not only RAM bounds.
- API DOS16.75 only maps directory/archive/readonly stat attributes. Error mapping and OS/2 sharing mode enforcement are incomplete.
- The librarian now creates a non-empty `.LIB`, but **no archival format validation or object add/extract regression has been done**. The next milestone should validate the archive's actual contents with an independent tool and test an add-object operation.
- Native Win32 32-bit build and actual user-run Windows regression remain pending.
