#ifndef OS2STARTUP_H
#define OS2STARTUP_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OS2C386_STARTUP_INFO {
    uint32_t env_va;
    uint32_t pgm_va;
    uint32_t arg_va;
    uint32_t stack_top;
    uint32_t initial_esp;
    uint32_t startup_bytes;
} OS2C386_STARTUP_INFO;

/* Adaptation of the proven WHP C/386 startup-area layout. */
int os2c386_build_startup_area(uint8_t *area,
                               size_t area_size,
                               uint32_t area_va,
                               const char *environment,
                               size_t environment_size,
                               const char *program,
                               const char *arg0,
                               OS2C386_STARTUP_INFO *info);

/* Proven frame: from stack top push cmd, env, 0, 0, 0. */
int os2c386_build_initial_stack(uint8_t *stack_object,
                                size_t stack_extent,
                                uint32_t stack_actual_base,
                                uint32_t stack_top,
                                OS2C386_STARTUP_INFO *info);

int os2c386_verify_initial_stack(const uint8_t *stack_object,
                                 size_t stack_extent,
                                 uint32_t stack_actual_base,
                                 const OS2C386_STARTUP_INFO *info);

#ifdef __cplusplus
}
#endif
#endif
