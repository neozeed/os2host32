#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
SRC=${1:?usage: build-le4c-canonical.sh /path/to/reactos-source}
BUILD=${BUILD_DIR:-"$ROOT/build"}
GEN="$BUILD/generated"
CLANG=${CLANG:-clang}
LLD=${LLD:-/usr/local/swift/usr/bin/lld-link}
AR=${AR:-/usr/local/swift/usr/bin/llvm-ar}
REQUIRED_HEAD=091855fc4f9de8052c8cf4a55830580aab5558da

ACTUAL_HEAD=$(git -C "$SRC" rev-parse HEAD)
if [ "$ACTUAL_HEAD" != "$REQUIRED_HEAD" ]; then
    echo "ERROR: ReactOS HEAD $ACTUAL_HEAD != required $REQUIRED_HEAD" >&2
    exit 1
fi
if [ -n "$(git -C "$SRC" status --porcelain)" ]; then
    echo "ERROR: ReactOS source tree is not clean before build" >&2
    git -C "$SRC" status --porcelain >&2
    exit 1
fi

echo "ReactOS HEAD: $ACTUAL_HEAD"
echo "ReactOS tree clean before build: YES"
rm -rf "$BUILD"
mkdir -p "$BUILD" "$GEN/sdk/include/psdk" "$GEN/sdk/include/ddk"

$CLANG -DUNIX_PATHS "$SRC/sdk/tools/hpp/hpp.c" -o "$BUILD/native-hpp"
(
    cd "$SRC/sdk/include/xdk"
    "$BUILD/native-hpp" winnt.template.h "$GEN/sdk/include/psdk/winnt.h"
    "$BUILD/native-hpp" ntdef.template.h "$GEN/sdk/include/psdk/ntdef.h"
    "$BUILD/native-hpp" devioctl.template.h "$GEN/sdk/include/psdk/devioctl.h"
    "$BUILD/native-hpp" wdm.template.h "$GEN/sdk/include/ddk/wdm.h"
    "$BUILD/native-hpp" ntddk.template.h "$GEN/sdk/include/ddk/ntddk.h"
    "$BUILD/native-hpp" ntifs.template.h "$GEN/sdk/include/ddk/ntifs.h"
)

INCLUDES="-I$ROOT/common -I$GEN/sdk/include/psdk -I$GEN/sdk/include/ddk -I$SRC/sdk/include -I$SRC/sdk/include/crt -I$SRC/sdk/include/ddk -I$SRC/sdk/include/ndk -I$SRC/sdk/include/psdk -I$SRC/sdk/include/reactos -I$SRC/sdk/include/reactos/libs -I$SRC/sdk/include/vcruntime -I$SRC/sdk/include/winrt -I$SRC/sdk/include/dxsdk -I$SRC/sdk/lib/pseh/include -I$SRC/sdk/include/reactos/subsys -I$SRC/sdk/lib/smlib"
DEFS="-D_M_IX86=600 -D_X86_=1 -D__REACTOS__=1 -DWINVER=0x0502 -D_WIN32_WINNT=0x0502 -DNTDDI_VERSION=0x05020000"
CFLAGS="--target=i686-pc-windows-msvc -std=c11 -Os -ffreestanding -fno-stack-protector -fno-builtin -fno-ident -fms-extensions -fms-compatibility -Wall -Wextra -Werror -Wno-microsoft-anon-tag -Wno-pragma-pack -Wno-ignored-pragma-intrinsic"

"$LLD" /lib /Brepro /def:"$ROOT/common/ntdll.def" /machine:x86 /out:"$BUILD/ntdll.lib"
"$LLD" /lib /Brepro /def:"$ROOT/common/kernel32_console.def" /machine:x86 /out:"$BUILD/kernel32-console.lib"

