/* Deterministic 80x24 ANSI/CP437 display tests; no OS/2 DLL dependency.
 * C89, Winsock 2 or POSIX. See TELNET-DISPLAY-TESTS.md for expected screens.
 * Telnet options: RFC 854 (protocol), 856 (binary), 857 (echo), 858 (SGA).
 * Single client at a time, no shell, authentication, or filesystem commands. */
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#include <io.h>
#include <fcntl.h>
typedef SOCKET Socket;
#define CLOSE closesocket
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
typedef int Socket;
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
#define CLOSE close
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define IAC 255
#define DONT 254
#define DO 253
#define WONT 252
#define WILL 251
#define SB 250
#define SE 240
#define BINARY 0
#define ECHO 1
#define SGA 3
#define BUFFER_SIZE 16384
struct Screen { unsigned char bytes[BUFFER_SIZE]; size_t used; };
struct Session {
    Socket socket;
    int state,command,page,failed,quit;
    unsigned char us[256],them[256]; /* 0=no, 1=offer pending, 2=yes */
};
static void append(struct Screen *s,const char *text)
{
    size_t n=strlen(text);
    if(n>sizeof(s->bytes)-s->used) { fputs("Screen buffer overflow\n",stderr);exit(1); }
    memcpy(s->bytes+s->used,text,n);s->used+=n;
}
static void at(struct Screen *s,int row,int column,const char *text)
{
    char cursor[32];sprintf(cursor,"\033[%d;%dH",row,column);append(s,cursor);append(s,text);
}
static void numbered(char *line,int row)
{
    int i; char label[40];
    for(i=0;i<79;++i) line[i]=(char)('0'+(i+1)%10);
    line[79]=0;
    sprintf(label,"ROW %02d %s | ",row,row==1?"TOP":row==24?"BOTTOM":"       ");
    memcpy(line,label,strlen(label));line[78]='#';
}
static void screen(struct Screen *s,int page)
{
    int row,column; char line[128],piece[80];
    static const unsigned char blocks[]={0xdb,0xdf,0xdc,0xb0,0xb1,0xb2,0xc4,0xb3};
    s->used=0;
    /* Explicit full scroll region, origin, and autowrap mode on every page.
     * Column 80 stays blank: never depend on delayed wrap at bottom-right. */
    append(s,"\033[0m\033[?6l\033[r\033[?7h\033[2J\033[H");
    if(page==1) {
        for(row=1;row<=24;++row) { numbered(line,row);at(s,row,1,line); }
        at(s,3,16,"PM DISPLAY R1 / PAGE 1 - every ROW 01..24 must be visible");
        at(s,5,16,"Lost rows: geometry/scrolling; clipped glyphs: inspect metrics.");
        at(s,23,16,"1 rows 2 pixels 3 colors 4 erase 5 scroll R redraw Q quit");
    } else if(page==2) {
        for(column=0;column<79;++column) line[column]=(char)blocks[column/10];
        line[79]=0;at(s,1,1,line);at(s,12,1,line);at(s,24,1,line);
        at(s,3,1,"PM DISPLAY R1 / PAGE 2 - CP437 top/middle/bottom pixel comparison");
        at(s,5,1,"Identical strips on rows 01, 12, 24. Each group is ten cells (last nine).");
        at(s,6,1,"Hex bytes: DB full | DF upper | DC lower | B0/B1/B2 shade | C4 line | B3 line");
        at(s,7,1,"Last two groups: C4 horizontal line, B3 vertical line. Select code page 437.");
        at(s,9,1,"HHHH MMMM WWWW XXXX yyyy gggg pppp qqqq ____ ---- | baseline sample");
        at(s,14,1,"HHHH MMMM WWWW XXXX yyyy gggg pppp qqqq ____ ---- | baseline sample");
        at(s,16,1,"Compare strip height at top and middle. DF/DC distinguish upper/lower loss.");
        at(s,18,1,"Wrong letters instead of blocks indicate encoding/font, not clipping.");
        at(s,22,1,"Keys 1..5 select page; R redraws exactly; Q disconnects.");
    } else if(page==3) {
        at(s,1,1,"PM DISPLAY R1 / PAGE 3 - SGR colors and attributes");
        for(row=0;row<8;++row) {
            sprintf(piece,"\033[0;%dm FG %d: HHHH MMMM yyyy gggg \033[0m",30+row,row);
            at(s,row+3,1,piece);
            sprintf(piece,"\033[0;37;%dm BG %d:              \033[0m",40+row,row);
            at(s,row+3,40,piece);
        }
        at(s,13,1,"Normal      HHHH MMMM yyyy gggg");
        at(s,14,1,"\033[1mBold        HHHH MMMM yyyy gggg\033[0m");
        at(s,15,1,"\033[4mUnderline   HHHH MMMM yyyy gggg\033[0m");
        at(s,16,1,"\033[7mReverse     HHHH MMMM yyyy gggg\033[0m");
        at(s,18,1,"Bold color intensity and underline style can vary with client settings.");
        at(s,24,1,"ROW 24 BOTTOM | Keys 1..5, R redraw, Q quit");
    } else if(page==4) {
        at(s,1,1,"PM DISPLAY R1 / PAGE 4 - overwrite, spaces, EL and ED");
        at(s,4,1,"SPACES: [XXXXXXXXXXXXXXXXXXXX]");at(s,4,10,"                    ");
        at(s,6,1,"EL0: KEEP/ERASE-ME");at(s,6,10,"\033[K");
        at(s,8,1,"EL2: ERASE THE ENTIRE LINE");at(s,8,10,"\033[2K");
        at(s,10,1,"OVERWRITE: abcdefghij");at(s,10,12,"0123456789");
        at(s,12,1,"EL1: ERASE-ME KEEP");at(s,12,13,"\033[1K");
        at(s,14,1,"ED0: KEEP/ERASE-ME");at(s,15,1,"ERASE THIS ROW TOO");
        at(s,14,10,"\033[J");
        at(s,18,1,"Expected: empty brackets row4; KEEP rows6,14; blank rows8,15;");
        at(s,19,1,"0123456789 row10; KEEP starts in column15 of row12.");
        at(s,24,1,"ROW 24 BOTTOM | Keys 1..5, R redraw, Q quit");
    } else {
        /* Write 30 lines, CRLF between lines but NOT after line 30.
         * On a 24-line terminal the final screen must be lines 07..30. */
        for(row=1;row<=30;++row) {
            if(row>1) append(s,"\r\n");
            sprintf(line,"SCROLL %02d | expected final row %02d | HHHH yyyy ____",
                    row,row-6>0?row-6:0);append(s,line);
        }
    }
    append(s,"\033[0m\033[23;79H");
}
static int send_all(Socket socket,const unsigned char *data,size_t length)
{
    int sent;
    while(length) {
        sent=send(socket,(const char *)data,(int)length,0);
        if(sent==SOCKET_ERROR) {
#ifndef _WIN32
            if(errno==EINTR) continue;
#endif
            return 0;
        }
        if(!sent) return 0;
        data+=sent;length-=(size_t)sent;
    }
    return 1;
}
static void option(struct Session *s,int command,int opt)
{
    unsigned char bytes[3];bytes[0]=IAC;bytes[1]=(unsigned char)command;bytes[2]=(unsigned char)opt;
    if(!send_all(s->socket,bytes,3)) s->failed=1;
}
static void display(struct Session *s)
{
    struct Screen output; unsigned char escaped[BUFFER_SIZE*2];size_t i,n=0;
    screen(&output,s->page);
    if(s->page==2 && s->us[BINARY]!=2) {
        output.used=0;append(&output,"\033[0m\033[2J\033[H");
        append(&output,"Page 2 needs Telnet BINARY output (8-bit CP437).\r\n");
        append(&output,"Enable binary negotiation in the client, then press R.\r\n");
        append(&output,"ASCII pages 1, 3, 4, 5 remain available.");
    }
    for(i=0;i<output.used;++i) {
        escaped[n++]=output.bytes[i];if(output.bytes[i]==IAC) escaped[n++]=IAC;
    }
    if(!send_all(s->socket,escaped,n)) s->failed=1;
    printf("Sent page %d (%lu display bytes)\n",s->page,(unsigned long)output.used);fflush(stdout);
}
static void negotiate(struct Session *s,int command,int opt)
{
    int allowed;
    if(command==DO) {
        allowed=opt==BINARY || opt==ECHO || opt==SGA;
        if(allowed) { if(!s->us[opt]) option(s,WILL,opt);s->us[opt]=2; }
        else option(s,WONT,opt);
    } else if(command==DONT) {
        if(s->us[opt]==2) option(s,WONT,opt);
        s->us[opt]=0;
    } else if(command==WILL) {
        allowed=opt==BINARY || opt==SGA;
        if(allowed) { if(!s->them[opt]) option(s,DO,opt);s->them[opt]=2; }
        else option(s,DONT,opt);
    } else { /* WONT */
        if(s->them[opt]==2) option(s,DONT,opt);
        s->them[opt]=0;
    }
}
static void input(struct Session *s,unsigned char ch)
{
    /* Constant memory even for arbitrarily long/fragmented subnegotiations. */
    if(s->state==1) {
        if(ch>=WILL && ch<=DONT) { s->command=ch;s->state=2; }
        else if(ch==SB) s->state=3;
        else s->state=0; /* literal IAC or other command: no page action */
    } else if(s->state==2) { negotiate(s,s->command,ch);s->state=0; }
    else if(s->state==3) { if(ch==IAC) s->state=4; }
    else if(s->state==4) { s->state=ch==SE?0:3; }
    else if(ch==IAC) s->state=1;
    else if(ch>='1' && ch<='5') { s->page=ch-'0';display(s); }
    else if(ch=='r' || ch=='R') display(s);
    else if(ch=='q' || ch=='Q') s->quit=1;
    /* CR LF, CR NUL and other input do not echo or move the display. */
}
static void session(Socket client)
{
    struct Session s; unsigned char buffer[1024];int n,i;
    memset(&s,0,sizeof(s));s.socket=client;s.page=1;
    s.us[ECHO]=s.us[SGA]=s.us[BINARY]=1;s.them[SGA]=s.them[BINARY]=1;
    option(&s,WILL,ECHO);option(&s,WILL,SGA);option(&s,DO,SGA);
    option(&s,WILL,BINARY);option(&s,DO,BINARY);display(&s);
    while(!s.failed && !s.quit) {
        n=recv(client,(char *)buffer,sizeof(buffer),0);
        if(n==SOCKET_ERROR) {
#ifndef _WIN32
            if(errno==EINTR) continue;
#endif
            break;
        }
        if(n<=0) break;
        for(i=0;i<n && !s.failed && !s.quit;++i) input(&s,buffer[i]);
    }
    CLOSE(client);puts("Disconnected; waiting for another client.");fflush(stdout);
}
int main(int argc,char **argv)
{
    const char *bind_ip="127.0.0.1";long port=2323;int i,once=0;
    Socket listener,client;struct sockaddr_in address;
    if(argc==3 && !strcmp(argv[1],"--dump")) {
        struct Screen output;
        if(strlen(argv[2])!=1 || argv[2][0]<'1' || argv[2][0]>'5') return 2;
#ifdef _WIN32
        /* Binary stdout prevents CRT expansion of existing CRLF sequences. */
        if(_setmode(_fileno(stdout),_O_BINARY)==-1) return 1;
#endif
        screen(&output,argv[2][0]-'0');
        return fwrite(output.bytes,1,output.used,stdout)==output.used?0:1;
    }
    for(i=1;i<argc;++i) {
        if(!strcmp(argv[i],"--bind") && i+1<argc) bind_ip=argv[++i];
        else if(!strcmp(argv[i],"--port") && i+1<argc) {
            char *end;port=strtol(argv[++i],&end,10);if(*end || port<1 || port>65535) goto usage;
        } else if(!strcmp(argv[i],"--once")) once=1;
        else goto usage;
    }
#ifdef _WIN32
    { WSADATA data;if(WSAStartup(MAKEWORD(2,2),&data)) { fputs("WSAStartup failed\n",stderr);return 1; } }
#else
    signal(SIGPIPE,SIG_IGN);
#endif
    memset(&address,0,sizeof(address));address.sin_family=AF_INET;
    address.sin_port=htons((unsigned short)port);address.sin_addr.s_addr=inet_addr(bind_ip);
    if(address.sin_addr.s_addr==INADDR_NONE) { fputs("Use a numeric IPv4 bind address\n",stderr);return 2; }
    listener=socket(AF_INET,SOCK_STREAM,0);
    if(listener==INVALID_SOCKET) { fputs("socket failed\n",stderr);return 1; }
#ifndef _WIN32
    { int reuse=1;setsockopt(listener,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse)); }
#endif
    if(bind(listener,(struct sockaddr *)&address,sizeof(address))==SOCKET_ERROR || listen(listener,4)==SOCKET_ERROR) {
        fputs("bind/listen failed: check address and whether the port is already in use\n",stderr);CLOSE(listener);return 1;
    }
    printf("PM DISPLAY R1 listening on %s:%ld; fixed 80x24 ANSI / CP437\n",bind_ip,port);
    puts("Keys: 1 rows, 2 pixels, 3 colors, 4 erase, 5 scroll, R redraw, Q quit.");
    puts("Ctrl+C stops the server. One client at a time.");fflush(stdout);
    do {
        client=accept(listener,NULL,NULL);
        if(client==INVALID_SOCKET) {
#ifndef _WIN32
            if(errno==EINTR) continue;
#endif
            break;
        }
        session(client);
    } while(!once);
    CLOSE(listener);
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
usage:
    fputs("Usage: telnet-display-server [--bind IPv4] [--port 2323] [--once]\n"
          "       telnet-display-server --dump 1..5 > page.ans\n",stderr);return 2;
}
