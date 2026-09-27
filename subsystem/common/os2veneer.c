#include "os2veneer.h"
#include <string.h>

static void wr32le(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xffu);
    p[1] = (uint8_t)((v >> 8) & 0xffu);
    p[2] = (uint8_t)((v >> 16) & 0xffu);
    p[3] = (uint8_t)((v >> 24) & 0xffu);
}

static uint32_t rd32le(const uint8_t *p)
{
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static int rel32(uint32_t from_next, uint32_t target, uint32_t *encoded)
{
    int64_t delta = (int64_t)(uint64_t)target - (int64_t)(uint64_t)from_next;
    if (delta < INT32_MIN || delta > INT32_MAX) return 0;
    *encoded = (uint32_t)(int32_t)delta;
    return 1;
}

int os2x86_emit_ordinal_veneer(uint8_t *destination,
                               size_t destination_size,
                               uint32_t veneer_va,
                               uint32_t gateway_va,
                               uint32_t ordinal)
{
    uint32_t displacement;
    if (!destination || destination_size < OS2X86_VENEER_STRIDE) return 0;
    if (!rel32(veneer_va + OS2X86_VENEER_CODE_SIZE, gateway_va, &displacement)) return 0;
    memset(destination, 0xcc, OS2X86_VENEER_STRIDE);
    destination[0] = 0xb8;                 /* mov eax, imm32 */
    wr32le(destination + 1, ordinal);
    destination[5] = 0xe9;                 /* jmp rel32 */
    wr32le(destination + 6, displacement);
    return 1;
}

int os2x86_verify_ordinal_veneer(const uint8_t *source,
                                 size_t source_size,
                                 uint32_t veneer_va,
                                 uint32_t gateway_va,
                                 uint32_t ordinal)
{
    uint32_t displacement;
    uint32_t expected;
    size_t i;
    if (!source || source_size < OS2X86_VENEER_STRIDE) return 0;
    if (source[0] != 0xb8 || rd32le(source + 1) != ordinal || source[5] != 0xe9) return 0;
    if (!rel32(veneer_va + OS2X86_VENEER_CODE_SIZE, gateway_va, &expected)) return 0;
    displacement = rd32le(source + 6);
    if (displacement != expected) return 0;
    for (i = OS2X86_VENEER_CODE_SIZE; i < OS2X86_VENEER_STRIDE; ++i)
        if (source[i] != 0xcc) return 0;
    return 1;
}
