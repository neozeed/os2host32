echo === M29N2b.2 DLL init/term lifecycle regression ===
set OS2LIBPATH=m29n2b2-dll;.
m29n2b2-dynamic.exe
if errorlevel 1 echo M29N2B2_DYNAMIC_FAILED
if errorlevel 1 goto done
m29n2b2-static.exe
if errorlevel 1 echo M29N2B2_STATIC_FAILED
if errorlevel 1 goto done
echo M29N2B2_LIFECYCLE_SCRIPT_OK
:done
