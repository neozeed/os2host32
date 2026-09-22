# Dec-1991 NT CMD `cfile.c` / `cpwork.c` recovery notes

Milestone 26 moves the ordinary file-manipulation commands out of the generic
CMD32 bootstrap and into a dedicated C89-oriented `cmdfile.c` layer.

The Dec-1991 NT CMD COFF symbols preserve the original Microsoft source-module
boundary:

| RVA | Symbol | Original module | M26 role |
|---:|---|---|---|
| `0001E5CA` | `_eCopy` | `cfile.c` | command wrapper |
| `00017AF4` | `_copy` | `cpwork.c` | COPY engine |
| `00017D4C` | `_do_normal_copy` | `cpwork.c` | ordinary COPY path |
| `00018560` | `_do_combine_copy` | `cpwork.c` | `a+b` concatenation path |
| `0001E5E2` | `_eDelete` | `cfile.c` | command wrapper |
| `0001E5FA` | `_DelWork` | `cfile.c` | delete implementation |
| `0001EE02` | `_eRename` | `cfile.c` | command wrapper |
| `0001EE16` | `_RenWork` | `cfile.c` | rename implementation |
| `0001F4A6` | `_eMove` | `cfile.c` | MOVE wrapper/parser |
| `0001F516` | `_MoveParse` | `cfile.c` | MOVE argument parser |
| `0001F996` | `_Move` | `cfile.c` | MOVE implementation |

The wrappers are small and feed the command's return value back into CMD's
process-wide errorlevel state.  For example `_eDelete` passes the parsed command
tail to `_DelWork`, then stores EAX in the global errorlevel.  `_eRename` follows
the same shape with `_RenWork`.  `_eCopy` delegates immediately to `_copy` in
`cpwork.c`, demonstrating that COPY was already large enough to be factored into
its own implementation module.

M26 is a semantic reconstruction, not a source-identical decompilation.  It
implements the common OS/2-shell cases first:

- `COPY source destination`
- wildcard COPY when destination is a directory
- `DEL` / `ERASE`, including wildcard filespecs
- `REN` / `RENAME` for a single source/destination
- `MOVE`, including wildcard moves into a directory
- cross-volume file MOVE fallback as copy+delete

Explicitly deferred rather than guessed:

- COPY concatenation (`a+b`), despite the recovered `_do_combine_copy` evidence
- wildcard RENAME transformations
- historical prompt/switch details
- multi-source MOVE grammar beyond a wildcard filespec into a directory

`NT1991_CFILE_DISASM.txt` contains the complete disassembly of the recovered
`cfile.c` range used for this milestone.
