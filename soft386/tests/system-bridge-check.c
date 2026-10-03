#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "soft386_system_bridge.h"

#ifndef __cdecl
#define __cdecl
#endif

static unsigned char ram[0x10000];
static int valid(void *o,uint32_t a,uint32_t n,int w){(void)o;(void)w;return a<sizeof(ram)&&n<=sizeof(ram)-a;}
static int rd(void *o,uint32_t a,void *d,uint32_t n){(void)o;if(!valid(NULL,a,n,0))return 0;memcpy(d,ram+a,n);return 1;}
static int wr(void *o,uint32_t a,const void *d,uint32_t n){(void)o;if(!valid(NULL,a,n,1))return 0;memcpy(ram+a,d,n);return 1;}
static int cstr(void *o,uint32_t a,char *d,uint32_t cap){uint32_t i;(void)o;if(!a||!cap)return 0;for(i=0;i<cap;i++){if(!valid(NULL,a+i,1,0))return 0;d[i]=(char)ram[a+i];if(!d[i])return 1;}return 0;}
static uint32_t r32(void *o,uint32_t a){(void)o;return (uint32_t)ram[a]|((uint32_t)ram[a+1]<<8)|((uint32_t)ram[a+2]<<16)|((uint32_t)ram[a+3]<<24);}
static void p16(uint32_t a,uint16_t v){ram[a]=(unsigned char)v;ram[a+1]=(unsigned char)(v>>8);}
static void p32(uint32_t a,uint32_t v){ram[a]=(unsigned char)v;ram[a+1]=(unsigned char)(v>>8);ram[a+2]=(unsigned char)(v>>16);ram[a+3]=(unsigned char)(v>>24);}

static int vio_tty_calls,kbd_calls,ses_calls;
#pragma pack(push,2)
struct TestStartData {
    uint16_t Length,Related,FgBg,TraceOpt;
    char *PgmTitle,*PgmName; unsigned char *PgmInputs,*TermQ,*Environment;
    uint16_t InheritOpt,SessionType; char *IconFile; uint32_t PgmHandle;
    uint16_t PgmControl,InitXPos,InitYPos,InitXSize,InitYSize,Reserved;
    char *ObjectBuffer; uint32_t ObjectBuffLen;
};
#pragma pack(pop)
static uint16_t __cdecl f_vio_tty(const char *s,uint16_t n,uint16_t h){if(h||n!=5||memcmp(s,"hello",5))return 87;vio_tty_calls++;return 0;}
static uint16_t __cdecl f_vio_pos(uint16_t *r,uint16_t *c,uint16_t h){if(h)return 6;*r=7;*c=9;return 0;}
static uint16_t __cdecl f_kbd_char(void *p,uint16_t wait,uint16_t h){unsigned char *b=(unsigned char*)p;if(h||wait!=1)return 87;memset(b,0,10);b[0]='X';b[1]=0x2d;b[2]=0x40;kbd_calls++;return 0;}
static uint16_t __cdecl f_kbd_flush(uint16_t h){if(h)return 6;kbd_calls++;return 0;}
static uint32_t __cdecl f_ses_title(uint32_t sid,char *title){if(sid!=12||strcmp(title,"Jar title"))return 87;ses_calls++;return 0;}
static uint32_t __cdecl f_ses_query(void *entries,uint32_t cap,uint32_t *count){unsigned char *p=(unsigned char*)entries;if(cap<1)return 87;memset(p,0,528);p[0]=0x34;p[1]=0x12;memcpy(p+8,"child.exe",10);*count=1;ses_calls++;return 0;}
static uint32_t __cdecl f_ses_start(struct TestStartData *sd,uint32_t *sid,uint32_t *pid){
    if(!sd||sd->Length!=60||!sd->PgmTitle||strcmp(sd->PgmTitle,"Jar child")||!sd->PgmName||strcmp(sd->PgmName,"child.exe"))return 87;
    if(!sd->PgmInputs||strcmp((char*)sd->PgmInputs,"-x")||!sd->Environment||strcmp((char*)sd->Environment,"A=B")||sd->Environment[4]!=0)return 87;
    if(!sd->ObjectBuffer||sd->ObjectBuffLen!=16)return 87;
    memset(sd->ObjectBuffer,0,16);
    memcpy(sd->ObjectBuffer,"OBJ",4);
    *sid=0x55;*pid=0x66;ses_calls++;return 0;
}

static void *vio_get(void *o,uint32_t ord){(void)o;if(ord==19)return (void*)(uintptr_t)f_vio_tty;if(ord==9)return (void*)(uintptr_t)f_vio_pos;return NULL;}
static void *kbd_get(void *o,uint32_t ord){(void)o;if(ord==4)return (void*)(uintptr_t)f_kbd_char;if(ord==13)return (void*)(uintptr_t)f_kbd_flush;return NULL;}
static void *ses_get(void *o,uint32_t ord){(void)o;if(ord==5)return (void*)(uintptr_t)f_ses_title;if(ord==17)return (void*)(uintptr_t)f_ses_start;if(ord==1000)return (void*)(uintptr_t)f_ses_query;return NULL;}

