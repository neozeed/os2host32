/* One Win64 WHP host per guest process. Only direct-child waits are modeled. */
#define MAX_CHILDREN 32u
struct ChildProcess {
 HANDLE process,mapping;
 struct ChildPacket *packet;
 uint32_t pid,mode,owner,launching;
};
static struct ChildProcess children[MAX_CHILDREN];
static void process_release(struct ChildProcess *c)
{
 if(c->packet)UnmapViewOfFile(c->packet);
 if(c->mapping)CloseHandle(c->mapping);
 if(c->process)CloseHandle(c->process);
 memset(c,0,sizeof(*c));
}
static void process_cleanup(void)
{
 uint32_t i;for(i=0;i<MAX_CHILDREN;i++)process_release(&children[i]);
 /* Async descendants intentionally continue; releasing our handle is not a kill. */
}
static uint32_t process_block_copy(struct Runtime *rt,uint32_t va,char *out,uint32_t cap,uint32_t *len,int args)
{
 uint32_t i=0,nul=0;
 if(!va)return 87;
 while(i<cap) {
  char b;
  if(!guest_range(va+i,1))return 87;
  b=(char)rt->ram[va+i];out[i++]=b;
  if(args) {
   if(!b&&++nul==2) {if(i==cap)return 111;out[i++]=0;*len=i;return 0;}
  } else if(i>=2&&!b&&!out[i-2]) {*len=i;return 0;}
 }
 return 111;
}
static uint32_t process_make_packet(struct Runtime *rt,const uint32_t *a,struct ChildPacket *p)
{
 char path[MAX_PATH];uint32_t rc,n,e,ar,env;DWORD got;uint8_t *file;uint32_t size,off;
 if(!a[6]||!guest_copy_cstr(rt,a[6],path,sizeof(path)))return 87;
 got=GetFullPathNameA(path,MAX_PATH,p->data,NULL);if(!got)return GetLastError();if(got>=MAX_PATH)return 111;
 file=load_file(p->data,&size);if(!file)return GetFileAttributesA(p->data)==INVALID_FILE_ATTRIBUTES?GetLastError():5;
 if(size<64||file[0]!='M'||file[1]!='Z'){free(file);return 193;}
 off=rd32(file+60);
 if(off>size||size-off<0xc4||file[off]!='L'||(file[off+1]!='E'&&file[off+1]!='X')||rd16(file+off+8)!=2||rd16(file+off+10)!=1||(rd32(file+off+16)&MOD_TYPE_MASK)==MOD_TYPE_DLL){free(file);return 193;}
 free(file);n=got+1;
 env=a[4]?a[4]:guest_u32(rt,GUEST_INFO+16);
 rc=process_block_copy(rt,env,p->data+n,CHILD_DATA_CAP-n,&e,0);if(rc)return rc;
 if(a[3]) {rc=process_block_copy(rt,a[3],p->data+n+e,CHILD_DATA_CAP-n-e,&ar,1);if(rc)return rc;}
 else {
  ar=n+2;if(ar>CHILD_DATA_CAP-n-e)return 111;
  memcpy(p->data+n+e,p->data,n);p->data[n+e+n]=0;p->data[n+e+n+1]=0;
 }
 p->magic=CHILD_MAGIC;p->version=1;p->program_len=n;p->env_len=e;p->args_len=ar;
 p->parent_pid=GetCurrentProcessId();p->ready=0;
 for(n=0;n<3;n++)if(rt->std_closed[n])p->closed_mask|=1u<<n;
 return child_packet_valid(p)?0:87;
}
static uint32_t process_launch(struct Runtime *rt,struct ChildProcess *c,struct ChildPacket *payload)
{
 SECURITY_ATTRIBUTES sa;STARTUPINFOEXA si;PROCESS_INFORMATION pi;
 HANDLE inherit[5],stds[3],log;SIZE_T bytes=0;uint32_t i,count=0,rc=0;
 char self[MAX_PATH],command[MAX_PATH+80];DWORD got;
 memset(&sa,0,sizeof(sa));sa.nLength=sizeof(sa);sa.bInheritHandle=TRUE;
 memset(&si,0,sizeof(si));memset(&pi,0,sizeof(pi));memset(stds,0,sizeof(stds));log=NULL;
 got=GetModuleFileNameA(NULL,self,MAX_PATH);if(!got)return GetLastError();if(got>=MAX_PATH)return 111;
 c->mapping=CreateFileMappingA(INVALID_HANDLE_VALUE,&sa,PAGE_READWRITE,0,sizeof(*payload),NULL);
 if(!c->mapping)return GetLastError();
 c->packet=(struct ChildPacket *)MapViewOfFile(c->mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(*payload));
 if(!c->packet){rc=GetLastError();goto done;}
 memcpy(c->packet,payload,sizeof(*payload));inherit[count++]=c->mapping;
 for(i=0;i<3;i++) {
  HANDLE h=os2_handle(rt,i);
  if(h==INVALID_HANDLE_VALUE){stds[i]=INVALID_HANDLE_VALUE;continue;}
  if(!DuplicateHandle(GetCurrentProcess(),h,GetCurrentProcess(),&stds[i],0,TRUE,DUPLICATE_SAME_ACCESS)){rc=GetLastError();goto done;}
  inherit[count++]=stds[i];
 }
 if(!DuplicateHandle(GetCurrentProcess(),(HANDLE)_get_osfhandle(_fileno(stderr)),GetCurrentProcess(),&log,0,TRUE,DUPLICATE_SAME_ACCESS)){rc=GetLastError();goto done;}
 inherit[count++]=log;c->packet->log_handle=(uint64_t)(uintptr_t)log;
 InitializeProcThreadAttributeList(NULL,1,0,&bytes);
 si.lpAttributeList=(LPPROC_THREAD_ATTRIBUTE_LIST)malloc(bytes);
 if(!si.lpAttributeList){rc=8;goto done;}
 if(!InitializeProcThreadAttributeList(si.lpAttributeList,1,0,&bytes)){rc=GetLastError();free(si.lpAttributeList);si.lpAttributeList=NULL;goto done;}
 if(!UpdateProcThreadAttribute(si.lpAttributeList,0,PROC_THREAD_ATTRIBUTE_HANDLE_LIST,inherit,count*sizeof(HANDLE),NULL,NULL)){rc=GetLastError();goto done;}
 si.StartupInfo.cb=sizeof(si);si.StartupInfo.dwFlags=STARTF_USESTDHANDLES;
 si.StartupInfo.hStdInput=stds[0];si.StartupInfo.hStdOutput=stds[1];si.StartupInfo.hStdError=stds[2];
 snprintf(command,sizeof(command),"\"%s\" --whp-child %llX",self,(unsigned long long)(uintptr_t)c->mapping);
 /* Host environment retains DLL/tool configuration; guest gets packet environment.
    Cwd is inherited at this call. No cmd.exe and no guest command-tail parsing. */
 console_cleanup(); /* Return cooked console mode before a child uses DosRead. */
 if(!CreateProcessA(self,command,NULL,NULL,TRUE,EXTENDED_STARTUPINFO_PRESENT,NULL,NULL,&si.StartupInfo,&pi)){rc=GetLastError();goto done;}
 CloseHandle(pi.hThread);c->process=pi.hProcess;c->pid=pi.dwProcessId;
 fprintf(stderr,"v2: child PID %u created: %s\n",c->pid,payload->data);
 done:
 if(si.lpAttributeList){DeleteProcThreadAttributeList(si.lpAttributeList);free(si.lpAttributeList);}
 for(i=0;i<3;i++)if(stds[i]&&stds[i]!=INVALID_HANDLE_VALUE)CloseHandle(stds[i]);
 if(log)CloseHandle(log);
 if(rc)process_release(c);
 return rc;
}
static void process_results(struct Runtime *rt,uint32_t out,uint32_t term,uint32_t result)
{guest_put_u32(rt,out,term);guest_put_u32(rt,out+4,result);}
static uint32_t process_collect(struct Runtime *rt,struct ChildProcess *c,uint32_t result,uint32_t pid_out)
{
 DWORD code;
 if(!GetExitCodeProcess(c->process,&code))return GetLastError();
 process_results(rt,result,c->packet->normal_exit?0:2,c->packet->normal_exit?c->packet->exit_rc:code);
 if(pid_out)guest_put_u32(rt,pid_out,c->pid);
 fprintf(stderr,"v2: child PID %u collected: term=%u result=%u\n",c->pid,c->packet->normal_exit?0:2,c->packet->normal_exit?c->packet->exit_rc:code);
 process_release(c);return 0;
}
/* Wait request queue_args: kind(1 exec/2 wait), slot or requested PID, results,
   PID output, object-name output, object-name capacity. */
