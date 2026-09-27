#ifndef OS2SHA256_H
#define OS2SHA256_H
#include <stddef.h>
#include <stdint.h>

typedef struct OS2_SHA256_CTX {
    uint32_t state[8];
    uint64_t bit_count;
    uint8_t buffer[64];
    size_t buffer_used;
} OS2_SHA256_CTX;

void os2_sha256_init(OS2_SHA256_CTX *ctx);
void os2_sha256_update(OS2_SHA256_CTX *ctx, const void *data, size_t size);
void os2_sha256_final(OS2_SHA256_CTX *ctx, uint8_t digest[32]);
void os2_sha256(const void *data, size_t size, uint8_t digest[32]);
#endif
