#ifndef R0_REACTOS_CANONICAL_H
#define R0_REACTOS_CANONICAL_H

/*
 * R0 canonical-source adapter.
 *
 * This header deliberately defines no SM/SB/native ABI structures itself.
 * All ABI-visible declarations come from the supplied ReactOS source tree.
 */
#define NTOS_MODE_USER
#include <ndk/lpctypes.h>
#include <ndk/lpcfuncs.h>
#include <ndk/mmtypes.h>
#include <ndk/mmfuncs.h>
#include <ndk/obfuncs.h>
#include <ndk/psfuncs.h>
#include <ndk/rtlfuncs.h>
#include <sm/ns.h>
#include <sm/smmsg.h>

/* R0 helper only: use ReactOS's canonical OBJECT_ATTRIBUTES shape. */
static __inline VOID
R0InitObjectAttributes(POBJECT_ATTRIBUTES ObjectAttributes,
                       PUNICODE_STRING ObjectName,
                       ULONG Attributes,
                       HANDLE RootDirectory,
                       PVOID SecurityDescriptor)
{
    InitializeObjectAttributes(ObjectAttributes,
                               ObjectName,
                               Attributes,
                               RootDirectory,
                               SecurityDescriptor);
}


#endif
