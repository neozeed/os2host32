#ifndef OS2VENEER_H
#define OS2VENEER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OS2X86_VENEER_STRIDE 16u
#define OS2X86_VENEER_CODE_SIZE 10u

/* Emit: mov eax, ordinal ; jmp rel32 gateway ; int3 padding. */
int os2x86_emit_ordinal_veneer(uint8_t *destination,
                               size_t destination_size,
                               uint32_t veneer_va,
                               uint32_t gateway_va,
                               uint32_t ordinal);

int os2x86_verify_ordinal_veneer(const uint8_t *source,
                                 size_t source_size,
                                 uint32_t veneer_va,
                                 uint32_t gateway_va,
                                 uint32_t ordinal);

#ifdef __cplusplus
}
#endif
#endif