int main(void)
{
    struct Soft386SystemBridge b;struct Soft386GuestMemoryOps m;uint32_t rc,esp=0x1000;int handled,id;
    memset(&b,0,sizeof(b));memset(&m,0,sizeof(m));m.valid=valid;m.read=rd;m.write=wr;m.read_cstr=cstr;m.read_u32=r32;
    soft386_system_bridge_set_provider(&b,"VIOCALLS",NULL,vio_get,NULL,0);
    soft386_system_bridge_set_provider(&b,"KBDCALLS",NULL,kbd_get,NULL,0);
    soft386_system_bridge_set_provider(&b,"SESMGR",NULL,ses_get,NULL,0);

    /* Flat VIO GetCurPos(row,col,hvio). */
    p32(esp+4,0x2000);p32(esp+8,0x2002);p32(esp+12,0);
    rc=soft386_system_bridge_dispatch32(&b,&m,"VIOCALLS",9,esp,&handled);
    if(rc||!handled||ram[0x2000]!=7||ram[0x2002]!=9)return 1;

    /* Packed C/386 VioWrtTTY: hvio WORD, count WORD, text DWORD. */
    memcpy(ram+0x2100,"hello",5);memset(ram+esp,0,32);p16(esp+4,0);p16(esp+6,5);p32(esp+8,0x2100);
    id=soft386_c386_desc_index("VIOCALLS",19);
    if(id<0)return 2;
    rc=soft386_system_bridge_dispatch_c386(&b,&m,(uint32_t)id+1,esp,&handled);
    if(rc||!handled||vio_tty_calls!=1)return 3;

    /* Flat KbdCharIn(info,IO_NOWAIT,hkbd). */
    memset(ram+esp,0,32);p32(esp+4,0x2200);p32(esp+8,1);p32(esp+12,0);
    rc=soft386_system_bridge_dispatch32(&b,&m,"KBDCALLS",4,esp,&handled);
    if(rc||ram[0x2200]!='X'||ram[0x2202]!=0x40||kbd_calls!=1)return 4;

    /* SESMGR title string and query copy-back. */
    strcpy((char*)ram+0x2300,"Jar title");memset(ram+esp,0,32);p32(esp+4,12);p32(esp+8,0x2300);
    rc=soft386_system_bridge_dispatch32(&b,&m,"SESMGR",5,esp,&handled);if(rc||ses_calls!=1)return 5;
    memset(ram+esp,0,32);p32(esp+4,0x2400);p32(esp+8,1);p32(esp+12,0x2600);
    rc=soft386_system_bridge_dispatch32(&b,&m,"SESMGR",1000,esp,&handled);
    if(rc||r32(NULL,0x2600)!=1||r32(NULL,0x2400)!=0x1234||memcmp(ram+0x2408,"child.exe",9)||ses_calls!=2)return 6;

    /* SESMGR.17 deep STARTDATA marshalling and copy-back. */
    memset(ram+0x2800,0,60);p16(0x2800,60);p32(0x2808,0x2900);p32(0x280c,0x2920);p32(0x2810,0x2940);p32(0x2818,0x2960);p32(0x2834,0x2980);p32(0x2838,16);
    strcpy((char*)ram+0x2900,"Jar child");strcpy((char*)ram+0x2920,"child.exe");strcpy((char*)ram+0x2940,"-x");memcpy(ram+0x2960,"A=B\0\0",5);memset(ram+0x2980,0xcc,16);
    memset(ram+esp,0,32);p32(esp+4,0x2800);p32(esp+8,0x2a00);p32(esp+12,0x2a04);
    rc=soft386_system_bridge_dispatch32(&b,&m,"SESMGR",17,esp,&handled);
    if(rc||r32(NULL,0x2a00)!=0x55||r32(NULL,0x2a04)!=0x66||memcmp(ram+0x2980,"OBJ",4)||ses_calls!=3)return 7;

    /* Historical DOSCALLS.32 must be recognized by the migration-helper
     * catalogue even though its execution stays in the jar scheduler. */
    id=soft386_c386_desc_index("DOSCALLS",32);
    if(id<0)return 8;
    {
        const struct Soft386C386ApiDesc *d=soft386_c386_desc_by_id((uint32_t)id+1u);
        if(!d||strcmp(d->name,"DosSleep")||d->count!=1||d->width[0]!=4)return 9;
    }
    if(soft386_c386_desc_count()<19)return 10;
    puts("Soft386 system DLL marshalling: PASS");
    return 0;
}
