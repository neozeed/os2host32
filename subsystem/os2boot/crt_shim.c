#include "../common/reactos_canonical.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Frozen LE1 parser portability shim for the NTDLL-only process vessel. */
void *calloc(size_t Count, size_t Size)
{
    size_t Total;
    if (Count != 0 && Size > (size_t)-1 / Count) return NULL;
    Total = Count * Size;
    return RtlAllocateHeap(RtlGetProcessHeap(), HEAP_ZERO_MEMORY, Total);
}

void *realloc(void *Pointer, size_t Size)
{
    if (Pointer == NULL)
        return RtlAllocateHeap(RtlGetProcessHeap(), 0, Size);
    if (Size == 0)
    {
        RtlFreeHeap(RtlGetProcessHeap(), 0, Pointer);
        return NULL;
    }
    return RtlReAllocateHeap(RtlGetProcessHeap(), 0, Pointer, Size);
}

void free(void *Pointer)
{
    if (Pointer != NULL)
        (void)RtlFreeHeap(RtlGetProcessHeap(), 0, Pointer);
}

