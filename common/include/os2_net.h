#ifndef OS2_NET_H
#define OS2_NET_H

/* IBM TCP/IP 1.x--4.0, flat i386 ABI (not the later sockaddr.sa_len ABI).
 * All public entry points use caller stack cleanup, like OS/2 _System.
 * Do not expose Winsock's SOCKET, hostent, servent or fd_set to the guest. */
#include <stdint.h>
#include <stddef.h>
#ifdef _WIN32
#define NETCALL __cdecl
#else
#define NETCALL
#endif

#define OS2NET_MAX_SOCKETS 256
typedef struct Os2NetAddress {
    uint16_t family, port;
    uint32_t address;
    unsigned char zero[8];
} Os2NetAddress;
typedef struct Os2NetHost {
    char *name;
    char **aliases;
    int32_t family, length;
    char **addresses;
} Os2NetHost;
typedef struct Os2NetService {
    char *name;
    char **aliases;
    int32_t port;
    char *protocol;
} Os2NetService;
typedef struct Os2NetLinger { int32_t on, seconds; } Os2NetLinger;

#if defined(_WIN32) && !defined(OS2NET_HOST_TEST)
typedef char net_pointer_is_32[sizeof(void *) == 4 ? 1 : -1];
typedef char net_host_layout[sizeof(Os2NetHost) == 20 &&
    offsetof(Os2NetHost, addresses) == 16 ? 1 : -1];
typedef char net_service_layout[sizeof(Os2NetService) == 16 &&
    offsetof(Os2NetService, port) == 8 ? 1 : -1];
#endif
typedef char net_address_layout[sizeof(Os2NetAddress) == 16 ? 1 : -1];

/* Native socket backend. The DLL veneer owns the original IBM export names. */
int os2net_attach(void);
void os2net_detach(void);
int os2net_init(void);
int os2net_errno(void);
void os2net_set_errno(int);
int os2net_accept(int, void *, int *);
int os2net_bind(int, const void *, int);
int os2net_connect(int, const void *, int);
int os2net_getpeername(int, void *, int *);
int os2net_getsockname(int, void *, int *);
int os2net_getsockopt(int, int, int, void *, int *);
int os2net_ioctl(int, uint32_t, void *, int);
int os2net_listen(int, int);
int os2net_recv(int, void *, int, int);
int os2net_recvfrom(int, void *, int, int, void *, int *);
int os2net_select(int *, int, int, int, int32_t);
int os2net_send(int, const void *, int, int);
int os2net_sendto(int, const void *, int, int, const void *, int);
int os2net_setsockopt(int, int, int, const void *, int);
int os2net_socket(int, int, int);
int os2net_close(int);
int os2net_shutdown(int, int);

/* Shared SO32DLL exports used by TCP32DLL, through its import library. */
int NETCALL SOCK_INIT(void);
void NETCALL SET_ERRNO(int);
#endif
