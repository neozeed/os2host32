CC ?= gcc
WINCC ?= i686-w64-mingw32-gcc
CFLAGS ?= -O2 -g -std=gnu99 -Wall -Wextra -Wno-unused-function -Wno-misleading-indentation
CPPFLAGS += -I tiny386 -I src -I ../common/include -DI386_ENABLE_FPU

TARGET = soft386_os2
WIN_TARGET = soft386_os2.exe
BRIDGE_CHECK = bridge-check
SYSTEM_BRIDGE_CHECK = system-bridge-check
PM_RUNTIME_CHECK = pm-runtime-check
PM_BRIDGE_CHECK = pm-bridge-check
R5B_RUNTIME_CHECK = r5b-runtime-check
QUEUE_BRIDGE_CHECK = queue-bridge-check
LDLIBS += -lm

CC_MACHINE := $(shell $(CC) -dumpmachine 2>/dev/null)
WINCC_MACHINE := $(shell $(WINCC) -dumpmachine 2>/dev/null)
ifneq ($(findstring mingw,$(CC_MACHINE)),)
ifneq ($(filter x86_64%,$(CC_MACHINE)),)
$(error Soft386 R5 native bridges require a 32-bit i386/i686 MinGW compiler; use RosBE/i386 or i686-w64-mingw32-gcc, not mingw64)
endif
endif
ifneq ($(findstring mingw,$(WINCC_MACHINE)),)
ifneq ($(filter x86_64%,$(WINCC_MACHINE)),)
$(error Soft386 R5 WINCC must target 32-bit i386/i686; do not use mingw64)
endif
endif

SRC = src/soft386_os2.c src/soft386_doscalls_bridge.c src/soft386_system_bridge.c src/soft386_pm_bridge.c src/soft386_queue_bridge.c src/soft386_net_bridge.c src/soft386_ne_kbd.c src/soft386_ne_file.c tiny386/i386.c tiny386/fpu.c ../common/doscalls/os2_doscalls_core.c ../common/nls/os2_nls.c ../common/nls/os2_nls_api.c ../common/msg/os2_msg.c ../common/kbd/os2_kbd.c
WIN_EXTRA_SRC = ../common/win32/os2_nls_win32.c ../common/win32/os2_kbd_win32.c

# Plain `make` is also useful from a native MinGW/RosBE shell.  In that case
# CC itself targets Windows, so include the Win32 NLS bootstrap source in the
# normal target as well.
ifneq (,$(findstring mingw,$(CC_MACHINE)))
HOST_EXTRA_SRC = $(WIN_EXTRA_SRC)
else
HOST_EXTRA_SRC =
HOST_LIBS = -pthread
endif

BRIDGE_CHECK_SRC = tests/bridge-check.c src/soft386_doscalls_bridge.c
SYSTEM_BRIDGE_CHECK_SRC = tests/system-bridge-check.c src/soft386_system_bridge.c

.PHONY: all win32 clean fixtures check check-quick check-ne
all: $(TARGET)

