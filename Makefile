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
DOSCALLS_SESSION_SRC = common/doscalls/os2_doscalls.c
DOSCALLS_WIN32_SRC = common/win32/os2_doscalls_win32.c
WIN32_COMMON_SRC = common/win32/os2_win32_services.c
NLS_CORE_SRC = common/nls/os2_nls.c
NLS_API_SRC = common/nls/os2_nls_api.c
NLS_COMMON_SRC = $(NLS_CORE_SRC) $(NLS_API_SRC)
NLS_WIN32_SRC = common/win32/os2_nls_win32.c
VIO_COMMON_SRC = common/vio/os2_vio.c
VIO_WIN32_SRC = common/win32/os2_vio_win32.c common/win32/os2_vio_win32_attr.c
QUEUE_COMMON_SRC = common/queue/os2_queue.c
QUEUE_WIN32_SRC = common/win32/os2_queue_win32.c
KBD_COMMON_SRC = common/kbd/os2_kbd.c
KBD_WIN32_SRC = common/win32/os2_kbd_win32.c
SESMGR_COMMON_SRC = common/sesmgr/os2_sesmgr.c
SESMGR_WIN32_SRC = common/win32/os2_sesmgr_win32.c
MOU_COMMON_SRC = common/mou/os2_mou.c
MOU_WIN32_SRC = common/win32/os2_mou_win32.c

.PHONY: all loader whp transformer shell dlls compat diagnostics verify common-check nls-check nls-win32-shim-check nls-static-check nls-table-check catalog-check wiring-check doscalls-check doscalls-veneer-check doscalls-static-check vio-check vio-win32-shim-check vio-static-check queue-check queue-veneer-check queue-win32-shim-check queue-static-check kbd-check kbd-veneer-check kbd-win32-shim-check kbd-static-check sesmgr-check sesmgr-veneer-check sesmgr-win32-shim-check sesmgr-static-check mou-check mou-veneer-check mou-win32-shim-check mou-static-check c386-hack-static-check clean

all: loader transformer shell dlls

loader: os2host32.exe

# Alternative Win64/WHP loader; intentionally separate from the MinGW default build.
whp:
	$(MAKE) -C whp

transformer: le2pe386.exe
shell: cmd32os2.exe

compat: dlls

dlls: DOSCALLS.dll KBDCALLS.dll VIOCALLS.dll QUECALLS.dll SESMGR.dll MOUCALLS.dll NLS.dll \
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
              $(DOSCALLS_SESSION_SRC) $(DOSCALLS_WIN32_SRC) \
              $(DOSCALLS_CORE_SRC) $(WIN32_COMMON_SRC) $(NLS_CORE_SRC) $(NLS_WIN32_SRC) \
              common/include/os2_personality.h common/include/os2_nls.h \
              common/include/os2_nls_backend.h common/include/os2_nls_win32.h \
              common/nls/os2_nls_tables.inc \
              common/include/os2_doscalls.h common/include/os2_doscalls_backend.h \
              common/include/os2_doscalls_win32.h common/include/os2_doscalls_core.h \
              common/include/os2_win32_services.h
	$(MINGW) $(C89FLAGS) $(COMMON_CPPFLAGS) -shared -o $@ \
		dlls/doscalls/doscalls.c $(DOSCALLS_SESSION_SRC) $(DOSCALLS_WIN32_SRC) \
		$(DOSCALLS_CORE_SRC) $(WIN32_COMMON_SRC) $(NLS_CORE_SRC) $(NLS_WIN32_SRC) \
		dlls/doscalls/doscalls.def \
		-lwinmm -Wl,--out-implib,libdoscalls.a

KBDCALLS.dll: dlls/kbdcalls/kbdcalls.c dlls/kbdcalls/kbdcalls.def \
              $(KBD_COMMON_SRC) $(KBD_WIN32_SRC) \
              common/include/os2_kbd.h common/include/os2_kbd_backend.h \
              common/include/os2_kbd_win32.h
	$(MINGW) $(C89FLAGS) $(COMMON_CPPFLAGS) -shared -o $@ \
		dlls/kbdcalls/kbdcalls.c $(KBD_COMMON_SRC) $(KBD_WIN32_SRC) \
		dlls/kbdcalls/kbdcalls.def -Wl,--out-implib,libkbdcalls.a

VIOCALLS.dll: dlls/viocalls/viocalls.c dlls/viocalls/viocalls.def \
              $(VIO_COMMON_SRC) $(VIO_WIN32_SRC) \
              common/include/os2_vio.h common/include/os2_vio_win32.h
	$(MINGW) $(C89FLAGS) $(COMMON_CPPFLAGS) -shared -o $@ \
		dlls/viocalls/viocalls.c $(VIO_COMMON_SRC) $(VIO_WIN32_SRC) \
		dlls/viocalls/viocalls.def -Wl,--out-implib,libviocalls.a

