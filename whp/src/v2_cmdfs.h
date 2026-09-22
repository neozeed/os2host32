/* CMD phase 1: filesystem/cwd plus real handle duplication and anonymous pipes.
   Process/session launch is reported unsupported until its lifecycle is wired. */
static uint32_t cmd_install_handle(struct Runtime *rt,HANDLE h,uint32_t target)
{
 HANDLE old;
 if(target<3) {
  DWORD which=target==0?STD_INPUT_HANDLE:target==1?STD_OUTPUT_HANDLE:STD_ERROR_HANDLE;
  old=os2_handle(rt,target);
  if(!SetStdHandle(which,h))return GetLastError();
  if(old!=INVALID_HANDLE_VALUE)CloseHandle(old);rt->std_closed[target]=0;
 } else {if(rt->files[target])CloseHandle(rt->files[target]);rt->files[target]=h;}
 return 0;
}
static uint32_t dispatch_cmdfs(struct Runtime *rt,uint32_t ordinal,uint32_t esp)
{
 uint32_t a[5],i,n,rc;char path[MAX_PATH],other[MAX_PATH];DWORD got;HANDLE h,dup;
 for(i=0;i<5;i++)a[i]=guest_range(esp+4+i*4,4)?guest_u32(rt,esp+4+i*4):0;
 switch(ordinal) {
 case 255:case 226:case 270:case 271:
  if(!a[0]||!guest_copy_cstr(rt,a[0],path,sizeof(path)))return 87;
  if(ordinal==255)return SetCurrentDirectoryA(path)?0:GetLastError();
  if(ordinal==226)return RemoveDirectoryA(path)?0:GetLastError();
  if(ordinal==270){if(a[1])return 87;return CreateDirectoryA(path,NULL)?0:GetLastError();}
  if(!a[1]||!guest_copy_cstr(rt,a[1],other,sizeof(other)))return 87;
  return MoveFileA(path,other)?0:GetLastError();
 case 275:
  if(!a[0]||!a[1]||!guest_range(a[0],4)||!guest_range(a[1],4))return 87;
  got=GetCurrentDirectoryA(sizeof(path),path);if(!got)return GetLastError();
  if(got>=sizeof(path)||path[1]!=':')return 15;
  guest_put_u32(rt,a[0],(uint32_t)(toupper((unsigned char)path[0])-'A'+1));
  guest_put_u32(rt,a[1],GetLogicalDrives());return 0;
 case 220:
  if(a[0]<1||a[0]>26)return 15;
  path[0]=(char)('A'+a[0]-1);path[1]=':';path[2]=0;
  return SetCurrentDirectoryA(path)?0:GetLastError();
 case 274:
  if(a[0]>26||!a[1]||!a[2]||!guest_range(a[2],4))return 87;
  n=guest_u32(rt,a[2]);
  if(a[0]) {other[0]=(char)('A'+a[0]-1);other[1]=':';other[2]='.';other[3]=0;got=GetFullPathNameA(other,sizeof(path),path,NULL);}
  else got=GetCurrentDirectoryA(sizeof(path),path);
  if(!got)return GetLastError();if(got>=sizeof(path)||path[1]!=':')return 15;
  i=2;if(path[i]=='\\'||path[i]=='/')i++;
  got=(DWORD)strlen(path+i)+1;guest_put_u32(rt,a[2],got);
  if(n<got)return 111;if(!guest_range(a[1],got))return 87;memcpy(rt->ram+a[1],path+i,got);return 0;
 case 260:
  if(!a[1]||!guest_range(a[1],4))return 87;
  h=os2_handle(rt,a[0]);if(h==INVALID_HANDLE_VALUE)return 6;
  n=guest_u32(rt,a[1]);
  if(n==0xffffffffu){for(n=3;n<MAX_FILES&&rt->files[n];n++);}
  if(n>=MAX_FILES)return 4;if(n==a[0])return 0;
  if(!DuplicateHandle(GetCurrentProcess(),h,GetCurrentProcess(),&dup,0,FALSE,DUPLICATE_SAME_ACCESS))return GetLastError();
  rc=cmd_install_handle(rt,dup,n);if(rc)CloseHandle(dup);else guest_put_u32(rt,a[1],n);return rc;
 case 239:
  if(!a[0]||!a[1]||!guest_range(a[0],4)||!guest_range(a[1],4)||a[0]==a[1])return 87;
  for(i=3;i<MAX_FILES&&rt->files[i];i++);
  for(n=i+1;n<MAX_FILES&&rt->files[n];n++);
  if(n>=MAX_FILES)return 4;
  if(!CreatePipe(&h,&dup,NULL,a[2]))return GetLastError();
  rt->files[i]=h;rt->files[n]=dup;guest_put_u32(rt,a[0],i);guest_put_u32(rt,a[1],n);return 0;
 case 323:
  {
   uint8_t *data;uint32_t size,off,flags;
   if(!a[0]||!a[1]||!guest_range(a[1],4)||!guest_copy_cstr(rt,a[0],path,sizeof(path)))return 87;
   data=load_file(path,&size);if(!data)return 2;
   if(size<64||data[0]!='M'||data[1]!='Z'){free(data);return 193;}
   off=rd32(data+0x3c);
   if(off>size||size-off<0x14||data[off]!='L'||(data[off+1]!='E'&&data[off+1]!='X')){free(data);return 193;}
   flags=rd32(data+off+0x10);free(data);
   /* FAPPTYP_WINDOWAPI=3; window compatibility field matches module bits. */
   guest_put_u32(rt,a[1],((flags>>8)&3u)|((flags&MOD_TYPE_DLL)?0x10u:0));return 0;
  }

 }
 return 1;
}
