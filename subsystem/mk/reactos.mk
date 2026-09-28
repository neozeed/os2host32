# Local ReactOS SDK description.
#
# This is intentionally a copied subset, not a ReactOS source-tree dependency.
# Keep the directory names parallel to sdk/include where practical:
#
#   reactos/
#     psdk/
#     ddk/
#     ndk/
#     reactos/
#       subsys/
#     lib/
#       libntdll.a
#       libkernel32.a
#       libsmlib.a
#
# The libraries should come from the same i386 ReactOS build corresponding to
# the headers.  In particular, use the real ReactOS import libraries rather
# than rebuilding ntdll.def with a different ABI decoration policy.

WINE := reactos/sdk/include/wine
ROS_PSDK := $(REACTOS_SDK)/psdk
ROS_DDK := $(REACTOS_SDK)/ddk
ROS_NDK := $(REACTOS_SDK)/ndk
ROS_INC := $(REACTOS_SDK)/reactos
ROS_LIB := $(REACTOS_SDK)/lib

ROS := $(CURDIR)/reactos
ROS_OUT := $(ROS)/output-MinGW-i386

ROS_INCLUDES := \
    -I$(CURDIR)/common \
    -I$(ROS_OUT)/sdk/include/psdk \
    -I$(ROS_OUT)/sdk/include/ddk \
    -I$(ROS)/include \
    -I$(ROS)/include/crt \
    -I$(ROS)/include/ddk \
    -I$(ROS)/include/ndk \
    -I$(ROS)/include/psdk \
    -I$(ROS)/include/reactos \
    -I$(ROS)/include/reactos/libs \
    -I$(ROS)/include/vcruntime \
    -I$(ROS)/include/winrt \
    -I$(ROS)/include/dxsdk \
    -I$(ROS)/lib/pseh/include \
    -I$(ROS)/include/reactos/subsys \
    -I$(ROS)/lib/smlib 



REACTOS_CPPFLAGS := $(ROS_INCLUDES)

# Match the frozen 32-bit ReactOS personality ABI used by the canonical build.
REACTOS_DEFS := \
	-D_M_IX86=600 \
	-D_X86_=1 \
	-D__REACTOS__=1 \
	-DWINVER=0x0502 \
	-D_WIN32_WINNT=0x0502 \
	-DNTDDI_VERSION=0x05020000

# These are deliberately real ReactOS/MinGW libraries.  The GNU i386 import
# libraries carry the stdcall decoration expected by GCC and avoid the old
# clang/lld-link /alternatename table.
NTDLL_LIB := $(ROS_LIB)/libntdll.a
KERNEL32_LIB := $(ROS_LIB)/libkernel32.a
SMLIB_LIB := $(ROS_LIB)/libsmlib.a

# Keep this list short and diagnostic.  Compilation will naturally identify
# additional headers as the local SDK is populated.
REACTOS_REQUIRED_FILES := \
	$(ROS_PSDK)/winnt.h \
	$(ROS_PSDK)/ntdef.h \
	$(ROS_DDK)/wdm.h \
	$(ROS_DDK)/ntddk.h \
	$(ROS_NDK)/ntndk.h \
	$(ROS_INC)/subsys/sm/ns.h \
	$(ROS_INC)/subsys/sm/smmsg.h \
	$(NTDLL_LIB) \
	$(KERNEL32_LIB) \
	$(SMLIB_LIB)