QUECALLS.dll: dlls/quecalls/quecalls.c dlls/quecalls/quecalls.def \
              $(QUEUE_COMMON_SRC) $(QUEUE_WIN32_SRC) \
              common/include/os2_queue.h common/include/os2_queue_backend.h \
              common/include/os2_queue_win32.h
	$(MINGW) $(C89FLAGS) $(COMMON_CPPFLAGS) -shared -o $@ \
		dlls/quecalls/quecalls.c $(QUEUE_COMMON_SRC) $(QUEUE_WIN32_SRC) \
		dlls/quecalls/quecalls.def -Wl,--out-implib,libquecalls.a

SESMGR.dll: dlls/sesmgr/sesmgr.c dlls/sesmgr/sesmgr.def \
            $(SESMGR_COMMON_SRC) $(SESMGR_WIN32_SRC) \
            common/include/os2_sesmgr.h common/include/os2_sesmgr_backend.h \
            common/include/os2_sesmgr_win32.h
	$(MINGW) $(C89FLAGS) $(COMMON_CPPFLAGS) -shared -o $@ \
		dlls/sesmgr/sesmgr.c $(SESMGR_COMMON_SRC) $(SESMGR_WIN32_SRC) \
		dlls/sesmgr/sesmgr.def -Wl,--out-implib,libsesmgr.a

MOUCALLS.dll: dlls/moucalls/moucalls.c dlls/moucalls/moucalls.def \
              $(MOU_COMMON_SRC) $(MOU_WIN32_SRC) \
              common/include/os2_mou.h common/include/os2_mou_backend.h \
              common/include/os2_mou_win32.h
	$(MINGW) $(C89FLAGS) $(COMMON_CPPFLAGS) -shared -o $@ \
		dlls/moucalls/moucalls.c $(MOU_COMMON_SRC) $(MOU_WIN32_SRC) \
		dlls/moucalls/moucalls.def -luser32 -Wl,--out-implib,libmoucalls.a

NLS.dll: dlls/nls/nls.c dlls/nls/nls.def DOSCALLS.dll
	$(MINGW) $(C89FLAGS) -shared -o $@ dlls/nls/nls.c dlls/nls/nls.def \
		libdoscalls.a -Wl,--out-implib,libnls.a

# Human-readable personality NLS smoke test. This is a diagnostic, not a
# production runtime prerequisite.
diagnostics: nlsinfo.exe

nlsinfo.exe: tools/nlsinfo.c NLS.dll DOSCALLS.dll
	$(MINGW) $(C89FLAGS) -o $@ tools/nlsinfo.c libnls.a libdoscalls.a

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


verify: common-check nls-table-check nls-check nls-win32-shim-check nls-static-check catalog-check wiring-check doscalls-check doscalls-veneer-check doscalls-static-check vio-check vio-win32-shim-check vio-static-check queue-check queue-veneer-check queue-win32-shim-check queue-static-check kbd-check kbd-veneer-check kbd-win32-shim-check kbd-static-check sesmgr-check sesmgr-veneer-check sesmgr-win32-shim-check sesmgr-static-check mou-check mou-veneer-check mou-win32-shim-check mou-static-check c386-hack-static-check

common-check: tests/common/personality-core-check.c $(DOSCALLS_CORE_SRC) \
              $(API_CATALOG_SRC) common/include/os2_personality.h \
              common/include/os2_doscalls_core.h common/include/os2_api_catalog.h \
              common/api/os2_api_catalog.inc
	$(HOSTCC) -std=c89 -O2 -Wall -Wextra -pedantic $(COMMON_CPPFLAGS) \
		-o personality-core-check tests/common/personality-core-check.c \
		$(DOSCALLS_CORE_SRC) $(API_CATALOG_SRC)
	./personality-core-check
	rm -f personality-core-check

nls-table-check: tools/gen_nls_tables.py common/nls/os2_nls_tables.inc
	python3 tools/gen_nls_tables.py --check

nls-check: tests/nls/nls-core-check.c $(NLS_COMMON_SRC) $(DOSCALLS_CORE_SRC) \
           common/include/os2_nls.h common/include/os2_nls_api.h \
           common/include/os2_nls_backend.h common/include/os2_personality.h common/nls/os2_nls_tables.inc
	$(HOSTCC) -std=c89 -O2 -Wall -Wextra -pedantic $(COMMON_CPPFLAGS) \
		-o nls-core-check tests/nls/nls-core-check.c \
		$(NLS_COMMON_SRC) $(DOSCALLS_CORE_SRC)
	./nls-core-check
	rm -f nls-core-check