STDCALL_ALIASES="\
/alternatename:__imp__NtCreateDirectoryObject@12=__imp__NtCreateDirectoryObject \
/alternatename:__imp__NtCreatePort@20=__imp__NtCreatePort \
/alternatename:__imp__NtCreateSection@28=__imp__NtCreateSection \
/alternatename:__imp__NtAcceptConnectPort@24=__imp__NtAcceptConnectPort \
/alternatename:__imp__NtCompleteConnectPort@4=__imp__NtCompleteConnectPort \
/alternatename:__imp__NtConnectPort@32=__imp__NtConnectPort \
/alternatename:__imp__NtReplyWaitReceivePort@16=__imp__NtReplyWaitReceivePort \
/alternatename:__imp__NtReplyPort@8=__imp__NtReplyPort \
/alternatename:__imp__NtCreateEvent@20=__imp__NtCreateEvent \
/alternatename:__imp__NtSetEvent@8=__imp__NtSetEvent \
/alternatename:__imp__NtResetEvent@8=__imp__NtResetEvent \
/alternatename:__imp__NtDelayExecution@8=__imp__NtDelayExecution \
/alternatename:__imp__NtDuplicateObject@28=__imp__NtDuplicateObject \
/alternatename:__imp__NtRequestWaitReplyPort@12=__imp__NtRequestWaitReplyPort \
/alternatename:__imp__NtQueryInformationProcess@20=__imp__NtQueryInformationProcess \
/alternatename:__imp__NtQueryVirtualMemory@24=__imp__NtQueryVirtualMemory \
/alternatename:__imp__NtProtectVirtualMemory@20=__imp__NtProtectVirtualMemory \
/alternatename:__imp__NtResumeThread@8=__imp__NtResumeThread \
/alternatename:__imp__NtWaitForSingleObject@12=__imp__NtWaitForSingleObject \
/alternatename:__imp__NtClose@4=__imp__NtClose \
/alternatename:__imp__NtTerminateProcess@8=__imp__NtTerminateProcess \
/alternatename:__imp__NtAllocateVirtualMemory@24=__imp__NtAllocateVirtualMemory \
/alternatename:__imp__NtFreeVirtualMemory@16=__imp__NtFreeVirtualMemory \
/alternatename:__imp__NtOpenDirectoryObject@12=__imp__NtOpenDirectoryObject \
/alternatename:__imp__NtOpenFile@24=__imp__NtOpenFile \
/alternatename:__imp__NtQueryInformationFile@20=__imp__NtQueryInformationFile \
/alternatename:__imp__NtReadFile@36=__imp__NtReadFile \
/alternatename:__imp__NtWriteFile@36=__imp__NtWriteFile \
/alternatename:__imp__NtSetInformationFile@20=__imp__NtSetInformationFile \
/alternatename:__imp__NtCreateFile@44=__imp__NtCreateFile \
/alternatename:__imp__NtDeleteFile@4=__imp__NtDeleteFile \
/alternatename:__imp__NtQueryFullAttributesFile@8=__imp__NtQueryFullAttributesFile \
/alternatename:__imp__NtQuerySystemTime@4=__imp__NtQuerySystemTime \
/alternatename:__imp__RtlSystemTimeToLocalTime@8=__imp__RtlSystemTimeToLocalTime \
/alternatename:__imp__RtlTimeToTimeFields@8=__imp__RtlTimeToTimeFields \
/alternatename:__imp__RtlGetFullPathName_U@16=__imp__RtlGetFullPathName_U \
/alternatename:__imp__RtlInitUnicodeString@8=__imp__RtlInitUnicodeString \
/alternatename:__imp__RtlCreateUserThread@40=__imp__RtlCreateUserThread \
/alternatename:__imp__RtlCreateSecurityDescriptor@8=__imp__RtlCreateSecurityDescriptor \
/alternatename:__imp__RtlCreateAcl@12=__imp__RtlCreateAcl \
/alternatename:__imp__RtlAddAccessAllowedAce@16=__imp__RtlAddAccessAllowedAce \
/alternatename:__imp__RtlSetDaclSecurityDescriptor@16=__imp__RtlSetDaclSecurityDescriptor \
/alternatename:__imp__RtlDosPathNameToNtPathName_U@16=__imp__RtlDosPathNameToNtPathName_U \
/alternatename:__imp__RtlFreeUnicodeString@4=__imp__RtlFreeUnicodeString \
/alternatename:__imp__RtlCreateProcessParameters@40=__imp__RtlCreateProcessParameters \
/alternatename:__imp__RtlDestroyProcessParameters@4=__imp__RtlDestroyProcessParameters \
/alternatename:__imp__RtlCreateUserProcess@40=__imp__RtlCreateUserProcess \
/alternatename:__imp__RtlAllocateHeap@12=__imp__RtlAllocateHeap \
/alternatename:__imp__RtlReAllocateHeap@16=__imp__RtlReAllocateHeap \
/alternatename:__imp__RtlFreeHeap@12=__imp__RtlFreeHeap \
/alternatename:__imp__RtlInitializeCriticalSection@4=__imp__RtlInitializeCriticalSection \
/alternatename:__imp__RtlEnterCriticalSection@4=__imp__RtlEnterCriticalSection \
/alternatename:__imp__RtlLeaveCriticalSection@4=__imp__RtlLeaveCriticalSection \
/alternatename:__imp__WriteConsoleA@20=__imp__WriteConsoleA \
/alternatename:__imp__ReadConsoleA@20=__imp__ReadConsoleA"

