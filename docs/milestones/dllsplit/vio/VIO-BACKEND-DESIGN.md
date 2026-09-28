# OS2HOST32 VIO R2 backend design

## R2 choice: common text-cell image

Phase 1 proved the Life-required Win32 VIO surface.  R2 now makes the
character/attribute screen image a first-class OS/2-side session concept.
Win32 remains the only backend and renderer.

The common state owns:

- current rows and columns;
- cursor row and column;
- OS/2 cursor type/visibility representation;
- default attribute;
- a dynamically-sized `Os2VioCell { character, attribute }` image;
- validity/lifetime state for that image.

This is still deliberately small.  There is no PM/AVIO state, ANSI parser,
VioRegister router, font model, alternate backend, or real 16-bit execution.

## Layering

```text
VIOCALLS exports
  -> common/vio/os2_vio.c             OS/2 validation/state/semantics
       -> Os2VioBackendOps
            -> common/win32/os2_vio_win32.c
                 Win32 Console renderer / snapshot provider
```

`dlls/viocalls/viocalls.c` remains a thin ABI facade and contains no Win32
API calls.  `common/include/os2_vio.h` contains no Win32 types.

## Cell-image initialization and resize

The backend interface gains one state-seeding operation:

```text
read_cells(cells, rows, columns)
```

On first screen-oriented VIO use, the common layer queries the actual console
buffer dimensions, allocates exactly `rows * columns` cells, and snapshots the
current host characters and attributes.  R2B deliberately does **not** poll
the host size on every VIO operation.  `VioGetMode` is the explicit resize
observation point; if it sees new dimensions the common buffer is resized and
re-snapshotted rather than assuming 80x25.

The Win32 implementation reads each console row with
`ReadConsoleOutputCharacterA` and `ReadConsoleOutputAttribute`.

## Direct writes

`VioWrtCharStr` remains a direct screen operation:

1. common layer validates HVIO/coordinates and clips to the screen extent;
2. Win32 backend renders the bytes with `WriteConsoleOutputCharacterA`;
3. only after successful rendering, the common cell image updates the
   character bytes while retaining existing attributes;
4. cursor position is unchanged.

`VioWrtCharStrAtt` follows the same flow but updates both character and
attribute state.  The Win32 backend still maps the attribute for rendering;
the common image keeps the original OS/2 byte.

## Scrolling

`VioScrollUp` keeps the proven Win32 renderer for compatibility, including the
Life full-clear form using `0xffff` bounds/count.  After a successful backend
scroll, the common layer performs the identical cell move/fill on its own
character/attribute image.  Partial scrolling is regression-tested as well as
full-screen clearing.

This ordering means a failed backend mutation is not falsely committed to the
OS/2-side state.

## TTY path

`VioWrtTTY` intentionally remains distinct from cell writes.  R2 does not add
an ANSI/VT/control parser and does not replace the working stream path.

The backend therefore still performs the proven `WriteFile`.  R2B makes this a
strict hot path: it performs **no** size poll, cursor poll, or screen-buffer
readback merely because bytes were written.  If a common cell image had already
been initialized, the write marks that image dirty because CR/LF/BS/wrapping or
scrolling may have changed cells.  `VioGetCurPos` queries the real backend cursor
lazily when a caller actually asks for it.

This is important for correctness as well as speed: `DosWrite(stdout)` is not
yet routed through VIO and can also move the real console cursor, so cursor
queries must remain backend-observed rather than blindly trusting a VIO-only
cache.  Redirected stdout retains the Phase-1 behavior because successful
`WriteFile` does not require a console screen buffer.

## Cursor mapping

OS/2 exposes scan-line start/end plus `attr`; Win32 exposes visibility and
percentage height.  The Win32 backend retains the stable 16-scan-line
approximation.  `attr == 0xffff` maps to hidden.  Exact scan-line equivalence
is not representable by Win32 Console.

## Attribute mapping

The common cell image stores the original OS/2 attribute byte.  Rendering maps
the low seven IBM text attribute bits directly to Win32 console bits.  OS/2
bit 7 is blink whereas Win32 interprets the same bit as background intensity;
the renderer drops bit 7 rather than changing blink into a bright background.

## Deliberate boundary: DosWrite

This milestone does not route `DosWrite(stdout)` through VIO.  Consequently an
out-of-band console mutation performed by DOSCALLS or another host writer can
change the real console without notifying the VIO cell image immediately.
R2B therefore treats the common cell image as optional/dirty after TTY or
out-of-band stream output rather than paying for an eager whole-screen readback.
A future stdout/VIO integration milestone can provide precise shared mutation
notification/routing if a read-side VIO API requires it.

Visible rendering remains backend-driven, so this limitation does not change
the proven Phase-1 output behavior.

## Backend interface after R2

The backend remains intentionally compact:

- query size;
- get/set cursor position;
- get/set cursor type;
- snapshot cells;
- write characters at coordinates;
- write characters plus attribute at coordinates;
- scroll up;
- write TTY stream.

No speculative callbacks were added for unimplemented APIs.
