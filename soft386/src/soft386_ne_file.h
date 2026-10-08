#ifndef SOFT386_NE_FILE_H
#define SOFT386_NE_FILE_H
#include <stdint.h>
/* OS/2 1.x FILESTATUS level 1: three packed FDATE/FTIME pairs,
 * two query-only ULONG sizes and a USHORT attribute word (22 bytes). */
uint32_t soft386_ne_set_file_info(int host_fd, uint16_t level,
                                  const uint8_t *status, uint16_t cb);
/* 16-bit DosSetFileInfo level 2: FEALIST (including ULONG cbList).
 * The caller validates the guest EAOP far pointers and copies/points only
 * to bounded guest memory; this adapter validates every FEA and sends it to
 * the host filesystem. *error_offset is a byte offset in the FEALIST. */
uint32_t soft386_ne_set_file_eas(int host_fd, const uint8_t *list,
                                uint32_t list_size, uint32_t *error_offset);
#endif
