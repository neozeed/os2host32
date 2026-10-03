/* Native IPv4 implementation of the old IBM SO32DLL socket ABI.
 * See docs/current/TELNETPM-WINSOCK.md for ABI evidence and limitations. */
#define WIN32_LEAN_AND_MEAN
#define FD_SETSIZE 256
#include <winsock2.h>
#include <windows.h>
#include <string.h>
#include "os2_net.h"

struct NetSocket {
    SOCKET handle;
    unsigned refs;
    int closing, nonblocking, shutdown_bits;
};
static struct NetSocket sockets[OS2NET_MAX_SOCKETS];
static CRITICAL_SECTION socket_lock;
static DWORD error_tls = TLS_OUT_OF_INDEXES;
static int started;

/* IBM's BSD socket errors share Winsock's 10000+ numbering. Winsock-only
 * startup/provider errors are not IBM errors; report SOCEOS2ERR for those. */
static int map_error(int e)
{
    if (e == 0 || e == 10004 || e == 10009 || e == 10013 || e == 10014 ||
        e == 10022 || e == 10024 || (e >= 10035 && e <= 10071)) return e;
    if (e == WSA_NOT_ENOUGH_MEMORY) return 10055;
    if (e == WSA_OPERATION_ABORTED) return 10004;
    return 10100;
}
void os2net_set_errno(int e)
{
    if (error_tls != TLS_OUT_OF_INDEXES)
        TlsSetValue(error_tls, (void *)(intptr_t)e);
}
int os2net_errno(void)
{
    return error_tls == TLS_OUT_OF_INDEXES ? 10100 :
        (int)(intptr_t)TlsGetValue(error_tls);
}
static int fail(int e) { os2net_set_errno(map_error(e)); return -1; }

int os2net_attach(void)
{
    int i;
    error_tls = TlsAlloc();
    if (error_tls == TLS_OUT_OF_INDEXES) return 0;
    InitializeCriticalSection(&socket_lock);
    for (i = 0; i < OS2NET_MAX_SOCKETS; ++i) sockets[i].handle = INVALID_SOCKET;
    return 1;
}
void os2net_detach(void)
{
    /* No Winsock calls under the loader lock. This personality has process
     * lifetime; Windows releases its sockets/WSA reference on process exit. */
    TlsFree(error_tls);
    DeleteCriticalSection(&socket_lock);
}
int os2net_init(void)
{
    WSADATA data;
    int rc = 0;
    EnterCriticalSection(&socket_lock);
    if (!started) {
        rc = WSAStartup(MAKEWORD(2, 2), &data);
        if (!rc && data.wVersion != MAKEWORD(2, 2)) {
            WSACleanup(); rc = WSAVERNOTSUPPORTED;
        }
        if (!rc) started = 1;
    }
    LeaveCriticalSection(&socket_lock);
    return rc ? fail(rc) : 0;
}
static int adopt(SOCKET h, int nonblocking)
{
    int i, e; u_long on = 1;
    /* Native I/O must never block indefinitely: guest blocking semantics are
     * implemented by cancellable readiness waits below. shutdown alone did
     * not release a Windows thread already blocked in recv in the R4 test. */
    if (ioctlsocket(h,FIONBIO,&on) == SOCKET_ERROR) {
        e = WSAGetLastError(); closesocket(h); return fail(e);
    }
    EnterCriticalSection(&socket_lock);
    for (i = 0; i < OS2NET_MAX_SOCKETS; ++i)
        if (sockets[i].handle == INVALID_SOCKET && !sockets[i].refs) break;
    if (i < OS2NET_MAX_SOCKETS) {
        sockets[i].handle = h; sockets[i].closing = 0;
        sockets[i].nonblocking = nonblocking; sockets[i].shutdown_bits = 0;
    }
    LeaveCriticalSection(&socket_lock);
    if (i == OS2NET_MAX_SOCKETS) { closesocket(h); return fail(WSAEMFILE); }
    return i;
}
static SOCKET borrow(int s)
{
    SOCKET h = INVALID_SOCKET;
    EnterCriticalSection(&socket_lock);
    if (s >= 0 && s < OS2NET_MAX_SOCKETS && !sockets[s].closing) {
        h = sockets[s].handle;
        if (h != INVALID_SOCKET) ++sockets[s].refs;
    }
    LeaveCriticalSection(&socket_lock);
    if (h == INVALID_SOCKET) fail(WSAENOTSOCK);
    return h;
}
static void release(int s)
{
    EnterCriticalSection(&socket_lock); --sockets[s].refs;
    LeaveCriticalSection(&socket_lock);
}
static int finish(int s, int rc)
{
    if (rc == SOCKET_ERROR) fail(WSAGetLastError());
    release(s); return rc;
}
static int finish_error(int s, int e)
{
    release(s); return fail(e);
}
static int socket_mode(int s, int *closing, int *shutdown_bits)
{
    int nonblocking;
    EnterCriticalSection(&socket_lock);
    nonblocking = sockets[s].nonblocking;
    *closing = sockets[s].closing; *shutdown_bits = sockets[s].shutdown_bits;
    LeaveCriticalSection(&socket_lock);
    return nonblocking;
}
/* mode: 0=read/accept, 1=write, 2=connect, 3=out-of-band receive.
 * Only the adapter waits. Each native select is bounded to 50 ms so close
 * can cancel a waiter without closing a handle underneath Winsock. */