static int process_poll_wait(struct Runtime *rt,struct GuestThread *t)
{
 uint32_t i,rc;int exists=0;
 if(t->queue_args[0]==1) {
  struct ChildProcess *c=&children[t->queue_args[1]];
  DWORD status=WaitForSingleObject(c->process,0);
  if(status==WAIT_FAILED){rc=GetLastError();process_release(c);sync_ready(t,rc);return 1;}
  if(InterlockedCompareExchange(&c->packet->ready,0,0)==1) {
   c->launching=0;
   if(c->mode==2) {
    process_results(rt,t->queue_args[2],c->pid,0);c->owner=0;sync_ready(t,0);return 1;
   }
   if(status==WAIT_OBJECT_0){rc=process_collect(rt,c,t->queue_args[2],0);sync_ready(t,rc);return 1;}
  } else if(status==WAIT_OBJECT_0) {
   fprintf(stderr,"v2: child failed before guest entry: %s\n",c->packet->data);
   if(t->queue_args[4]&&t->queue_args[5]) {
    size_t n=strlen(c->packet->data);if(n>=t->queue_args[5])n=t->queue_args[5]-1;
    memcpy(rt->ram+t->queue_args[4],c->packet->data,n);rt->ram[t->queue_args[4]+n]=0;
   }
   process_release(c);sync_ready(t,31);return 1;
  }
  return 0;
 }
 for(i=0;i<MAX_CHILDREN;i++) {
  struct ChildProcess *c=&children[i];DWORD status;
  if(!c->process||c->mode!=2||c->launching||c->owner||(t->queue_args[1]&&c->pid!=t->queue_args[1]))continue;
  exists=1;status=WaitForSingleObject(c->process,0);
  if(status==WAIT_FAILED){sync_ready(t,GetLastError());return 1;}
  if(status==WAIT_OBJECT_0){rc=process_collect(rt,c,t->queue_args[2],t->queue_args[3]);sync_ready(t,rc);return 1;}
 }
 if(!exists){sync_ready(t,128);return 1;}
 return 0;
}
static uint32_t dispatch_process(struct Runtime *rt,uint32_t ordinal,uint32_t esp)
{
 uint32_t a[7]={0},i,rc,slot;struct GuestThread *t=current_guest_thread(rt);
 if(!t)return 1;
 for(i=0;i<(ordinal==283?7u:5u);i++){if(!guest_range(esp+4+i*4,4))return 87;a[i]=guest_u32(rt,esp+4+i*4);}
 if(ordinal==283) {
  struct ChildPacket *payload;
  if(a[2]!=0&&a[2]!=2)return 1;
  if((int32_t)a[1]<0 || (a[1]&&(!a[0]||!guest_range(a[0],a[1]))) || !a[5]||!guest_range(a[5],8))return 87;
  if(a[1])rt->ram[a[0]]=0;process_results(rt,a[5],0,0);
  for(slot=0;slot<MAX_CHILDREN&&children[slot].process;slot++);
  if(slot==MAX_CHILDREN)return 89;
  payload=(struct ChildPacket *)calloc(1,sizeof(*payload));if(!payload)return 8;
  rc=process_make_packet(rt,a,payload);
  if(!rc)rc=process_launch(rt,&children[slot],payload);
  if(rc && a[1]) {
   char name[MAX_PATH];if(a[6]&&guest_copy_cstr(rt,a[6],name,sizeof(name))) {
    size_t n=strlen(name);if(n>=a[1])n=a[1]-1;memcpy(rt->ram+a[0],name,n);rt->ram[a[0]+n]=0;
   }
  }
  free(payload);if(rc)return rc;
  children[slot].mode=a[2];children[slot].owner=t->tid;children[slot].launching=1;
  t->queue_args[0]=1;t->queue_args[1]=slot;t->queue_args[2]=a[5];t->queue_args[3]=0;t->queue_args[4]=a[0];t->queue_args[5]=a[1];
 } else {
  int exists=0;
  if(a[0]!=0)return 1; /* DCWA_PROCESS: direct children */
  if(a[1]>1||!a[2]||!a[3]||!guest_range(a[2],8)||!guest_range(a[3],4))return 87;
  process_results(rt,a[2],0,0);guest_put_u32(rt,a[3],0);
  for(i=0;i<MAX_CHILDREN;i++) {
   struct ChildProcess *c=&children[i];DWORD status;
   if(!c->process||c->mode!=2||c->launching||c->owner||(a[4]&&c->pid!=a[4]))continue;
   exists=1;status=WaitForSingleObject(c->process,0);
   if(status==WAIT_FAILED)return GetLastError();
   if(status==WAIT_OBJECT_0)return process_collect(rt,c,a[2],a[3]);
  }
  if(!exists)return 128;if(a[1])return 129;
  t->queue_args[0]=2;t->queue_args[1]=a[4];t->queue_args[2]=a[2];t->queue_args[3]=a[3];t->queue_args[4]=t->queue_args[5]=0;
 }
 sync_block(rt,THREAD_WAIT_PROCESS,0,10);return 0;
}
