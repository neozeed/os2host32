# Milestone 27 - COPY /B and binary concatenation

M27 extends the recovered Dec-1991 `cfile.c` / `cpwork.c` boundary with the historically important binary COPY syntax.

`/B` is accepted before, after and between COPY operands, including `copy /b source.exe destination.exe`, `copy /b part1.bin+part2.bin whole.bin`, and `copy part1.bin /b + part2.bin /b whole.bin /b`.

M26's ordinary `CopyFileA` path was already byte-for-byte safe. M27 adds historical `/B` syntax plus a real binary concatenation path using `CreateFileA` / `ReadFile` / `WriteFile`; byte 1Ah (Ctrl-Z), NULs and high-bit bytes are not interpreted.

`COPY /A` is deliberately rejected until its historical text/EOF semantics are reconstructed. Wildcards inside a `+` source list are also deferred.

The `examples` directory includes exact binary fixtures and `m27-copy-binary.cmd`. From ordinary Windows CMD, verify the result with `fc /b m27-combined.bin m27-expected.bin`.

A particularly useful OS/2 test is `copy /b args.exe args-copy.exe` followed by `args-copy one two three`.
