#define MAX_PATH 260
#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)
#define INVALID_FILE_ATTRIBUTES 0xffffffffu
#define DUPLICATE_SAME_ACCESS 2
#define FILE_MAP_ALL_ACCESS 0xf001fu
#define PAGE_READWRITE 4
#define PROC_THREAD_ATTRIBUTE_HANDLE_LIST 1
#define STARTF_USESTDHANDLES 0x100
#define EXTENDED_STARTUPINFO_PRESENT 0x80000
#define WAIT_OBJECT_0 0
#define WAIT_TIMEOUT 258
#define STD_ERROR_HANDLE 2
#define _fileno(f) 2
#define _get_osfhandle(f) ((intptr_t)100)
typedef size_t SIZE_T;
typedef struct {DWORD nLength;void *lpSecurityDescriptor;int bInheritHandle;} SECURITY_ATTRIBUTES;
typedef struct {DWORD cb,dwFlags;HANDLE hStdInput,hStdOutput,hStdError;} STARTUPINFOA;
typedef void *LPPROC_THREAD_ATTRIBUTE_LIST;
typedef struct {STARTUPINFOA StartupInfo;LPPROC_THREAD_ATTRIBUTE_LIST lpAttributeList;} STARTUPINFOEXA;
typedef struct {HANDLE hProcess,hThread;DWORD dwProcessId,dwThreadId;} PROCESS_INFORMATION;
struct MockProcess {int done;DWORD exitcode;};
static struct MockProcess mock_processes[64];
static uint32_t mock_process_count,mock_inherit_count;
static int mock_create_failure;
static HANDLE GetCurrentProcess(void){return (HANDLE)(uintptr_t)99;}
static DWORD GetCurrentProcessId(void){return 77;}
static DWORD GetFileAttributesA(const char *p){(void)p;return INVALID_FILE_ATTRIBUTES;}
static DWORD GetFullPathNameA(const char *p,DWORD cap,char *out,void *last){size_t n=strlen(p);(void)last;if(n<cap)memcpy(out,p,n+1);return (DWORD)n;}
static DWORD GetModuleFileNameA(void *module,char *out,DWORD cap){(void)module;assert(cap>20);strcpy(out,"C:\\whp host.exe");return (DWORD)strlen(out);}
static HANDLE CreateFileMappingA(HANDLE f,SECURITY_ATTRIBUTES *sa,DWORD prot,DWORD hi,DWORD lo,const char *name){(void)f;(void)prot;(void)hi;(void)name;assert(sa->bInheritHandle);return calloc(1,lo);}
static void *MapViewOfFile(HANDLE h,DWORD flags,DWORD hi,DWORD lo,size_t cb){(void)flags;(void)hi;(void)lo;(void)cb;return h;}
static int UnmapViewOfFile(void *p){free(p);return 1;}
static int DuplicateHandle(HANDLE a,HANDLE b,HANDLE c,HANDLE *out,DWORD access,int inherit,DWORD flags){(void)a;(void)c;(void)access;(void)flags;assert(inherit);*out=(HANDLE)((uintptr_t)b+1000);return 1;}
static int InitializeProcThreadAttributeList(void *p,DWORD n,DWORD flags,SIZE_T *size){(void)n;(void)flags;*size=32;return p!=NULL;}
static int UpdateProcThreadAttribute(void *p,DWORD flags,uintptr_t key,void *data,SIZE_T size,void *previous,void *ret){(void)p;(void)flags;(void)key;(void)data;(void)previous;(void)ret;mock_inherit_count=(uint32_t)(size/sizeof(HANDLE));return 1;}
static void DeleteProcThreadAttributeList(void *p){(void)p;}
static int CreateProcessA(const char *self,char *cmd,void *ps,void *ts,int inherit,DWORD flags,void *env,const char *cwd,STARTUPINFOA *si,PROCESS_INFORMATION *pi)
{
 (void)ps;(void)ts;(void)cwd;assert(!strcmp(self,"C:\\whp host.exe")&&strstr(cmd,"--whp-child")&&inherit&&flags==EXTENDED_STARTUPINFO_PRESENT&&!env);
 assert(si->dwFlags==STARTF_USESTDHANDLES&&mock_inherit_count==5);
 if(mock_create_failure)return 0;
 assert(mock_process_count<64);pi->hProcess=&mock_processes[mock_process_count];pi->dwProcessId=100+mock_process_count++;
 memset(pi->hProcess,0,sizeof(struct MockProcess));pi->hThread=(HANDLE)(uintptr_t)3;return 1;
}
static DWORD WaitForSingleObject(HANDLE h,DWORD timeout){assert(!timeout);return ((struct MockProcess *)h)->done?WAIT_OBJECT_0:WAIT_TIMEOUT;}
static int GetExitCodeProcess(HANDLE h,DWORD *code){*code=((struct MockProcess *)h)->exitcode;return 1;}
static LONG InterlockedCompareExchange(volatile LONG *p,LONG val,LONG expected){LONG old=*p;if(old==expected)*p=val;return old;}
