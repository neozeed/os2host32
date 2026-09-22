# Milestone 26 - recovered `cfile.c` boundary

M26 makes the mundane-but-essential shell commands real:

```text
COPY
DEL / ERASE
REN / RENAME
MOVE
```

They occupy the same recovered command-module boundary as the Dec-1991 NT CMD:
`eCopy`, `eDelete`, `eRename`, and `eMove` in `cfile.c`, with COPY's larger
engine historically split into `cpwork.c`.

The implementation remains C89-oriented for the eventual MSC 8 / VC1 build.

## Quick test

Run `examples\\m26-file-test.cmd` from a disposable directory.  It creates an
`m26work` tree, exercises single COPY, RENAME, MOVE, wildcard COPY and wildcard
DEL, then removes the test tree.

Interactive examples:

```text
copy one.txt two.txt
copy *.txt backup
ren old.txt new.txt
move new.txt archive
del archive\\*.txt
```

## Deliberate limits

This milestone does not fake features whose recovered implementation is more
complex.  `COPY a+b out` and wildcard RENAME transformations report that they
are not implemented yet.  Those can be lifted from the recovered `cpwork.c`
and `RenWork` paths later.
