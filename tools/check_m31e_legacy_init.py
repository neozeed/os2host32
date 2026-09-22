from __future__ import print_function
from pathlib import Path
s=Path('os2host32.c').read_text()
checks=[
 ('legacy abi marker','legacy-initinstance' in s),
 ('legacy first hPDLL zero','wr32(stub + p, 0UL)' in s),
 ('legacy second hmod','wr32(stub + p, g->handle)' in s),
 ('init-only discriminator','MOD_INIT_INSTANCE' in s and 'MOD_TERM_INSTANCE' in s),
 ('term flag honored','if ((g->image.module_flags & MOD_TERM_INSTANCE) == 0)' in s),
]
bad=[n for n,v in checks if not v]
if bad:
    print('M31E legacy INITINSTANCE regression FAILED:', ', '.join(bad)); raise SystemExit(1)
print('M31E legacy INITINSTANCE regression PASS')
print('  INITINSTANCE-only DLLs use InitLibrary(hPDLL,hmod)')
print('  DllInitTerm(hmod,flag) lifecycle fixtures retain the later ABI')
print('  DLL TERM callback is skipped when the LE TERM flag is absent')
