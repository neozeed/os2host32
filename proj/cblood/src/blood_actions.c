#include "blood_actions.h"
#include <string.h>

int blood_game_bit(const BloodGame *s, unsigned bit)
{
    if (!s || bit >= 2048u) return -1;
    return (s->flags[bit >> 3] >> (bit & 7u)) & 1u;
}
int blood_game_set_bit(BloodGame *s, unsigned bit, int on)
{
    unsigned mask;
    if (!s || bit >= 2048u) return -1;
    mask = 1u << (bit & 7u);
    if (on) s->flags[bit >> 3] |= (uint8_t)mask;
    else s->flags[bit >> 3] &= (uint8_t)~mask;
    return 0;
}
static unsigned field(unsigned record, unsigned local) { return 100u + 5u * record + local; }
static void set_field(BloodGame *s, unsigned record, unsigned local, int on)
{
    (void)blood_game_set_bit(s, field(record, local), on);
}
int blood_take_aboard(BloodGame *s)
{
    if (!s || s->active_record >= 64) return -1;
    s->onboard = 1;
    s->carried_record = s->active_record;
    s->pending_extension = (uint16_t)blood_game_bit(s, 5);
    set_field(s, s->active_record, 2, 0);
    s->active_record = s->active_bank = BLOOD_NO_CHARACTER;
    s->mode = (uint16_t)((s->mode & 0x00ffu) | 0x0100u);
    return 0;
}
int blood_release_aboard(BloodGame *s)
{
    BloodCharacter *c;
    if (!s || s->carried_record >= 64) return -1;
    s->onboard = 0;
    s->active_record = s->carried_record;
    s->active_bank = (uint16_t)(s->active_record >> 2);
    c = &s->character[s->active_record];
    set_field(s,s->active_record,0,1);
    set_field(s,s->active_record,3,1);
    set_field(s,s->active_record,2,0);
    c->actual_x = s->planet_x; c->actual_y = s->planet_y;
    if (c->wanted_x == c->actual_x && c->wanted_y == c->actual_y)
        set_field(s,s->active_record,2,1);
    return 0;
}
int blood_disintegrate_aboard(BloodGame *s)
{
    if (!s || s->carried_record >= 64) return -1;
    /* 3EF8: the original primitive assumes its UI caller checked onboard.
     * It keeps the remembered record and does not wipe its coordinates. */
    s->onboard = 0;
    set_field(s,s->carried_record,0,0); /* IsAlive */
    set_field(s,s->carried_record,3,0); /* IsThere */
    s->mode &= 0x02ffu;
    /* 3F16 draws the updated console; state effects resume at 3F19. */
    if (s->pending_extension)
        s->grace_minutes = (uint16_t)(s->grace_minutes + 60u);
    s->pending_extension = 0;
    return 0;
}
int blood_select_planet(BloodGame *s)
{
    unsigned i;
    if (!s || s->destroyed_write > sizeof(s->destroyed_xy) || (s->destroyed_write & 1u)) return -1;
    s->active_record = s->active_bank = BLOOD_NO_CHARACTER;
    s->hostile_flight = 0;
    for (i=0; i<s->destroyed_write; i+=2)
        if (s->destroyed_xy[i]==s->planet_x && s->destroyed_xy[i+1]==s->planet_y) return 0;
    for (i=0; i<64; ++i) {
        BloodCharacter *c=&s->character[i];
        if (c->actual_x != s->planet_x || c->actual_y != s->planet_y) continue;
        s->active_record=(uint16_t)i; s->active_bank=(uint16_t)(i>>2);
        set_field(s,i,2,0);
        if (c->wanted_x==s->planet_x && c->wanted_y==s->planet_y) set_field(s,i,2,1);
        s->hostile_flight=(uint8_t)blood_game_bit(s,field(i,4));
        return 1;
    }
    /* 46D7..46DD: ordinary planets use X parity, not a new coin flip on landing. */
    s->hostile_flight=s->planet_x & 1u;
    return 1;
}
int blood_can_converse(const BloodGame *s)
{
    if (!s) return -1;
    if (s->active_record >= 64) return 0;
    return blood_game_bit(s,field(s->active_record,0)) &&
           blood_game_bit(s,field(s->active_record,3)) &&
           !(s->onboard && s->carried_record==s->active_record);
}
int blood_destroy_planet(BloodGame *s)
{
    unsigned i, last_actual=64;
    if (!s || s->destroyed_write >= sizeof(s->destroyed_xy) || (s->destroyed_write & 1u)) return -1;
    for (i=0; i<64; ++i) {
        BloodCharacter *c=&s->character[i];
        if (c->wanted_x==s->planet_x && c->wanted_y==s->planet_y) c->wanted_y|=0x80u;
        if (c->actual_x==s->planet_x && c->actual_y==s->planet_y) {
            c->actual_y|=0x80u; last_actual=i;
        }
    }
    /* DL retains the LAST match. All matched coordinates get their mark,
     * but only that character has its flags cleared. Preserve the quirk. */
    if (last_actual<64) {
        set_field(s,last_actual,0,0);
        set_field(s,last_actual,1,0);
    }
    s->destroyed_xy[s->destroyed_write]=s->planet_x;
    s->destroyed_xy[s->destroyed_write+1]=s->planet_y;
    s->destroyed_write=(uint16_t)(s->destroyed_write+2u);
    if (s->destroyed_write>=512) s->destroyed_write=0;
    s->destroyed_count=(uint16_t)(s->destroyed_count+1u);
    if (s->destroyed_count<11u) blood_game_set_bit(s,39u+s->destroyed_count,0);
    s->active_record=s->active_bank=BLOOD_NO_CHARACTER;
    return 0;
}
int blood_record_visit(BloodGame *s)
{
    if (!s || s->visited_write>=256 || (s->visited_write & 1u)) return -1;
    s->visited_xy[s->visited_write]=s->planet_x;
    s->visited_xy[s->visited_write+1]=s->planet_y;
    s->visited_write=(uint16_t)(s->visited_write+2u);
    if (s->visited_write>=256) s->visited_write=0;
    return 0;
}
int blood_was_visited(const BloodGame *s)
{
    unsigned i;
    if (!s || s->visited_write>256 || (s->visited_write&1u)) return -1;
    for (i=0;i<s->visited_write;i+=2)
        if (s->visited_xy[i]==s->planet_x && s->visited_xy[i+1]==s->planet_y) return 1;
    return 0;
}
int blood_teleport_ending_flag(const BloodGame *s) { return blood_game_bit(s,999); }

