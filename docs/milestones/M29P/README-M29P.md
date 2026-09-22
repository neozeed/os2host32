# Milestone 29P — file builtins and small-stack hardening

M29P is the final planned CMD compatibility pass after M29O/O1.  It stays out
of the loader, SESMGR, guest-DLL manager and far16 bridge code and concentrates
on the mundane file-command paths that a real command processor exercises all
the time.

## COPY no longer consumes the historical stack

The old reconstructed `CmdFileCopy()` declared both of these workspaces as
automatic arrays:

```
192 x 1024 byte COPY tokens
 64 x 1024 byte source names
```

plus a destination buffer.  That is about 257 KiB of automatic storage before
the nested 32 KiB transfer buffer in `cf_append_binary_file()` is entered.  It
was completely inappropriate for the deliberately historical 60 KiB C/386
shell stack.

M29P moves both the parser/source workspace and the 32 KiB transfer buffer to
heap storage, releases them on every success/error path, and leaves the shell
stack for control flow rather than bulk file I/O.  `TYPE` receives the same
treatment for its transfer buffer, and the simple path/info builtins use
`MAX_PATH`-sized automatic paths rather than 4 KiB command-line buffers.

A host compiler stack-usage audit of this source reports `CmdFileCopy()` at
about 3.5 KiB rather than hundreds of KiB.  That number is only a host-side
sanity check; the real authority remains the Microsoft C/386 build on the test
machine.

## Filesystem compatibility fixes

M29P also tightens several common file-command cases:

* `REN` / `RENAME` implements wildcard destination transformations such as
  `ren *.txt *.bak`, including `?` and long filename components.
* `COPY file file` is rejected before the destination is opened, preventing a
  self-copy from truncating its source.
* `DIR /B` emits bare names and is suitable for redirection/batch use.
* `DIR /A` includes hidden/system entries; ordinary DIR filters those entries.
* quoted destination directories, wildcard COPY/DEL/MOVE, binary COPY and
  binary concatenation remain on the OS/2-facing file boundary.

This is still a semantic reconstruction, not source-identical Microsoft code.
`COPY /A`, wildcard sources inside a `+` concatenation, the more elaborate DIR
switch family (`/P`, `/W`, attribute selector expressions), and backend-heavy
commands such as DATE/TIME/VOL/CHCP remain explicit rather than guessed.

## Regression

Build normally on the C/386 machine:

```
make
build-os2-shell.cmd
```

Run the new regression from either the tree root:

```
examples\m29p-filesystem-test.cmd
```

or from the examples directory:

```
cd examples
m29p-filesystem-test.cmd
```

Run it once in native `cmd32os2.exe` and once in the hosted C/386 shell:

```
os2host32.exe --run cmd32os2_os2.exe
```

The test deliberately performs a large binary COPY of `cmd32os2_os2.exe`; even
a tiny COPY would enter the formerly giant stack frame, but the executable
copy also exercises repeated transfer-buffer reads/writes.

A successful run contains:

```
M29P_HEAP_COPY_OK
M29P_WILDCARD_RENAME_OK
M29P_DIR_BARE_OK
M29P_SELF_COPY_GUARD_OK
M29P_COPY_CONCAT_OK
M29P_MOVE_OK
M29P_TYPE_OK
M29P_FILESYSTEM_OK
```

The self-copy case intentionally prints an error and must return non-zero; the
source must still exist afterward.

## Host checks used for this bundle

The bundle was checked with:

```
make m29p-file-check
make boundary-check
make parser-check
make batch-check
make m29o-batch-check
make m29o1-parser-check
make env-check
make pipeline-boundary-check
make console-boundary-check
```

`m29p-file-check` includes direct unit tests for the wildcard rename transform
and a source guard that rejects reintroduction of the giant COPY automatic
arrays/transfer buffer.
