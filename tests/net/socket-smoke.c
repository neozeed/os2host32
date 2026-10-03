/* Real Windows/Winsock test through the original OS/2 ordinal ABI. No guest
 * executable, external server, DNS outside localhost, or Python required. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "os2_net.h"

#define FN(ret,name,args) typedef ret (NETCALL *name##Fn) args; static name##Fn name
FN(int,init,(void));
FN(int,net_error,(void));
FN(void,set_error,(int));
FN(int,create_socket,(int,int,int));
FN(int,close_socket,(int));
FN(int,bind_socket,(int,const void *,int));
FN(int,listen_socket,(int,int));
FN(int,connect_socket,(int,const void *,int));
FN(int,accept_socket,(int,void *,int *));
FN(int,socket_name,(int,void *,int *));
FN(int,peer_name,(int,void *,int *));
FN(int,send_bytes,(int,const void *,int,int));
FN(int,recv_bytes,(int,void *,int,int));
FN(int,send_to,(int,const void *,int,int,const void *,int));
FN(int,recv_from,(int,void *,int,int,void *,int *));
FN(int,select_sockets,(int *,int,int,int,int32_t));
FN(int,socket_ioctl,(int,uint32_t,void *,int));
FN(int,set_option,(int,int,int,const void *,int));
FN(int,get_option,(int,int,int,void *,int *));
FN(int,shutdown_socket,(int,int));
FN(uint32_t,swap_long,(uint32_t));
FN(uint16_t,swap_short,(uint16_t));
FN(uint32_t,parse_address,(const char *));
FN(char *,format_address,(uint32_t));
FN(Os2NetHost *,host_by_name,(const char *));
FN(Os2NetHost *,host_by_address,(const char *,int,int));
FN(Os2NetService *,service_by_name,(const char *,const char *));
FN(Os2NetService *,service_by_port,(int,const char *));
FN(int,host_name,(char *,int));
FN(int,host_error,(void));

#define BIND_API(mod,var,ord) do { FARPROC p = GetProcAddress(mod,(LPCSTR)(ULONG_PTR)ord); \
    if (!p) { printf("MISSING ordinal %u\n",(unsigned)ord); return 1; } \
    memcpy(&var,&p,sizeof(var)); } while (0)
#define CHECK(expr) do { ++checks; if (!(expr)) { \
    printf("FAIL line %d: %s; sock_errno=%d Win32=%lu\n",__LINE__,#expr, \
        net_error ? net_error() : -1,(unsigned long)GetLastError()); return 1; } } while (0)

static int ready(int s)
{
    int a = s; return select_sockets(&a,1,0,0,2000) == 1;
}
static int receive_all(int s,char *buffer,int length)
{
    int total = 0, n;
    while (total < length) {
        if (!ready(s)) return -1;
        n = recv_bytes(s,buffer+total,length-total,0);
        if (n <= 0) return -1;
        total += n;
    }
    return total;
}
static DWORD WINAPI worker(LPVOID unused)
{
    Os2NetHost *h;
    (void)unused;
    if (net_error() != 0) return 1;
    h = host_by_name("127.0.0.1");
    if (!h || h->family != 2 || h->length != 4) return 2;
    if (close_socket(-1) != -1 || net_error() != 10038) return 3;
    return 0;
}
struct Reader { int socket, operation; HANDLE entered; };
static DWORD WINAPI blocked_reader(LPVOID context)
{
    struct Reader *reader = (struct Reader *)context;
    char byte; int rc, e, array = reader->socket;
    SetEvent(reader->entered);
    if (reader->operation == 0) rc = recv_bytes(reader->socket,&byte,1,0);
    else if (reader->operation == 1) rc = accept_socket(reader->socket,NULL,NULL);
    else if (reader->operation == 2) rc = recv_from(reader->socket,&byte,1,0,NULL,NULL);
    else rc = select_sockets(&array,1,0,0,-1);
    e = net_error();
    /* A close that wins the scheduling race can reject the new borrow. */
    return rc == -1 && (e == 10038 || e == 10004) && array == reader->socket ? 0 : 1;
}
static int cancel_wait(int socket_value, int operation)
{
    static const char *names[] = {"recv","accept","recvfrom","select(-1)"};
    struct Reader reader; HANDLE thread; DWORD result, start, elapsed;
    int checks = 0, rc;
    reader.socket = socket_value; reader.operation = operation;
    reader.entered = CreateEventA(NULL,TRUE,FALSE,NULL); CHECK(reader.entered != NULL);
    thread = CreateThread(NULL,0,blocked_reader,&reader,0,NULL); CHECK(thread != NULL);
    CHECK(WaitForSingleObject(reader.entered,2000) == WAIT_OBJECT_0); Sleep(100);
    start = GetTickCount(); rc = close_socket(socket_value); elapsed = GetTickCount()-start;
    printf("Close during %s: result=%d, elapsed=%lu ms\n",names[operation],rc,(unsigned long)elapsed);
    CHECK(rc == 0 && elapsed < 2000);
    CHECK(WaitForSingleObject(thread,2000) == WAIT_OBJECT_0);
    CHECK(GetExitCodeThread(thread,&result) && result == 0);
    CloseHandle(thread); CloseHandle(reader.entered);
    return 0;
}
int main(void)
{
    HMODULE so, tcp; HANDLE thread; DWORD result;
    Os2NetAddress address, remote; Os2NetHost *host; Os2NetService *service;
    Os2NetLinger linger_value, linger_out;
    int checks = 0, listener, client, peer, udp, sender, n, list[3], value, i;
    char buffer[256], saved_host[256]; const char payload[8] = {0,'t','e','l','n','e','t',(char)255};
    uint32_t on = 1;
    so = LoadLibraryA("SO32DLL.dll"); tcp = LoadLibraryA("TCP32DLL.dll");
    CHECK(so && tcp);
    BIND_API(so,init,26); BIND_API(so,net_error,20); BIND_API(so,set_error,35);
    BIND_API(so,create_socket,16); BIND_API(so,close_socket,17);
    BIND_API(so,bind_socket,2); BIND_API(so,listen_socket,9);
    BIND_API(so,connect_socket,3); BIND_API(so,accept_socket,1);
    BIND_API(so,socket_name,6); BIND_API(so,peer_name,5);
    BIND_API(so,send_bytes,13); BIND_API(so,recv_bytes,10);
    BIND_API(so,send_to,14); BIND_API(so,recv_from,11);
    BIND_API(so,select_sockets,12); BIND_API(so,socket_ioctl,8);
    BIND_API(so,set_option,15); BIND_API(so,get_option,7);
    BIND_API(so,shutdown_socket,25);
    BIND_API(tcp,swap_long,3); BIND_API(tcp,swap_short,4);
    BIND_API(tcp,parse_address,5); BIND_API(tcp,format_address,10);
    BIND_API(tcp,host_by_name,11); BIND_API(tcp,host_by_address,12);
    BIND_API(tcp,service_by_name,24); BIND_API(tcp,service_by_port,23);
    BIND_API(tcp,host_name,44); BIND_API(tcp,host_error,51);
    puts("SO32DLL/TCP32DLL: 30 exports loaded by ordinal");
    CHECK(init() == 0 && init() == 0);
    CHECK(sizeof(Os2NetHost) == 20 && sizeof(Os2NetService) == 16);
    CHECK(swap_short(0x1234) == 0x3412 && swap_long(0x12345678) == 0x78563412);
    CHECK(parse_address("127.0.0.1") == 0x0100007fu);
    CHECK(parse_address("invalid") == 0xffffffffu);
    CHECK(!strcmp(format_address(0x0100007fu),"127.0.0.1"));
    CHECK(host_name(buffer,sizeof(buffer)) == 0);
    host = host_by_name("localhost");
    CHECK(host && host->family == 2 && host->length == 4 && host->addresses && host->addresses[0]);
    CHECK(host->name && strlen(host->name) < sizeof(saved_host)); strcpy(saved_host,host->name);
    service = service_by_name("telnet","tcp");
    CHECK(service && service->port == 0x1700 && !strcmp(service->protocol,"tcp"));
    CHECK(!strcmp(host->name,saved_host));
    service = service_by_port(swap_short(23),"tcp"); CHECK(service && service->port == 0x1700);
    CHECK(host_by_name(NULL) == NULL && host_error() == 3);
    host = host_by_name("127.0.0.1"); CHECK(host && host_error() == 0);
    /* Same guest mutation seen in TELNETPM's alternate-address retry path. */
    ++host->addresses;
    host = host_by_name("127.0.0.1"); CHECK(host && host->length == 4);
    set_error(10061);
    thread = CreateThread(NULL,0,worker,NULL,0,NULL); CHECK(thread != NULL);
    CHECK(WaitForSingleObject(thread,5000) == WAIT_OBJECT_0);
    CHECK(GetExitCodeThread(thread,&result) && result == 0); CloseHandle(thread);
    CHECK(net_error() == 10061 && host->length == 4);
    puts("Resolver layout, byte order, result ownership, and per-thread errors PASS");

    memset(&address,0,sizeof(address)); address.family = 2; address.address = 0x0100007fu;
    listener = create_socket(2,1,0); client = create_socket(2,1,0);
    CHECK(listener >= 0 && listener < 256 && client >= 0 && client < 256);
    CHECK(bind_socket(listener,&address,sizeof(address)) == 0);
    n = sizeof(address); CHECK(socket_name(listener,&address,&n) == 0 && n == 16 && address.port);
    CHECK(listen_socket(listener,1) == 0);
    CHECK(connect_socket(client,&address,sizeof(address)) == 0 && ready(listener));
    n = sizeof(remote); peer = accept_socket(listener,&remote,&n); CHECK(peer >= 0 && n == 16);
    n = sizeof(remote); CHECK(peer_name(client,&remote,&n) == 0 && remote.port == address.port);
    CHECK(send_bytes(client,payload,8,0) == 8 && ready(peer));
    list[0] = peer; list[1] = client; list[2] = client;
    CHECK(select_sockets(list,2,1,0,2000) == 2 && list[0] == peer && list[1] == -1 && list[2] == client);
    value = 0; CHECK(socket_ioctl(peer,0x4004667fu,&value,4) == 0 && value > 0);
    n = recv_bytes(peer,buffer,8,2); CHECK(n > 0 && !memcmp(buffer,payload,n));
    CHECK(receive_all(peer,buffer,8) == 8 && !memcmp(buffer,payload,8));
    list[0] = peer; CHECK(select_sockets(list,1,0,0,0) == 0 && list[0] == peer);
    CHECK(socket_ioctl(peer,0x8004667eu,&on,4) == 0);
    CHECK(recv_bytes(peer,buffer,1,0) == -1 && net_error() == 10035);
    n = sizeof(remote); CHECK(socket_name(peer,&remote,&n) == 0 && net_error() == 10035);
    linger_value.on = 1; linger_value.seconds = 3;
    CHECK(set_option(peer,0xffff,128,&linger_value,sizeof(linger_value)) == 0);
    n = sizeof(linger_out);
    CHECK(get_option(peer,0xffff,128,&linger_out,&n) == 0 && n == 8 && linger_out.on == 1 && linger_out.seconds == 3);
    value = 1; CHECK(set_option(peer,6,1,&value,4) == 0);
    CHECK(set_option(peer,0xffff,0x1005,&value,4) == -1 && net_error() == 10042);
    CHECK(socket_ioctl(peer,0xdeadbeefu,&value,4) == -1 && net_error() == 10045);
    CHECK(shutdown_socket(client,1) == 0 && ready(peer));
    CHECK(recv_bytes(peer,buffer,1,0) == 0);
    /* Return to blocking mode before a close with positive SO_LINGER. */
    on = 0; CHECK(socket_ioctl(peer,0x8004667eu,&on,4) == 0);
    CHECK(close_socket(client) == 0 && close_socket(peer) == 0 && close_socket(listener) == 0);
    CHECK(close_socket(client) == -1 && net_error() == 10038);
    puts("TCP loopback, select, nonblocking I/O, linger, shutdown and close PASS");

    address.port = 0; listener = create_socket(2,1,0); client = create_socket(2,1,0);
    CHECK(listener >= 0 && client >= 0 && bind_socket(listener,&address,16) == 0);
    n = sizeof(address); CHECK(socket_name(listener,&address,&n) == 0 && listen_socket(listener,1) == 0);
    CHECK(connect_socket(client,&address,16) == 0 && ready(listener));
    peer = accept_socket(listener,NULL,NULL); CHECK(peer >= 0);
    CHECK(cancel_wait(peer,0) == 0);
    CHECK(close_socket(client) == 0 && cancel_wait(listener,1) == 0);
    for (i = 2; i < 4; ++i) {
        udp = create_socket(2,2,0); address.port = 0;
        CHECK(udp >= 0 && bind_socket(udp,&address,16) == 0);
        CHECK(cancel_wait(udp,i) == 0);
    }
    puts("Cross-thread close cancels recv, accept, recvfrom and infinite select PASS");

    udp = create_socket(2,2,0); sender = create_socket(2,2,0); CHECK(udp >= 0 && sender >= 0);
    address.port = 0; CHECK(bind_socket(udp,&address,sizeof(address)) == 0);
    n = sizeof(address); CHECK(socket_name(udp,&address,&n) == 0);
    CHECK(send_to(sender,payload,8,0,&address,sizeof(address)) == 8 && ready(udp));
    n = sizeof(remote); CHECK(recv_from(udp,buffer,8,0,&remote,&n) == 8 && n == 16 && !memcmp(buffer,payload,8));
    CHECK(close_socket(udp) == 0 && close_socket(sender) == 0);
    CHECK(create_socket(23,1,0) == -1 && net_error() == 10047);
    for (i = 0; i < 32; ++i) { udp = create_socket(2,2,0); CHECK(udp >= 0 && close_socket(udp) == 0); }
    printf("SO32DLL/TCP32DLL native Winsock smoke: %d checks PASS\n",checks);
    return 0;
}
