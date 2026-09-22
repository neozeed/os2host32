# WHP OS/2 V2 loader build
#
# Build from an x64 Visual Studio Developer Command Prompt.
# This Makefile intentionally produces only whp_os2_v2_hi.exe.  The loader
# links the same common DOSCALLS personality sources used by DOSCALLS.DLL;
# loader-specific scheduling, process and callback operations remain in whp/.

CC := cl
CFLAGS ?= /nologo /W4 /O2 /D_CRT_SECURE_NO_WARNINGS
CPPFLAGS = /I src /I ../common/include
LDLIBS ?= WinHvPlatform.lib User32.lib Gdi32.lib

TARGET = whp_os2_v2_hi.exe
SRC = \
	src/whp_os2_v2_hi.c \
	../common/doscalls/os2_doscalls_core.c \
	../common/api/os2_api_catalog.c \
	../common/win32/os2_win32_services.c
HEADERS = \
	../common/include/os2_personality.h \
	../common/include/os2_doscalls_core.h \
	../common/include/os2_api_catalog.h \
	../common/include/os2_win32_services.h \
	../common/api/os2_api_catalog.inc \
	src/v2_beep.h \
	src/v2_callback.h \
	src/v2_cmdfs.h \
	src/v2_console.h \
	src/v2_far16.h \
	src/v2_fileio.h \
	src/v2_find.h \
	src/v2_gpi.h \
	src/v2_info.h \
	src/v2_pm.h \
	src/v2_process.h \
	src/v2_process_boot.h \
	src/v2_queue.h \
	src/v2_sync.h \
	src/v2_sync_types.h

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRC) $(HEADERS)
	$(CC) $(CFLAGS) $(CPPFLAGS) /Fe:$@ $(SRC) /link $(LDLIBS)

clean:
	-del /Q $(TARGET) whp_os2_v2_hi.obj os2_doscalls_core.obj os2_api_catalog.obj os2_win32_services.obj 2>NUL
