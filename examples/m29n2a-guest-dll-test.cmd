echo === M29N2a real LE/LX guest DLL loader regression ===
set OS2LIBPATH=m29n2a-dll;.
set OS2_TRACE_MODULES=1
m29n2a-import
if errorlevel 1 echo M29N2A_GUEST_DLL_CHAIN_FAILED
if not errorlevel 1 echo M29N2A_GUEST_DLL_CHAIN_SCRIPT_OK
