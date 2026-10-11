#ifndef BLOOD_UPCOM_H
#define BLOOD_UPCOM_H
/* Captain Blood B2 R16: source-derived, renderer-independent UpCom interpreter.
 * Evidence addresses are OFFSETS IN THE RECOVERED MZ FILE (SHA in README).
 * No x86 emulator, DOS interrupts, host GUI, or game-specific English sentences
 * are used by this module. Supply original EGP bytes AFTER the 7-byte file
 * envelope. The caller owns that mutable bank for the life of an encounter.
 */
#include <stddef.h>
#include <stdint.h>
#define BLOOD_FLAG_BYTES 256u  /* the original saved dictionary prefix */
#define BLOOD_PHRASE_BYTES 32u

typedef struct BloodUpcom {
    uint8_t *bank;
    size_t bank_size;
    uint8_t flags[BLOOD_FLAG_BYTES];
    uint8_t input[BLOOD_PHRASE_BYTES];
    uint8_t reply[BLOOD_PHRASE_BYTES];
    uint16_t minutes;
    uint16_t rng_low, rng_high;  /* original DS:10FB,10FD, NOT host random() */
    uint16_t start_pc, start_word;
    uint16_t pc, word_index;
    uint8_t force_countdown;  /* DS:0368 */
    uint8_t active;           /* DS:0792: a previous scan yielded a sentence */
    uint8_t end_requested;    /* DS:0793: empty assignment action */
    uint8_t yielded;          /* DS:0794 */
    uint8_t condition_mode;   /* DS:0795 */
    uint8_t reserved_796;     /* cleared by the original fresh-scan entry */
    uint32_t steps;           /* diagnostic only, bounded record operations */
    uint16_t last_text_pc;    /* diagnostic only, 0xffff if no reply */
    int error;               /* -1 malformed bank, -2 execution budget */
} BloodUpcom;

int blood_flag(const BloodUpcom *v, unsigned bit);
int blood_set_flag(BloodUpcom *v, unsigned bit, int value);
uint16_t blood_upcom_random(BloodUpcom *v);

/* Establish cursors and clear sentence-used bits, but do not clear story flags.
 * Caller can copy the 256-byte state prefix from a RAM snapshot beforehand.
 */
int blood_upcom_attach(BloodUpcom *v, uint8_t *body, size_t size);
/* Original encounter setup subset 3F97..3FD1. Local flags 1..29 are reset;
 * record_slot is 0..3, and wanted_y carries the original destroyed high bit. */
int blood_upcom_encounter(BloodUpcom *v, unsigned record_slot, uint8_t wanted_y);

/* Original 4109 (fresh!=0) or 415D (fresh==0), down to 4446 return.
 * Does not clear yielded/end_requested; those belong to the calling wrapper.
 * Return 1 = reply, 0 = no reply/end, negative = bounded input error.
 */
int blood_upcom_scan(BloodUpcom *v, int fresh);
/* UI-free equivalents of the 4080/40A0 wrappers. Input is ORIGINAL token bytes,
 * not dictionary screen indexes. Special tokens 1..3 include two operand bytes.
 * begin may try eight scans; submit one; next continues an active scan.
 */
int blood_upcom_begin(BloodUpcom *v);
int blood_upcom_submit(BloodUpcom *v, const uint8_t *tokens, size_t bytes);
int blood_upcom_next(BloodUpcom *v);
#endif
