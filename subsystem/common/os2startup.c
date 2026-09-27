#include "os2startup.h"
#include <string.h>

static size_t cstrlen(const char *s) { size_t n = 0; while (s[n] != 0) ++n; return n; }

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

int os2c386_build_startup_area(uint8_t *area,
                               size_t area_size,
                               uint32_t area_va,
                               const char *environment,
                               size_t environment_size,
                               const char *program,
                               const char *arg0,
                               OS2C386_STARTUP_INFO *info)
{
    size_t pgm_len, arg0_len, total, pos;
    if (!area || !environment || !program || !arg0 || !info) return 0;
    if (environment_size < 2u || environment[environment_size - 1u] != 0 ||
        environment[environment_size - 2u] != 0) return 0;
    pgm_len = cstrlen(program);
    arg0_len = cstrlen(arg0);
    if (environment_size > SIZE_MAX - pgm_len - arg0_len - 5u) return 0;
    total = environment_size + pgm_len + 1u + arg0_len + 1u + 2u;
    if (total > area_size || total > UINT32_MAX || area_va > UINT32_MAX - (uint32_t)total) return 0;

    memset(area, 0, area_size);
    memcpy(area, environment, environment_size);
    info->env_va = area_va;
    pos = environment_size;
    info->pgm_va = area_va + (uint32_t)pos;
    memcpy(area + pos, program, pgm_len); pos += pgm_len; area[pos++] = 0;
    info->arg_va = area_va + (uint32_t)pos;
    memcpy(area + pos, arg0, arg0_len); pos += arg0_len; area[pos++] = 0;
    area[pos++] = 0; /* empty tail */
    area[pos++] = 0;
    info->startup_bytes = (uint32_t)pos;
    return pos == total;
}

int os2c386_build_initial_stack(uint8_t *stack_object,
                                size_t stack_extent,
                                uint32_t stack_actual_base,
                                uint32_t stack_top,
                                OS2C386_STARTUP_INFO *info)
{
    uint32_t offset, esp;
    if (!stack_object || !info || stack_top < stack_actual_base) return 0;
    offset = stack_top - stack_actual_base;
    if (offset > stack_extent || offset < 20u) return 0;
    esp = stack_top;
    esp -= 4u; wr32le(stack_object + (esp - stack_actual_base), info->arg_va);
    esp -= 4u; wr32le(stack_object + (esp - stack_actual_base), info->env_va);
    esp -= 4u; wr32le(stack_object + (esp - stack_actual_base), 0u);
    esp -= 4u; wr32le(stack_object + (esp - stack_actual_base), 0u);
    esp -= 4u; wr32le(stack_object + (esp - stack_actual_base), 0u);
    info->stack_top = stack_top;
    info->initial_esp = esp;
    return 1;
}

int os2c386_verify_initial_stack(const uint8_t *stack_object,
                                 size_t stack_extent,
                                 uint32_t stack_actual_base,
                                 const OS2C386_STARTUP_INFO *info)
{
    uint32_t off;
    if (!stack_object || !info || info->initial_esp < stack_actual_base) return 0;
    off = info->initial_esp - stack_actual_base;
    if (off > stack_extent || 20u > stack_extent - off) return 0;
    return rd32le(stack_object + off + 0u) == 0u &&
           rd32le(stack_object + off + 4u) == 0u &&
           rd32le(stack_object + off + 8u) == 0u &&
           rd32le(stack_object + off + 12u) == info->env_va &&
           rd32le(stack_object + off + 16u) == info->arg_va;
}
