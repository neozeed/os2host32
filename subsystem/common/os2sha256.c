#include "os2sha256.h"
#include <string.h>

static uint32_t rotr(uint32_t x, unsigned n) { return (x >> n) | (x << (32u - n)); }
static uint32_t be32(const uint8_t *p) { return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3]; }
static void put_be32(uint8_t *p, uint32_t v) { p[0]=(uint8_t)(v>>24); p[1]=(uint8_t)(v>>16); p[2]=(uint8_t)(v>>8); p[3]=(uint8_t)v; }

static const uint32_t K[64] = {
0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u};

static void transform(OS2_SHA256_CTX *ctx, const uint8_t block[64])
{
    uint32_t w[64], a,b,c,d,e,f,g,h, t1,t2;
    unsigned i;
    for (i=0;i<16;i++) w[i]=be32(block+4u*i);
    for (i=16;i<64;i++) {
        uint32_t s0=rotr(w[i-15],7)^rotr(w[i-15],18)^(w[i-15]>>3);
        uint32_t s1=rotr(w[i-2],17)^rotr(w[i-2],19)^(w[i-2]>>10);
        w[i]=w[i-16]+s0+w[i-7]+s1;
    }
    a=ctx->state[0]; b=ctx->state[1]; c=ctx->state[2]; d=ctx->state[3];
    e=ctx->state[4]; f=ctx->state[5]; g=ctx->state[6]; h=ctx->state[7];
    for (i=0;i<64;i++) {
        uint32_t S1=rotr(e,6)^rotr(e,11)^rotr(e,25);
        uint32_t ch=(e&f)^((~e)&g);
        uint32_t S0=rotr(a,2)^rotr(a,13)^rotr(a,22);
        uint32_t maj=(a&b)^(a&c)^(b&c);
        t1=h+S1+ch+K[i]+w[i]; t2=S0+maj;
        h=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
    }
    ctx->state[0]+=a; ctx->state[1]+=b; ctx->state[2]+=c; ctx->state[3]+=d;
    ctx->state[4]+=e; ctx->state[5]+=f; ctx->state[6]+=g; ctx->state[7]+=h;
}

void os2_sha256_init(OS2_SHA256_CTX *ctx)
{
    static const uint32_t init[8]={0x6a09e667u,0xbb67ae85u,0x3c6ef372u,0xa54ff53au,0x510e527fu,0x9b05688cu,0x1f83d9abu,0x5be0cd19u};
    memcpy(ctx->state,init,sizeof(init)); ctx->bit_count=0; ctx->buffer_used=0;
}
void os2_sha256_update(OS2_SHA256_CTX *ctx, const void *data_, size_t size)
{
    const uint8_t *data=(const uint8_t*)data_;
    ctx->bit_count += (uint64_t)size * 8u;
    while (size) {
        size_t take=64u-ctx->buffer_used; if (take>size) take=size;
        memcpy(ctx->buffer+ctx->buffer_used,data,take); ctx->buffer_used+=take; data+=take; size-=take;
        if (ctx->buffer_used==64u) { transform(ctx,ctx->buffer); ctx->buffer_used=0; }
    }
}
void os2_sha256_final(OS2_SHA256_CTX *ctx, uint8_t digest[32])
{
    uint64_t bits=ctx->bit_count; unsigned i;
    ctx->buffer[ctx->buffer_used++]=0x80;
    if (ctx->buffer_used>56u) { while(ctx->buffer_used<64u) ctx->buffer[ctx->buffer_used++]=0; transform(ctx,ctx->buffer); ctx->buffer_used=0; }
    while(ctx->buffer_used<56u) ctx->buffer[ctx->buffer_used++]=0;
    for(i=0;i<8;i++) ctx->buffer[63u-i]=(uint8_t)(bits>>(8u*i));
    transform(ctx,ctx->buffer);
    for(i=0;i<8;i++) put_be32(digest+4u*i,ctx->state[i]);
    memset(ctx,0,sizeof(*ctx));
}
void os2_sha256(const void *data, size_t size, uint8_t digest[32]) { OS2_SHA256_CTX c; os2_sha256_init(&c); os2_sha256_update(&c,data,size); os2_sha256_final(&c,digest); }