$CLANG $CFLAGS $DEFS $INCLUDES -c "$SRC/sdk/lib/smlib/smclient.c" -o "$BUILD/smclient-canonical.obj"
"$AR" rc "$BUILD/smlib-canonical.lib" "$BUILD/smclient-canonical.obj"

# LE4C extends frozen R5 personality semantics only with the exact hi.exe minimum API surface.
$CLANG $CFLAGS $DEFS $INCLUDES -c "$ROOT/os2ss/os2ss.c" -o "$BUILD/os2ss.obj"
$CLANG $CFLAGS $DEFS $INCLUDES -c "$ROOT/os2ss/process.c" -o "$BUILD/process.obj"
$CLANG $CFLAGS $DEFS $INCLUDES -c "$ROOT/os2ss/api.c" -o "$BUILD/api.obj"
"$LLD" /Brepro /machine:x86 /subsystem:windows /entry:entry /nodefaultlib /opt:ref /opt:icf \
    $STDCALL_ALIASES /out:"$BUILD/OS2SS.EXE" \
    "$BUILD/os2ss.obj" "$BUILD/process.obj" "$BUILD/api.obj" \
    "$BUILD/smlib-canonical.lib" "$BUILD/ntdll.lib"

$CLANG $CFLAGS $DEFS $INCLUDES -c "$ROOT/launcher/os2le4claunch.c" -o "$BUILD/os2le4claunch.obj"
"$LLD" /Brepro /machine:x86 /subsystem:console /entry:entry /nodefaultlib /opt:ref /opt:icf \
    $STDCALL_ALIASES /out:"$BUILD/OS2LE4CLAUNCH.EXE" \
    "$BUILD/os2le4claunch.obj" "$BUILD/smlib-canonical.lib" "$BUILD/kernel32-console.lib" "$BUILD/ntdll.lib"

# OS2BOOT is a native subsystem-5 PE vessel. Frozen LE1 common parser is linked unchanged.
$CLANG $CFLAGS $DEFS $INCLUDES -c "$ROOT/common/os2loader.c" -o "$BUILD/os2loader.obj"
$CLANG $CFLAGS $DEFS $INCLUDES -c "$ROOT/common/os2image.c" -o "$BUILD/os2image.obj"
$CLANG $CFLAGS $DEFS $INCLUDES -c "$ROOT/common/os2veneer.c" -o "$BUILD/os2veneer.obj"
$CLANG $CFLAGS $DEFS $INCLUDES -c "$ROOT/common/os2startup.c" -o "$BUILD/os2startup.obj"
$CLANG $CFLAGS $DEFS $INCLUDES -c "$ROOT/common/os2sha256.c" -o "$BUILD/os2sha256.obj"
$CLANG $CFLAGS $DEFS $INCLUDES -c "$ROOT/os2boot/crt_shim.c" -o "$BUILD/crt_shim.obj"
$CLANG $CFLAGS $DEFS $INCLUDES -c "$ROOT/os2boot/os2boot.c" -o "$BUILD/os2boot.obj"
"$LLD" /Brepro /machine:x86 /base:0x00400000 /subsystem:windows /entry:entry /nodefaultlib /opt:ref /opt:icf \
    $STDCALL_ALIASES /out:"$BUILD/OS2BOOT.EXE" \
    "$BUILD/os2boot.obj" "$BUILD/os2loader.obj" "$BUILD/os2image.obj" "$BUILD/os2veneer.obj" "$BUILD/os2startup.obj" "$BUILD/os2sha256.obj" "$BUILD/crt_shim.obj" "$BUILD/ntdll.lib"
python3 "$ROOT/tools/patch_pe_subsystem.py" "$BUILD/OS2BOOT.EXE" 5

python3 "$ROOT/tools/verify_pe.py" "$BUILD/OS2SS.EXE"
python3 "$ROOT/tools/verify_pe.py" "$BUILD/OS2LE4CLAUNCH.EXE"
python3 "$ROOT/tools/verify_pe.py" "$BUILD/OS2BOOT.EXE"

if [ -n "$(git -C "$SRC" status --porcelain)" ]; then
    echo "ERROR: ReactOS source tree changed during build" >&2
    git -C "$SRC" status --porcelain >&2
    exit 1
fi

echo "ReactOS tree clean after build: YES"
