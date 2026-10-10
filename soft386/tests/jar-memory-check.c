/* Exercise the actual jar allocator and DOS dispatch, with a hostile native
 * provider: memory reporting must never ask that provider for ordinal 348. */
#define main soft386_vessel_main
#include "../src/soft386_os2.c"
#undef main
#include <assert.h>

#define MIB 0x100000u
#define STACK 0x1000u
#define OUT 0x2000u
#define PTR 0x2100u
static unsigned provider_lookups;
static void *native_provider(void *opaque, uint32_t ordinal)
{
    (void)opaque;
    assert(ordinal != 348u);
    provider_lookups++;
    return NULL;
}
static uint32_t call(struct Runtime *rt, uint32_t ordinal,
                     uint32_t a, uint32_t b, uint32_t c, uint32_t d)
{
    guest_put_u32(rt, STACK+4, a); guest_put_u32(rt, STACK+8, b);
    guest_put_u32(rt, STACK+12, c); guest_put_u32(rt, STACK+16, d);
    guest_put_u32(rt, STACK+20, 0);
    return dispatch_doscalls(rt, ordinal, STACK);
}
static void budget(struct Runtime *rt, uint32_t free_bytes, uint32_t largest)
{
    unsigned before = provider_lookups;
    assert(!call(rt, 348, 17, 21, OUT, 20));
    assert(provider_lookups == before);
    assert(guest_u32(rt, OUT) == RAM_SIZE);
    assert(guest_u32(rt, OUT+4) == RAM_SIZE-free_bytes);
    assert(guest_u32(rt, OUT+8) == free_bytes);
    assert(guest_u32(rt, OUT+12) == largest);
    assert(guest_u32(rt, OUT+16) == largest);
}
static uint32_t allocate(struct Runtime *rt, uint32_t n)
{
    assert(!call(rt, 299, PTR, n, PAG_READ|PAG_WRITE|PAG_COMMIT, 0));
    return guest_u32(rt, PTR);
}
static void reset(struct Runtime *rt)
{
    memset(rt->allocs, 0, sizeof(rt->allocs));
    rt->alloc_next = GUEST_ALLOC_BASE;
}
int main(void)
{
    struct Runtime *rt = calloc(1, sizeof(*rt));
    struct Os2MemoryStatus value;
    uint32_t a,b,c,i,blocks[MAX_ALLOCS];
    assert(rt); rt->ram=calloc(1,RAM_SIZE); assert(rt->ram);
    soft386_doscalls_bridge_set_provider(&rt->native_dos,NULL,native_provider,NULL,0);
    reset(rt); budget(rt,32*MIB,32*MIB);
    a=allocate(rt,1); budget(rt,32*MIB-4096,32*MIB-4096);
    assert(!call(rt,305,a,4096,PAG_READ,0));
    budget(rt,32*MIB-4096,32*MIB-4096); /* protection doesn't free backing */
    assert(!call(rt,304,a,0,0,0)); budget(rt,32*MIB,32*MIB-4096);
    b=allocate(rt,4096); assert(b==a); budget(rt,32*MIB-4096,32*MIB-4096);
    memset(rt->ram+OUT,0xcc,24);
    assert(call(rt,348,17,21,OUT,19)==87 && rt->ram[OUT]==0xcc);
    assert(call(rt,348,17,21,RAM_SIZE-4,20)==87);
    assert(call(rt,348,0,21,OUT,88)==87);
    assert(call(rt,348,21,17,OUT,20)==87);
    assert(!call(rt,348,11,12,OUT,8));
    assert(guest_u32(rt,OUT)==20 && guest_u32(rt,OUT+4)==0);
    assert(soft_query_memory_status(NULL,&value)==87);
    assert(soft_query_memory_status(rt,NULL)==87);

    reset(rt); a=allocate(rt,16*MIB); b=allocate(rt,8*MIB); c=allocate(rt,8*MIB);
    budget(rt,0,0); assert(call(rt,299,PTR,4096,3,0)==8);
    assert(!call(rt,304,a,0,0,0)); assert(!call(rt,304,c,0,0,0));
    budget(rt,24*MIB,16*MIB); /* aggregate differs from largest hole */
    assert(call(rt,299,PTR,20*MIB,3,0)==8);
    c=allocate(rt,4*MIB); assert(c==a);
    budget(rt,8*MIB,8*MIB); /* reuse retains the whole 16 MiB block */
    assert(!call(rt,304,c,0,0,0)); assert(!call(rt,304,b,0,0,0));
    budget(rt,32*MIB,16*MIB); /* freed neighbours are not coalesced */

    reset(rt);
    for(i=0;i<MAX_ALLOCS;i++) blocks[i]=allocate(rt,4096);
    budget(rt,0,0); /* untouched tail is unusable without a virgin slot */
    assert(call(rt,299,PTR,4096,3,0)==8);
    assert(!call(rt,304,blocks[42],0,0,0)); budget(rt,4096,4096);
    assert(call(rt,299,PTR,8192,3,0)==8);
    assert(allocate(rt,1)==blocks[42]); budget(rt,0,0);
    soft386_doscalls_bridge_close(&rt->native_dos);
    free(rt->ram); free(rt);
    puts("Soft386 jar memory PASS: local dispatch, allocation/free, fragmentation, slot exhaustion, validation");
    return 0;
}
