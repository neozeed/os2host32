/* R32: original cockpit sprites, UpCom pictographs and alien animation.
 * Native C shell.  OS/2 PM uses r32/os2/blood_os2_pm.c directly.
 */
#ifdef BLOOD_OS2PM
#include "blood_os2_pm.h"
#else
#include <SDL2/SDL.h>
#endif
#include "blood_world.h"
#include "blood_planet_surface.h"
#include "blood_terrain.h"
#include "blood_upcom.h"
#include "blood_mouse.h"
#include "blood_ui.h"
#include "blood_globe.h"
#include "blood_effects.h"
#include "blood_conversation.h"
#ifndef BLOOD_OS2PM
#include "blood_speaker.h"
#endif
#include "blood_transition.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <ctype.h>

#ifdef BLOOD_OS2PM
#ifdef snprintf
#undef snprintf
#endif
#ifdef vsnprintf
#undef vsnprintf
#endif
static int blood_os2_vsnprintf(char *out,size_t cap,const char *fmt,va_list args)
{
 char tmp[2048];
 int n;
 size_t copy;
 if(!out||!cap)return 0;
 n=vsprintf(tmp,fmt,args);
 if(n<0){out[0]=0;return n;}
 copy=(size_t)n;
 if(copy>=cap)copy=cap-1;
 memcpy(out,tmp,copy);
 out[copy]=0;
 return n;
}
static int blood_os2_snprintf(char *out,size_t cap,const char *fmt,...)
{
 va_list args;
 int n;
 va_start(args,fmt);
 n=blood_os2_vsnprintf(out,cap,fmt,args);
 va_end(args);
 return n;
}
#define snprintf blood_os2_snprintf
#define vsnprintf blood_os2_vsnprintf
#endif

#define W 320
#define H 200
#define FLIGHT_LIMIT 20000u
static const uint8_t PAL[16][3]={{0,0,0},{0,0,170},{0,170,0},{0,170,170},{170,0,0},{170,0,170},{170,85,0},{170,170,170},{85,85,85},{85,85,255},{85,255,85},{85,255,255},{255,85,85},{255,85,255},{255,255,85},{255,255,255}};
typedef enum Scene{TITLE,ORBIT,DETAIL,BRIDGE,GALAXY,WARP,ARRIVAL,DESTROY,STATIC,LAUNCH,FLIGHT,LANDSCAPE,RELEASE,ENCOUNTER,ENDING} Scene;
typedef struct Art{uint16_t w,h;uint8_t pixels[W*H];int valid;}Art;
typedef struct App{
#ifdef BLOOD_OS2PM
 BloodPM *pm;int speaker_muted;
#else
 BloodSpeaker speaker;SDL_AudioDeviceID audio_device;int speaker_ready,audio_paused;
 SDL_Window *window;SDL_Renderer *renderer;SDL_Texture *screen;
#endif
 uint8_t indexed[W*H],font[472],palette_table[64],texture[16384],ega[64000];
 uint8_t globe_stream[BLOOD_GLOBE_STREAM_BYTES];
 BloodStar stars[512];
 BloodStarField starfield;uint8_t effect_pixels[64000],fade_pixels[64000],static_source[65536];
 uint8_t arrival_masks[28],destruction_tables[44];unsigned effect_step,effect_phase,effect_colour,effect_radius,effect_outer,fade_ticks,static_offset,static_frames;
 uint16_t effect_speed,effect_deceleration;
 uint8_t textcol, bank[65536],vocab[128][16];size_t banklen;
 uint32_t rgba[W*H],frame;uint32_t last_clock_ms,irq_accum;uint16_t minutes,service_clock;uint8_t seconds,irq_divider,service_irq_divider,title_page;Art cockpit,ending,galaxy,title[2];
 BloodGame game;BloodUpcom vm;BloodPlanetSurface appearance;
 BloodTerrain *terrain;BloodTerrainTables tables;
 uint16_t rnglo,rnghi,bankcache;uint8_t zoom,longitude,selectedx,selectedy;
 unsigned scene_ticks,flight_updates,landscape_pass;int running,changed,phase,drag_axis,has_terrain,dialogue_ready,f8_held;
 uint8_t flight_buttons;int flight_recenter;
 BloodEyes eyes;BloodHand hand;BloodTremor tremor;int tremor_x,tremor_y;BloodAlien alien;
 uint8_t sheets[5][4000],icons[6080],dictionary[2560],iconmap[128];
 uint8_t reply_icons[BLOOD_REPLY_ICONS],scan_page;
 uint8_t alien_pixels[64000],phrase[8],phrase_len,phrase_cursor,icon_base,mouse_buttons;
 int mouse_x,mouse_y;uint16_t eye_mask;uint8_t mouth_open,mouth_delay,icon_offset,reply_visible,reply_delay,reply_presenting;
 BloodSaveImage save;char save_path[512];
 unsigned ending_phase;
 unsigned fade_kind,fade_bottom,fade_step,title_cursor,title_remaining,warp_pending;
 uint8_t transition_target[64000],dos_low_memory[0x4080];
 unsigned dos_low_memory_loaded;uint32_t static_fallback_rng;
 uint32_t transition_irq_accum,static_irq_accum,flight_irq_accum;
 int debug_open;unsigned debug_count,debug_scroll,debug_input_len;
 char debug_lines[512][41],debug_input[80];
 int safe_flight;char status[120],message[256],assets[512];Scene scene;
}App;
static int path(char *out,size_t cap,const char *dir,const char *name){return snprintf(out,cap,"%s/%s",dir,name)<(int)cap;}
static int read_exact(const char *file,void *data,size_t count){FILE *f=fopen(file,"rb");int ok=0;if(!f)return 0;ok=fread(data,1,count,f)==count && fgetc(f)==EOF;fclose(f);return ok;}
static int load_art(Art *a,const char *dir,const char *name){char p[1024];FILE*f;uint8_t h[4];size_t n;
 if(!path(p,sizeof(p),dir,name))return 0;
 f=fopen(p,"rb");if(!f)return 0;
 if(fread(h,1,4,f)!=4){fclose(f);return 0;}a->w=h[0]|(h[1]<<8);a->h=h[2]|(h[3]<<8);n=(size_t)a->w*a->h;
 a->valid=a->w==W && a->h<=H && fread(a->pixels,1,n,f)==n && fgetc(f)==EOF;
 fclose(f);return a->valid;
}
/* Original numerical tables are extracted once into assets; the executable
 * is never loaded, interpreted, or even opened at R20 runtime. */
static int load_tables(App*a,const char *assets){char p[1024];unsigned i;
 if(!path(p,sizeof(p),assets,"terrain-tables.bin")||
    !read_exact(p,&a->tables,sizeof(a->tables)))return 0;
 if(!path(p,sizeof(p),assets,"palette-table.bin")||
    !read_exact(p,a->palette_table,64))return 0;
 for(i=0;i<64;i++)if(a->palette_table[i]>15)return 0;
 return 1;
}
static void rect(App*a,int x,int y,int w,int h,uint8_t color){int xx,yy;for(yy=y;yy<y+h;yy++)if(yy>=0&&yy<H)for(xx=x;xx<x+w;xx++)if(xx>=0&&xx<W)a->indexed[yy*W+xx]=color;}
static void line(App*a,int x1,int y1,int x2,int y2,uint8_t c){int dx=abs(x2-x1),sx=x1<x2?1:-1,dy=-abs(y2-y1),sy=y1<y2?1:-1,err=dx+dy,e;
 for(;;){if((unsigned)x1<W&&(unsigned)y1<H)a->indexed[y1*W+x1]=c;if(x1==x2&&y1==y2)break;e=2*err;if(e>=dy){err+=dy;x1+=sx;}if(e<=dx){err+=dx;y1+=sy;}}}
static void background(App*a,const Art*art){memset(a->indexed,0,sizeof(a->indexed));if(art->valid)memcpy(a->indexed,art->pixels,(size_t)art->w*art->h);}
static unsigned ega_pixel(const uint8_t*page,unsigned x,unsigned y){unsigned off=y*40u+x/8u,bit=128u>>(x&7u),v=0,i;for(i=0;i<4;i++)if(page[i*8000u+off]&bit)v|=1u<<i;return v;}
static void blit_ega(App*a,const uint8_t*page,int x1,int y1,int x2,int y2){int x,y;for(y=y1;y<y2;y++)for(x=x1;x<x2;x++)a->indexed[y*W+x]=(uint8_t)ega_pixel(page,(unsigned)x,(unsigned)y);}
static void debug_draw(App*a);
static void present(App*a){
#ifdef BLOOD_OS2PM
 blood_pm_present(a->pm,a->indexed);
#else
 unsigned i,c;for(i=0;i<W*H;i++){c=a->indexed[i]&15; a->rgba[i]=0xff000000u|((uint32_t)PAL[c][0]<<16)|((uint32_t)PAL[c][1]<<8)|PAL[c][2];}
 SDL_UpdateTexture(a->screen,0,a->rgba,W*4);SDL_RenderClear(a->renderer);SDL_RenderCopy(a->renderer,a->screen,0,0);SDL_RenderPresent(a->renderer);
#endif
}
static uint16_t eye_mask(App*a,Scene s){
 switch(s){
 case ORBIT:return blood_select_planet(&a->game)==1?0x1e:0x10;
 case DETAIL:return 0x12;
 case BRIDGE:return a->game.mode;
 case LANDSCAPE:return 0;
 case RELEASE:return a->game.onboard?0x200:0;
 case ENCOUNTER:return blood_game_bit(&a->game,6)==1&&!a->game.onboard&&!a->vm.end_requested?0x200:0;
 case GALAXY:return 0x10;
 case ENDING:return 0x3ff;
 default:return 0;}}
static void speaker_stop(App*a);
static void scene(App*a,Scene s){
 Scene old=a->scene;
 if(a->scene!=s)speaker_stop(a);
 a->fade_ticks=0;a->fade_kind=0;a->title_remaining=0;
 /* Original callers, not a universal fade: 58D0 enters the globe,
  * 6E44 enters the galaxy; 2A62 clears the map before hyperspace. */
 if((old==BRIDGE&&(s==ORBIT||s==GALAXY))||(old==GALAXY&&s==WARP)){
  memcpy(a->fade_pixels,a->indexed,sizeof(a->indexed));
  a->fade_ticks=9;a->fade_kind=1;a->fade_bottom=s==GALAXY?149:147;
  a->fade_step=1;a->transition_irq_accum=0;
  blood_transition_step(a->fade_pixels,0,a->fade_bottom,NULL);
 }
 a->scene=s;a->scene_ticks=0;a->phase=0;
 if(s==ENDING){a->ending_phase=0;memcpy(a->effect_pixels,a->ending.pixels,W*H);}
 a->eye_mask=s==ENDING?0:eye_mask(a,s);blood_eyes_request(&a->eyes,a->eye_mask);}
