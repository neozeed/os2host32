/* Private host-to-host startup transport. No guest address crosses processes. */
#define CHILD_DATA_CAP (GUEST_STARTUP_LIMIT-GUEST_STARTUP)
#define CHILD_MAGIC 0x32504857u
struct ChildPacket {
 uint32_t magic,version;
 volatile LONG ready;
 uint32_t parent_pid,program_len,env_len,args_len,closed_mask,normal_exit,exit_rc;
 uint64_t log_handle;
 char data[CHILD_DATA_CAP]; /* program NUL, environment block, argument block */
};
static struct ChildPacket *child_boot;
static int child_packet_valid(const struct ChildPacket *p)
{
 uint32_t n=p->program_len,e=p->env_len,a=p->args_len;
 if(p->magic!=CHILD_MAGIC||p->version!=1||!n||n>MAX_PATH||e<2||a<3)return 0;
 if(e>CHILD_DATA_CAP-n||a>CHILD_DATA_CAP-n-e)return 0;
 if(p->data[n-1]||memchr(p->data,0,n-1))return 0;
 if(p->data[n+e-1]||p->data[n+e-2])return 0;
 if(p->data[n+e+a-1]||p->data[n+e+a-2]||!memchr(p->data+n+e,0,a-2))return 0;
 return 1;
}
static void child_startup(uint8_t *ram,uint32_t *env,uint32_t *args,uint32_t *pgm)
{
 uint32_t n=child_boot->program_len,e=child_boot->env_len,a=child_boot->args_len;
 *pgm=GUEST_STARTUP;*env=GUEST_STARTUP+n;*args=*env+e;
 memcpy(ram+GUEST_STARTUP,child_boot->data,n+e+a);
}
static int child_attach(const char *token)
{
 char *end;uint64_t value=strtoull(token,&end,16);HANDLE mapping,guest_err=NULL;int fd;
 if(!*token||*end||!value)return 0;
 mapping=(HANDLE)(uintptr_t)value;
 child_boot=(struct ChildPacket *)MapViewOfFile(mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(*child_boot));
 CloseHandle(mapping);
 if(!child_boot||!child_packet_valid(child_boot))return 0;
 /* CRT stderr and the guest's handle 2 must own different native handles.
    _dup2 closes its previous descriptor; preserve guest stderr first. */
 if(!(child_boot->closed_mask&4)) {
  if(!DuplicateHandle(GetCurrentProcess(),GetStdHandle(STD_ERROR_HANDLE),GetCurrentProcess(),&guest_err,0,FALSE,DUPLICATE_SAME_ACCESS))return 0;
  if(!SetStdHandle(STD_ERROR_HANDLE,guest_err)){CloseHandle(guest_err);return 0;}
 }
 fd=_open_osfhandle((intptr_t)child_boot->log_handle,_O_WRONLY|_O_TEXT);
 if(fd<0)return 0;
 if(_dup2(fd,2)!=0){_close(fd);return 0;}
 if(fd!=2)_close(fd);
 /* CRT descriptor replacement may also update the Win32 standard handle. */
 if(guest_err && !SetStdHandle(STD_ERROR_HANDLE,guest_err))return 0;
 /* Apply explicit guest compatibility choices to the child host too. */
 {
  static const char *names[]={"WHP_OS2_FIND_LAYOUT","WHP_OS2_FIND_ZERO_COUNT","WHP_OS2_SARIEN_GEOMETRY"};
  uint32_t i;
  for(i=0;i<3;i++) {
   const char *p=child_boot->data+child_boot->program_len;
   const char *limit=p+child_boot->env_len;size_t key=strlen(names[i]);
   SetEnvironmentVariableA(names[i],NULL);
   while(p<limit&&*p) {
    const char *zero=(const char *)memchr(p,0,(size_t)(limit-p));
    if(!zero)return 0;
    if((size_t)(zero-p)>key && !_strnicmp(p,names[i],key)&&p[key]=='=') {SetEnvironmentVariableA(names[i],p+key+1);break;}
    p=zero+1;
   }
  }
 }
 return 1;
}
