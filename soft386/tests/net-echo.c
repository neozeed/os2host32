/* Local-only, one-connection Windows smoke peer; no Python required. */
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
typedef SOCKET sock_t;
#define CLOSE closesocket
#else
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
typedef int sock_t;
#define CLOSE close
#define INVALID_SOCKET (-1)
#endif
#include <stdio.h>
#include <string.h>
int main(void){sock_t listener,peer;struct sockaddr_in a;fd_set ready;struct timeval timeout;char data[8];int got=0,n,sent=0;
#ifdef _WIN32
 WSADATA w;if(WSAStartup(MAKEWORD(2,2),&w))return 1;
#endif
 listener=socket(AF_INET,SOCK_STREAM,0);if(listener==INVALID_SOCKET)return 2;
 memset(&a,0,sizeof(a));a.sin_family=AF_INET;a.sin_addr.s_addr=htonl(INADDR_LOOPBACK);a.sin_port=htons(23230);
 if(bind(listener,(struct sockaddr *)&a,sizeof(a))||listen(listener,1)){puts("Cannot listen on 127.0.0.1:23230; close another smoke peer and retry.");CLOSE(listener);return 3;}
 puts("R5C echo peer: waiting on 127.0.0.1:23230 (30 second limit)");fflush(stdout);
 FD_ZERO(&ready);FD_SET(listener,&ready);timeout.tv_sec=30;timeout.tv_usec=0;
 if(select((int)listener+1,&ready,NULL,NULL,&timeout)<=0){CLOSE(listener);return 4;}
 peer=accept(listener,NULL,NULL);CLOSE(listener);if(peer==INVALID_SOCKET)return 5;
 while(got<8){FD_ZERO(&ready);FD_SET(peer,&ready);timeout.tv_sec=30;timeout.tv_usec=0;
  if(select((int)peer+1,&ready,NULL,NULL,&timeout)<=0){CLOSE(peer);return 6;}
  n=recv(peer,data+got,8-got,0);if(n<=0){CLOSE(peer);return 7;}got+=n;}
 if(memcmp(data,"jar\0data",8)){CLOSE(peer);return 8;}
 while(sent<8){n=send(peer,data+sent,8-sent,0);if(n<=0){CLOSE(peer);return 9;}sent+=n;}
 CLOSE(peer);puts("R5C echo peer PASS: eight binary bytes echoed");
#ifdef _WIN32
 WSACleanup();
#endif
 return 0;
}
