#ifndef FPU_H
#define FPU_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

typedef struct FPU FPU;

FPU *fpu_new();
void fpu_delete(FPU *fpu);

/* Opaque scheduler snapshot helpers.  FPU contains no host pointers in the
 * current Tiny386 implementation, so a byte-exact in-process snapshot is
 * sufficient and keeps the internal x87 representation private. */
size_t fpu_state_size(void);
bool fpu_save_state(FPU *fpu, void *dst, size_t cb);
bool fpu_load_state(FPU *fpu, const void *src, size_t cb);
bool fpu_exec1(FPU *fpu, void *cpu, int op, int group, unsigned int i);
bool fpu_exec2(FPU *fpu, void *cpu, bool opsz16, int op, int group, int seg, uword addr);

#endif /* FPU_H */
