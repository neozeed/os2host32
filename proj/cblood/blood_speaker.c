#include "blood_speaker.h"
#include <string.h>
static unsigned word(const uint8_t*p){return p[0]+256u*p[1];}
int blood_speaker_init(BloodSpeaker*s,const uint8_t*p,size_t n,const uint8_t*t,size_t tn,uint32_t rate)
{
 if(!s||!p||n!=BLOOD_SOUND_BYTES||!t||tn!=BLOOD_SOUND_TABLE_BYTES||rate<8000u||rate>192000u)return -1;
 memset(s,0,sizeof(*s));memcpy(s->programs,p,n);memcpy(s->tables,t,tn);s->sample_rate=rate;
 s->blocked=255;s->countdown=1;s->rate=8;s->rate_count=1;
 for(unsigned i=0;i<15;i++)if(word(t+4*i)<0x998u||word(t+4*i)>=0x998u+n||!t[4*i+2])return s->error=-1;
 return 0;
}
int blood_speaker_play(BloodSpeaker*s,unsigned id)
{
 if(!s||s->error||id>=15)return -1;
 s->main_script=(uint16_t)word(s->tables+4*id);s->pc=s->main_script;
 s->rate=s->tables[4*id+2];s->rate_count=1;s->countdown=1;s->blocked=0;
 return 0;
}
void blood_speaker_stop(BloodSpeaker*s){if(s){s->blocked=255;s->gate=0;s->countdown=1;}}
void blood_speaker_irq(BloodSpeaker*s)
{
 if(!s||s->error)return;
 if(--s->rate_count)return;
 s->rate_count=s->rate;
 if(s->blocked)return;
 /* Original 39F3 performs forty microsteps per sound IRQ. The duration
  * count includes 40; parser controls reset it to 1 and restart that loop. */
 for(unsigned fuel=0;fuel<4096;){
  unsigned remaining=40;
  do {s->countdown=(uint16_t)(s->countdown-1u);if(!s->countdown)break;}while(--remaining);
  if(s->countdown)return;
  if(s->pc<0x998u||s->pc>=0x998u+BLOOD_SOUND_BYTES){s->error=-1;s->gate=0;return;}
  unsigned op=s->programs[s->pc++-0x998u];fuel++;
  if(op==255){s->pc=s->main_script;s->countdown=1;continue;}
  if(!(op&128u)){s->duration=(uint8_t)(op&127u);s->countdown=1;continue;}
  if(!(op&64u)){
   if(op&32u)s->tempo=(uint8_t)(op&31u);
   else {unsigned group=op&31u;if(group>=17u){s->error=-1;s->gate=0;return;}s->base=(uint16_t)word(s->tables+60+2*group);}
   s->countdown=1;continue;
  }
  if(op&32u){s->countdown=1;return;} /* original E0..FE yields without tone change */
  unsigned note=op&15u;
  if(note){unsigned index=(unsigned)(uint16_t)(s->base+note-1u);
   if(index>=64u){s->error=-1;s->gate=0;return;}
   s->divisor=(uint16_t)word(s->tables+94+2*index);s->gate=1;s->reload_serial++;
  }else s->gate=0;
  s->countdown=(uint16_t)((unsigned)s->duration*s->tempo+40u);return;
 }
 s->error=-2;s->gate=0;
}
void blood_speaker_render(BloodSpeaker*s,int16_t*out,size_t n)
{
 if(!s||!out)return;
 if(s->paused||s->error){memset(out,0,n*sizeof(*out));return;}
 const uint64_t irq_period=(uint64_t)5970u*s->sample_rate;
 for(size_t i=0;i<n;i++){
  s->irq_phase+=1193182u;
  while(s->irq_phase>=irq_period){s->irq_phase-=irq_period;blood_speaker_irq(s);}
  if(s->audio_serial!=s->reload_serial){s->audio_serial=s->reload_serial;s->tone_phase=0;}
  unsigned divisor=s->divisor?s->divisor:65536u;
  uint64_t period=(uint64_t)divisor*s->sample_rate;
  out[i]=(int16_t)(s->gate&&!s->muted&&!s->error?(s->tone_phase<(uint64_t)((divisor+1u)/2u)*s->sample_rate?2200:-2200):0);
  if(s->gate)s->tone_phase=(s->tone_phase+1193182u)%period;
 }
}