nls-win32-shim-check: tests/nls/nls-win32-shim-check.c \
                      tests/nls/win32-stub/win32_nls_stub.c \
                      tests/nls/win32-stub/windows.h \
                      $(NLS_CORE_SRC) $(NLS_WIN32_SRC) \
                      common/include/os2_nls.h common/include/os2_nls_backend.h \
                      common/include/os2_nls_win32.h
	$(HOSTCC) -std=c89 -O2 -Wall -Wextra -pedantic \
		-Itests/nls/win32-stub $(COMMON_CPPFLAGS) \
		-o nls-win32-shim-check tests/nls/nls-win32-shim-check.c \
		tests/nls/win32-stub/win32_nls_stub.c \
		$(NLS_CORE_SRC) $(NLS_WIN32_SRC)
	./nls-win32-shim-check
	rm -f nls-win32-shim-check

nls-static-check:
	python3 tools/check_nls_r2.py

catalog-check:
	python3 tools/check_api_catalog.py

wiring-check:
	python3 tools/check_shared_personality.py


doscalls-check: tests/doscalls/doscalls-core-check.c $(DOSCALLS_SESSION_SRC) $(NLS_CORE_SRC) \
               common/include/os2_doscalls.h common/include/os2_doscalls_backend.h \
               common/include/os2_nls.h common/include/os2_nls_backend.h
	$(HOSTCC) -std=c89 -O2 -Wall -Wextra -pedantic $(COMMON_CPPFLAGS) \
		-o doscalls-core-check tests/doscalls/doscalls-core-check.c \
		$(DOSCALLS_SESSION_SRC) $(NLS_CORE_SRC)
	./doscalls-core-check
	rm -f doscalls-core-check

doscalls-veneer-check: tests/doscalls/doscalls-veneer-check.c \
                         dlls/doscalls/doscalls.c $(DOSCALLS_SESSION_SRC) $(NLS_CORE_SRC) \
                         common/include/os2_doscalls.h common/include/os2_doscalls_backend.h \
                         common/include/os2_doscalls_win32.h
	$(HOSTCC) -std=c89 -O2 -Wall -Wextra -pedantic $(COMMON_CPPFLAGS) \
		-o doscalls-veneer-check tests/doscalls/doscalls-veneer-check.c \
		dlls/doscalls/doscalls.c $(DOSCALLS_SESSION_SRC) $(NLS_CORE_SRC)
	./doscalls-veneer-check
	rm -f doscalls-veneer-check

doscalls-static-check:
	python3 tools/check_doscalls_r2.py

vio-check: tests/vio/vio-core-check.c $(VIO_COMMON_SRC) \
           common/win32/os2_vio_win32_attr.c common/include/os2_vio.h \
           common/include/os2_vio_win32.h
	$(HOSTCC) -std=c89 -O2 -Wall -Wextra -pedantic $(COMMON_CPPFLAGS) \
		-o vio-core-check tests/vio/vio-core-check.c $(VIO_COMMON_SRC) \
		common/win32/os2_vio_win32_attr.c
	./vio-core-check
	rm -f vio-core-check

vio-win32-shim-check: tests/vio/vio-win32-shim-check.c \
                        tests/vio/win32-stub/win32_vio_stub.c \
                        tests/vio/win32-stub/windows.h \
                        $(VIO_COMMON_SRC) $(VIO_WIN32_SRC) \
                        dlls/viocalls/viocalls.c common/include/os2_vio.h \
                        common/include/os2_vio_win32.h
	$(HOSTCC) -std=c89 -O2 -Wall -Wextra -pedantic \
		-Itests/vio/win32-stub $(COMMON_CPPFLAGS) \
		-o vio-win32-shim-check tests/vio/vio-win32-shim-check.c \
		tests/vio/win32-stub/win32_vio_stub.c dlls/viocalls/viocalls.c \
		$(VIO_COMMON_SRC) $(VIO_WIN32_SRC)
	./vio-win32-shim-check
	rm -f vio-win32-shim-check

vio-static-check:
	python3 tools/check_vio_life_surface.py


queue-check: tests/queue/queue-core-check.c $(QUEUE_COMMON_SRC) \
             common/include/os2_queue.h common/include/os2_queue_backend.h
	$(HOSTCC) -std=c89 -O2 -Wall -Wextra -pedantic $(COMMON_CPPFLAGS) \
		-o queue-core-check tests/queue/queue-core-check.c $(QUEUE_COMMON_SRC)
	./queue-core-check
	rm -f queue-core-check

