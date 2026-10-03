/* Keep the IBM public names separate from Winsock's declarations. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "os2_net.h"
int NETCALL ACCEPT(int s,void *a,int *n) { return os2net_accept(s,a,n); }
int NETCALL BIND(int s,const void *a,int n) { return os2net_bind(s,a,n); }
int NETCALL CONNECT(int s,const void *a,int n) { return os2net_connect(s,a,n); }
int NETCALL GETPEERNAME(int s,void *a,int *n) { return os2net_getpeername(s,a,n); }
int NETCALL GETSOCKNAME(int s,void *a,int *n) { return os2net_getsockname(s,a,n); }
int NETCALL GETSOCKOPT(int s,int l,int o,void *a,int *n) { return os2net_getsockopt(s,l,o,a,n); }
int NETCALL IOCTL(int s,uint32_t c,void *p,int n) { return os2net_ioctl(s,c,p,n); }
int NETCALL LISTEN(int s,int n) { return os2net_listen(s,n); }
int NETCALL RECV(int s,void *b,int n,int f) { return os2net_recv(s,b,n,f); }
int NETCALL RECVFROM(int s,void *b,int n,int f,void *a,int *l) { return os2net_recvfrom(s,b,n,f,a,l); }
int NETCALL SELECT(int *s,int r,int w,int e,int32_t t) { return os2net_select(s,r,w,e,t); }
int NETCALL SEND(int s,const void *b,int n,int f) { return os2net_send(s,b,n,f); }
int NETCALL SENDTO(int s,const void *b,int n,int f,const void *a,int l) { return os2net_sendto(s,b,n,f,a,l); }
int NETCALL SETSOCKOPT(int s,int l,int o,const void *a,int n) { return os2net_setsockopt(s,l,o,a,n); }
int NETCALL SOCKET(int d,int t,int p) { return os2net_socket(d,t,p); }
int NETCALL SOCLOSE(int s) { return os2net_close(s); }
int NETCALL SOCK_ERRNO(void) { return os2net_errno(); }
int NETCALL SHUTDOWN(int s,int h) { return os2net_shutdown(s,h); }
int NETCALL SOCK_INIT(void) { return os2net_init(); }
void NETCALL SET_ERRNO(int e) { os2net_set_errno(e); }
BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,LPVOID reserved)
{
    (void)instance; (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) return os2net_attach();
    if (reason == DLL_PROCESS_DETACH) os2net_detach();
    return TRUE;
}
