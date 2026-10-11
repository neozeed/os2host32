#ifndef BLOOD_SPEAKER_H
#define BLOOD_SPEAKER_H
#include <stdint.h>
#include <stddef.h>
#define BLOOD_SOUND_BYTES 1616u
#define BLOOD_SOUND_TABLE_BYTES 222u
/* Native port of 3B87/3B68/39DF. Addresses are original DS offsets.
 * The table and SOUND.BIN are data; no x86 executable is used at runtime. */
typedef struct BloodSpeaker {
 uint8_t programs[BLOOD_SOUND_BYTES],tables[BLOOD_SOUND_TABLE_BYTES];
 uint16_t pc,main_script,base,countdown,divisor;
 uint8_t rate,rate_count,tempo,duration,blocked,gate;
 uint32_t reload_serial,audio_serial,sample_rate;
 uint64_t irq_phase,tone_phase;
 int muted,paused,error;
} BloodSpeaker;
int blood_speaker_init(BloodSpeaker*,const uint8_t *programs,size_t bytes,
                       const uint8_t *tables,size_t table_bytes,uint32_t sample_rate);
int blood_speaker_play(BloodSpeaker*,unsigned id);
void blood_speaker_stop(BloodSpeaker*);
void blood_speaker_irq(BloodSpeaker*);
/* Signed native-endian mono PCM, modest fixed amplitude. No allocation/locks. */
void blood_speaker_render(BloodSpeaker*,int16_t *samples,size_t count);
#endif
