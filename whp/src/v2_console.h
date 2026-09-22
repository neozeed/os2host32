/* Packed C/386 Pascal arguments; pointer tokens remain flat guest addresses. */
static HANDLE console_input;
static DWORD console_old_mode;
static int console_mode_saved;
static WORD console_repeat;
static KEY_EVENT_RECORD console_key;
static void console_cleanup(void)
{
 HANDLE h=GetStdHandle(STD_INPUT_HANDLE);DWORD mode;
 if(console_mode_saved) {
  if(GetConsoleMode(h,&mode))SetConsoleMode(h,console_old_mode);
  else SetConsoleMode(console_input,console_old_mode);
 }
 console_mode_saved=0;console_repeat=0;
}
static uint32_t console_key_read(struct Runtime *rt,uint32_t out,int *ready)
{
 INPUT_RECORD rec;DWORD count,mode;HANDLE h=GetStdHandle(STD_INPUT_HANDLE);
 *ready=0;
 if(!out || !guest_range(out,10))return 87;
 if(!GetConsoleMode(h,&mode))return 6; /* redirected input uses DosRead, not KbdCharIn */
 if(!console_mode_saved) {
  console_input=h;console_old_mode=mode;
  if(!SetConsoleMode(h,mode&~(ENABLE_LINE_INPUT|ENABLE_ECHO_INPUT|ENABLE_PROCESSED_INPUT)))return GetLastError();
  console_mode_saved=1;
 }
 /* Bound one poll so a stream of mouse/resize records cannot starve peers. */
 for(count=0;!console_repeat && count<64;count++) {
  DWORD got;
  if(!PeekConsoleInputA(h,&rec,1,&got))return GetLastError();
  if(!got)break;
  if(!ReadConsoleInputA(h,&rec,1,&got))return GetLastError();
  if(got && rec.EventType==KEY_EVENT && rec.Event.KeyEvent.bKeyDown) {
   KEY_EVENT_RECORD *k=&rec.Event.KeyEvent;
   if(!k->uChar.AsciiChar && (k->wVirtualKeyCode==VK_SHIFT || k->wVirtualKeyCode==VK_CONTROL || k->wVirtualKeyCode==VK_MENU || k->wVirtualKeyCode==VK_CAPITAL || k->wVirtualKeyCode==VK_NUMLOCK || k->wVirtualKeyCode==VK_SCROLL))continue;
   console_key=*k;console_repeat=k->wRepeatCount?k->wRepeatCount:1;
  }
 }
 memset(rt->ram+out,0,10);
 if(console_repeat) {
  DWORD c=console_key.dwControlKeyState;uint16_t shift=0;
  if(c&SHIFT_PRESSED)shift|=3;
  if(c&(LEFT_CTRL_PRESSED|RIGHT_CTRL_PRESSED))shift|=4;
  if(c&(LEFT_ALT_PRESSED|RIGHT_ALT_PRESSED))shift|=8;
  if(c&SCROLLLOCK_ON)shift|=16;if(c&NUMLOCK_ON)shift|=32;if(c&CAPSLOCK_ON)shift|=64;
  rt->ram[out]=(uint8_t)console_key.uChar.AsciiChar;
  rt->ram[out+1]=(uint8_t)console_key.wVirtualScanCode;
  rt->ram[out+2]=0x40;file_put16(rt->ram+out+4,shift);
  guest_put_u32(rt,out+6,(uint32_t)GetTickCount64());console_repeat--;*ready=1;
 }
 return 0;
}
static int console_poll_wait(struct Runtime *rt,struct GuestThread *t)
{
 int ready;uint32_t rc=console_key_read(rt,t->queue_args[0],&ready);
 if(!rc&&!ready)return 0;
 sync_ready(t,rc);return 1;
}
static uint32_t dispatch_console(struct Runtime *rt,uint32_t id,uint32_t esp)
{
 const struct Far16Desc *d;uint32_t a[7]={0},i,at=esp+4,rc;
 HANDLE h=GetStdHandle(STD_OUTPUT_HANDLE);CONSOLE_SCREEN_BUFFER_INFO info;COORD pos;DWORD done;
 if(!id||id>7)return 1;d=&far16_apis[id-1];
 for(i=0;i<d->count;i++) {
  if(!guest_range(at,d->width[i]))return 87;
  a[i]=d->width[i]==2?rd16(rt->ram+at):guest_u32(rt,at);at+=d->width[i];
 }
 if(a[0])return 6;
 if(id==6) {
  int ready;
  if(a[1]>1)return 87;
  rc=console_key_read(rt,a[2],&ready);
  if(!rc&&!ready&&!a[1]) {
   current_guest_thread(rt)->queue_args[0]=a[2];
   sync_block(rt,THREAD_WAIT_KBD,0,10);
  }
  return rc;
 }
 if(id==7) {console_repeat=0;return FlushConsoleInputBuffer(GetStdHandle(STD_INPUT_HANDLE))?0:GetLastError();}
 if(id==4) {
  if(a[1]&&(!a[2]||!guest_range(a[2],a[1])))return 87;
  if(!a[1])return 0;
  return WriteFile(h,rt->ram+a[2],a[1],&done,NULL)?(done==a[1]?0:29):GetLastError();
 }
 if(!GetConsoleScreenBufferInfo(h,&info))return GetLastError();
 if(id==2) {
  if(!a[1]||!a[2]||!guest_range(a[1],2)||!guest_range(a[2],2))return 87;
  file_put16(rt->ram+a[1],(uint16_t)(info.dwCursorPosition.X-info.srWindow.Left));
  file_put16(rt->ram+a[2],(uint16_t)(info.dwCursorPosition.Y-info.srWindow.Top));return 0;
 }
 if(id==3) {
  if(a[1]>(uint32_t)(info.srWindow.Right-info.srWindow.Left)||a[2]>(uint32_t)(info.srWindow.Bottom-info.srWindow.Top))return 87;
  pos.X=(SHORT)(info.srWindow.Left+a[1]);pos.Y=(SHORT)(info.srWindow.Top+a[2]);
  return SetConsoleCursorPosition(h,pos)?0:GetLastError();
 }
 if(id==5) {
  uint32_t cb;
  if(!a[1]||!guest_range(a[1],2))return 87;cb=rd16(rt->ram+a[1]);
  if(cb<12||!guest_range(a[1],cb))return 87;
  /* Only the documented prefix is supported; preserve the caller's cb. */
  memset(rt->ram+a[1]+2,0,10);rt->ram[a[1]+2]=1;rt->ram[a[1]+3]=4;
  file_put16(rt->ram+a[1]+4,(uint16_t)(info.srWindow.Right-info.srWindow.Left+1));
  file_put16(rt->ram+a[1]+6,(uint16_t)(info.srWindow.Bottom-info.srWindow.Top+1));return 0;
 }
 if(id==1) {
  SMALL_RECT rect;CHAR_INFO cell;uint32_t height,width,lines,row;
  if(!a[1]||!guest_range(a[1],2))return 87;
  width=(uint32_t)(info.srWindow.Right-info.srWindow.Left+1);height=(uint32_t)(info.srWindow.Bottom-info.srWindow.Top+1);
  if(a[3]==0xffff)a[3]=width-1;if(a[4]==0xffff)a[4]=height-1;
  if(a[6]>a[4]||a[5]>a[3]||a[3]>=width||a[4]>=height)return 87;
  rect.Left=(SHORT)(info.srWindow.Left+a[5]);rect.Right=(SHORT)(info.srWindow.Left+a[3]);
  rect.Top=(SHORT)(info.srWindow.Top+a[6]);rect.Bottom=(SHORT)(info.srWindow.Top+a[4]);
  cell.Char.AsciiChar=(CHAR)rt->ram[a[1]];cell.Attributes=rt->ram[a[1]+1];
  height=a[4]-a[6]+1;width=a[3]-a[5]+1;lines=a[2];if(!lines)return 0;
  if(lines>=height) {
   for(row=0;row<height;row++) {
    pos.X=rect.Left;pos.Y=(SHORT)(rect.Top+row);
    if(!FillConsoleOutputCharacterA(h,cell.Char.AsciiChar,width,pos,&done)||!FillConsoleOutputAttribute(h,cell.Attributes,width,pos,&done))return GetLastError();
   }
   return 0;
  }
  pos.X=rect.Left;pos.Y=(SHORT)(rect.Top-lines);
  return ScrollConsoleScreenBufferA(h,&rect,&rect,pos,&cell)?0:GetLastError();
 }
 return 1;
}
