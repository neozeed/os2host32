/* IBM TCP32DLL resolver/address utilities over Winsock. Never return a native
 * hostent: IBM uses 32-bit ints and its address-list pointer is at byte 16. */
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#include <string.h>
#include <stdio.h>
#include "os2_net.h"

#define NET_RESULT_BYTES 65536u
struct ResolverState { void *host, *service; int error; char address[16]; };
struct Arena { char *base; size_t used; };
static DWORD resolver_tls = TLS_OUT_OF_INDEXES;

static struct ResolverState *state(void)
{
    struct ResolverState *s = (struct ResolverState *)TlsGetValue(resolver_tls);
    if (!s) {
        s = (struct ResolverState *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*s));
        if (!s) { SET_ERRNO(10055); return NULL; }
        if (!TlsSetValue(resolver_tls,s)) {
            HeapFree(GetProcessHeap(),0,s); SET_ERRNO(10055); return NULL;
        }
    }
    return s;
}
static void free_state(void)
{
    struct ResolverState *s = (struct ResolverState *)TlsGetValue(resolver_tls);
    if (!s) return;
    if (s->host) HeapFree(GetProcessHeap(),0,s->host);
    if (s->service) HeapFree(GetProcessHeap(),0,s->service);
    HeapFree(GetProcessHeap(),0,s); TlsSetValue(resolver_tls,NULL);
}
static void *take(struct Arena *a, size_t n)
{
    size_t p = (a->used + sizeof(void *) - 1) & ~(sizeof(void *) - 1);
    if (p > NET_RESULT_BYTES || n > NET_RESULT_BYTES - p) return NULL;
    a->used = p + n; return a->base + p;
}
static char *copy_string(struct Arena *a, const char *str)
{
    size_t n = str ? strlen(str) : 0;
    char *p = (char *)take(a,n+1);
    if (p) { if (n) memcpy(p,str,n); p[n] = 0; }
    return p;
}
static char **copy_list(struct Arena *a, char *const *list, size_t item_size)
{
    size_t n = 0, i; char **p;
    if (list) while (list[n]) { if (++n > 4096) return NULL; }
    p = (char **)take(a,(n+1)*sizeof(char *));
    if (!p) return NULL;
    for (i = 0; i < n; ++i) {
        if (item_size) {
            p[i] = (char *)take(a,item_size);
            if (p[i]) memcpy(p[i],list[i],item_size);
        } else p[i] = copy_string(a,list[i]);
        if (!p[i]) return NULL;
    }
    p[n] = NULL; return p;
}
static void resolver_error(struct ResolverState *s, int e)
{
    switch (e) {
    case WSAHOST_NOT_FOUND: s->error = 1; break;
    case WSATRY_AGAIN: s->error = 2; break;
    case WSANO_DATA: s->error = 4; break;
    default: s->error = 3; break;
    }
    SET_ERRNO(e == WSA_NOT_ENOUGH_MEMORY || e == WSAENOBUFS ? 10055 :
        e == WSAEFAULT ? 10014 : e == WSAEINVAL ? 10022 : 10100);
}
static Os2NetHost *copy_host(struct ResolverState *s, const struct hostent *h)
{
    struct Arena a; Os2NetHost *result;
    if (h->h_addrtype != AF_INET || h->h_length != 4) {
        resolver_error(s,WSANO_DATA); return NULL;
    }
    a.base = (char *)HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,NET_RESULT_BYTES); a.used = 0;
    if (!a.base) { resolver_error(s,WSAENOBUFS); return NULL; }
    result = (Os2NetHost *)take(&a,sizeof(*result));
    result->name = copy_string(&a,h->h_name);
    result->aliases = copy_list(&a,h->h_aliases,0);
    result->family = 2; result->length = 4;
    result->addresses = copy_list(&a,h->h_addr_list,4);
    if (!result->name || !result->aliases || !result->addresses) {
        HeapFree(GetProcessHeap(),0,a.base); resolver_error(s,WSAENOBUFS); return NULL;
    }
    if (s->host) HeapFree(GetProcessHeap(),0,s->host);
    s->host = a.base; s->error = 0; return result;
}
static Os2NetService *copy_service(struct ResolverState *s, const struct servent *v)
{
    struct Arena a; Os2NetService *result;
    a.base = (char *)HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,NET_RESULT_BYTES); a.used = 0;
    if (!a.base) { SET_ERRNO(10055); return NULL; }
    result = (Os2NetService *)take(&a,sizeof(*result));
    result->name = copy_string(&a,v->s_name);
    result->aliases = copy_list(&a,v->s_aliases,0);
    /* Zero extend Winsock's signed short, preserving network byte order. */
    result->port = (uint16_t)v->s_port;
    result->protocol = copy_string(&a,v->s_proto);
    if (!result->name || !result->aliases || !result->protocol) {
        HeapFree(GetProcessHeap(),0,a.base); SET_ERRNO(10055); return NULL;
    }
    if (s->service) HeapFree(GetProcessHeap(),0,s->service);
    s->service = a.base; return result;
}
uint32_t NETCALL LSWAP(uint32_t v)
{
    return (v>>24) | ((v>>8)&0xff00u) | ((v<<8)&0xff0000u) | (v<<24);
}
uint16_t NETCALL BSWAP(uint16_t v) { return (uint16_t)((v>>8)|(v<<8)); }
uint32_t NETCALL INET_ADDR(const char *str)
{
    if (!str) { SET_ERRNO(10014); return 0xffffffffu; }
    if (SOCK_INIT()) return 0xffffffffu;
    return (uint32_t)inet_addr(str);
}
char *NETCALL INET_NTOA(uint32_t address)
{
    struct ResolverState *s = state(); const unsigned char *b = (const unsigned char *)&address;
    if (!s) return NULL;
    sprintf(s->address,"%u.%u.%u.%u",(unsigned)b[0],(unsigned)b[1],(unsigned)b[2],(unsigned)b[3]);
    return s->address;
}
Os2NetHost *NETCALL GETHOSTBYNAME(const char *name)
{
    struct ResolverState *s = state(); struct hostent *h;
    if (!s) return NULL;
    if (!name) { resolver_error(s,WSAEFAULT); return NULL; }
    if (SOCK_INIT()) { s->error = 3; return NULL; }
    h = gethostbyname(name);
    if (!h) { resolver_error(s,WSAGetLastError()); return NULL; }
    return copy_host(s,h);
}
Os2NetHost *NETCALL GETHOSTBYADDR(const char *address, int len, int family)
{
    struct ResolverState *s = state(); struct hostent *h;
    if (!s) return NULL;
    if (!address) { resolver_error(s,WSAEFAULT); return NULL; }
    if (len != 4 || family != 2) { resolver_error(s,WSAEINVAL); return NULL; }
    if (SOCK_INIT()) { s->error = 3; return NULL; }
    h = gethostbyaddr(address,len,AF_INET);
    if (!h) { resolver_error(s,WSAGetLastError()); return NULL; }
    return copy_host(s,h);
}
Os2NetService *NETCALL GETSERVBYNAME(const char *name, const char *protocol)
{
    struct ResolverState *s = state(); struct servent *v;
    if (!s) return NULL;
    if (!name) { SET_ERRNO(10014); return NULL; }
    if (SOCK_INIT()) return NULL;
    v = getservbyname(name,protocol);
    if (!v) { SET_ERRNO(10100); return NULL; }
    return copy_service(s,v);
}
Os2NetService *NETCALL GETSERVBYPORT(int port, const char *protocol)
{
    struct ResolverState *s = state(); struct servent *v;
    if (!s) return NULL;
    if (SOCK_INIT()) return NULL;
    v = getservbyport((uint16_t)port,protocol);
    if (!v) { SET_ERRNO(10100); return NULL; }
    return copy_service(s,v);
}
int NETCALL GETHOSTNAME(char *buffer, int len)
{
    if (!buffer || len <= 0) { SET_ERRNO(10014); return -1; }
    if (SOCK_INIT()) return -1;
    if (gethostname(buffer,len) == SOCKET_ERROR) {
        int e = WSAGetLastError(); SET_ERRNO(e == WSAEFAULT ? 10014 : 10100); return -1;
    }
    return 0;
}
int NETCALL TCP_H_ERRNO(void)
{
    struct ResolverState *s = state(); return s ? s->error : 3;
}
BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,LPVOID reserved)
{
    (void)instance; (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        resolver_tls = TlsAlloc(); return resolver_tls != TLS_OUT_OF_INDEXES;
    }
    if (reason == DLL_THREAD_DETACH || reason == DLL_PROCESS_DETACH) free_state();
    if (reason == DLL_PROCESS_DETACH) TlsFree(resolver_tls);
    return TRUE;
}