$(TARGET): $(SRC) $(HOST_EXTRA_SRC) $(wildcard src/*.h)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SRC) $(HOST_EXTRA_SRC) -o $@ $(LDLIBS) $(HOST_LIBS)

win32: $(WIN_TARGET)

$(WIN_TARGET): $(SRC) $(WIN_EXTRA_SRC) $(wildcard src/*.h)
	$(WINCC) $(CPPFLAGS) $(CFLAGS) $(SRC) $(WIN_EXTRA_SRC) -o $@ $(LDLIBS)

$(BRIDGE_CHECK): $(BRIDGE_CHECK_SRC) src/soft386_doscalls_bridge.h
	$(CC) -I src -O2 -g -std=gnu99 -Wall -Wextra -Werror $(BRIDGE_CHECK_SRC) -o $@

$(SYSTEM_BRIDGE_CHECK): $(SYSTEM_BRIDGE_CHECK_SRC) src/soft386_system_bridge.h
	$(CC) -I src -O2 -g -std=gnu99 -Wall -Wextra -Werror $(SYSTEM_BRIDGE_CHECK_SRC) -o $@

$(PM_BRIDGE_CHECK): tests/pm-bridge-check.c src/soft386_pm_bridge.c $(wildcard src/*.h)
	$(CC) -I src $(CFLAGS) tests/pm-bridge-check.c src/soft386_pm_bridge.c -o $@

$(PM_RUNTIME_CHECK): tests/pm-runtime-check.c $(SRC) $(wildcard src/*.h)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/pm-runtime-check.c $(filter-out src/soft386_os2.c,$(SRC)) $(HOST_EXTRA_SRC) -o $@ $(LDLIBS) $(HOST_LIBS)

$(QUEUE_BRIDGE_CHECK): tests/queue-bridge-check.c src/soft386_queue_bridge.c $(wildcard src/*.h)
	$(CC) -I src $(CFLAGS) tests/queue-bridge-check.c src/soft386_queue_bridge.c -o $@

$(R5B_RUNTIME_CHECK): tests/r5b-runtime-check.c tests/queue-bridge-check.c $(SRC) $(wildcard src/*.h)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/r5b-runtime-check.c $(filter-out src/soft386_os2.c,$(SRC)) $(HOST_EXTRA_SRC) -o $@ $(LDLIBS) $(HOST_LIBS)

fixtures:
	python3 tests/mkfixtures.py tests/fixtures
	python3 tests/mkfixtures_r5.py tests/fixtures
	python3 tests/mkfixtures_r5a.py tests/fixtures
	python3 tests/mkfixtures_r5b.py tests/fixtures

check-ne: all
	python3 tests/check-ne-pascal-abi.py
	python3 tests/check-ne-modules.py ./$(TARGET) tests/fixtures/link386-os2beta-ne16.exe
	python3 tests/check-ne-kbdflush.py ./$(TARGET)
	python3 tests/check-ne-nls-casemap.py ./$(TARGET)
	python3 tests/check-ne-rc-files.py ./$(TARGET)
	python3 tests/check-ne-args.py ./$(TARGET) tests/fixtures/args-ne16.exe
	python3 tests/check-ne-link386.py ./$(TARGET) tests/fixtures/link386-os2beta-ne16.exe
	python3 tests/check-ne-lib-add.py ./$(TARGET) tests/fixtures/lib-os2beta-ne16.exe tests/fixtures/huge-omf.obj
	python3 tests/check-ne-lib-create.py ./$(TARGET) tests/fixtures/lib-os2beta-ne16.exe
	python3 tests/check-ne-lib.py ./$(TARGET) tests/fixtures/lib-os2beta-ne16.exe
	./$(TARGET) tests/fixtures/void-ne16.exe
	python3 tests/check-ne-printf.py ./$(TARGET) tests/fixtures/tiny-printf-ne16.exe
	python3 tests/check-ne-medium.py ./$(TARGET) tests/fixtures/medium-printf-ne16.exe
	python3 tests/check-ne-large.py ./$(TARGET) tests/fixtures/large-printf-ne16.exe
	python3 tests/check-ne-compact.py ./$(TARGET) tests/fixtures/compact-printf-ne16.exe
	python3 tests/check-ne-huge.py ./$(TARGET) tests/fixtures/huge-printf-ne16.exe

check-quick: all fixtures $(BRIDGE_CHECK) $(SYSTEM_BRIDGE_CHECK) check-ne
	./$(TARGET) tests/fixtures/hello-soft386.le
	./$(TARGET) tests/fixtures/thread-soft386.le
	./$(TARGET) tests/fixtures/sync-soft386.le
	./$(TARGET) tests/fixtures/memory-soft386.le
	./$(BRIDGE_CHECK)
	./$(SYSTEM_BRIDGE_CHECK)

check: check-r5c check-ne all fixtures $(BRIDGE_CHECK) $(SYSTEM_BRIDGE_CHECK) $(PM_RUNTIME_CHECK) $(PM_BRIDGE_CHECK) $(R5B_RUNTIME_CHECK) $(QUEUE_BRIDGE_CHECK)
	python3 tests/test_r4.py ./$(TARGET) ./$(BRIDGE_CHECK) ./$(SYSTEM_BRIDGE_CHECK)
	python3 tests/test_r5.py ./$(TARGET) ./$(PM_RUNTIME_CHECK) ./$(PM_BRIDGE_CHECK)
	python3 tests/test_r5a.py ./$(TARGET)
	python3 tests/test_r5b.py ./$(TARGET) ./$(R5B_RUNTIME_CHECK) ./$(QUEUE_BRIDGE_CHECK)

clean:
	rm -f net-bridge-check r5c-runtime-check telnet-profile-check net-echo.exe $(TARGET) $(WIN_TARGET) $(BRIDGE_CHECK) $(SYSTEM_BRIDGE_CHECK) $(PM_RUNTIME_CHECK) $(PM_BRIDGE_CHECK) $(R5B_RUNTIME_CHECK) $(QUEUE_BRIDGE_CHECK)
	rm -f tests/fixtures/*.le tests/fixtures/*.lx tests/fixtures/*soft386.exe tests/fixtures/PMJAR.DLL tests/fixtures/OUTER.DLL tests/fixtures/INNER.DLL tests/fixtures/LEGACY.DLL
	rm -f tests/fixtures/RESOURCE.DLL

net-bridge-check: tests/net-bridge-check.c src/soft386_net_bridge.c src/soft386_doscalls_bridge.c src/soft386_net_bridge.h
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/net-bridge-check.c src/soft386_net_bridge.c src/soft386_doscalls_bridge.c -pthread -o $@
r5c-runtime-check: tests/r5c-runtime-check.c $(SRC) $(wildcard src/*.h)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/r5c-runtime-check.c $(filter-out src/soft386_os2.c,$(SRC)) $(HOST_EXTRA_SRC) -o $@ $(LDLIBS) $(HOST_LIBS)
check-r5c: r5c-runtime-check net-bridge-check
	python3 tests/mkfixtures_r5c.py tests/fixtures
	./net-bridge-check
	./r5c-runtime-check tests/fixtures/net-soft386.le
	./r5c-runtime-check tests/fixtures/exit-soft386.le

telnet-profile-check: tests/telnet-profile-check.c $(SRC) $(wildcard src/*.h)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/telnet-profile-check.c $(filter-out src/soft386_os2.c,$(SRC)) $(HOST_EXTRA_SRC) -o $@ $(LDLIBS) $(HOST_LIBS)