queue-veneer-check: tests/queue/queue-veneer-check.c dlls/quecalls/quecalls.c \
                    $(QUEUE_COMMON_SRC) common/include/os2_queue.h \
                    common/include/os2_queue_backend.h common/include/os2_queue_win32.h
	$(HOSTCC) -std=c89 -O2 -Wall -Wextra -pedantic $(COMMON_CPPFLAGS) \
		-o queue-veneer-check tests/queue/queue-veneer-check.c \
		dlls/quecalls/quecalls.c $(QUEUE_COMMON_SRC)
	./queue-veneer-check
	rm -f queue-veneer-check

queue-win32-shim-check: tests/queue/queue-win32-shim-check.c \
                         tests/queue/win32-stub/win32_queue_stub.c \
                         tests/queue/win32-stub/windows.h \
                         $(QUEUE_COMMON_SRC) $(QUEUE_WIN32_SRC) \
                         common/include/os2_queue.h common/include/os2_queue_backend.h \
                         common/include/os2_queue_win32.h
	$(HOSTCC) -std=c89 -O2 -Wall -Wextra -pedantic \
		-Itests/queue/win32-stub $(COMMON_CPPFLAGS) \
		-o queue-win32-shim-check tests/queue/queue-win32-shim-check.c \
		tests/queue/win32-stub/win32_queue_stub.c \
		$(QUEUE_COMMON_SRC) $(QUEUE_WIN32_SRC)
	./queue-win32-shim-check
	rm -f queue-win32-shim-check

queue-static-check:
	python3 tools/check_queue_r2.py


kbd-check: tests/kbd/kbd-core-check.c $(KBD_COMMON_SRC) \
           common/include/os2_kbd.h common/include/os2_kbd_backend.h
	$(HOSTCC) -std=c89 -O2 -Wall -Wextra -pedantic $(COMMON_CPPFLAGS) \
		-o kbd-core-check tests/kbd/kbd-core-check.c $(KBD_COMMON_SRC)
	./kbd-core-check
	rm -f kbd-core-check

kbd-veneer-check: tests/kbd/kbd-veneer-check.c dlls/kbdcalls/kbdcalls.c \
                  $(KBD_COMMON_SRC) common/include/os2_kbd.h \
                  common/include/os2_kbd_backend.h common/include/os2_kbd_win32.h
	$(HOSTCC) -std=c89 -O2 -Wall -Wextra -pedantic $(COMMON_CPPFLAGS) \
		-o kbd-veneer-check tests/kbd/kbd-veneer-check.c \
		dlls/kbdcalls/kbdcalls.c $(KBD_COMMON_SRC)
	./kbd-veneer-check
	rm -f kbd-veneer-check

kbd-win32-shim-check: tests/kbd/kbd-win32-shim-check.c \
                      tests/kbd/win32-stub/win32_kbd_stub.c \
                      tests/kbd/win32-stub/windows.h \
                      $(KBD_COMMON_SRC) $(KBD_WIN32_SRC) \
                      common/include/os2_kbd.h common/include/os2_kbd_backend.h \
                      common/include/os2_kbd_win32.h
	$(HOSTCC) -std=c89 -O2 -Wall -Wextra -pedantic \
		-Itests/kbd/win32-stub $(COMMON_CPPFLAGS) \
		-o kbd-win32-shim-check tests/kbd/kbd-win32-shim-check.c \
		tests/kbd/win32-stub/win32_kbd_stub.c \
		$(KBD_COMMON_SRC) $(KBD_WIN32_SRC)
	./kbd-win32-shim-check
	rm -f kbd-win32-shim-check

kbd-static-check:
	python3 tools/check_kbd_r2.py

sesmgr-check: tests/sesmgr/sesmgr-core-check.c $(SESMGR_COMMON_SRC) \
               common/include/os2_sesmgr.h common/include/os2_sesmgr_backend.h
	$(HOSTCC) -std=c89 -O2 -Wall -Wextra -pedantic $(COMMON_CPPFLAGS) \
		-o sesmgr-core-check tests/sesmgr/sesmgr-core-check.c $(SESMGR_COMMON_SRC)
	./sesmgr-core-check
	rm -f sesmgr-core-check

