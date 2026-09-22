/* R7: GA OS/2 information blocks. Explicit little-endian guest layouts.
 * Kept outside the variable-sized startup strings; all addresses guest-only.
 * TIB: exception head, stack low, stack high, TIB2, version, ordinal.
 * No exception dispatcher is implied by maintaining the exception-chain head.
 */
static uint32_t info_tib_address(uint32_t slot)
{
    return GUEST_INFO + 0x100u + slot * 0x100u;
}

static void init_process_info(struct Runtime *rt, uint32_t env, uint32_t cmd,
                              uint32_t module_flags)
{
    uint8_t *p = rt->ram + GUEST_INFO;
    memset(p, 0, GUEST_INFO_LIMIT - GUEST_INFO);
    wr32(p, 1u);       /* virtual PID; one guest process */
    wr32(p + 4, 0u);   /* no emulated parent */
    wr32(p + 8, 1u);   /* main executable's reserved module handle */
    wr32(p + 12, cmd); /* argv0 NUL command-tail NUL NUL */
    wr32(p + 16, env);
    /* LX windowing flags: PM, window-compatible, or fullscreen console. */
    wr32(p + 24, (module_flags & 0x300u) == 0x300u ? 3u :
                 ((module_flags & 0x300u) == 0x200u ? 2u : 0u));
}

static void init_thread_info(struct Runtime *rt, uint32_t slot)
{
    struct GuestThread *t = &rt->threads[slot];
    uint32_t base = info_tib_address(slot);
    uint32_t selector = (3u + slot) * 8u;
    uint8_t *p = rt->ram + base;
    uint8_t *d = rt->ram + GUEST_GDT + selector;
    WHV_X64_SEGMENT_REGISTER *fs = &t->regs[14].Segment;
    memset(p, 0, 0x100u);
    wr32(p, 0xffffffffu);
    wr32(p + 4, t->stack_base);
    wr32(p + 8, t->stack_base + t->stack_size);
    wr32(p + 12, base + 0x40u);
    wr32(p + 16, 20u);
    wr32(p + 20, t->tid);
    wr32(p + 0x40, t->tid);
    wr32(p + 0x44, 0x0200u); /* regular class, delta zero */
    wr32(p + 0x48, 20u);
    /* Real GDT entry, so reloading FS also restores this thread's base.
       64KiB byte-granular, writable data, D/B=1, ring 0 like existing CS/DS. */
    d[0] = 0xff; d[1] = 0xff;
    d[2] = (uint8_t)base; d[3] = (uint8_t)(base >> 8);
    d[4] = (uint8_t)(base >> 16); d[5] = 0x93;
    d[6] = 0x40; d[7] = (uint8_t)(base >> 24);
    memset(fs, 0, sizeof(*fs));
    fs->Base = base;
    fs->Limit = 0xffffu;
    fs->Selector = (UINT16)selector;
    fs->Attributes = 0x4093;
}

static uint32_t get_info_blocks(struct Runtime *rt, uint32_t pptib, uint32_t pppib)
{
    /* NULL omits an output. Validate both before modifying either. */
    if ((pptib && !guest_range(pptib, 4)) ||
        (pppib && !guest_range(pppib, 4)))
        return OS2_ERROR_INVALID_PARAMETER;
    if (rt->current_thread < 0 || rt->current_thread >= (int)MAX_THREADS)
        return OS2_ERROR_INVALID_FUNCTION;
    if (pptib) guest_put_u32(rt, pptib, info_tib_address((uint32_t)rt->current_thread));
    if (pppib) guest_put_u32(rt, pppib, GUEST_INFO);
    return OS2_NO_ERROR;
}