int blood_save_decode(BloodSaveImage *s, const uint8_t *f, size_t n)
{
    if (!s || !f || n!=BLOOD_SAVE_FILE_SIZE) return -1;
    /* Loader discards the envelope; do not require it to equal the prefix.
     * The ORIGINAL WRITER does duplicate it (verified on supplied BLOOD.GAM). */
    memcpy(s->global_block,f+7,BLOOD_SAVE_GLOBAL_SIZE);
    memcpy(s->dictionary_prefix,f+7+BLOOD_SAVE_GLOBAL_SIZE,256);
    memcpy(s->passenger_image,f+7+BLOOD_SAVE_GLOBAL_SIZE+256,4096);
    return 0;
}
int blood_save_encode(const BloodSaveImage *s, uint8_t *f, size_t n)
{
    if (!s || !f || n<BLOOD_SAVE_FILE_SIZE) return -1;
    memcpy(f,s->global_block,7);
    memcpy(f+7,s->global_block,BLOOD_SAVE_GLOBAL_SIZE);
    memcpy(f+7+BLOOD_SAVE_GLOBAL_SIZE,s->dictionary_prefix,256);
    memcpy(f+7+BLOOD_SAVE_GLOBAL_SIZE+256,s->passenger_image,4096);
    return 0;
}
