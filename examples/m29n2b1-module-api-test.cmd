echo === M29N2b.1 runtime module-manager regression ===
set OS2LIBPATH=m29n2a-dll;.
set OS2_TRACE_MODULES=1
m29n2b1-dynamic
if errorlevel 1 echo M29N2B1_DYNAMIC_MODULE_API_SCRIPT_BAD
if not errorlevel 1 echo M29N2B1_DYNAMIC_MODULE_API_SCRIPT_OK
