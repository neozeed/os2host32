# Soft386 NE-H3L — 16-bit NLS bridge

Parent: NE-H3K. Focus: original OS/2 1.x NLS module ordinal 4,
`DosGetDBCSEv(USHORT cbBuf, PCOUNTRYCODE pctryc, PCHAR pchBuf)`.

Only `soft386/src/soft386_os2.c` and the LINK386 regression are modified.
The existing native 32-bit system DLLs are unchanged. This milestone dispatches
LINK386's module index 3 to the new 16-bit NLS adapter. Module-name-aware
resolution for other NE images is still future work; index 3 must not be
assumed to denote NLS for arbitrary future programs.

Arguments: 16-bit Pascal far-call stack: FAR output at +4, FAR countrycode
at +8, WORD size at +12, 10 argument bytes cleaned on far return. Countrycode
is two 16-bit words (country, codepage). Guest segment pointers are checked.
Call goes to existing common `os2_nls_query_dbcs_env`, retaining its country,
codepage, SBCS/DBCS semantics and return errors; no separate NLS tables.

Live host-run discovery: LINK386 prints its 1.01.015 banner, prompts for object
modules, reports L1020 on stdin EOF and exits through Dos16Exit(rc=2).
Unlike H3J, it no longer halts at NLS ordinal 4. Later startup still logs
`unsupported DOS16.91` (`Dos16GetEnv`) and needs proper implementation.

Regression: `make check-ne`; LINK386 test confirms banner, parser,
normal no-argument error, clean process termination, and no NLS.4 failure.
Native Windows runtime pending user verification.
