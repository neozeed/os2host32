# M31G R17 - NLS.5 DosQueryCtryInfo + NEKO resource-DLL path clarification

R16 fixed the generic zero/duplicate-base LX DLL layout needed by NEKO.DLL.
The next Windows run from a new R16 directory returned PMWP.203/DosLoadModule
rc=126 before any `GUESTMOD LOAD` line. This is lookup failure, not execution or
mapping failure.

NEKO.DLL remains a resource-only V1 target in practical terms: it has no entry
object, no imports and no fixups, but OS/2 still loads the DLL as a module so a
real HMODULE can identify its RT_POINTER/menu/dialog/string resources. Loading
a module does not imply calling code; `entry object 0` means the compatibility
loader has no DLL init/term entry to execute.

Bare-name DosLoadModule follows the OS/2 library path. The host bridge already
adds `.dll` to `NEKO` and searches `OS2LIBPATH`; it intentionally does not
silently search beside the EXE. Include `.` explicitly when the resource DLL is
in the current directory, for example:

    set OS2LIBPATH=.;C:\cl386-research\os2_2.0\x\OS2\APPS

or put the actual directory containing NEKO.DLL in OS2LIBPATH.

The independent E.EXE path next reaches NLS ordinal 5. Historical 32-bit OS/2
bindings identify it as:

    APIRET DosQueryCtryInfo(ULONG cb, PCOUNTRYCODE pcc,
                            PCOUNTRYINFO pci, PULONG pcbActual)

R17 adds a real current-locale implementation. COUNTRYCODE/COUNTRYINFO use the
32-bit OS/2 layout (ULONG country, codepage and date-format fields; total
COUNTRYINFO size 44 bytes). Country/codepage zero map to the host user's locale
and OEM code page. Formatting fields come from Win32 locale data. Explicit
foreign country/codepage requests return ERROR_NO_COUNTRY_OR_CODEPAGE rather
than lying with host formatting data. Short buffers are partially filled and
return ERROR_NLS_TABLE_TRUNCATED.

No NEKO or E executable-name checks are introduced.