static int wait_socket(int s, SOCKET h, int mode)
{
    fd_set ready, excepts; struct timeval tv;
    int rc, closing, stopped;
    for (;;) {
        socket_mode(s,&closing,&stopped);
        if (closing) return WSAEINTR;
        if (stopped & ((mode == 0 || mode == 3) ? 1 : 2)) return WSAESHUTDOWN;
        FD_ZERO(&ready); FD_SET(h,&ready);
        FD_ZERO(&excepts); if (mode == 2) FD_SET(h,&excepts);
        tv.tv_sec = 0; tv.tv_usec = 50000;
        rc = select(0,mode == 0 ? &ready : NULL,
            mode == 1 || mode == 2 ? &ready : NULL,
            mode == 3 ? &ready : mode == 2 ? &excepts : NULL,&tv);
        if (rc == SOCKET_ERROR) return WSAGetLastError();
        socket_mode(s,&closing,&stopped);
        if (closing) return WSAEINTR;
        if (rc > 0) return 0;
    }
}
static int retry_io(int s, SOCKET h, int e, int mode)
{
    int closing, stopped, nonblocking;
    if (e != WSAEWOULDBLOCK && e != WSAEINPROGRESS) return e;
    nonblocking = socket_mode(s,&closing,&stopped);
    if (closing) return WSAEINTR;
    if (nonblocking) return e;
    return wait_socket(s,h,mode);
}
static int address_in(const void *p, int len, struct sockaddr_in *a)
{
    Os2NetAddress old;
    if (!p) return fail(WSAEFAULT);
    if (len < (int)sizeof(old)) return fail(WSAEINVAL);
    memcpy(&old, p, sizeof(old));
    if (old.family != 2) return fail(WSAEAFNOSUPPORT);
    memset(a, 0, sizeof(*a)); a->sin_family = AF_INET;
    a->sin_port = old.port; a->sin_addr.s_addr = old.address;
    return 0;
}
static int address_buffer(void *p, int *len)
{
    if (!p || !len) return fail(WSAEFAULT);
    return *len < (int)sizeof(Os2NetAddress) ? fail(WSAEFAULT) : 0;
}
static void address_out(void *p, int *len, const struct sockaddr_in *a)
{
    Os2NetAddress old;
    memset(&old, 0, sizeof(old)); old.family = 2;
    old.port = a->sin_port; old.address = a->sin_addr.s_addr;
    memcpy(p, &old, sizeof(old)); *len = sizeof(old);
}
int os2net_socket(int family, int type, int protocol)
{
    SOCKET h;
    if (family != 2) return fail(WSAEAFNOSUPPORT);
    if (type != 1 && type != 2) return fail(WSAESOCKTNOSUPPORT);
    if (os2net_init()) return -1;
    h = socket(AF_INET, type == 1 ? SOCK_STREAM : SOCK_DGRAM, protocol);
    if (h == INVALID_SOCKET) return fail(WSAGetLastError());
    return adopt(h,0);
}
int os2net_accept(int s, void *p, int *len)
{
    struct sockaddr_in a;
    int n = sizeof(a), e, nonblocking, closing, stopped;
    SOCKET h, result;
    if (p && address_buffer(p, len)) return -1;
    h = borrow(s); if (h == INVALID_SOCKET) return -1;
    nonblocking = socket_mode(s,&closing,&stopped);
    do {
        n = sizeof(a);
        result = accept(h, p ? (struct sockaddr *)&a : NULL, p ? &n : NULL);
        e = result == INVALID_SOCKET ? retry_io(s,h,WSAGetLastError(),0) : 0;
    } while (result == INVALID_SOCKET && !e);
    release(s);
    if (e) return fail(e);
    if (p) address_out(p, len, &a);
    return adopt(result,nonblocking);
}
int os2net_bind(int s, const void *p, int len)
{
    struct sockaddr_in a; SOCKET h;
    if (address_in(p, len, &a)) return -1;
    h = borrow(s); if (h == INVALID_SOCKET) return -1;
    return finish(s, bind(h, (const struct sockaddr *)&a, sizeof(a)));
}
int os2net_connect(int s, const void *p, int len)
{
    struct sockaddr_in a; SOCKET h; int rc, e, n;
    if (address_in(p, len, &a)) return -1;
    h = borrow(s); if (h == INVALID_SOCKET) return -1;
    rc = connect(h, (const struct sockaddr *)&a, sizeof(a));
    if (rc != SOCKET_ERROR) { release(s); return rc; }
    e = retry_io(s,h,WSAGetLastError(),2);
    if (e) return finish_error(s,e);
    n = sizeof(e);
    rc = getsockopt(h,SOL_SOCKET,SO_ERROR,(char *)&e,&n);
    if (rc == SOCKET_ERROR) e = WSAGetLastError();
    if (e) return finish_error(s,e);
    release(s); return 0;
}
static int query_address(int s, void *p, int *len, int peer)
{
    struct sockaddr_in a; SOCKET h; int n = sizeof(a), rc;
    if (address_buffer(p, len)) return -1;
    h = borrow(s); if (h == INVALID_SOCKET) return -1;
    rc = peer ? getpeername(h, (struct sockaddr *)&a, &n) :
                getsockname(h, (struct sockaddr *)&a, &n);
    rc = finish(s, rc);
    if (!rc) address_out(p, len, &a);
    return rc;
}
int os2net_getpeername(int s, void *p, int *n) { return query_address(s,p,n,1); }
int os2net_getsockname(int s, void *p, int *n) { return query_address(s,p,n,0); }
int os2net_listen(int s, int backlog)
{
    SOCKET h = borrow(s); if (h == INVALID_SOCKET) return -1;
    return finish(s, listen(h, backlog));
}
/* IBM and Winsock use bits 1/2/4 for OOB/PEEK/DONTROUTE. Do not pass
 * unknown OS/2 flags through to an unrelated Winsock extension. */
