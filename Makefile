# os2host32 cleaned-tree production build
#
# Builds only the active runtime deliverables from the reorganized source tree:
#   loader/os2host32.c             -> os2host32.exe
#   transformer/le2pe386.c         -> le2pe386.exe
#   shell/*                        -> cmd32os2.exe
#   dlls/*                         -> OS/2 compatibility DLL veneers/backends
#   common/*                       -> shared native/WHP personality semantics
#
# Historical milestone/regression build logic is preserved at:
#   docs/current/Makefile-M31-original

MINGW ?= i686-w64-mingw32-gcc
HOSTCC ?= cc
C89FLAGS ?= -std=c89 -O2 -Wall -Wextra -pedantic
COMMON_CPPFLAGS = -Icommon/include
API_CATALOG_SRC = common/api/os2_api_catalog.c
DOSCALLS_CORE_SRC = common/doscalls/os2_doscalls_core.c
WIN32_COMMON_SRC = common/win32/os2_win32_services.c

.PHONY: all loader whp transformer shell dlls compat verify common-check catalog-check wiring-check clean

all: loader transformer shell dlls

loader: os2host32.exe

# Alternative Win64/WHP loader; intentionally separate from the MinGW default build.
whp:
	$(MAKE) -C whp

transformer: le2pe386.exe
shell: cmd32os2.exe

compat: dlls

dlls: DOSCALLS.dll KBDCALLS.dll VIOCALLS.dll QUECALLS.dll SESMGR.dll NLS.dll \
      PMWIN.dll PMGPI.dll PMSHAPI.dll PMWP.dll HELPMGR.dll

os2host32.exe: loader/os2host32.c loader/os2host32.def
	$(MINGW) $(C89FLAGS) -Wl,--disable-dynamicbase,--image-base,0x400000 \
		-o $@ loader/os2host32.c loader/os2host32.def

le2pe386.exe: transformer/le2pe386.c $(API_CATALOG_SRC) \
                 common/include/os2_api_catalog.h common/api/os2_api_catalog.inc
	$(MINGW) -O2 -Wall $(COMMON_CPPFLAGS) -o $@ \
		transformer/le2pe386.c $(API_CATALOG_SRC)

cmd32os2.exe: shell/cmd32os2.c shell/cmdparse.c shell/cmdparse.h \
              shell/cmdbatch.c shell/cmdbatch.h shell/cmdfile.c shell/cmdfile.h \
              shell/cmdos2_win32.c shell/cmdos2_session_win32.c \
              shell/cmdos2_env.c shell/cmdos2_env.h shell/cmdos2.h
	$(MINGW) $(C89FLAGS) -Ishell -o $@ \
		shell/cmd32os2.c shell/cmdparse.c shell/cmdbatch.c shell/cmdfile.c \
		shell/cmdos2_win32.c shell/cmdos2_session_win32.c shell/cmdos2_env.c

DOSCALLS.dll: dlls/doscalls/doscalls.c dlls/doscalls/doscalls.def \
              $(DOSCALLS_CORE_SRC) $(WIN32_COMMON_SRC) \
              common/include/os2_personality.h \
              common/include/os2_doscalls_core.h \
              common/include/os2_win32_services.h
	$(MINGW) $(C89FLAGS) $(COMMON_CPPFLAGS) -shared -o $@ \
		dlls/doscalls/doscalls.c $(DOSCALLS_CORE_SRC) $(WIN32_COMMON_SRC) \
		dlls/doscalls/doscalls.def \
		-lwinmm -Wl,--out-implib,libdoscalls.a

KBDCALLS.dll: dlls/kbdcalls/kbdcalls.c dlls/kbdcalls/kbdcalls.def
	$(MINGW) $(C89FLAGS) -shared -o $@ dlls/kbdcalls/kbdcalls.c dlls/kbdcalls/kbdcalls.def \
		-Wl,--out-implib,libkbdcalls.a

VIOCALLS.dll: dlls/viocalls/viocalls.c dlls/viocalls/viocalls.def
	$(MINGW) $(C89FLAGS) -shared -o $@ dlls/viocalls/viocalls.c dlls/viocalls/viocalls.def \
		-Wl,--out-implib,libviocalls.a