static void portrait_transition(App*a,const uint8_t *from,const uint8_t *to){
 memcpy(a->fade_pixels,from,64000);memcpy(a->transition_target,to,64000);
 a->fade_ticks=20;a->fade_kind=2;a->fade_step=0;
}
static void transition_tick(App*a,unsigned elapsed){
 BloodUpcom random;memset(&random,0,sizeof(random));random.rng_low=a->rnglo;random.rng_high=a->rnghi;
 if(a->fade_kind==1){
  a->transition_irq_accum+=elapsed*1193182u;
  /* Exactly twelve divisor-5970 IRQs per original 01F4 wait. */
  while(a->fade_ticks&&a->transition_irq_accum>=71640000u){
   a->transition_irq_accum-=71640000u;
   blood_transition_step(a->fade_pixels,a->fade_step++,a->fade_bottom,&random);a->fade_ticks--;
  }
 }else if(a->fade_kind==2){
  blood_portrait_dissolve(a->fade_pixels,a->transition_target,&random,1000);
  if(!--a->fade_ticks)blood_portrait_finish(a->fade_pixels,a->transition_target);
 }
 a->rnglo=random.rng_low;a->rnghi=random.rng_high;
}
#ifndef BLOOD_OS2PM
static void speaker_callback(void *context,uint8_t *stream,int bytes)
{blood_speaker_render(&((App*)context)->speaker,(int16_t*)stream,(size_t)bytes/2u);}
#else
static void speaker_beep(int freq,int duration)
{
 blood_pm_queue_beep(freq,duration);
}
static void speaker_pause(int duration)
{
 blood_pm_queue_beep(0,duration);
}
#endif
static void speaker_play(App*a,unsigned id){
#ifdef BLOOD_OS2PM
 if(a->speaker_muted)return;
 switch(id){
 case 0:
  speaker_beep(659,90);speaker_pause(35);speaker_beep(784,90);speaker_pause(35);
  speaker_beep(988,120);speaker_pause(45);speaker_beep(784,90);speaker_pause(35);
  speaker_beep(659,160);speaker_pause(50);speaker_beep(523,220);break;
 case 1:
  speaker_beep(110,260);speaker_pause(40);speaker_beep(73,220);break;
 case 2:
  speaker_beep(880,55);speaker_pause(20);speaker_beep(1175,55);break;
 case 4:
  speaker_beep(147,80);speaker_pause(25);speaker_beep(196,90);break;
 case 5:
  speaker_beep(740,35);speaker_pause(12);speaker_beep(880,35);break;
 case 6:
  speaker_beep(196,150);speaker_pause(35);speaker_beep(147,180);speaker_pause(35);
  speaker_beep(98,260);break;
 case 14:
  speaker_beep(523,55);speaker_pause(20);speaker_beep(659,55);speaker_pause(20);
  speaker_beep(784,75);break;
 default:break;
 }
 return;
#else
 if(!a->speaker_ready)return;
 if(a->audio_device)SDL_LockAudioDevice(a->audio_device);
 blood_speaker_play(&a->speaker,id);
 if(a->audio_device)SDL_UnlockAudioDevice(a->audio_device);
#endif
}
static void speaker_stop(App*a){
#ifdef BLOOD_OS2PM
 (void)a;
 return;
#else
 if(!a->speaker_ready)return;
 if(a->audio_device)SDL_LockAudioDevice(a->audio_device);
 blood_speaker_stop(&a->speaker);
 if(a->audio_device)SDL_UnlockAudioDevice(a->audio_device);
#endif
}
static void speaker_flight_events(App*a,unsigned mask,unsigned last){
#ifdef BLOOD_OS2PM
 if(mask&2u)blood_pm_queue_beep(147,120);
 if(mask&4u)blood_pm_queue_beep(880,70);
 if(last==2)blood_pm_queue_beep(147,120);
 else if(last==3)blood_pm_queue_beep(880,70);
 (void)a;
 return;
#else
 if(!a->speaker_ready)return;
 if(a->audio_device)SDL_LockAudioDevice(a->audio_device);
 if(last)mask&=~(1u<<(last-1u));
 if(mask&1u)blood_speaker_stop(&a->speaker);
 if(mask&2u)blood_speaker_play(&a->speaker,4);
 if(mask&4u)blood_speaker_play(&a->speaker,2);
 if(last==1)blood_speaker_stop(&a->speaker);
 else if(last)blood_speaker_play(&a->speaker,last==2?4:2);
 if(a->audio_device)SDL_UnlockAudioDevice(a->audio_device);
#endif
}
static void speaker_mute(App*a){
#ifdef BLOOD_OS2PM
 a->speaker_muted=!a->speaker_muted;
 blood_pm_set_title(a->pm,a->speaker_muted?"Captain Blood - beeper muted (F9)":"Captain Blood - beeper on (F9)");
#else
 int muted;
 if(a->audio_device)SDL_LockAudioDevice(a->audio_device);
 a->speaker.muted=!a->speaker.muted;
 muted=a->speaker.muted;
 if(a->audio_device)SDL_UnlockAudioDevice(a->audio_device);
 SDL_SetWindowTitle(a->window,muted?"Captain Blood - speaker muted (F9)":"Captain Blood - speaker on (F9)");
#endif
}
static int speaker_load(App*a){
#ifdef BLOOD_OS2PM
 (void)a;
 return 1;
#else
 char p[1024];uint8_t programs[1623],tables[BLOOD_SOUND_TABLE_BYTES];
 if(!path(p,sizeof(p),a->assets,"SOUND.BIN")||!read_exact(p,programs,sizeof(programs))||programs[0]!=253)return 0;
 if(!path(p,sizeof(p),a->assets,"speaker-tables.bin")||!read_exact(p,tables,sizeof(tables)))return 0;
 if(blood_speaker_init(&a->speaker,programs+7,1616,tables,sizeof(tables),48000))return 0;
 a->speaker_ready=1;return 1;
#endif
}
static void speaker_open(App*a){
#ifdef BLOOD_OS2PM
 (void)a;
 return;
#else
 SDL_AudioSpec want,got;memset(&want,0,sizeof(want));
 if(SDL_InitSubSystem(SDL_INIT_AUDIO)){fprintf(stderr,"Speaker audio unavailable: %s\n",SDL_GetError());return;}
 want.freq=48000;want.format=AUDIO_S16SYS;want.channels=1;want.samples=512;want.callback=speaker_callback;want.userdata=a;
 a->audio_device=SDL_OpenAudioDevice(NULL,0,&want,&got,SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
 if(!a->audio_device){fprintf(stderr,"Speaker audio unavailable: %s\n",SDL_GetError());return;}
 if(got.freq<8000||got.freq>192000){SDL_CloseAudioDevice(a->audio_device);a->audio_device=0;return;}
 a->speaker.sample_rate=(uint32_t)got.freq;SDL_PauseAudioDevice(a->audio_device,0);
#endif
}
static void speaker_pause_sync(App*a){
#ifdef BLOOD_OS2PM
 (void)a;
 return;
#else
 if(a->audio_device&&a->audio_paused!=a->debug_open){
  a->audio_paused=a->debug_open;SDL_PauseAudioDevice(a->audio_device,a->audio_paused);
 }
#endif
}
static int load_ui(App*a){
 static const char *names[5]={"MAIN.EGS","BOUT1.EGS","BOUT2.EGS","BOUF1.EGS","BOUF2.EGS"};
 char p[1024];uint8_t b[6088];FILE *f;unsigned i;size_t n;
 for(i=0;i<5;i++){
  if(!path(p,sizeof(p),a->assets,names[i])||!(f=fopen(p,"rb")))return 0;
  n=fread(b,1,sizeof(b),f);fclose(f);
  if(n!=4008||b[0]!=0xfd)return 0;
  memcpy(a->sheets[i],b+7,4000);
 }
 if(!path(p,sizeof(p),a->assets,"IKONES.EGX")||!read_exact(p,b,6087)||b[0]!=0xfd)return 0;
 memcpy(a->icons,b+7,6080);
 if(!path(p,sizeof(p),a->assets,"DICOGB.BIN")||!read_exact(p,b,2567)||b[0]!=0xfd)return 0;
 memcpy(a->dictionary,b+7,2560);
 if(!path(p,sizeof(p),a->assets,"save-template.bin")||!read_exact(p,&a->save,sizeof(a->save)))return 0;
 if(!path(p,sizeof(p),a->assets,"globe-stream.bin")||!read_exact(p,a->globe_stream,sizeof(a->globe_stream)))return 0;
 if(!path(p,sizeof(p),a->assets,"arrival-masks.bin")||!read_exact(p,a->arrival_masks,sizeof(a->arrival_masks)))return 0;
 if(!speaker_load(a))return 0;
 if(!path(p,sizeof(p),a->assets,"DOSLOW.BIN"))return 0;
 {FILE *low=fopen(p,"rb");if(low){
   size_t count=fread(a->dos_low_memory,1,sizeof(a->dos_low_memory),low);fclose(low);
   if(count!=sizeof(a->dos_low_memory))return 0;
   a->dos_low_memory_loaded=1;
  }}
 if(!path(p,sizeof(p),a->assets,"destruction-tables.bin")||!read_exact(p,a->destruction_tables,sizeof(a->destruction_tables)))return 0;
 return 1;
}
static void choose_world(App*a,uint8_t x,uint8_t y){a->game.planet_x=x;a->game.planet_y=y;a->selectedx=x;a->selectedy=y;a->changed=0;blood_select_planet(&a->game);
 if(blood_planet_surface_generate(x,y,a->palette_table,64,a->texture,sizeof(a->texture),&a->appearance))snprintf(a->status,sizeof(a->status),"SURFACE GENERATION FAILED");
 blood_stars_init(a->stars,y);
 a->longitude=128;a->zoom=0;memset(a->ega,0,sizeof(a->ega));}
static void new_game(App*a){char p[1024];memset(&a->game,0,sizeof(a->game));a->game.active_record=BLOOD_NO_CHARACTER;a->game.carried_record=BLOOD_NO_CHARACTER;
 a->tremor.delay_x=1;a->tremor.delay_y=1;a->tremor.delta_x=4;a->tremor.delta_y=2;a->tremor_x=a->tremor_y=0;
 a->game.mode=0xe0;a->game.grace_minutes=120;a->minutes=0;a->seconds=0;a->irq_accum=0;a->irq_divider=0;a->service_clock=0;a->service_irq_divider=0;
#ifdef BLOOD_OS2PM
 a->last_clock_ms=blood_pm_ticks();
#else
 a->last_clock_ms=SDL_GetTicks();
#endif
 a->rnglo=0x1341;a->rnghi=0x2a15;a->bankcache=0xffff;
 /* The original dictionary prefix initializes the 2048 status bits. */
 if(path(p,sizeof(p),a->assets,"dictionary-prefix.bin"))read_exact(p,a->game.flags,256);
 if(blood_generate_galaxy(&a->game,&a->rnglo,&a->rnghi,&a->bankcache)) {snprintf(a->status,sizeof(a->status),"GALAXY GENERATOR FAILED");return;}
 /* DICOGB.BIN's exact 256-byte prefix already contains initial character
  * alive/present/hostile bits. Do NOT mark every record present. */
 choose_world(a,a->game.planet_x,a->game.planet_y);scene(a,ORBIT);
}
static void text_phrase(App*a,const uint8_t*words,char*out,size_t cap){size_t p=0,n;unsigned i,v;char str[32];out[0]=0;for(i=0;i<32&&words[i]!=255;i++){
 v=words[i];if(v>3&&v<128)snprintf(str,sizeof(str),"%s",a->vocab[v]);else snprintf(str,sizeof(str),"[%u]",v);
 n=strlen(str);if(p+n+2>=cap)break;if(p)out[p++]=' ';memcpy(out+p,str,n);p+=n;out[p]=0;if(v<=3)i+=2;}}
static void sync_to_vm(App*a){memcpy(a->vm.flags,a->game.flags,256);a->vm.rng_low=a->rnglo;a->vm.rng_high=a->rnghi;a->vm.minutes=a->minutes;}
static void sync_from_vm(App*a){memcpy(a->game.flags,a->vm.flags,256);a->rnglo=a->vm.rng_low;a->rnghi=a->vm.rng_high;}
static int encounter_begin(App*a){char name[32],p[1024];FILE*f;size_t n;unsigned bank=a->game.active_bank;
 unsigned x,y;
 a->dialogue_ready=0;if(bank>=16||!blood_can_converse(&a->game))return 0;
 /* The encounter entry reseeds before loading/initializing its alien bank.
  * Inheriting the flight RNG changed force/rejection flags after HOWDY. */
 blood_conversation_seed(a->minutes,a->seconds,a->service_clock,&a->rnglo,&a->rnghi);
 snprintf(name,sizeof(name),"P%c.EGP",'A'+bank);if(!path(p,sizeof(p),a->assets,name))return 0;f=fopen(p,"rb");if(!f)return 0;
 n=fread(a->bank,1,sizeof(a->bank),f);fclose(f);if(n<30||n>=sizeof(a->bank)||a->bank[0]!=0xfd)return 0;
 memset(&a->vm,0,sizeof(a->vm));sync_to_vm(a);
 if(blood_upcom_attach(&a->vm,a->bank+7,n-7)||blood_upcom_encounter(&a->vm,a->game.active_record&3,a->game.character[a->game.active_record].wanted_y))return 0;
 a->dialogue_ready=1;
 a->banklen=n-7;memcpy(a->iconmap,a->dictionary+256,128);
 if(blood_ui_available(a->iconmap,a->dictionary+384,a->bank+7,a->banklen))return 0;
 memset(a->phrase,0,sizeof(a->phrase));a->phrase_len=0;a->phrase_cursor=0;a->icon_base=2;a->icon_offset=0;
 memcpy(a->alien_pixels,a->cockpit.pixels,W*H);
 /* Preserve the completed native planet render before the alien caches its
  * background. Redrawing the cockpit here erased the landing landscape. */
 if(a->has_terrain)for(y=20;y<146;y++)for(x=32;x<288;x++)
  a->alien_pixels[y*W+x]=(uint8_t)ega_pixel(a->ega,x,y);
 if(blood_alien_attach(&a->alien,a->bank+7,a->banklen))return 0;
 a->alien.rng_low=a->rnglo;a->alien.rng_high=a->rnghi;
 if(blood_alien_step(&a->alien,a->alien_pixels))return 0;
 a->rnglo=a->alien.rng_low;a->rnghi=a->alien.rng_high;
 /* Preserve 3F97's encounter-local flags, including slot + 1 and hostility.
  * Copying game flags here erased them and skipped the alien's dialogue path. */
 a->vm.rng_low=a->rnglo;a->vm.rng_high=a->rnghi;
 if(blood_conversation_begin(&a->vm,a->dictionary,&a->game,a->reply_icons)<0)return 0;
 sync_from_vm(a);
 a->mouth_open=0;a->mouth_delay=8;a->reply_visible=0;a->reply_delay=1;a->reply_presenting=1;blood_alien_speech(&a->alien,1);
 a->message[0]=0;if(a->vm.active)text_phrase(a,a->vm.reply,a->message,sizeof(a->message));return 1;
}
static void dialogue_submit(App*a){
 int continuing;
 if(!a->dialogue_ready)return;
 speaker_stop(a);
 if(a->vm.end_requested){scene(a,BRIDGE);return;}
 continuing=a->vm.active;
 sync_to_vm(a);
 if(blood_conversation_click(&a->vm,a->phrase,a->dictionary,&a->game,a->reply_icons)<0){strcpy(a->status,"UPCOM INTERPRETER ERROR");return;}
 sync_from_vm(a);
 if(!continuing&&a->phrase_len){a->phrase_len=0;a->phrase_cursor=0;}
 a->message[0]=0;
 if(a->vm.active)text_phrase(a,a->vm.reply,a->message,sizeof(a->message));
 a->reply_visible=0;a->reply_delay=1;a->reply_presenting=1;blood_alien_speech(&a->alien,1);
}
static void enter_bridge(App*a){int access=blood_select_planet(&a->game);a->game.mode=blood_console_after_planet(a->game.mode,access,blood_was_visited(&a->game)==1);scene(a,BRIDGE);}
static void terrain_rng(App*a);
static void fly_start(App*a,int shortcut){a->flight_irq_accum=0;a->flight_buttons=0;a->flight_recenter=1;a->has_terrain=0;a->flight_updates=0;a->landscape_pass=0;memset(a->ega,0,sizeof(a->ega));if(!a->terrain)return;
 if(shortcut){if(blood_terrain_shortcut(a->terrain,&a->tables,a->game.planet_x,a->game.planet_y))return;
  if(blood_terrain_finish(a->terrain,&a->tables,a->ega,BLOOD_TERRAIN_EGA_BYTES))return;
  terrain_rng(a);a->has_terrain=1;scene(a,LANDSCAPE);return;}
 if(blood_terrain_init(a->terrain,&a->tables,a->game.planet_x,a->game.planet_y)||blood_terrain_prepare(a->terrain,&a->tables))return;
 if(a->safe_flight)a->terrain->mem[BT_NO_COLLISION]=1; /* assisted test only */
 terrain_rng(a);scene(a,FLIGHT);a->has_terrain=1;
}
static void launch_start(App*a){scene(a,LAUNCH);a->flight_buttons=0;}
/* Original 4BA4 descent: heights 2,6,..66, then 64,66,68 (tube bob). */
static unsigned oorx_height(unsigned tick){
 if(tick<17)return 2+4*tick;
 return tick==17?64:tick==18?66:68;
}
/* Logical coordinates already contain DOS X/2. Recenter once per actual
 * terrain update, not once per event-loop iteration. Hardware timing remains
 * the inherited prototype cadence. */
static void recenter_flight(App*a){
#ifdef BLOOD_OS2PM
 blood_pm_warp_mouse(a->pm,160,175);
#else
 int x,y;
 if(!a->window||!a->renderer)return;
 SDL_RenderLogicalToWindow(a->renderer,160.0f,175.0f,&x,&y);
 SDL_WarpMouseInWindow(a->window,x,y);
#endif
}
static BloodFlightInput control(App*a,int mx,int my){
 BloodFlightInput in=blood_mouse_flight(mx,my,a->flight_buttons);
 return in;
}
static void terrain_rng(App*a){a->rnglo=a->terrain->rng_low;a->rnghi=a->terrain->rng_high;}
/* Synthetic missing-memory fallback, independent of the DOS gameplay RNG. */
static void static_fallback(App*a){
 unsigned i;
 if(a->dos_low_memory_loaded)return;
#ifdef BLOOD_OS2PM
 if(!a->static_fallback_rng)a->static_fallback_rng=blood_pm_ticks()^0x9e3779b9u;
#else
 if(!a->static_fallback_rng)a->static_fallback_rng=SDL_GetTicks()^0x9e3779b9u;
#endif
 if(!a->static_fallback_rng)a->static_fallback_rng=1;
 for(i=0;i<sizeof(a->dos_low_memory);i++){
  uint32_t v=a->static_fallback_rng;v^=v<<13;v^=v>>17;v^=v<<5;
  a->static_fallback_rng=v;a->dos_low_memory[i]=(uint8_t)v;
 }
}
static void failed_flight(App*a){
 BloodUpcom random;
 memcpy(a->static_source,a->terrain->mem,sizeof(a->static_source));
 memset(&random,0,sizeof(random));random.rng_low=a->rnglo;random.rng_high=a->rnghi;
 blood_static_init(a->static_source,&random);a->rnglo=random.rng_low;a->rnghi=random.rng_high;
 a->static_offset=blood_upcom_random(&random)>>8;a->rnglo=random.rng_low;a->rnghi=random.rng_high;
 static_fallback(a);a->static_frames=0;a->static_irq_accum=0;scene(a,STATIC);speaker_play(a,1);
}
static void tick_flight(App*a,int mx,int my){unsigned sounds=0;BloodFlightInput in;int rc=0;if(!a->has_terrain)return;
 in=control(a,mx,my);
 if(a->phase==0){rc=blood_terrain_step(a->terrain,&a->tables,&in,(uint8_t)(a->safe_flight?0:a->game.hostile_flight),a->ega,32000,&sounds);recenter_flight(a);terrain_rng(a);
  /* Terrain returns original 3B68/3B87 events in execution order. */
  speaker_flight_events(a,sounds,a->terrain->sound_event);
  if(rc==2||a->terrain->failed){failed_flight(a);return;}
  if(rc==1){a->flight_buttons=0;return;}
  if(blood_terrain_word(a->terrain,BT_PROGRESS)>=180){if(blood_terrain_landing_begin(a->terrain))return;speaker_stop(a);a->phase=1;}
 }else{rc=blood_terrain_landing_step(a->terrain,&a->tables,&in,a->ega,32000);recenter_flight(a);terrain_rng(a);if(rc==1){a->phase=2;
   if(blood_terrain_finish(a->terrain,&a->tables,a->ega,32000))return;
   speaker_stop(a);a->has_terrain=1;a->landscape_pass=0;scene(a,LANDSCAPE);}}
 if(rc<0){snprintf(a->status,sizeof(a->status),"TERRAIN UPDATE ERROR");scene(a,BRIDGE);}terrain_rng(a);a->flight_updates++;
 if(a->flight_updates>FLIGHT_LIMIT)scene(a,BRIDGE);
}
/* C library landscape currently exposes an entire loop with callback. Capture
 * all 128 ORIGINAL intermediate indexed frames once and present incrementally. */
static uint8_t *landframes=NULL;
static void land_cb(void*ctx,const uint8_t*page,unsigned layer,unsigned slice){unsigned idx=16*layer+slice;unsigned x,y;uint8_t*dst;(void)ctx;if(idx<128&&landframes){dst=landframes+idx*W*H;
 for(y=20;y<146;y++)for(x=32;x<288;x++)dst[y*W+x]=(uint8_t)ega_pixel(page,x,y);}}
static void ensure_landscape(App*a){if(!a->has_terrain||a->phase==7)return;
 if(!landframes)landframes=(uint8_t*)malloc(128u*W*H);
 if(!landframes)return;
 memset(landframes,0,128u*W*H);if(blood_terrain_landscape(a->terrain,a->ega,32000,land_cb,a))return;
 terrain_rng(a);a->phase=7;a->landscape_pass=0;(void)blood_record_visit(&a->game);
}
static void separator(App*a,int x,int y,int end){line(a,x,y-1,end,y-1,1);line(a,x,y,end,y,7);line(a,x,y+1,end,y+1,1);}
static void draw_upcom(App*a){
 unsigned i,y,token,ruler;
 int px,py;
 char label[14];
 memcpy(a->indexed,a->alien_pixels,W*H);
 rect(a,48,135,224,41,0);rect(a,176,110,128,22,0);rect(a,16,110,128,22,0);
 separator(a,16,108,144);separator(a,16,133,144);separator(a,176,108,304);separator(a,176,133,304);
 separator(a,51,176,269);separator(a,51,177,269);
 {
  uint8_t grid[64000];memset(grid,0,sizeof(grid));
  for(i=0;i<30;i++){
   unsigned id=a->icon_base+i-(a->icon_offset?2:0);
   int gx=48+(int)(i/2)*16-(a->icon_offset?8:0);
   if(id<128)blood_ui_icon(grid,a->icons,id,a->iconmap[id]&128?8:15,(unsigned)gx,137+(i&1)*19);
  }
  for(y=137;y<175;y++)memcpy(a->indexed+y*320+48,grid+y*320+48,224);
 }
 for(i=0;i<a->phrase_len;i++)blood_ui_icon(a->indexed,a->icons,a->phrase[i],15,176+i*16,112);
 for(i=0;i<a->reply_visible&&i<8;i++)
  if(a->reply_icons[i]<128)blood_ui_icon(a->indexed,a->icons,a->reply_icons[i],15,16+i*16,112);
 px=a->mouse_x+3;py=a->mouse_y+1;token=255;
 if(px>=48&&px<272&&py>=137&&py<175){unsigned id=a->icon_base+(((unsigned)(px/8-6)-(unsigned)a->icon_offset)&~1u)+(py>=156);if(id<128)token=a->iconmap[id]&127u;}
 if(px>=176&&px<304&&py>=112&&py<130){unsigned at=(unsigned)(px-176)/16;if(at<a->phrase_len)token=a->iconmap[a->phrase[at]]&127u;}
 if(px>=16&&px<144&&py>=112&&py<130)token=blood_conversation_hover(a->reply_icons,a->reply_visible,a->iconmap,px,py);
 memset(label,' ',13);label[13]=0;
 if(token<128){size_t len=strlen((char*)a->vocab[token]);if(len>13)len=13;memcpy(label,a->vocab[token],len);}
 blood_ui_text(a->indexed,a->font,label,204,96);
 if(a->phrase_cursor<8)blood_ui_sprite(a->indexed,a->sheets[0],4000,183,5,176+a->phrase_cursor*16,131);
 blood_ui_sprite(a->indexed,a->sheets[2],4000,a->mouth_open?48:75,28,144,107);
 /* Original movable ruler: base*3/2, rounded down to four pixels. */
 ruler=((a->icon_base*3/2)&~3u)+72u;
 rect(a,(int)ruler,190,33,6,7);
}
static void draw_scene(App*a){unsigned x,y,yy,n,sheet;int inset,left,top,right,bottom;uint8_t c;uint16_t visible;char digits[16];
 switch(a->scene){
 case TITLE:if(a->title_remaining)memcpy(a->indexed,a->fade_pixels,64000);else background(a,&a->title[a->title_page]);break;
 case ORBIT:case DETAIL:case ARRIVAL:
  background(a,&a->cockpit);rect(a,0,20,320,126,0);
  if(a->scene!=DETAIL){
   blood_stars_draw(a->indexed,a->stars);
   if(blood_select_planet(&a->game)==1){
    uint8_t filtered[BLOOD_GLOBE_STREAM_BYTES];const uint8_t *stream=a->globe_stream;size_t n=sizeof(a->globe_stream);
    if(a->scene==ARRIVAL){unsigned stage=a->scene_ticks/3;if(stage>13)stage=13;
     n=blood_globe_filter(stream,n,(uint16_t)(a->arrival_masks[stage*2]|(a->arrival_masks[stage*2+1]<<8)),filtered);stream=filtered;}
    blood_globe_draw(a->indexed,a->texture,stream,n,a->longitude);
   }
  }
  else{memset(a->ega,0,sizeof(a->ega));blood_planet_detail(a->texture,16384,a->ega,64000,a->zoom,a->longitude);blit_ega(a,a->ega+a->scan_page*32000u,0,21,320,145);}
  break;
 case BRIDGE:case LAUNCH:
  background(a,&a->cockpit);blood_oorx_draw(a->indexed,a->cockpit.pixels,(const uint8_t (*)[4000])a->sheets,oorx_height(a->scene_ticks));
  if(a->game.onboard)blood_passenger_draw(a->indexed,a->save.passenger_image);
  break;
 case GALAXY:
  background(a,&a->cockpit);
  if(a->galaxy.valid)for(yy=0;yy<a->galaxy.h&&yy+20<148;yy++)memcpy(a->indexed+(yy+20)*W,a->galaxy.pixels+yy*W,W);
  x=37u+a->selectedx;y=20u+a->selectedy;
  line(a,(int)x,x<=86||x>=234?39:20,(int)x,147,15);line(a,y<39&&y>=24?87:0,(int)y,y<39&&y>=24?235:319,(int)y,15);
  {char coords[8];int hx=a->mouse_x-33,hy=a->mouse_y-19;
   if(hx<0)hx=0;
   if(hx>255)hx=255;
   if(hy<0)hy=0;
   if(hy>127)hy=127;
   snprintf(coords,sizeof(coords),"%3u%c%3u",(unsigned)hx,1,(unsigned)hy);blood_ui_text(a->indexed,a->font,coords,8,28);
   snprintf(coords,sizeof(coords),"%3u%c%3u",a->selectedx,1,a->selectedy);blood_ui_text(a->indexed,a->font,coords,240,28);
  }break;
 case WARP:case DESTROY:
  background(a,&a->cockpit);for(yy=20;yy<146;yy++)memcpy(a->indexed+yy*320,a->effect_pixels+yy*320,320);break;
 case STATIC:background(a,&a->cockpit);blood_static_draw(a->indexed,a->static_frames==0?a->static_source:a->dos_low_memory+0x80,a->static_offset);break;
 case FLIGHT:background(a,&a->cockpit);blit_ega(a,a->ega,32,20,288,146);break;
 case RELEASE:case LANDSCAPE:
  background(a,&a->cockpit);
  if((a->phase==7||a->scene==RELEASE)&&landframes){n=a->landscape_pass<128?a->landscape_pass:127;
   for(yy=20;yy<146;yy++)memcpy(a->indexed+yy*W+32,landframes+n*W*H+yy*W+32,256);}
  /* Original 742C/26B1: grey frame between two blue outlines. */
  for(inset=-1;inset<=1;inset++){
   c=(uint8_t)(inset?1:7);left=30+inset;top=18+inset;right=289-inset;bottom=147-inset;
   line(a,left,top,right,top,c);line(a,left,bottom,right,bottom,c);
   line(a,left,top,left,bottom,c);line(a,right,top,right,bottom,c);
  }
  break;
 case ENCOUNTER:draw_upcom(a);break;
 case ENDING:memcpy(a->indexed,a->effect_pixels,W*H);break;
 }
 if(a->scene!=TITLE&&a->scene!=ENDING){
  snprintf(digits,sizeof(digits),"%3u",a->game.planet_x);blood_ui_text(a->indexed,a->font,digits,128,6);
  snprintf(digits,sizeof(digits),"%3u",a->game.planet_y);blood_ui_text(a->indexed,a->font,digits,168,6);
  if(a->scene!=ORBIT&&a->scene!=DETAIL&&a->scene!=GALAXY&&a->scene!=ARRIVAL&&a->scene!=WARP&&a->scene!=DESTROY&&a->scene!=STATIC){
   snprintf(digits,sizeof(digits),"%4u",a->minutes%10000);blood_ui_text(a->indexed,a->font,digits,136,22);
   snprintf(digits,sizeof(digits),"%02u",a->seconds);blood_ui_text(a->indexed,a->font,digits,176,22);
  }
  visible=a->scene==ENCOUNTER?0x200:
   a->scene==BRIDGE||a->scene==LAUNCH?0x3ff:0x2fe;
  blood_eyes_draw_mask(a->indexed,&a->eyes,(const uint8_t (*)[4000])a->sheets,visible);
  /* The ordinary central eye is BOUF1 row 120; map travel alone uses BOUT1. */
  if(a->scene!=ENCOUNTER&&a->scene!=RELEASE){
   sheet=a->scene==GALAXY?1:3;
   blood_ui_sprite(a->indexed,a->sheets[sheet],4000,120,24,144,152);
  }
  /* Original viewport frame from 28F4/5ABA. */
  if(a->scene==ORBIT||a->scene==DETAIL||a->scene==ARRIVAL||a->scene==WARP||a->scene==GALAXY||a->scene==DESTROY){
   blood_transition_step(a->indexed,9,a->scene==GALAXY?149:147,NULL);
  }
  /* The hand overlays both the viewport and its separator. */
  if(a->scene!=WARP&&a->scene!=LAUNCH&&a->scene!=FLIGHT&&a->scene!=ARRIVAL&&a->scene!=DESTROY&&a->scene!=STATIC)
   blood_hand_draw(a->indexed,a->sheets[0],&a->hand,a->mouse_x,a->mouse_y,a->scene!=GALAXY);
 }
 if(a->fade_ticks)memcpy(a->indexed,a->fade_pixels,64000);
 if(a->debug_open)debug_draw(a);
}
static void save_word(uint8_t *g,unsigned dos,unsigned value){unsigned p=dos-0x373;g[p]=(uint8_t)value;g[p+1]=(uint8_t)(value>>8);}
static unsigned load_word(const uint8_t *g,unsigned dos){unsigned p=dos-0x373;return g[p]+256u*g[p+1];}
static int disk_control(App*a){
 uint8_t data[BLOOD_SAVE_FILE_SIZE],*g=a->save.global_block;
 unsigned onboard,carried,secs;
 FILE *f;
 int ok;
 if(a->minutes<5){
  BloodSaveImage in;if(!read_exact(a->save_path,data,sizeof(data))||blood_save_decode(&in,data,sizeof(data)))return 0;
  g=in.global_block;
  onboard=g[0x79f-0x373];carried=load_word(g,0x7a0);secs=load_word(g,0x79b);
  if(onboard>1||(onboard&&carried>=64)||secs>=60||load_word(g,0x778)>510||load_word(g,0x780)>254)return 0;
  a->save=in;g=a->save.global_block;
  memcpy(a->game.character,g+2,256);memcpy(a->game.visited_xy,g+0x102,256);memcpy(a->game.destroyed_xy,g+0x202,512);
  memcpy(a->game.flags,a->save.dictionary_prefix,256);
#define LOAD_FIELD(field,off) a->game.field=(uint16_t)load_word(g,off)
  LOAD_FIELD(destroyed_write,0x778);LOAD_FIELD(destroyed_count,0x77a);LOAD_FIELD(visited_write,0x780);
  LOAD_FIELD(active_record,0x77c);LOAD_FIELD(active_bank,0x77e);LOAD_FIELD(carried_record,0x7a0);
  LOAD_FIELD(mode,0x7a2);LOAD_FIELD(pending_extension,0x797);LOAD_FIELD(grace_minutes,0x79d);
#undef LOAD_FIELD
  a->game.planet_x=g[0x775-0x373];a->game.planet_y=g[0x776-0x373];a->game.onboard=(uint8_t)onboard;
  a->minutes=(uint16_t)load_word(g,0x799);a->seconds=(uint8_t)secs;
  choose_world(a,a->game.planet_x,a->game.planet_y);scene(a,BRIDGE);return 1;
 }
 memcpy(g+2,a->game.character,256);memcpy(g+0x102,a->game.visited_xy,256);memcpy(g+0x202,a->game.destroyed_xy,512);
 memcpy(a->save.dictionary_prefix,a->game.flags,256);
#define SAVE_FIELD(field,off) save_word(g,off,a->game.field)
 SAVE_FIELD(destroyed_write,0x778);SAVE_FIELD(destroyed_count,0x77a);SAVE_FIELD(visited_write,0x780);
 SAVE_FIELD(active_record,0x77c);SAVE_FIELD(active_bank,0x77e);SAVE_FIELD(carried_record,0x7a0);
 SAVE_FIELD(mode,0x7a2);SAVE_FIELD(pending_extension,0x797);SAVE_FIELD(grace_minutes,0x79d);
#undef SAVE_FIELD
 g[0x775-0x373]=a->game.planet_x;g[0x776-0x373]=a->game.planet_y;g[0x79f-0x373]=a->game.onboard;
 save_word(g,0x799,a->minutes);save_word(g,0x79b,a->seconds);
 if(blood_save_encode(&a->save,data,sizeof(data)))return 0;
 f=fopen(a->save_path,"wb");if(!f)return 0;
 ok=fwrite(data,1,sizeof(data),f)==sizeof(data);if(fclose(f))ok=0;return ok?2:0;
}
static void upcom_pulse(App*a,int x,int y);
static void warp_start(App*a);
static void destroy_start(App*a);
static void pointer_position(App*a,int *x,int *y){
 int min;
 *x=*x<8?8:*x>288?288:*x;
 min=a->scene==GALAXY?19:a->scene==ENCOUNTER?112:a->scene==BRIDGE?136:108;
 *y=*y<min?min:*y>196?196:*y;
}
static void press(App*a,int x,int y){
 int hit;
 x+=a->tremor_x;y+=a->tremor_y;
 pointer_position(a,&x,&y);a->mouse_x=x;a->mouse_y=y;
 if(a->scene==TITLE){new_game(a);return;}
 if(a->scene==ENCOUNTER){
  int px=x+3,py=y+1;
  if(px>=48&&px<272&&py>=137&&py<175&&a->phrase_len<8){
   unsigned id=a->icon_base+(((unsigned)(px/8-6)-(unsigned)a->icon_offset)&~1u)+(py>=156);
   if(id<128&&!(a->iconmap[id]&128)){
    {unsigned i;for(i=a->phrase_len;i>a->phrase_cursor;i--)a->phrase[i]=a->phrase[i-1];}
    a->phrase[a->phrase_cursor]=(uint8_t)id;if(a->phrase_cursor<7)a->phrase_cursor++;a->phrase_len++;
   }
  }else upcom_pulse(a,x,y);
  return;
 }
 hit=blood_console_hit((uint16_t)x,(uint16_t)y,a->mouse_buttons&2?0x40:0x80,a->eyes.state);
 if(a->eyes.busy)return;
 switch(a->scene){
 case ORBIT:
  if(hit==6)launch_start(a);
  else if(hit==7)destroy_start(a);
  else if(hit==8){
   blood_select_planet(&a->game);
   blood_planet_detail_markers(a->game.planet_x,a->game.hostile_flight,a->texture,sizeof(a->texture),&a->rnglo,&a->rnghi);
   a->zoom=0;a->scan_page=0;scene(a,DETAIL);
  }
  else if(hit==5||hit==255)enter_bridge(a);
  break;
 case DETAIL:if(hit==8)a->zoom^=1;else if(hit==5||hit==255){blood_planet_clear_markers(a->texture,sizeof(a->texture));scene(a,ORBIT);}break;
 case BRIDGE:case LANDSCAPE:
  if(a->scene==LANDSCAPE&&a->landscape_pass<128)break;
  if(hit==4){a->selectedx=a->game.planet_x;a->selectedy=a->game.planet_y;a->changed=0;scene(a,GALAXY);}
  else if(hit==3)scene(a,ORBIT);
  else if(hit==9&&blood_was_visited(&a->game)==1)fly_start(a,1);
  else if(hit==1&&a->game.onboard){
   memcpy(a->transition_target,a->indexed,64000);
   {unsigned yy;for(yy=40;yy<110;yy++)memcpy(a->transition_target+yy*320+40,a->cockpit.pixels+yy*320+40,80);}
   blood_disintegrate_aboard(&a->game);scene(a,BRIDGE);
   portrait_transition(a,a->indexed,a->transition_target);
  }
  else if(hit==2){int ok=disk_control(a);
#ifdef BLOOD_OS2PM
   blood_pm_set_title(a->pm,ok?(ok==1?"Captain Blood - loaded BLOOD-R32.GAM":"Captain Blood - saved BLOOD-R32.GAM"):"Captain Blood - cannot read/write BLOOD-R32.GAM");
#else
   SDL_SetWindowTitle(a->window,ok?(ok==1?"Captain Blood - loaded BLOOD-R32.GAM":"Captain Blood - saved BLOOD-R32.GAM"):"Captain Blood - cannot read/write BLOOD-R32.GAM");
#endif
  }
  break;
 case RELEASE:
  if(hit==0&&a->game.onboard){
   if(!blood_release_aboard(&a->game)){a->game.mode&=255u;if(encounter_begin(a))scene(a,ENCOUNTER);else enter_bridge(a);}
  }else if(hit==255)enter_bridge(a);
  break;
 case GALAXY:if(hit==5){if(a->changed)warp_start(a);else enter_bridge(a);}else if(hit==255&&y>=168)enter_bridge(a);break;
 default:break;
 }
}
static void upcom_pulse(App*a,int x,int y){
 int hit;
 int px=x+3,py=y+1;
 if(px>=144&&px<176&&py>=107&&py<135){dialogue_submit(a);return;}
 if(px>=280&&px<300&&py>=156&&py<168){enter_bridge(a);return;}
 if(px>=280&&px<300&&py>=136&&py<148&&a->phrase_cursor){
  unsigned at=a->phrase_cursor==7&&a->phrase_len==8?7:--a->phrase_cursor;if(a->phrase_len){a->phrase_len--;memmove(a->phrase+at,a->phrase+at+1,a->phrase_len-at);a->phrase[a->phrase_len]=0;}return;
 }
 if(px>=176&&px<304&&py>=112&&py<137){
  unsigned at=(unsigned)(px-176)/16;a->phrase_cursor=(uint8_t)(at<a->phrase_len?at:a->phrase_len);return;
 }
 hit=blood_console_hit((uint16_t)x,(uint16_t)y,0x80,a->eyes.state);
 if(hit==0&&!a->vm.end_requested&&blood_game_bit(&a->game,6)==1&&!a->game.onboard){
  /* Preserve the animation's current shared RNG, then execute 6C9D's
   * dedicated small pose. Capturing the full conversation sprite is wrong. */
  memcpy(a->alien_pixels,a->cockpit.pixels,W*H); /* 3179 calls 4B30 before 6C9D. */
  a->alien.rng_low=a->rnglo;a->alien.rng_high=a->rnghi;
  if(blood_alien_passenger_pose(&a->alien,a->alien_pixels)){strcpy(a->status,"PASSENGER DRAW ERROR");return;}
  a->rnglo=a->alien.rng_low;a->rnghi=a->alien.rng_high;
  blood_passenger_capture(a->save.passenger_image,a->alien_pixels);
  memcpy(a->game.flags,a->vm.flags,256);
  if(!blood_take_aboard(&a->game)){
   if(blood_teleport_ending_flag(&a->game))scene(a,ENDING);else enter_bridge(a);
   portrait_transition(a,a->cockpit.pixels,a->alien_pixels);
  }
 }
}
static void drag_map(App*a,int mx,int my,int first){
 int x=mx-33,y=my-19;(void)first;
 if(x<0)x=0;
 if(x>255)x=255;
 if(y<0)y=0;
 if(y>127)y=127;
 /* 6FD7..6FFD chooses the nearer coordinate on each sample, not a locked axis. */
 a->drag_axis=abs(x-a->selectedx)<=abs(y-a->selectedy)?0:1;
 if(a->drag_axis==0)a->selectedx=(uint8_t)x;else a->selectedy=(uint8_t)y;
 a->changed=a->selectedx!=a->game.planet_x||a->selectedy!=a->game.planet_y;
}
static void effect_rng(App*a){a->rnglo=a->starfield.random.rng_low;a->rnghi=a->starfield.random.rng_high;}
static void warp_start(App*a){
 scene(a,WARP);a->warp_pending=1;a->effect_phase=0;a->effect_step=0;a->effect_speed=1;a->effect_colour=1;
}
static void warp_prepare(App*a){
 a->warp_pending=0;memset(a->effect_pixels,0,sizeof(a->effect_pixels));
 blood_starfield_init(&a->starfield,a->rnglo,a->rnghi,256,12,160,83);
 blood_starfield_step(&a->starfield,a->effect_pixels,0);effect_rng(a);speaker_play(a,14);
}
static void warp_tick(App*a){
 uint8_t *p;
 unsigned n,batch,sub;
 if(a->warp_pending)warp_prepare(a);
 p=a->effect_pixels;n=a->effect_step;
 /* Each phase follows the loops at 2AB0..2C3D. Four line/circle iterations
  * share a presentation frame; star updates remain individually visible. */
 batch=a->effect_phase>=2&&a->effect_phase<=5?4:1;
 for(sub=0;sub<batch&&a->scene==WARP;sub++){
  n=a->effect_step;
  switch(a->effect_phase){
  case 0:
   blood_starfield_step(&a->starfield,p,a->effect_speed);a->effect_speed+=9;
   if(a->effect_speed>=0x1a0){a->effect_phase=1;a->effect_step=0;a->effect_colour=1;continue;}break;
  case 1:
   a->effect_colour=(a->effect_colour+1)&15;
   blood_effect_circle(p,160,83,2+2*n,(uint8_t)a->effect_colour,0);
   blood_starfield_step(&a->starfield,p,512);
   if(n==49){a->effect_phase=2;a->effect_step=0;a->effect_colour=1;continue;}break;
  case 2:{
   int si=159-(int)n;
   blood_effect_line(p,160,83,si,20,(uint8_t)a->effect_colour,0);
   blood_effect_line(p,160,83,si,145,(uint8_t)a->effect_colour,0);
   blood_effect_line(p,160,83,319-si,145,(uint8_t)a->effect_colour,0);
   blood_effect_line(p,160,83,319-si,20,(uint8_t)a->effect_colour,0);
   if(si<126&&!(si&1)){int y=20+si/2;
    blood_effect_line(p,160,83,0,y,(uint8_t)a->effect_colour,0);blood_effect_line(p,160,83,319,y,(uint8_t)a->effect_colour,0);
    y=145-si/2;blood_effect_line(p,160,83,319,y,(uint8_t)a->effect_colour,0);blood_effect_line(p,160,83,0,y,(uint8_t)a->effect_colour,0);
   }
   a->effect_colour=(a->effect_colour+1)&15;
   if(n==159){a->effect_phase=3;a->effect_step=0;continue;}break;}
  case 3:
   blood_effect_line(p,0,83,(int)n,20,(uint8_t)a->effect_colour,1);
   blood_effect_line(p,0,83,(int)n,145,(uint8_t)a->effect_colour,1);
   blood_effect_line(p,319,83,319-(int)n,145,(uint8_t)a->effect_colour,1);
   blood_effect_line(p,319,83,319-(int)n,20,(uint8_t)a->effect_colour,1);
   a->effect_colour=(a->effect_colour+1)&3;
   if(n==159){a->effect_phase=4;a->effect_step=0;continue;}break;
  case 4:
   blood_effect_line(p,0,83,160,20+(int)n,(uint8_t)a->effect_colour,1);
   blood_effect_line(p,319,83,160,20+(int)n,(uint8_t)a->effect_colour,1);
   blood_effect_line(p,319,83,160,145-(int)n,(uint8_t)a->effect_colour,1);
   blood_effect_line(p,0,83,160,145-(int)n,(uint8_t)a->effect_colour,1);
   a->effect_colour=(a->effect_colour+1)&3;
   if(n==63){a->effect_phase=5;a->effect_step=0;continue;}break;
  case 5:
   blood_effect_circle(p,160,83,2+n,3,1);
   if(n==98){a->effect_phase=6;a->effect_step=0;
    blood_starfield_init(&a->starfield,a->starfield.random.rng_low,a->starfield.random.rng_high,256,12,160,83);continue;}break;
  case 6:
   blood_effect_circle(p,160,83,2+n,0,0);blood_starfield_step(&a->starfield,p,512);
   if(n==98){a->effect_phase=7;a->effect_step=0;a->effect_speed=480;a->effect_deceleration=0x900;
    memset(p,0,64000);continue;}break;
  case 7:
   if((a->effect_deceleration>>8)>=2)a->effect_deceleration-=2;
   if(a->effect_speed<(a->effect_deceleration>>8)){
    effect_rng(a);choose_world(a,a->selectedx,a->selectedy);scene(a,ARRIVAL);return;
   }
   a->effect_speed-=(a->effect_deceleration>>8);blood_starfield_step(&a->starfield,p,a->effect_speed);break;
  }
  a->effect_step++;
 }
 effect_rng(a);
}
static void destruction_noise(App*a){
 uint16_t seed=a->effect_speed;
 unsigned column,row,k,yy,xx,mask,spacing,half,x,y;
 uint8_t c,mid;
 /* 688F: 2 columns x 8 rows expanded to 32x32 blocks, masked by pairs
  * at 11B6. 5BF1 subsequently creates the remaining mirrored surface. */
 for(column=0;column<2;column++)for(row=0;row<8;row++){
  k=column*8+row;c=(uint8_t)(seed=blood_planet_random16(seed));
  c=(uint8_t)((c&a->destruction_tables[k*2+1])|a->destruction_tables[k*2]);
  for(yy=0;yy<32;yy++)for(xx=0;xx<32;xx++)a->static_source[column*8192+row*32+yy*256+xx]=c;
 }
 mask=127;
 for(spacing=32;spacing>1;spacing/=2){half=spacing/2;
  for(x=0;x<256;x+=spacing)for(y=0;y<64;y+=spacing){
   mid=(uint8_t)((a->static_source[y*256+x]+a->static_source[((y+spacing)&63)*256+x])/2);
   blood_planet_surface_adjust(mid,8,(uint8_t)mask,&seed,&a->static_source[(y+half)*256+x]);
  }
  for(y=0;y<64;y+=half)for(x=0;x<256;x+=spacing){
   mid=(uint8_t)((a->static_source[y*256+x]+a->static_source[y*256+((x+spacing)&255)])/2);
   blood_planet_surface_adjust(mid,3,(uint8_t)mask,&seed,&a->static_source[y*256+x+half]);
  }
  mask/=2;
 }
 a->effect_speed=seed;
}
static void destruction_bands(App*a,unsigned threshold,unsigned table){
 uint8_t lut[256];
 unsigned i,value,band,byte,row,bit;uint8_t c;
 for(i=0;i<256;i++){
  value=256+i;c=0;
  if(value>=threshold&&value<threshold+16)c=a->destruction_tables[table];
  else if(value>=threshold+16&&value<threshold+48)c=a->destruction_tables[table+1];
  else if(value>=threshold+48&&value<threshold+96)c=a->destruction_tables[table+2];
  else if(value>=threshold+96&&value<=threshold+160)c=a->destruction_tables[table+3];
  lut[i]=c;
 }
 /* 67D6: 40 EGA bytes per band, four symmetric scanlines on each side. */
 for(band=0;band<16;band++)for(byte=0;byte<40;byte++){
  c=lut[a->static_source[0x2041-256*band+2*byte]];
  for(row=0;row<4;row++)for(bit=0;bit<8;bit++){
   a->effect_pixels[(83-4*band-row)*320+byte*8+bit]=c;
   a->effect_pixels[(83+4*band+row)*320+byte*8+bit]=c;
  }
 }
}
static void destroy_start(App*a){
 if(blood_destroy_planet(&a->game))return;
 scene(a,DESTROY);speaker_play(a,6);a->effect_phase=0;a->effect_step=0;a->effect_colour=1;
 memcpy(a->effect_pixels,a->indexed,sizeof(a->effect_pixels));a->effect_speed=a->appearance.rng;
}
static void destroy_tick(App*a){
 unsigned n=a->effect_step,i;BloodUpcom r;memset(&r,0,sizeof(r));r.rng_low=a->rnglo;r.rng_high=a->rnghi;
 switch(a->effect_phase){
 case 0:
  for(i=0;i<128;i++){uint16_t v=blood_upcom_random(&r);a->texture[v&0x3fff]=(uint8_t)(v&15);}
  a->rnglo=r.rng_low;a->rnghi=r.rng_high;
  a->longitude=(uint8_t)(a->longitude+a->appearance.rotation_step);
  memset(a->effect_pixels,0,sizeof(a->effect_pixels));blood_stars_draw(a->effect_pixels,a->stars);
  blood_globe_draw(a->effect_pixels,a->texture,a->globe_stream,sizeof(a->globe_stream),a->longitude);
  if(n==15){blood_starfield_init(&a->starfield,a->rnglo,a->rnghi,384,0,128,84);
   a->effect_phase=1;a->effect_step=0;effect_rng(a);return;}break;
 case 1:
  blood_starfield_step(&a->starfield,a->effect_pixels,384);effect_rng(a);
  if(n==95){a->effect_phase=2;a->effect_step=0;a->effect_radius=2;a->effect_outer=0;return;}break;
 case 2:{
  unsigned radius=a->effect_radius;
  blood_effect_circle(a->effect_pixels,128,84,radius,0,0);
  a->effect_colour++;if(a->effect_colour>=15)a->effect_colour=1;
  blood_effect_circle(a->effect_pixels,128,84,radius+18,(uint8_t)a->effect_colour,0);
  a->effect_radius+=18;
  if(a->effect_radius>140){a->effect_outer++;a->effect_radius=2+a->effect_outer;
   if(a->effect_outer==18){destruction_noise(a);a->effect_phase=3;a->effect_step=0;return;}}
  break;}
 case 3:
  if(n<38)destruction_bands(a,512-12*n,32);
  else if(n<94)destruction_bands(a,512-8*(n-38),36);
  else destruction_bands(a,64+16*(n-94),40);
  if(n>=123){scene(a,ORBIT);return;}break;
 }
 a->effect_step++;
}
static void title_start(App*a,unsigned page){
 memcpy(a->fade_pixels,a->indexed,64000);memcpy(a->transition_target,a->title[page].pixels,64000);
 a->title_page=(uint8_t)page;a->title_cursor=0;a->title_remaining=64000;
}
static void title_tick(App*a){
 if(a->title_remaining){
  /* 4AB4 has no IRQ wait: only presentation batches use host timing. */
  blood_title_exchange(a->fade_pixels,a->transition_target,&a->title_cursor,1600);
  a->title_remaining-=1600;if(!a->title_remaining)a->seconds=0;return;
 }
 /* 4970/49AD use JA, not JAE: holds expire at seconds 7 and 6. */
 if(a->seconds>(a->title_page?5u:6u))title_start(a,a->title_page^1u);
}
static int load_titles(App*a){
 unsigned y;
 if(!load_art(&a->title[0],a->assets,"GENERIK.EGX.idx")||
    !load_art(&a->title[1],a->assets,"BEBEEGA.EGX.idx"))return 0;
 /* 487E saves 26 bytes x 6 rows at x=80; 48C3/4967 restore this
  * original disk prompt onto both images. */
 for(y=0;y<6;y++)memcpy(a->title[1].pixels+y*W+80,a->title[0].pixels+y*W+80,208);
 return 1;
}
static void advance(App*a,int mx,int my){
 uint32_t now,elapsed;
 int px,py,rawx,rawy,miny;
 uint16_t mask,redraw;
 unsigned i,icon;
 uint8_t counts[10];
 BloodUpcom random;
 if(a->debug_open){
#ifdef BLOOD_OS2PM
  a->last_clock_ms=blood_pm_ticks();
#else
  a->last_clock_ms=SDL_GetTicks();
#endif
  return;
 }
 /* Original PIT channel 0 divisor is 5970 at 1,193,182 Hz. Accumulate
  * interrupts independently of SDL's frame rate. The original game carries
  * one elapsed second for each 200 IRQs, then 60 seconds per minute. */
#ifdef BLOOD_OS2PM
 now=blood_pm_ticks();
#else
 now=SDL_GetTicks();
#endif
 elapsed=now-a->last_clock_ms;
 if(!a->last_clock_ms){elapsed=0;}a->last_clock_ms=now;
 if(elapsed>250u)elapsed=250u; /* avoid huge jumps after window suspension */
 a->irq_accum+=elapsed*1193182u;
 while(a->irq_accum>=5970000u){a->irq_accum-=5970000u;
  if(++a->service_irq_divider==12u){a->service_irq_divider=0;a->service_clock++;}
  if(++a->irq_divider==200u){a->irq_divider=0;if(++a->seconds==60u){a->seconds=0;a->minutes++;}}}
 /* 4F69 clears DS:01F4 after its service wait. This SDL service boundary
  * precedes UI dispatch; IRQ phase continues independently. */
 a->service_clock=0;
 if(a->scene==TITLE){title_tick(a);return;}
 a->frame++;px=mx;py=my;pointer_position(a,&px,&py);
 rawx=px;rawy=py;miny=a->scene==GALAXY?19:a->scene==ENCOUNTER?112:a->scene==BRIDGE?136:108;
 blood_hand_tremor(&a->tremor,a->minutes,a->game.grace_minutes,a->seconds,
                   a->scene==ENDING||a->scene==FLIGHT,&px,&py,8,288,miny,196);
 a->tremor_x=px-rawx;a->tremor_y=py-rawy;a->mouse_x=px;a->mouse_y=py;
 if(a->fade_ticks){transition_tick(a,elapsed);return;}
 a->scene_ticks++;
 mask=eye_mask(a,a->scene);
 if(a->scene==ENDING){
  /* 31BE..31CA repeatedly close (0), then open (3FF), waiting for each
   * 56FA staggered sequence. Suppress hand tremor as DOS sets 10F3. */
  if(!a->eyes.busy){a->ending_phase^=1u;a->eye_mask=a->ending_phase?0x3ff:0;blood_eyes_request(&a->eyes,a->eye_mask);}
  memcpy(counts,a->eyes.countdown,10);
  blood_eyes_tick(&a->eyes);redraw=0;
  for(i=0;i<10;i++)if(counts[i]!=a->eyes.countdown[i])redraw|=(uint16_t)(0x200u>>i);
  blood_eyes_draw_mask(a->effect_pixels,&a->eyes,(const uint8_t (*)[4000])a->sheets,redraw);return;
 }
 if(mask!=a->eye_mask){a->eye_mask=mask;blood_eyes_request(&a->eyes,mask);}
 blood_eyes_tick(&a->eyes);blood_hand_tick(&a->hand,a->mouse_buttons!=0);
 if(a->scene==ENCOUNTER){
  /* Actions are dispatched once at button-down, at the click position.
   * The original hand pulse remains visual; polling it lost short clicks. */
  if(a->scene==ENCOUNTER){
   /* 3486: one service wait before every slot, two more after
    * a nonzero icon. Empty tail slots do not repeat the speech delay. */
   if(a->reply_presenting&&a->reply_delay==2&&a->reply_visible&&a->reply_icons[a->reply_visible-1])speaker_stop(a);
   if(a->reply_presenting&&!--a->reply_delay){
    if(a->reply_visible<8){
     icon=a->reply_icons[a->reply_visible++];if(icon)speaker_play(a,5);a->reply_delay=icon?3:1;
     if(a->reply_visible==8){if(icon)a->reply_delay=2;else a->reply_presenting=0;}
    }else a->reply_presenting=0;
    if(!a->reply_presenting)blood_alien_speech(&a->alien,0);
   }
   if(!a->reply_presenting&&!--a->mouth_delay){a->mouth_delay=8;if(a->vm.active||a->mouth_open)a->mouth_open^=1;}
   if(a->scene_ticks%5==0&&!a->vm.end_requested){
    a->alien.rng_low=a->rnglo;a->alien.rng_high=a->rnghi;
    if(blood_alien_step(&a->alien,a->alien_pixels))strcpy(a->status,"ALIEN DRAW ERROR");
    a->rnglo=a->alien.rng_low;a->rnghi=a->alien.rng_high;
   }
   if(a->scene_ticks%3==0){
    int px=a->mouse_x+3,py=a->mouse_y+1;
    if(py>=180&&py<=200&&px>=24&&px<=56){
     if(a->icon_offset){a->icon_offset=0;if(a->icon_base>2)a->icon_base-=2;}
     else if(a->icon_base>2)a->icon_offset=1;
    }
    if(py>=180&&py<=200&&px>=264&&px<=296){
     if(a->icon_offset)a->icon_offset=0;
     else if(a->icon_base<=92){a->icon_offset=1;a->icon_base+=2;}
    }
    if(a->hand.state&&py>=184&&py<=200&&px>=56&&px<264){
     int value=px-72-20;if(value<0)value=0;value=(value/3)*2;
     if(value<2)value=2;
     if(value>=92)value=94;
     a->icon_base=(uint8_t)value;a->icon_offset=0;
    }
   }
  }
 }

 /* 4F69 calls 500A: EGA page alternates while 65C6 writes opposite
  * white/black missile squares to the two pages. Positions stay in texture. */
 if(a->scene==DETAIL)a->scan_page^=1;
 if((a->scene==ORBIT||a->scene==DETAIL) && (a->frame%3u==0))a->longitude=(uint8_t)(a->longitude+a->appearance.rotation_step*(a->scene==DETAIL?3u:1u));
 if(a->scene==WARP)warp_tick(a);
 if(a->scene==DESTROY)destroy_tick(a);
 if(a->scene==ARRIVAL){if(a->scene_ticks%3==0)a->longitude=(uint8_t)(a->longitude+a->appearance.rotation_step);
  if(a->scene_ticks>=42){scene(a,ORBIT);a->fade_ticks=0;}}
 if(a->scene==STATIC){
  a->static_irq_accum+=elapsed*1193182u;
  while(a->scene==STATIC&&a->static_irq_accum>=71640000u){
   a->static_irq_accum-=71640000u;
   if(++a->static_frames>=24){a->has_terrain=0;enter_bridge(a);}
   else{static_fallback(a);memset(&random,0,sizeof(random));random.rng_low=a->rnglo;random.rng_high=a->rnghi;
    a->static_offset=blood_upcom_random(&random)>>8;a->rnglo=random.rng_low;a->rnghi=random.rng_high;}
  }
 }

 if(a->scene==LAUNCH&&a->scene_ticks>=22){fly_start(a,0);return;}
 if(a->scene==FLIGHT){
  a->flight_irq_accum+=elapsed*1193182u;
  while(a->scene==FLIGHT&&a->flight_irq_accum>=71640000u){
   a->flight_irq_accum-=71640000u;tick_flight(a,mx,my);mx=160;my=175;
  }
 }
 if(a->scene==LANDSCAPE){if(a->phase!=7)ensure_landscape(a);else if(a->scene_ticks%2==0&&a->landscape_pass<128){a->landscape_pass++;if(a->landscape_pass==128){if(encounter_begin(a))scene(a,ENCOUNTER);else scene(a,RELEASE);}}}
}

#ifndef BLOOD_OS2PM
#include "blood_debug.inc"
#else
static void debug_draw(App*a)
{
 (void)a;
}
static int debug_pm_event(App*a,const BloodPMEvent *e)
{
 (void)a;
 (void)e;
 return 0;
}
#endif
#ifndef BLOOD_OS2PM
static void handle_event(App*a,const SDL_Event *e){
 int x,y;
 if(debug_event(a,e)){speaker_pause_sync(a);return;}
 if(e->type==SDL_KEYDOWN&&e->key.keysym.sym==SDLK_F9&&!e->key.repeat){speaker_mute(a);return;}
 if(e->type==SDL_KEYDOWN&&e->key.keysym.sym==SDLK_ESCAPE){if(a->scene==TITLE)a->running=0;else enter_bridge(a);return;}
 if(e->type==SDL_QUIT){a->running=0;return;}
 if(e->type!=SDL_MOUSEBUTTONDOWN||a->scene==FLIGHT||a->fade_ticks)return;
 if(e->button.button!=SDL_BUTTON_LEFT&&e->button.button!=SDL_BUTTON_RIGHT)return;
 /* SDL_RenderSetLogicalSize already scales queued mouse-event positions.
  * SDL_GetMouseState below still supplies window pixels for held steering. */
 x=e->button.x;y=e->button.y;
 a->mouse_buttons=e->button.button==SDL_BUTTON_RIGHT?2:1;
 if(!a->hand.state){a->hand.state=2;a->hand.frame=1;a->hand.delay=1;}
 if(a->scene==GALAXY&&y>=19&&y<147)drag_map(a,x+a->tremor_x,y+a->tremor_y,1);
 else press(a,x,y);
}
#else
static void handle_pm_event(App*a,const BloodPMEvent *e){
 int x,y;
 if(debug_pm_event(a,e)){speaker_pause_sync(a);return;}
 if(e->type==BLOODPM_EVENT_KEYDOWN&&e->key==BLOODPM_KEY_F9){speaker_mute(a);return;}
 if(e->type==BLOODPM_EVENT_KEYDOWN&&e->key==BLOODPM_KEY_ESC){if(a->scene==TITLE)a->running=0;else enter_bridge(a);return;}
 if(e->type==BLOODPM_EVENT_QUIT){a->running=0;return;}
 if(e->type!=BLOODPM_EVENT_MOUSEDOWN||a->scene==FLIGHT||a->fade_ticks)return;
 if(e->button!=BLOODPM_BUTTON_LEFT&&e->button!=BLOODPM_BUTTON_RIGHT)return;
 x=e->x;y=e->y;
 a->mouse_buttons=e->button==BLOODPM_BUTTON_RIGHT?2:1;
 if(!a->hand.state){a->hand.state=2;a->hand.frame=1;a->hand.delay=1;}
 if(a->scene==GALAXY&&y>=19&&y<147)drag_map(a,x+a->tremor_x,y+a->tremor_y,1);
 else press(a,x,y);
}
#endif
int main(int argc,char**argv){App *a;char p[1024];
#ifdef BLOOD_OS2PM
 BloodPMEvent e;int mx,my,left,right;uint32_t last,now;const uint8_t*keys;
#else
 SDL_Event e;int mx,my;uint32_t last,now,buttons;const uint8_t*keys;int rawx,rawy,down,right;float fx,fy;
#endif
 if(argc!=2){fprintf(stderr,"Usage: %s <R20-assets-dir>\n",argv[0]);return 2;}
 a=(App*)calloc(1,sizeof(*a));if(!a)return 2;snprintf(a->assets,sizeof(a->assets),"%s",argv[1]);a->terrain=(BloodTerrain*)calloc(1,sizeof(BloodTerrain));
 if(!a->terrain||!load_tables(a,a->assets)){fprintf(stderr,"R20: missing extracted original numeric tables\n");return 2;}
 if(!path(p,sizeof(p),a->assets,"ui-font.bin")||!read_exact(p,a->font,sizeof(a->font))){fprintf(stderr,"missing R20 decoded font\n");return 2;}
 if(!path(p,sizeof(p),a->assets,"vocab.dat")||!read_exact(p,a->vocab,sizeof(a->vocab))){fprintf(stderr,"missing original vocabulary\n");return 2;}
 load_art(&a->cockpit,a->assets,"TABLEAU.EGX.idx");load_art(&a->ending,a->assets,"TOTABEGA.EGX.idx");load_art(&a->galaxy,a->assets,"GALAX.EGX.idx");
 if(!load_titles(a)){fprintf(stderr,"missing original EGA title images\n");return 2;}
 if(!load_ui(a)){fprintf(stderr,"missing R32 original UI assets\n");return 2;}
#ifdef BLOOD_OS2PM
 if(blood_pm_platform_init()!=0){fprintf(stderr,"PM init failed\n");return 2;}
 a->pm=blood_pm_create("Captain Blood - R32 OS/2 PM");
 if(!a->pm){fprintf(stderr,"PM window failed\n");return 2;}
 a->mouse_x=160;a->mouse_y=175;blood_pm_warp_mouse(a->pm,160,175);
#else
 if(SDL_Init(SDL_INIT_VIDEO)!=0){fprintf(stderr,"SDL init: %s\n",SDL_GetError());return 2;}
 a->window=SDL_CreateWindow("Captain Blood - R32 native UI",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,960,600,SDL_WINDOW_SHOWN|SDL_WINDOW_RESIZABLE);
 a->renderer=a->window?SDL_CreateRenderer(a->window,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC):0;
 if(!a->renderer&&a->window)a->renderer=SDL_CreateRenderer(a->window,-1,0);
 if(!a->renderer){fprintf(stderr,"SDL renderer: %s\n",SDL_GetError());return 2;}
 SDL_RenderSetLogicalSize(a->renderer,W,H);a->screen=SDL_CreateTexture(a->renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,W,H);
 if(!a->screen){fprintf(stderr,"SDL texture: %s\n",SDL_GetError());return 2;}
 SDL_ShowCursor(0);a->mouse_x=160;a->mouse_y=175;
#endif
 speaker_open(a);
 strcpy(a->save_path,"BLOOD-R32.GAM");
 a->running=1;scene(a,TITLE);title_start(a,0);speaker_play(a,0);
#ifdef BLOOD_OS2PM
 last=blood_pm_ticks();
#else
 last=SDL_GetTicks();
#endif
 while(a->running){
#ifdef BLOOD_OS2PM
  now=blood_pm_ticks();
  while(blood_pm_poll(a->pm,&e))handle_pm_event(a,&e);
  blood_pm_get_mouse(a->pm,&mx,&my,&left,&right);
  a->mouse_buttons=(uint8_t)((left?1u:0u)|(right?2u:0u));
  if(a->scene==FLIGHT){
   a->flight_buttons=(uint8_t)((left?1u:0u)|(right?2u:0u));
   if(a->flight_recenter){recenter_flight(a);mx=160;my=175;a->flight_recenter=0;}
  }else{
   if(!a->debug_open&&left&&a->scene==GALAXY&&my>=19&&my<147)drag_map(a,mx+a->tremor_x,my+a->tremor_y,0);
  }
  keys=blood_pm_keys();if(!a->debug_open&&a->scene==TITLE&&keys[' '])new_game(a);
  if(!a->debug_open&&keys[BLOODPM_KEY_F8]&&!a->f8_held){a->safe_flight=!a->safe_flight;
   if(a->scene==FLIGHT)a->terrain->mem[BT_NO_COLLISION]=(uint8_t)a->safe_flight;}
  a->f8_held=keys[BLOODPM_KEY_F8]!=0;
  if(now-last>=30u){last=now;if(a->scene==FLIGHT&&a->flight_recenter){recenter_flight(a);mx=160;my=175;a->flight_recenter=0;}advance(a,mx,my);draw_scene(a);present(a);}else blood_pm_sleep(2);
#else
  now=SDL_GetTicks();
  while(SDL_PollEvent(&e))handle_event(a,&e);
  SDL_GetMouseState(&rawx,&rawy);SDL_RenderWindowToLogical(a->renderer,rawx,rawy,&fx,&fy);mx=(int)fx;my=(int)fy;
  buttons=SDL_GetMouseState(0,0);down=(buttons&SDL_BUTTON_LMASK)!=0;
  right=(buttons&SDL_BUTTON_RMASK)!=0;
  a->mouse_buttons=(uint8_t)((down?1u:0u)|(right?2u:0u));
  if(a->scene==FLIGHT){
   a->flight_buttons=(uint8_t)((down?1u:0u)|(right?2u:0u));
   if(a->flight_recenter){recenter_flight(a);mx=160;my=175;a->flight_recenter=0;}
  }else{
   if(!a->debug_open&&down&&a->scene==GALAXY&&my>=19&&my<147)drag_map(a,mx+a->tremor_x,my+a->tremor_y,0);
  }
  keys=SDL_GetKeyboardState(0);if(!a->debug_open&&a->scene==TITLE&&keys[44])new_game(a);
  /* SDL_SCANCODE_F8=65. Assisted mode disables original collision/hostile
   * flight ONLY when explicitly requested; never part of normal gameplay. */
  if(!a->debug_open&&keys[65]&&!a->f8_held){a->safe_flight=!a->safe_flight;
   if(a->scene==FLIGHT)a->terrain->mem[BT_NO_COLLISION]=(uint8_t)a->safe_flight;}
  a->f8_held=keys[65]!=0;
  if(now-last>=30u){last=now;if(a->scene==FLIGHT&&a->flight_recenter){recenter_flight(a);mx=160;my=175;a->flight_recenter=0;}advance(a,mx,my);draw_scene(a);present(a);}else SDL_Delay(2);
#endif
 }
#ifndef BLOOD_OS2PM
 if(a->audio_device)SDL_CloseAudioDevice(a->audio_device);
 free(landframes);SDL_DestroyTexture(a->screen);SDL_DestroyRenderer(a->renderer);SDL_DestroyWindow(a->window);SDL_Quit();free(a->terrain);free(a);return 0;
#else
 free(landframes);blood_pm_destroy(a->pm);free(a->terrain);free(a);return 0;
#endif
}
