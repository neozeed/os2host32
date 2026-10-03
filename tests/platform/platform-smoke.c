/* Native Windows integration checks. Loads the real DLLs by OS/2 ordinal. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#define FN(name,args) typedef DWORD (__cdecl *name##Fn) args;static name##Fn name
FN(open_file,(const char *,DWORD *,DWORD *,DWORD,DWORD,DWORD,DWORD,void *,DWORD));
FN(close_file,(DWORD));FN(write_file,(DWORD,const void *,DWORD,DWORD *));
FN(copy_file,(const char *,const char *,DWORD));FN(delete_file,(const char *));
FN(reset,(DWORD));FN(set_max,(DWORD));FN(query_mode,(DWORD,DWORD *));
FN(set_mode,(DWORD,DWORD));FN(duplicate,(DWORD,DWORD *));
FN(fs_info,(DWORD,DWORD,void *,DWORD));FN(frequency,(DWORD *));FN(counter,(uint32_t *));
FN(resource_size,(DWORD,DWORD,DWORD,DWORD *));
FN(insert_message,(const char *const *,DWORD,const char *,DWORD,char *,DWORD,DWORD *));
FN(get_message,(const void *,const char *const *,DWORD,char *,DWORD,DWORD,const char *,DWORD *));
typedef int (__cdecl *RegisterFn)(DWORD,WORD,WORD,const void *,DWORD);
static RegisterFn register_resource;
#define CHECK(c) do { ++checks;if(!(c)) { printf("FAIL line %d: %s Win32=%lu\n",__LINE__,#c,(unsigned long)GetLastError());return 1; } } while(0)
#define BIND(m,n,o) do { FARPROC p=GetProcAddress(m,(LPCSTR)(uintptr_t)o);CHECK(p!=NULL);memcpy(&n,&p,sizeof(n)); } while(0)
int main(void)
{
    HMODULE dos,msg,pm;DWORD h,action,actual,mode,h2,freq,n;FARPROC proc;
    char temp[MAX_PATH],dir[MAX_PATH],a[MAX_PATH],b[MAX_PATH],path[MAX_PATH],out[80],cwd[MAX_PATH];
    unsigned char info[24],file[49],segment[22];uint32_t t0[2],t1[2];
    const char *table[]={"world"};HANDLE native;unsigned checks=0;size_t len;
    dos=LoadLibraryA("DOSCALLS.dll");msg=LoadLibraryA("MSG.dll");pm=LoadLibraryA("PMWIN.dll");CHECK(dos && msg && pm);
    BIND(dos,open_file,273);BIND(dos,close_file,257);BIND(dos,write_file,282);
    BIND(dos,copy_file,258);BIND(dos,delete_file,110);BIND(dos,reset,254);
    BIND(dos,set_max,209);BIND(dos,query_mode,276);BIND(dos,set_mode,221);BIND(dos,duplicate,260);
    BIND(dos,fs_info,278);BIND(dos,frequency,362);BIND(dos,counter,363);BIND(dos,resource_size,572);
    BIND(msg,insert_message,4);BIND(msg,get_message,6);
    CHECK(GetTempPathA(sizeof(temp),temp)>0);CHECK(GetTempFileNameA(temp,"r11",0,dir)!=0);
    CHECK(DeleteFileA(dir) && CreateDirectoryA(dir,NULL));
    len=strlen(dir);CHECK(len+12<sizeof(a));strcpy(a,dir);strcat(a,"\\a.bin");strcpy(b,dir);strcat(b,"\\b.bin");
    CHECK(set_max(64)==0);CHECK(set_max(20)==87);CHECK(set_max(257)==8);
    CHECK(open_file(a,&h,&action,0,0,0x12,0x40c2,NULL,0)==0);
    CHECK(query_mode(h,&mode)==0 && mode==0x40c2);
    CHECK(write_file(h,"R11",3,&actual)==0 && actual==3);CHECK(reset(h)==0);
    h2=h;CHECK(duplicate(h,&h2)==0 && h2==h);CHECK(query_mode(h,&mode)==0 && mode==0x40c2);
    CHECK(set_mode(h,0x4000)==0);CHECK(query_mode(h,&mode)==0 && mode==0x4042);
    CHECK(set_mode(h,0x4080)==0);CHECK(set_mode(h,0)==50);
    h2=0xffffffffUL;CHECK(duplicate(h,&h2)==0 && h2!=h);
    CHECK(query_mode(h2,&mode)==0 && mode==0x4042);CHECK(close_file(h2)==0);
    CHECK(reset(0xffffffffUL)==0);CHECK(close_file(h)==0);CHECK(query_mode(h,&mode)==6);
    CHECK(copy_file(a,b,0)==0);CHECK(copy_file(a,b,0)!=0);CHECK(copy_file(a,b,1)==0);CHECK(copy_file(a,b,2)==50);
    native=CreateFileA(b,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);CHECK(native!=INVALID_HANDLE_VALUE);
    CHECK(ReadFile(native,out,sizeof(out),&actual,NULL) && actual==3 && !memcmp(out,"R11",3));CloseHandle(native);
    CHECK(SetFileAttributesA(b,FILE_ATTRIBUTE_READONLY));CHECK(delete_file(b)==5);
    CHECK(SetFileAttributesA(b,FILE_ATTRIBUTE_NORMAL));CHECK(delete_file(b)==0);CHECK(delete_file(a)==0);
    CHECK(GetCurrentDirectoryA(sizeof(cwd),cwd)>0);CHECK(cwd[1]==':');n=(DWORD)((cwd[0]|32)-'a'+1);
    memset(info,0xcc,sizeof(info));CHECK(fs_info(n,1,info,17)==111 && info[0]==0xcc);
    CHECK(fs_info(n,1,info,18)==0 && info[18]==0xcc && (info[16] || info[17]));
    CHECK(fs_info(n,2,info,17)==0 && info[4]<=11);CHECK(fs_info(n,99,info,sizeof(info))==124);
    CHECK(frequency(&freq)==0 && freq>0);CHECK(counter(t0)==0);Sleep(5);CHECK(counter(t1)==0);
    CHECK(t1[1]>t0[1] || (t1[1]==t0[1] && t1[0]>t0[0]));
    proc=GetProcAddress(pm,"OS2PM_RegisterResource");CHECK(proc!=NULL);memcpy(&register_resource,&proc,sizeof(register_resource));
    CHECK(register_resource(0,10,420,"abc",3));CHECK(resource_size(0,10,420,&n)==0 && n==3);
    CHECK(resource_size(0,10,421,&n)==2 && n==0);
    puts("DOSCALLS ordinal, file state/copy/flush/delete, disk, timer and resource checks PASS");
    memset(file,0,sizeof(file));memcpy(file,"\xffMKMSGF\0TST",11);
    file[11]=1;file[13]=42;file[15]=1;file[16]=2;file[18]=31;file[31]=33;
    memcpy(file+33,"EHello %1!\r\n",12);
    strcpy(path,dir);strcat(path,"\\r11.msg");
    native=CreateFileA(path,GENERIC_WRITE,0,NULL,CREATE_NEW,0,NULL);CHECK(native!=INVALID_HANDLE_VALUE);
    CHECK(WriteFile(native,file,45,&actual,NULL) && actual==45);CloseHandle(native);
    memset(segment,0,sizeof(segment));memcpy(segment,"\xffMSGSEG32\0",10);segment[14]=18;segment[20]=segment[21]=255;
    CHECK(get_message(segment,table,1,out,sizeof(out),42,path,&actual)==0);
    CHECK(actual==23 && !memcmp(out,"TST0042: Hello world!\r\n",23));
    CHECK(get_message(NULL,table,1,out,3,42,path,&actual)==316 && actual==3 && !memcmp(out,"TST",3));
    CHECK(get_message(NULL,NULL,0,out,sizeof(out),43,path,&actual)==317 && actual==0);
    CHECK(SetEnvironmentVariableA("DPATH",dir));
    CHECK(get_message(NULL,table,1,out,sizeof(out),42,"r11.msg",&actual)==0);
    CHECK(insert_message(table,1,"%1 %2",5,out,sizeof(out),&actual)==0 && actual==8 && !memcmp(out,"world %2",8));
    segment[18]=1;CHECK(get_message(segment,table,1,out,sizeof(out),42,path,&actual)==50);
    CHECK(DeleteFileA(path) && RemoveDirectoryA(dir));
    puts("MSG ordinal, bound empty-table fallback, DPATH, prefix, insertion and truncation checks PASS");
    printf("R11 native platform: %u checks PASS\n",checks);return 0;
}
