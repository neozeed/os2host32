OBJDIR := $(BUILD_DIR)/obj

OS2SS_EXE := $(BUILD_DIR)/OS2SS.EXE
LAUNCHER_EXE := $(BUILD_DIR)/OS2LE4CLAUNCH.EXE
OS2BOOT_EXE := $(BUILD_DIR)/OS2BOOT.EXE
PROGRAMS := $(OS2SS_EXE) $(LAUNCHER_EXE) $(OS2BOOT_EXE)

COMMON_CFLAGS := \
	-std=gnu11 \
	-Os \
	-ffreestanding \
	-fno-stack-protector \
	-fno-builtin \
	-fno-ident \
	-fms-extensions \
	-Wno-error
#	-Wall -Wextra -Werror

CPPFLAGS += $(REACTOS_DEFS) $(REACTOS_CPPFLAGS)
CFLAGS += $(COMMON_CFLAGS)

# No MinGW CRT/startup objects are wanted in the subsystem binaries.  All
# required imports are explicit and entry() is our actual PE entry point.
COMMON_LDFLAGS := \
	-nostdlib \
	-Wl,--gc-sections \
	-Wl,--entry,_entry

OS2SS_OBJS := \
	$(OBJDIR)/os2ss/os2ss.o \
	$(OBJDIR)/os2ss/process.o \
	$(OBJDIR)/os2ss/api.o

LAUNCHER_OBJS := \
	$(OBJDIR)/launcher/os2le4claunch.o

OS2BOOT_OBJS := \
	$(OBJDIR)/os2boot/os2boot.o \
	$(OBJDIR)/os2boot/le4c_thunks.o \
	$(OBJDIR)/common/os2loader.o \
	$(OBJDIR)/common/os2image.o \
	$(OBJDIR)/common/os2veneer.o \
	$(OBJDIR)/common/os2startup.o \
	$(OBJDIR)/common/os2sha256.o \
	$(OBJDIR)/os2boot/crt_shim.o

OS2BOOT_THUNK_OBJ := $(OBJDIR)/os2boot/le4c_thunks.o

$(OS2BOOT_THUNK_OBJ): os2boot/le4c_thunks.S
	@mkdir -p $(dir $@)
	$(CC) -c $< -o $@

# Dependency files make edits incremental: touching api.c relinks only OS2SS.
DEPFILES := $(OS2SS_OBJS:.o=.d) $(LAUNCHER_OBJS:.o=.d) $(OS2BOOT_OBJS:.o=.d)
-include $(DEPFILES)

$(OBJDIR)/%.o: %.c
	@mkdir -p "$(dir $@)"
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c "$<" -o "$@"

$(OS2SS_EXE): $(OS2SS_OBJS) $(SMLIB_LIB) $(NTDLL_LIB)
	@mkdir -p "$(dir $@)"
	$(CC) $(COMMON_LDFLAGS) -Wl,--subsystem,windows -o "$@" \
		$(OS2SS_OBJS) $(SMLIB_LIB) $(NTDLL_LIB)

$(LAUNCHER_EXE): $(LAUNCHER_OBJS) $(SMLIB_LIB) $(KERNEL32_LIB) $(NTDLL_LIB)
	@mkdir -p "$(dir $@)"
	$(CC) $(COMMON_LDFLAGS) -Wl,--subsystem,console -o "$@" \
		$(LAUNCHER_OBJS) $(SMLIB_LIB) $(KERNEL32_LIB) $(NTDLL_LIB)

$(OS2BOOT_EXE): $(OS2BOOT_OBJS) $(NTDLL_LIB) tools/patch_pe_subsystem.py
	@mkdir -p "$(dir $@)"
	$(CC) -nostdlib \
	    -Wl,--gc-sections \
	    -Wl,--entry,_entry \
	    -Wl,--subsystem,5 \
	    -Wl,--image-base,0x00400000 \
	    -Wl,--major-os-version,6 \
	    -Wl,--minor-os-version,0 \
	    -Wl,--major-subsystem-version,6 \
	    -Wl,--minor-subsystem-version,0 \
	    -Xlinker "--stack=0x100000,0x1000" \
	    -Wl,--no-seh \
	    -Wl,--tsaware \
	    -o "$@" \
	    $(OS2BOOT_OBJS) \
	    $(NTDLL_LIB)

#	$(CC) $(COMMON_LDFLAGS) -Wl,--subsystem,windows -Wl,--image-base,0x00400000 \
#		-o "$@" $(OS2BOOT_OBJS) $(NTDLL_LIB)
#	$(PYTHON) tools/patch_pe_subsystem.py "$@" 5
