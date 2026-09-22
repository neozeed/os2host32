# Milestone 28A4 — FILEFINDBUF3 ABI correction

M28A3 proved that a genuine C/386-built LX client can include `os2.h`, link
`OS2386.LIB`, import DOSCALLS by ordinal and execute through OS2HOST32.  Its
first real `DosFindFirst` also exposed a hidden ABI bug in the compatibility
DLL.

The M28A native-only path had used a private find-buffer layout beginning with
the creation date.  The actual 32-bit OS/2 `FILEFINDBUF3` begins with a 32-bit
`oNextEntryOffset`.  Since both the native producer and native decoder omitted
that field, all M28A native CMD regressions passed while the layout was still
wrong for a genuine OS/2 client.

The symptom from the M28A3 smoke test was diagnostic: an actual 1147-byte
`CHANGES-M17.txt` appeared as `GES-M17.txt` with size 4096.  Reading every
field four bytes late explains both observations exactly: `cbFile` landed on
`cbFileAlloc`, and `achName` began at the fifth filename character.

M28A4 fixes the producer and updates the native bootstrap decoder so existing
CMD behavior remains unchanged.  Rebuild `DOSCALLS.dll`, rerun the normal M28A
regressions, then rebuild/run `cmdos2_os2_smoke.exe`.  The smoke result should
now show the full filename and its logical file size, followed by
`M28A3_OS2_BACKEND_OK`.
