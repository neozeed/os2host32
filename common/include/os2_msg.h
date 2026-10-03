#ifndef OS2_MSG_H
#define OS2_MSG_H
#include <stdint.h>
uint32_t os2_msg_insert(const char *const *,uint32_t,const char *,uint32_t,char *,uint32_t,uint32_t *);
uint32_t os2_msg_from_file(const unsigned char *,uint32_t,uint32_t,const char *const *,uint32_t,char *,uint32_t,uint32_t *);
#endif