QUECALLS.dll: dlls/quecalls/quecalls.c dlls/quecalls/quecalls.def
	$(MINGW) $(C89FLAGS) -shared -o $@ dlls/quecalls/quecalls.c dlls/quecalls/quecalls.def \
		-Wl,--out-implib,libquecalls.a

SESMGR.dll: dlls/sesmgr/sesmgr.c dlls/sesmgr/sesmgr.def
	$(MINGW) $(C89FLAGS) -shared -o $@ dlls/sesmgr/sesmgr.c dlls/sesmgr/sesmgr.def \
		-Wl,--out-implib,libsesmgr.a

NLS.dll: dlls/nls/nls.c dlls/nls/nls.def
	$(MINGW) $(C89FLAGS) -shared -o $@ dlls/nls/nls.c dlls/nls/nls.def \
		-Wl,--out-implib,libnls.a

PMWIN.dll: dlls/pmwin/pmwin.c dlls/pmwin/pmwin.def dlls/pm-common/pmcompat.h
	$(MINGW) $(C89FLAGS) -Idlls/pm-common -shared -o $@ \
		dlls/pmwin/pmwin.c dlls/pmwin/pmwin.def -luser32 -lgdi32 \
		-Wl,--out-implib,libpmwin.a

PMGPI.dll: dlls/pmgpi/pmgpi.c dlls/pmgpi/pmgpi.def dlls/pm-common/pmcompat.h
	$(MINGW) $(C89FLAGS) -Idlls/pm-common -shared -o $@ \
		dlls/pmgpi/pmgpi.c dlls/pmgpi/pmgpi.def -lgdi32 \
		-Wl,--out-implib,libpmgpi.a

PMSHAPI.dll: dlls/pmshapi/pmshapi.c dlls/pmshapi/pmshapi.def
	$(MINGW) $(C89FLAGS) -shared -o $@ dlls/pmshapi/pmshapi.c dlls/pmshapi/pmshapi.def \
		-Wl,--out-implib,libpmshapi.a

PMWP.dll: dlls/pmwp/pmwp.c dlls/pmwp/pmwp.def
	$(MINGW) $(C89FLAGS) -shared -o $@ dlls/pmwp/pmwp.c dlls/pmwp/pmwp.def \
		-Wl,--out-implib,libpmwp.a

HELPMGR.dll: dlls/helpmgr/helpmgr.c dlls/helpmgr/helpmgr.def
	$(MINGW) $(C89FLAGS) -shared -o $@ dlls/helpmgr/helpmgr.c dlls/helpmgr/helpmgr.def \
		-Wl,--out-implib,libhelpmgr.a


verify: common-check catalog-check wiring-check

common-check: tests/common/personality-core-check.c $(DOSCALLS_CORE_SRC) \
              $(API_CATALOG_SRC) common/include/os2_personality.h \
              common/include/os2_doscalls_core.h common/include/os2_api_catalog.h \
              common/api/os2_api_catalog.inc
	$(HOSTCC) -std=c89 -O2 -Wall -Wextra -pedantic $(COMMON_CPPFLAGS) \
		-o personality-core-check tests/common/personality-core-check.c \
		$(DOSCALLS_CORE_SRC) $(API_CATALOG_SRC)
	./personality-core-check
	rm -f personality-core-check

catalog-check:
	python3 tools/check_api_catalog.py

wiring-check:
	python3 tools/check_shared_personality.py

clean:
	rm -f os2host32.exe le2pe386.exe cmd32os2.exe personality-core-check \
	      DOSCALLS.dll KBDCALLS.dll VIOCALLS.dll QUECALLS.dll SESMGR.dll NLS.dll \
	      PMWIN.dll PMGPI.dll PMSHAPI.dll PMWP.dll HELPMGR.dll \
	      libdoscalls.a libkbdcalls.a libviocalls.a libquecalls.a libsesmgr.a \
	      libnls.a libpmwin.a libpmgpi.a libpmshapi.a libpmwp.a libhelpmgr.a
