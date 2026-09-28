# Life OS/2 import / bridge map

The supplied `lifeos2.c` defines `INCL_16`, so Microsoft C/386 emits historical far16 migration thunks for the classic Base APIs. The loader continues to treat those thunks as metadata and patches their 32-bit helpers to native host facades; no 16-bit CPU execution is added.

| API | Historical target | C/386 Pascal-frame widths, low-to-high | Status |
|---|---:|---|---|
| VioScrollUp | VIOCALLS.7 | 2,4,2,2,2,2,2 | existing bridge preserved |
| VioGetCurPos | VIOCALLS.9 | 2,4,4 | existing bridge preserved |
| VioWrtCharStr | VIOCALLS.13 | 2,2,2,2,4 | new Life bridge |
| VioSetCurPos | VIOCALLS.15 | 2,2,2 | existing bridge preserved |
| VioWrtTTY | VIOCALLS.19 | 2,2,4 | existing bridge preserved |
| VioGetMode | VIOCALLS.21 | 2,4 | existing bridge preserved; Life does not call it |
| VioGetCurType | VIOCALLS.27 | 2,4 | new Life bridge |
| VioSetCurType | VIOCALLS.32 | 2,4 | new Life bridge |
| VioWrtCharStrAtt | VIOCALLS.48 | 2,4,2,2,2,4 | new Life bridge |
| KbdCharIn | KBDCALLS.4 | 2,2,4 | existing bridge preserved |
| DosSleep | DOSCALLS.32 | 4 | new narrow Life prerequisite bridge; host export forwards to existing flat DosSleep implementation |

The VIO ordinal values come from the supplied historical SDK `bseord.h`. `DOSCALLS.32` is the 16-bit `DosSleep`; `DOSCALLS.229` remains the existing flat 32-bit `DosSleep`.

## Source-level Life usage

Life uses screen clear via `VioScrollUp(...,0xffff,0xffff,0xffff,...)`, direct row/column positioning, cursor hide/show, raw character bytes 1/2, one-byte text attributes, nonblocking `KbdCharIn(IO_NOWAIT)`, arrow/Insert/Delete scan codes, and `DosSleep`.

## Build / scan status

The exact supplied source is copied to `tests/vio/lifeos2.c` unchanged (SHA-256 `d6e0b5f7bdd0b8b23d48913e91186a1da6d8409604123964274677e9e1945119`). Build instructions are in `scripts/legacy/build-lifeos2.cmd`. This worker environment does not contain Microsoft C/386/LINK386, so no fresh LE import scan or Life runtime result is claimed.