sesmgr-veneer-check: tests/sesmgr/sesmgr-veneer-check.c dlls/sesmgr/sesmgr.c \
                     $(SESMGR_COMMON_SRC) common/include/os2_sesmgr.h \
                     common/include/os2_sesmgr_backend.h common/include/os2_sesmgr_win32.h
	$(HOSTCC) -std=c89 -O2 -Wall -Wextra -pedantic -DOS2_SESMGR_NO_DLLMAIN \
		$(COMMON_CPPFLAGS) -o sesmgr-veneer-check \
		tests/sesmgr/sesmgr-veneer-check.c dlls/sesmgr/sesmgr.c $(SESMGR_COMMON_SRC)
	./sesmgr-veneer-check
	rm -f sesmgr-veneer-check

sesmgr-win32-shim-check: tests/sesmgr/sesmgr-win32-shim-check.c \
                          tests/sesmgr/win32-stub/win32_sesmgr_stub.c \
                          tests/sesmgr/win32-stub/windows.h \
                          $(SESMGR_COMMON_SRC) $(SESMGR_WIN32_SRC) \
                          common/include/os2_sesmgr.h common/include/os2_sesmgr_backend.h \
                          common/include/os2_sesmgr_win32.h
	$(HOSTCC) -std=c89 -O2 -Wall -Wextra -pedantic \
		-Itests/sesmgr/win32-stub $(COMMON_CPPFLAGS) \
		-o sesmgr-win32-shim-check tests/sesmgr/sesmgr-win32-shim-check.c \
		tests/sesmgr/win32-stub/win32_sesmgr_stub.c \
		$(SESMGR_COMMON_SRC) $(SESMGR_WIN32_SRC)
	./sesmgr-win32-shim-check
	rm -f sesmgr-win32-shim-check

sesmgr-static-check:
	python3 tools/check_sesmgr_r2.py

mou-check: tests/mou/mou-core-check.c $(MOU_COMMON_SRC) \
           common/include/os2_mou.h common/include/os2_mou_backend.h
	$(HOSTCC) -std=c89 -O2 -Wall -Wextra -pedantic $(COMMON_CPPFLAGS) \
		-o mou-core-check tests/mou/mou-core-check.c $(MOU_COMMON_SRC)
	./mou-core-check
	rm -f mou-core-check

mou-veneer-check: tests/mou/mou-veneer-check.c dlls/moucalls/moucalls.c \
                  $(MOU_COMMON_SRC) common/include/os2_mou.h \
                  common/include/os2_mou_backend.h common/include/os2_mou_win32.h
	$(HOSTCC) -std=c89 -O2 -Wall -Wextra -pedantic $(COMMON_CPPFLAGS) \
		-o mou-veneer-check tests/mou/mou-veneer-check.c \
		dlls/moucalls/moucalls.c $(MOU_COMMON_SRC)
	./mou-veneer-check
	rm -f mou-veneer-check

mou-win32-shim-check: tests/mou/mou-win32-shim-check.c \
                      tests/mou/win32-stub/win32_mou_stub.c \
                      tests/mou/win32-stub/windows.h \
                      $(MOU_COMMON_SRC) $(MOU_WIN32_SRC) \
                      common/include/os2_mou.h common/include/os2_mou_backend.h \
                      common/include/os2_mou_win32.h
	$(HOSTCC) -std=c89 -O2 -Wall -Wextra -pedantic \
		-Itests/mou/win32-stub $(COMMON_CPPFLAGS) \
		-o mou-win32-shim-check tests/mou/mou-win32-shim-check.c \
		tests/mou/win32-stub/win32_mou_stub.c \
		$(MOU_COMMON_SRC) $(MOU_WIN32_SRC)
	./mou-win32-shim-check
	rm -f mou-win32-shim-check

mou-static-check:
	python3 tools/check_mou_r2.py

clean:
	rm -f os2host32.exe le2pe386.exe cmd32os2.exe nlsinfo.exe personality-core-check nls-core-check nls-win32-shim-check doscalls-core-check doscalls-veneer-check vio-core-check vio-win32-shim-check queue-core-check queue-veneer-check queue-win32-shim-check kbd-core-check kbd-veneer-check kbd-win32-shim-check sesmgr-core-check sesmgr-veneer-check sesmgr-win32-shim-check mou-core-check mou-veneer-check mou-win32-shim-check \
	      DOSCALLS.dll KBDCALLS.dll VIOCALLS.dll QUECALLS.dll SESMGR.dll MOUCALLS.dll NLS.dll \
	      PMWIN.dll PMGPI.dll PMSHAPI.dll PMWP.dll HELPMGR.dll \
	      libdoscalls.a libkbdcalls.a libviocalls.a libquecalls.a libsesmgr.a libmoucalls.a \
	      libnls.a libpmwin.a libpmgpi.a libpmshapi.a libpmwp.a libhelpmgr.a


c386-hack-static-check:
	python3 tools/check_c386_hack_bridge.py