static int buffer_flags(const void *p, int len, int flags, int writing)
{
    if (len < 0) return fail(WSAEINVAL);
    if (len && !p) return fail(WSAEFAULT);
    if (flags & ~(writing ? 5 : 3)) return fail(WSAEOPNOTSUPP);
    return 0;
}
int os2net_recv(int s, void *p, int len, int flags)
{
    SOCKET h; int rc, e;
    if (buffer_flags(p, len, flags, 0)) return -1;
    h = borrow(s); if (h == INVALID_SOCKET) return -1;
    do {
        rc = recv(h, (char *)p, len, flags);
        e = rc == SOCKET_ERROR ? retry_io(s,h,WSAGetLastError(),flags & 1 ? 3 : 0) : 0;
    } while (rc == SOCKET_ERROR && !e);
    if (e) return finish_error(s,e);
    release(s); return rc;
}
int os2net_send(int s, const void *p, int len, int flags)
{
    SOCKET h; int rc, e;
    if (buffer_flags(p, len, flags, 1)) return -1;
    h = borrow(s); if (h == INVALID_SOCKET) return -1;
    do {
        rc = send(h, (const char *)p, len, flags);
        e = rc == SOCKET_ERROR ? retry_io(s,h,WSAGetLastError(),1) : 0;
    } while (rc == SOCKET_ERROR && !e);
    if (e) return finish_error(s,e);
    release(s); return rc;
}
int os2net_recvfrom(int s, void *p, int len, int flags, void *from, int *flen)
{
    struct sockaddr_in a; SOCKET h; int n = sizeof(a), rc, e;
    if (buffer_flags(p, len, flags, 0)) return -1;
    if (from && address_buffer(from, flen)) return -1;
    h = borrow(s); if (h == INVALID_SOCKET) return -1;
    do {
        n = sizeof(a);
        rc = recvfrom(h, (char *)p, len, flags,
            from ? (struct sockaddr *)&a : NULL, from ? &n : NULL);
        e = rc == SOCKET_ERROR ? retry_io(s,h,WSAGetLastError(),flags & 1 ? 3 : 0) : 0;
    } while (rc == SOCKET_ERROR && !e);
    if (e) return finish_error(s,e);
    release(s);
    if (rc >= 0 && from) address_out(from, flen, &a);
    return rc;
}
int os2net_sendto(int s, const void *p, int len, int flags, const void *to, int tlen)
{
    struct sockaddr_in a; SOCKET h; int rc, e;
    if (buffer_flags(p, len, flags, 1)) return -1;
    if (to && address_in(to, tlen, &a)) return -1;
    h = borrow(s); if (h == INVALID_SOCKET) return -1;
    do {
        rc = sendto(h, (const char *)p, len, flags,
            to ? (const struct sockaddr *)&a : NULL, to ? sizeof(a) : 0);
        e = rc == SOCKET_ERROR ? retry_io(s,h,WSAGetLastError(),1) : 0;
    } while (rc == SOCKET_ERROR && !e);
    if (e) return finish_error(s,e);
    release(s); return rc;
}
int os2net_shutdown(int s, int how)
{
    SOCKET h; int rc;
    if (how < 0 || how > 2) return fail(WSAEINVAL);
    h = borrow(s); if (h == INVALID_SOCKET) return -1;
    rc = shutdown(h, how);
    if (!rc) {
        EnterCriticalSection(&socket_lock);
        sockets[s].shutdown_bits |= how == 2 ? 3 : how == 0 ? 1 : 2;
        LeaveCriticalSection(&socket_lock);
    }
    return finish(s,rc);
}
int os2net_close(int s)
{
    SOCKET h = INVALID_SOCKET; int rc, e, busy = 0, nonblocking = 0;
    DWORD started_wait; u_long mode;
    EnterCriticalSection(&socket_lock);
    if (s >= 0 && s < OS2NET_MAX_SOCKETS && !sockets[s].closing) {
        h = sockets[s].handle;
        if (h != INVALID_SOCKET) {
            sockets[s].closing = 1; busy = sockets[s].refs != 0;
            nonblocking = sockets[s].nonblocking;
            ++sockets[s].refs;
        }
    }
    LeaveCriticalSection(&socket_lock);
    if (h == INVALID_SOCKET) return fail(WSAENOTSOCK);
    /* Polling operations see closing and return SOCEINTR. Use elapsed time,
     * not 2000 Sleep(1) calls (which took ~30 seconds on the user's Windows). */
    if (busy) {
        started_wait = GetTickCount();
        for (;;) {
            EnterCriticalSection(&socket_lock);
            busy = sockets[s].refs > 1;
            LeaveCriticalSection(&socket_lock);
            if (!busy || (DWORD)(GetTickCount()-started_wait) >= 2000) break;
            Sleep(1);
        }
        if (busy) {
            EnterCriticalSection(&socket_lock);
            sockets[s].closing = 0; --sockets[s].refs;
            LeaveCriticalSection(&socket_lock);
            return fail(WSAEINPROGRESS);
        }
    }
    /* Positive linger is valid for a blocking guest socket. Only now, with
     * no outstanding borrowers, may its native handle become blocking. */
    e = 0; mode = 0;
    if (!nonblocking && ioctlsocket(h,FIONBIO,&mode) == SOCKET_ERROR) e = WSAGetLastError();
    rc = e ? SOCKET_ERROR : closesocket(h);
    if (rc == SOCKET_ERROR) {
        if (!e) e = WSAGetLastError();
        mode = 1; ioctlsocket(h,FIONBIO,&mode);
    }
    EnterCriticalSection(&socket_lock);
    if (!rc) sockets[s].handle = INVALID_SOCKET;
    else sockets[s].closing = 0;
    --sockets[s].refs;
    LeaveCriticalSection(&socket_lock);
    return e ? fail(e) : rc;
}
int os2net_ioctl(int s, uint32_t cmd, void *p, int len)
{
    SOCKET h; u_long value; long native_cmd; int rc;
    /* Old OS/2 ioctl has FOUR arguments, unlike ioctlsocket. */
    if (cmd == 0x8004667eu) native_cmd = FIONBIO;
    else if (cmd == 0x4004667fu) native_cmd = FIONREAD;
    else if (cmd == 0x40047307u) native_cmd = SIOCATMARK;
    else return fail(WSAEOPNOTSUPP);
    if (!p || len < 4) return fail(WSAEFAULT);
    value = 0;
    h = borrow(s); if (h == INVALID_SOCKET) return -1;
    if (cmd == 0x8004667eu) {
        uint32_t v; memcpy(&v,p,4);
        EnterCriticalSection(&socket_lock); sockets[s].nonblocking = v != 0;
        LeaveCriticalSection(&socket_lock); release(s); return 0;
    }
    rc = finish(s, ioctlsocket(h, native_cmd, &value));
    if (!rc && cmd != 0x8004667eu) { uint32_t v = (uint32_t)value; memcpy(p,&v,4); }
    return rc;
}
static int option(int level, int opt, int *nl, int *no)
{
    if (level == 6 && opt == 1) { *nl = IPPROTO_TCP; *no = TCP_NODELAY; return 0; }
    if (level != 0xffff) return fail(WSAENOPROTOOPT);
    *nl = SOL_SOCKET;
    switch (opt) {
    case 0x0004: *no = SO_REUSEADDR; break;
    case 0x0008: *no = SO_KEEPALIVE; break;
    case 0x0010: *no = SO_DONTROUTE; break;
    case 0x0020: *no = SO_BROADCAST; break;
    case 0x0080: *no = SO_LINGER; break;
    case 0x0100: *no = SO_OOBINLINE; break;
    case 0x1001: *no = SO_SNDBUF; break;
    case 0x1002: *no = SO_RCVBUF; break;
    case 0x1007: *no = SO_ERROR; break;
    case 0x1008: *no = SO_TYPE; break;
    default: return fail(WSAENOPROTOOPT);
    }
    return 0;
}
int os2net_setsockopt(int s, int level, int opt, const void *p, int len)
{
    SOCKET h; int nl, no, n, v; struct linger linger_value; Os2NetLinger old;
    const char *arg;
    if (option(level,opt,&nl,&no)) return -1;
    if (!p) return fail(WSAEFAULT);
    if (nl == SOL_SOCKET && no == SO_LINGER) {
        if (len < (int)sizeof(old)) return fail(WSAEINVAL);
        memcpy(&old,p,sizeof(old));
        if (old.seconds < 0 || old.seconds > 65535) return fail(WSAEINVAL);
        linger_value.l_onoff = old.on != 0;
        linger_value.l_linger = (u_short)old.seconds;
        arg = (const char *)&linger_value; n = sizeof(linger_value);
    } else {
        if (len < 4) return fail(WSAEINVAL);
        memcpy(&v,p,4); arg = (const char *)&v; n = sizeof(v);
    }
    h = borrow(s); if (h == INVALID_SOCKET) return -1;
    return finish(s, setsockopt(h,nl,no,arg,n));
}
int os2net_getsockopt(int s, int level, int opt, void *p, int *len)
{
    SOCKET h; int nl, no, n, v, rc, needed; struct linger linger_value;
    Os2NetLinger old; char *arg;
    if (option(level,opt,&nl,&no)) return -1;
    needed = nl == SOL_SOCKET && no == SO_LINGER ? 8 : 4;
    if (!p || !len || *len < needed) return fail(WSAEFAULT);
    arg = needed == 8 ? (char *)&linger_value : (char *)&v;
    n = needed == 8 ? sizeof(linger_value) : sizeof(v);
    h = borrow(s); if (h == INVALID_SOCKET) return -1;
    rc = finish(s, getsockopt(h,nl,no,arg,&n));
    if (!rc) {
        if (needed == 8) {
            old.on = linger_value.l_onoff; old.seconds = linger_value.l_linger;
            memcpy(p,&old,sizeof(old));
        } else {
            if (nl == SOL_SOCKET && no == SO_ERROR) v = map_error(v);
            memcpy(p,&v,4);
        }
        *len = needed;
    }
    return rc;
}
int os2net_select(int *array, int nr, int nw, int ne, int32_t ms)
{
    fd_set sets[3], original[3]; SOCKET handles[OS2NET_MAX_SOCKETS * 3];
    struct timeval tv; int i, total, group, rc, ready = 0, e = 0;
    int closing, stopped; DWORD start, elapsed, wait_ms;
    if (nr < 0 || nw < 0 || ne < 0 || nr > OS2NET_MAX_SOCKETS ||
        nw > OS2NET_MAX_SOCKETS || ne > OS2NET_MAX_SOCKETS || ms < -1)
        return fail(WSAEINVAL);
    total = nr + nw + ne;
    if (total && !array) return fail(WSAEFAULT);
    if (!total) { Sleep(ms < 0 ? INFINITE : (DWORD)ms); return 0; }
    for (i = 0; i < 3; ++i) FD_ZERO(&sets[i]);
    for (i = 0; i < total; ++i) {
        handles[i] = borrow(array[i]);
        if (handles[i] == INVALID_SOCKET) {
            while (i) { --i; release(array[i]); }
            return -1;
        }
        group = i < nr ? 0 : i < nr + nw ? 1 : 2;
        FD_SET(handles[i], &sets[group]);
    }
    memcpy(original,sets,sizeof(sets)); start = GetTickCount();
    for (;;) {
        for (i = 0; i < total; ++i) {
            socket_mode(array[i],&closing,&stopped);
            if (closing) { e = WSAEINTR; break; }
        }
        if (e) { rc = SOCKET_ERROR; break; }
        elapsed = GetTickCount()-start;
        wait_ms = ms < 0 ? 50 : elapsed >= (DWORD)ms ? 0 : (DWORD)ms-elapsed;
        if (wait_ms > 50) wait_ms = 50;
        tv.tv_sec = 0; tv.tv_usec = wait_ms*1000;
        memcpy(sets,original,sizeof(sets));
        rc = select(0, nr ? &sets[0] : NULL, nw ? &sets[1] : NULL,
            ne ? &sets[2] : NULL, &tv);
        if (rc == SOCKET_ERROR) { e = WSAGetLastError(); break; }
        for (i = 0; i < total; ++i) {
            socket_mode(array[i],&closing,&stopped);
            if (closing) { e = WSAEINTR; break; }
        }
        if (e) { rc = SOCKET_ERROR; break; }
        if (rc || (ms >= 0 && (DWORD)(GetTickCount()-start) >= (DWORD)ms)) break;
    }
    for (i = 0; i < total; ++i) {
        release(array[i]);
        /* IBM documents array edits only for a positive return. Preserve it
         * on timeout/error; never compact the three adjacent groups. */
        if (rc > 0) {
            group = i < nr ? 0 : i < nr + nw ? 1 : 2;
            if (FD_ISSET(handles[i], &sets[group])) ++ready;
            else array[i] = -1;
        }
    }
    return e ? fail(e) : rc > 0 ? ready : rc;
}
