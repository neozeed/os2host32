#include <ctype.h>
#define MAX_PATH 260
#define FALSE 0
#define DUPLICATE_SAME_ACCESS 2
#define MOD_TYPE_DLL 0x8000u
static HANDLE std_mock[3];
static HANDLE GetCurrentProcess(void){return NULL;}
static int SetStdHandle(DWORD which,HANDLE h){assert(which<3);std_mock[which]=h;return 1;}
static int DuplicateHandle(HANDLE p,HANDLE h,HANDLE q,HANDLE *out,DWORD access,int inherit,DWORD flags){int fd;(void)p;(void)q;(void)access;(void)inherit;(void)flags;fd=dup(fdof(h));if(fd<0){maperr();return 0;}*out=(HANDLE)(intptr_t)(fd+1);return 1;}
static int CreatePipe(HANDLE *a,HANDLE *b,void *sa,DWORD size){int fds[2];(void)sa;(void)size;if(pipe(fds)){maperr();return 0;}*a=(HANDLE)(intptr_t)(fds[0]+1);*b=(HANDLE)(intptr_t)(fds[1]+1);return 1;}
static int SetCurrentDirectoryA(const char *p){if(chdir(p)){maperr();return 0;}return 1;}
static int RemoveDirectoryA(const char *p){if(rmdir(p)){maperr();return 0;}return 1;}
static int CreateDirectoryA(const char *p,void *sa){(void)sa;if(mkdir(p,0700)){maperr();return 0;}return 1;}
static int MoveFileA(const char *a,const char *b){struct stat st;if(!stat(b,&st)){last=183;return 0;}if(rename(a,b)){maperr();return 0;}return 1;}
/* Fixed Windows-shaped cwd for marshaling; filesystem operations above use POSIX. */
static DWORD GetCurrentDirectoryA(DWORD cb,char *p){const char *cwd="C:\\TEST";if(cb>strlen(cwd))strcpy(p,cwd);return (DWORD)strlen(cwd);}
static DWORD GetLogicalDrives(void){return 4;}
static uint32_t rd32(const uint8_t *p){uint32_t v;memcpy(&v,p,4);return v;}
static uint8_t *load_file(const char *p,uint32_t *size){FILE *f=fopen(p,"rb");uint8_t *b;long n;if(!f)return NULL;fseek(f,0,SEEK_END);n=ftell(f);rewind(f);b=malloc((size_t)n);assert(b);assert(fread(b,1,(size_t)n,f)==(size_t)n);fclose(f);*size=(uint32_t)n;return b;}
#include "../v2_cmdfs.h"
static uint32_t fcall(struct Runtime *r,uint32_t ord,uint32_t a,uint32_t b,uint32_t c){guest_put_u32(r,104,a);guest_put_u32(r,108,b);guest_put_u32(r,112,c);return dispatch_cmdfs(r,ord,100);}
static void test_cmdfs(void){
 struct Runtime r;uint32_t a,b,c;char tmp[]="/tmp/whp-cmdfs-XXXXXX",cwd[4096];
 memset(&r,0,sizeof(r));r.ram=calloc(1,RAM_SIZE);assert(r.ram);assert(getcwd(cwd,sizeof(cwd)));assert(mkdtemp(tmp));assert(chdir(tmp)==0);
 strcpy((char*)r.ram+1000,"sub");strcpy((char*)r.ram+1100,"new");
 assert(fcall(&r,270,1000,0,0)==0);assert(fcall(&r,271,1000,1100,0)==0);
 assert(fcall(&r,255,1100,0,0)==0);assert(chdir("..")==0);assert(fcall(&r,226,1100,0,0)==0);
 assert(fcall(&r,275,2000,2004,0)==0&&guest_u32(&r,2000)==3&&guest_u32(&r,2004)==4);
 guest_put_u32(&r,2000,2);assert(fcall(&r,274,0,2100,2000)==111&&guest_u32(&r,2000)==5);
 assert(fcall(&r,274,0,2100,2000)==0&&!strcmp((char*)r.ram+2100,"TEST"));
 assert(fcall(&r,239,2000,2004,0)==0);a=guest_u32(&r,2000);b=guest_u32(&r,2004);
 guest_put_u32(&r,2008,0xffffffffu);assert(fcall(&r,260,b,2008,0)==0);c=guest_u32(&r,2008);assert(c!=b);
 assert(CloseHandle(r.files[b]));r.files[b]=NULL;assert(write(fdof(r.files[c]),"OK",2)==2);
 assert(read(fdof(r.files[a]),r.ram+2200,2)==2&&!memcmp(r.ram+2200,"OK",2));
 assert(CloseHandle(r.files[a])&&CloseHandle(r.files[c]));r.files[a]=r.files[c]=NULL;
 assert(fcall(&r,260,127,2008,0)==6);assert(fcall(&r,239,2000,2000,0)==87);
 assert(fcall(&r,280,0,0,0)==1&&fcall(&r,283,0,0,0)==1);
 assert(chdir(cwd)==0&&rmdir(tmp)==0);free(r.ram);
 puts("cmdfs-check PASS: cwd marshaling, mkdir/move/remove, real pipe/duplicate lifetime and explicit process boundaries");
}
