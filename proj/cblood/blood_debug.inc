/* Developer console: native SDL overlay; no host shell and no flag writes.
 * Pause the simulation while inspecting. ADVANCE changes only the game clock;
 * original script conditions are evaluated by the next dialogue action. */
static void debug_line(App*a,const char *fmt,...)
{
 if(a->debug_count>=512)return;
 va_list args;va_start(args,fmt);vsnprintf(a->debug_lines[a->debug_count++],41,fmt,args);va_end(args);
}
static void debug_status(App*a)
{
 unsigned current=(unsigned)a->minutes*60u+a->seconds,limit=(unsigned)a->game.grace_minutes*60u;
 debug_line(a,"CLOCK %u:%02u GRACE %u MIN",a->minutes,a->seconds,a->game.grace_minutes);
 if(current<limit)debug_line(a,"HAND FAILURE IN %u:%02u",(limit-current)/60,(limit-current)%60);
 else debug_line(a,"HAND FAILURE +%u:%02u",(current-limit)/60,(current-limit)%60);
 debug_line(a,"SEVERE +20 MIN; STRONGER +25 MIN");
 debug_line(a,"WORLD %u,%u MODE %04X",a->game.planet_x,a->game.planet_y,a->game.mode);
 debug_line(a,"ACTIVE %u BANK %u",a->game.active_record,a->game.active_bank);
 debug_line(a,"ABOARD %u RECORD %u REWARD %u",a->game.onboard,a->game.carried_record,a->game.pending_extension);
 debug_line(a,"VM PC %04X SPEAK %u END %u",a->vm.pc,a->vm.active,a->vm.end_requested);
 debug_line(a,"FLAG 5=%d 6=%d 8=%d 999=%d",blood_game_bit(&a->game,5),blood_game_bit(&a->game,6),blood_game_bit(&a->game,8),blood_game_bit(&a->game,999));
 debug_line(a,"TIMERS SHOWS SCRIPT TIME GATES");
 debug_line(a,"RENDEZVOUS ALSO NEEDS SCRIPT FLAGS");
}
static void debug_timers(App*a,int filter)
{
 uint8_t b[65536];char name[32],file[1024];unsigned found=0;
 debug_line(a,"SCRIPT GATES / TIME IN MINUTES");
 debug_line(a,"NOT GUARANTEED RENDEZVOUS EVENTS");
 for(unsigned bank=0;bank<16;bank++){
  if(filter>=0&&(unsigned)filter!=bank)continue;
  snprintf(name,sizeof(name),"P%c.EGP",'A'+bank);if(!path(file,sizeof(file),a->assets,name))continue;
  FILE*f=fopen(file,"rb");if(!f)continue;size_t n=fread(b,1,sizeof(b),f);fclose(f);
  if(n<11||b[0]!=0xfd)continue;
  uint8_t *q=b+7;n-=7;
  if(a->dialogue_ready&&a->game.active_bank==bank){q=a->vm.bank;n=a->vm.bank_size;}
  size_t pc=q[0]+256u*q[1];
  while(pc+2<=n&&q[pc]!=255){
   unsigned len=q[pc]&31u,kind=q[pc]&224u;if(pc+2+len>n)break;
   if((kind==160||kind==192)&&len>=2&&!(len&1u)){
    unsigned t=q[pc+2]*256u+q[pc+3];int time_ok=kind==160?a->minutes>=t:a->minutes<=t,flags_ok=1;
    for(unsigned k=2;k<len;k+=2){unsigned raw=q[pc+2+k]*256u+q[pc+3+k],negative=raw&32768u,bit=negative?(0u-raw)&65535u:raw;int value=blood_game_bit(&a->game,bit);if(value<0||(negative?value:!value))flags_ok=0;}
    debug_line(a,"P%c %04X TIME %s %u %s",'A'+bank,(unsigned)pc,kind==160?">=":"<=",t,time_ok?"YES":"NO");
    if(kind==160&&a->minutes<t)debug_line(a,"  OPENS IN %u:%02u",((unsigned)t*60u-((unsigned)a->minutes*60u+a->seconds))/60,((unsigned)t*60u-((unsigned)a->minutes*60u+a->seconds))%60);
    debug_line(a,"  LOCAL FLAGS %s CMD %02X",flags_ok?"PASS":"BLOCK",q[pc+1]);
    for(unsigned k=2;k<len;k+=2){unsigned raw=q[pc+2+k]*256u+q[pc+3+k],neg=raw&32768u,bit=neg?(0u-raw)&65535u:raw;debug_line(a,"  FLAG %u NEED %u HAVE %d",bit,!neg,blood_game_bit(&a->game,bit));}
    found++;
   }
   pc+=2+len;
  }
 }
 if(!found)debug_line(a,"NO TIME GATES IN THESE BANKS");
 debug_line(a,"ADJACENT CONDITIONS ALSO APPLY");
}
static void debug_command(App*a,const char *input)
{
 char command[80];size_t n=strlen(input);if(n>=sizeof(command))n=sizeof(command)-1;
 for(size_t i=0;i<n;i++)command[i]=(char)toupper((unsigned char)input[i]);
 command[n]=0;
 a->debug_count=0;a->debug_scroll=0;
 char verb[20],argument[40],extra[20];unsigned value=0;
 int fields=sscanf(command,"%19s %39s %19s",verb,argument,extra),numeric=0;
 if(fields==2){numeric=argument[0]!=0;uint32_t parsed=0;
  for(unsigned i=0;argument[i];i++){if(argument[i]<'0'||argument[i]>'9'){numeric=0;break;}parsed=parsed*10u+(unsigned)(argument[i]-'0');if(parsed>86400u){numeric=0;break;}}
  if(numeric)value=(unsigned)parsed;
 }
 if(fields<1||!strcmp(verb,"HELP")){
  debug_line(a,"F12 / ESC CLOSE; PGUP/PGDN SCROLL");
  debug_line(a,"STATUS       CLOCK AND GAME STATE");
  debug_line(a,"ALIENS       ALL 64 LOCATIONS/FLAGS");
  debug_line(a,"ALIEN N      RECORD 0..63 DETAILS");
  debug_line(a,"FLAGS        ALL 2048 RAW BITS");
  debug_line(a,"FLAG N       ONE RAW BIT");
  debug_line(a,"TIMERS       ALL BANK TIME GATES");
  debug_line(a,"TIMERS N     BANK 0..15 TIME GATES");
  debug_line(a,"ADVANCE N    ADD N GAME SECONDS");
  debug_line(a,"ADVANCE 60   ADD ONE MINUTE");
  debug_line(a,"CLOSE        RESUME GAME");
  debug_line(a,"INSPECTION PAUSES CLOCK AND INPUT");
 }else if(!strcmp(verb,"STATUS")&&fields==1)debug_status(a);
 else if(!strcmp(verb,"ADVANCE")&&fields==2&&numeric&&value<=86400u){
  uint32_t total=(uint32_t)a->minutes*60u+a->seconds+value;
  a->minutes=(uint16_t)(total/60u);a->seconds=(uint8_t)(total%60u);a->vm.minutes=a->minutes;
  a->last_clock_ms=SDL_GetTicks();debug_line(a,"ADVANCED %u SECONDS",value);debug_status(a);
 }else if((!strcmp(verb,"ALIENS")&&fields==1)||(!strcmp(verb,"ALIEN")&&fields==2&&numeric&&value<64)){
  debug_line(a,"ID BANK ACTUAL WANTED FLAGS 0..4");
  unsigned first=fields==2?value:0,last=fields==2?value+1:64;
  for(unsigned i=first;i<last;i++){
   BloodCharacter*c=&a->game.character[i];char bits[6];for(unsigned k=0;k<5;k++)bits[k]=(char)('0'+blood_game_bit(&a->game,100+5*i+k));bits[5]=0;
   debug_line(a,"%02u P%c %3u,%3u %3u,%3u %s%s",i,'A'+(i>>2),c->actual_x,c->actual_y,c->wanted_x,c->wanted_y,bits,a->game.onboard&&a->game.carried_record==i?" *":"");
  }
  debug_line(a,"BITS: ALIVE / NOT DESTROYED / AT DEST");
  debug_line(a,"      THERE / HOSTILE; * = CARRIED");
  debug_line(a,"Y HIGH BIT MARKS DESTROYED WORLD");
 }else if(!strcmp(verb,"FLAGS")&&fields==1){
  debug_line(a,"BIT RANGE: LOWEST BIT AT LEFT");
  for(unsigned i=0;i<2048;i+=32){char bits[33];for(unsigned k=0;k<32;k++)bits[k]=(char)('0'+blood_game_bit(&a->game,i+k));bits[32]=0;debug_line(a,"%04u %s",i,bits);}
 }else if(!strcmp(verb,"FLAG")&&fields==2&&numeric&&value<2048)debug_line(a,"FLAG %u = %d",value,blood_game_bit(&a->game,value));
 else if(!strcmp(verb,"TIMERS")&&(fields==1||(fields==2&&numeric&&value<16)))debug_timers(a,fields==1?-1:(int)value);
 else if(!strcmp(verb,"CLOSE")&&fields==1){a->debug_open=0;SDL_StopTextInput();a->last_clock_ms=SDL_GetTicks();}
 else debug_line(a,"INVALID COMMAND. TYPE HELP");
}
static void debug_draw(App*a)
{
 rect(a,0,0,W,H,0);blood_ui_text(a->indexed,a->font,"R32 DEBUG - SIMULATION PAUSED",0,0);
 for(unsigned i=0;i<22&&a->debug_scroll+i<a->debug_count;i++)blood_ui_text(a->indexed,a->font,a->debug_lines[a->debug_scroll+i],0,9+i*8);
 char footer[41];snprintf(footer,sizeof(footer),"LINES %u/%u  PGUP/PGDN",a->debug_scroll+1,a->debug_count);blood_ui_text(a->indexed,a->font,footer,0,185);
 char prompt[41];snprintf(prompt,sizeof(prompt),"> %.38s",a->debug_input);blood_ui_text(a->indexed,a->font,prompt,0,193);
}
static int debug_event(App*a,const SDL_Event*e)
{
 if(e->type==SDL_KEYDOWN&&e->key.keysym.sym==SDLK_F12&&!e->key.repeat){
  a->debug_open=!a->debug_open;a->last_clock_ms=SDL_GetTicks();a->mouse_buttons=a->flight_buttons=0;
  if(a->debug_open){SDL_StartTextInput();a->debug_input_len=0;a->debug_input[0]=0;debug_command(a,"HELP");}else SDL_StopTextInput();return 1;
 }
 if(!a->debug_open)return 0;
 if(e->type==SDL_QUIT)return 0;
 if(e->type==SDL_TEXTINPUT){
  for(unsigned i=0;e->text.text[i]&&i<32;i++){unsigned c=(unsigned char)e->text.text[i];if(c>=32&&c<=126&&a->debug_input_len<38){a->debug_input[a->debug_input_len++]=(char)toupper(c);a->debug_input[a->debug_input_len]=0;}}
 }else if(e->type==SDL_KEYDOWN){
  switch(e->key.keysym.sym){
  case SDLK_ESCAPE:a->debug_open=0;SDL_StopTextInput();a->last_clock_ms=SDL_GetTicks();break;
  case SDLK_RETURN:debug_command(a,a->debug_input);a->debug_input_len=0;a->debug_input[0]=0;break;
  case SDLK_BACKSPACE:if(a->debug_input_len)a->debug_input[--a->debug_input_len]=0;break;
  case SDLK_PAGEUP:a->debug_scroll=a->debug_scroll>20?a->debug_scroll-20:0;break;
  case SDLK_PAGEDOWN:if(a->debug_scroll+22<a->debug_count)a->debug_scroll+=20;break;
  default:break;
  }
 }
 return 1;
}
