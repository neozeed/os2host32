#include "blood_upcom.h"
#include <string.h>
#define USED 0x80u
#define MAX_STEPS 100000u

typedef struct Record {
    uint16_t pc, wi, next, next_wi;
    unsigned kind, length, command;
    int sentinel;
} Record;

static int fail(BloodUpcom *v, int error) { v->error = error; return 0; }
static int fuel(BloodUpcom *v)
{
    if (++v->steps > MAX_STEPS) return fail(v, -2);
    return 1;
}
int blood_flag(const BloodUpcom *v, unsigned bit)
{
    if (!v || bit >= 8u * BLOOD_FLAG_BYTES) return -1;
    return (v->flags[bit >> 3] >> (bit & 7u)) & 1u;
}
int blood_set_flag(BloodUpcom *v, unsigned bit, int value)
{
    unsigned mask;
    if (!v || bit >= 8u * BLOOD_FLAG_BYTES) return -1;
    mask = 1u << (bit & 7u);
    if (value) v->flags[bit >> 3] |= (uint8_t)mask;
    else v->flags[bit >> 3] &= (uint8_t)~mask;
    return 0;
}
static uint16_t add16(unsigned a, unsigned b, unsigned carry, unsigned *out)
{
    uint32_t r = (uint32_t)a + b + carry;
    *out = r > 65535u;
    return (uint16_t)r;
}
uint16_t blood_upcom_random(BloodUpcom *v)
{
    /* Original 5850..5881; deliberately retain carry between 16-bit stages. */
    uint32_t p = (uint32_t)v->rng_high * 0xb0a1u;
    uint16_t bx = (uint16_t)p, cx = (uint16_t)(p >> 16), ax, dx;
    unsigned c;
    p = (uint32_t)v->rng_low * 0xb0a1u;
    bx = add16(bx, (unsigned)(p >> 16), 0, &c);
    cx = add16(cx, (uint16_t)p, c, &c);
    p = (uint32_t)v->rng_high * 0x12b9u;
    ax = add16((unsigned)(p >> 16), bx, 0, &c);
    dx = add16((uint16_t)p, cx, c, &c);
    ax = (uint16_t)(ax + 1u);
    if (!ax) dx = (uint16_t)(dx + 1u);
    v->rng_low = dx;
    v->rng_high = ax;
    return ax;
}
static unsigned be16(const uint8_t *p) { return ((unsigned)p[0] << 8) | p[1]; }
static unsigned le16(const uint8_t *p) { return p[0] | ((unsigned)p[1] << 8); }

/* Original 4032/4048 count WORD OCCURRENCES separately from payload bytes.
 * A special token occupies three bytes but advances word metadata only once. */
static int read_record(BloodUpcom *v, Record *r)
{
    size_t p = v->pc, end, i;
    unsigned words = 0;
    if (!fuel(v)) return 0;
    if (p >= v->bank_size) return fail(v, -1);
    memset(r, 0, sizeof(*r));
    r->pc = v->pc; r->wi = v->word_index;
    if (v->bank[p] == 0xff) { r->sentinel = 1; return 1; }
    if (p + 2 > v->bank_size) return fail(v, -1);
    r->kind = v->bank[p] & 0xe0u;
    r->length = v->bank[p] & 0x1fu;
    r->command = v->bank[p + 1];
    end = p + 2u + r->length;
    if (end > v->bank_size || end > 65535u) return fail(v, -1);
    if (r->kind == 0) {
        for (i = p + 2; i < end; ++words) {
            unsigned step = v->bank[i] <= 3 ? 3u : 1u;
            /* Token 0 has inconsistent legacy decoder branches; no shipped
             * DOS EGP sentence uses it. Reject it rather than invent behavior. */
            if (v->bank[i] == 0 || i + step > end) return fail(v, -1);
            i += step;
        }
    } else if (r->kind == 0x40 || r->kind == 0x80 ||
               r->kind == 0xa0 || r->kind == 0xc0) {
        if (r->length & 1u) return fail(v, -1);
    } else return fail(v, -1);
    r->next = (uint16_t)end;
    r->next_wi = (uint16_t)(v->word_index + words);
    return 1;
}
static void advance(BloodUpcom *v, const Record *r)
{
    v->pc = r->next; v->word_index = r->next_wi;
}
/* 4546/454C/4552: three bit streams interleaved per eight word occurrences,
 * high-bit first. This is NOT a dictionary property of the token itself. */
static int word_flag(BloodUpcom *v, unsigned word, unsigned column)
{
    size_t pos = 0x10u + (word >> 3) * 3u + column;
    if (pos >= v->bank_size) { fail(v, -1); return 0; }
    return (v->bank[pos] & (0x80u >> (word & 7u))) != 0;
}
static int bit_word(BloodUpcom *v, unsigned raw, int mutate)
{
    unsigned negative = raw & 0x8000u;
    unsigned bit = negative ? (0u - raw) & 0xffffu : raw;
    int actual;
    if (bit >= BLOOD_FLAG_BYTES * 8u) return fail(v, -1);
    if (mutate) { blood_set_flag(v, bit, !negative); return 1; }
    actual = blood_flag(v, bit);
    return negative ? !actual : actual;
}
/* 423D: assignments are effects of the previous accepted sentence/condition.
 * A flagged once-only assignment is marked here. Empty assignments request
 * encounter closure but do not themselves advance the original PC. */
static int assignments(BloodUpcom *v)
{
    Record r;
    for (;;) {
        unsigned i;
        if (!read_record(v, &r)) return 0;
        if (r.sentinel || r.kind != 0x40) return 1;
        if (r.command & 1u) {
            if (r.command & USED) { advance(v, &r); continue; }
            v->bank[r.pc + 1] |= USED;
        }
        if (!r.length) { v->end_requested = 1; return 1; }
        for (i = 0; i < r.length; i += 2)
            if (!bit_word(v, be16(v->bank + r.pc + 2 + i), 1)) return 0;
        advance(v, &r);
    }
}
static int condition(BloodUpcom *v, const Record *r)
{
    unsigned at = 0, raw;
    if (!r->length) return 1;
    if (r->kind != 0x80) {
        raw = be16(v->bank + r->pc + 2);
        /* 429D JA failure / 42AE JB failure: BOTH are inclusive at equality. */
        if (r->kind == 0xa0 && raw > v->minutes) return 0;
        if (r->kind == 0xc0 && raw < v->minutes) return 0;
        at = 2;
    }
    for (; at < r->length; at += 2) {
        if (!bit_word(v, be16(v->bank + r->pc + 2 + at), 0)) return 0;
        if (v->error) return 0;
    }
    return 1;
}
/* 427F: adjacent conditional records form a conjunction. On first failure,
 * remaining adjacent conditions are stepped over (not evaluated), but their
 * command bits still contribute to the skip/stop policy. */
static int condition_group(BloodUpcom *v)
{
    Record r;
    int accepted = 1;
    for (;;) {
        if (!read_record(v, &r)) return 0;
        if (r.sentinel || !(r.kind & 0x80u)) return accepted;
        if (accepted && !condition(v, &r)) accepted = 0;
        if (v->error) return 0;
        v->condition_mode |= (uint8_t)r.command;
        advance(v, &r);
    }
}
/* Enumerate input symbols, retaining only token IDs for matching. Operands
 * accompanying a coordinate/time token do NOT participate in 430A matching. */
static int input_ids(BloodUpcom *v, uint8_t out[32], unsigned *count)
{
    size_t p = 0;
    unsigned n = 0;
    while (p < BLOOD_PHRASE_BYTES && v->input[p] != 0xff) {
        unsigned step = v->input[p] >= 1 && v->input[p] <= 3 ? 3u : 1u;
        if (p + step >= BLOOD_PHRASE_BYTES || n == 32) return fail(v, -1);
        out[n++] = v->input[p]; p += step;
    }
    if (p >= BLOOD_PHRASE_BYTES) return fail(v, -1);
    *count = n;
    return 1;
}
static int contains(const uint8_t *p, unsigned n, unsigned token)
{
    unsigned i;
    for (i = 0; i < n; ++i) if (p[i] == token) return 1;
    return 0;
}
/* 430A..4443: source-faithful text eligibility, NOT an English string matcher. */
static int text_match(BloodUpcom *v, const Record *r)
{
    uint8_t input[32], tokens[32];
    unsigned input_n, token_n = 0, i, p, any_required = 0;
    if (!(r->command & 1u) && (r->command & USED)) return 0;
    if (r->command & 8u)
        if (((blood_upcom_random(v) >> 8) & 7u) == 7u) return 0;
    if (!input_ids(v, input, &input_n)) return 0;
    for (p = r->pc + 2u; p < r->next; ) {
        unsigned token = v->bank[p];
        tokens[token_n++] = (uint8_t)token;
        p += token <= 3u ? 3u : 1u;
    }
    if (r->command & 4u) {
        /* 4332..439C accepts an ORDERED SUBSEQUENCE, allowing intervening
         * and trailing input tokens. The archived ST legend says exact
         * match; do not silently substitute that for these DOS instructions. */
        unsigned at = 0;
        if (!token_n) return 0;
        for (i = 0; i < input_n && at < token_n; ++i)
            if (input[i] == tokens[at]) ++at;
        return at == token_n;
    }
    if (r->command & 2u) return 1;
    if (!token_n) return 0;
    for (i = 0; i < token_n; ++i) {
        if (word_flag(v, r->wi + i, 0)) {
            ++any_required;
            if (!contains(input, input_n, tokens[i])) return 0;
        }
    }
    if (v->error) return 0;
    if (any_required) return 1;
    for (i = 0; i < token_n; ++i)
        if (!word_flag(v, r->wi + i, 0) && !word_flag(v, r->wi + i, 2) &&
            contains(input, input_n, tokens[i])) return 1;
    return 0;
}
static int emit(BloodUpcom *v, const Record *r)
{
    unsigned p, word = r->wi, used = 0;
    for (p = r->pc + 2u; p < r->next; ++word) {
        unsigned n = v->bank[p] <= 3u ? 3u : 1u;
        if (!word_flag(v, word, 1)) {
            if (used + n >= BLOOD_PHRASE_BYTES) return fail(v, -1);
            memcpy(v->reply + used, v->bank + p, n); used += n;
        }
        p += n;
    }
    if (v->error) return 0;
    if (!used) return 0;
    v->reply[used] = 0xff;
    v->yielded = 1;
    v->last_text_pc = r->pc;
    return 1;
}
int blood_upcom_attach(BloodUpcom *v, uint8_t *body, size_t size)
{
    Record r;
    if (!v || !body || size < 18 || size > 65536u) return -1;
    v->bank = body; v->bank_size = size;
    v->error = 0; v->steps = 0;
    v->start_pc = (uint16_t)le16(body); v->start_word = 0;
    v->pc = v->start_pc; v->word_index = 0;
    v->last_text_pc = 0xffff;
    if (v->pc < 16 || v->pc >= size) return -1;
    for (;;) {
        if (!read_record(v, &r)) return v->error;
        if (r.sentinel) break;
        if (!r.kind) v->bank[r.pc + 1] &= 0x7fu;
        advance(v, &r);
    }
    v->pc = v->start_pc; v->word_index = v->start_word;
    return 0;
}
int blood_upcom_encounter(BloodUpcom *v, unsigned slot, uint8_t wanted_y)
{
    unsigned i;
    if (!v || slot > 3) return -1;
    for (i = 1; i <= 29; ++i) blood_set_flag(v, i, 0);
    blood_set_flag(v, slot + 1, 1);
    if (wanted_y & 0x80u) blood_set_flag(v, 8, 1);
    v->active = v->end_requested = v->yielded = 0;
    return 0;
}
int blood_upcom_scan(BloodUpcom *v, int fresh)
{
    Record r, selected;
    if (!v || !v->bank) return -1;
    v->error = 0; v->steps = 0; v->last_text_pc = 0xffff;
    if (fresh) {
        v->force_countdown = (uint8_t)(v->force_countdown - 1u);
        if (!v->force_countdown || ((blood_upcom_random(v) >> 8) & 15u) == 3u) {
            blood_set_flag(v, 7, 1); v->force_countdown = 16;
        }
        blood_set_flag(v, 9, 0);
        if (((blood_upcom_random(v) >> 8) & 7u) == 2u) blood_set_flag(v, 9, 1);
        v->condition_mode = 0; v->reserved_796 = 0;
        v->pc = v->start_pc; v->word_index = v->start_word;
        if (!assignments(v)) return v->error;
    }
    for (;;) {
        v->condition_mode = 0;
        if (!read_record(v, &r)) return v->error;
        if (r.sentinel) return 0;
        if (r.kind & 0x80u) {
            if (condition_group(v)) {
                if (!assignments(v)) return v->error;
                continue;
            }
            if (v->error) return v->error;
            if (v->condition_mode & 0x80u) return 0;
            /* 4260: skip body; if mode 40 was accumulated, ignore further
             * condition blocks until a header with boundary bit 20 occurs. */
            for (;;) {
                if (!read_record(v, &r)) return v->error;
                if (r.sentinel) return 0;
                if ((r.kind & 0x80u) &&
                    (!(v->condition_mode & 0x40u) || (r.command & 0x20u))) break;
                advance(v, &r);
            }
            continue;
        }
        if (r.kind == 0x40u) { advance(v, &r); continue; }
        selected = r;
        if (!text_match(v, &r)) {
            if (v->error) return v->error;
            advance(v, &r); continue;
        }
        v->bank[r.pc + 1] |= USED;
        advance(v, &r);
        if (!assignments(v)) return v->error;
        if (emit(v, &selected)) return 1;
        if (v->error) return v->error;
    }
}
int blood_upcom_begin(BloodUpcom *v)
{
    unsigned i;
    int rc = 0;
    if (!v) return -1;
    memset(v->input, 0xff, 8); v->force_countdown = 16;
    v->end_requested = v->yielded = 0;
    for (i = 0; i < 8; ++i) {
        rc = blood_upcom_scan(v, 1);
        if (rc < 0 || v->yielded) break;
    }
    v->active = v->yielded != 0;
    return rc;
}
int blood_upcom_submit(BloodUpcom *v, const uint8_t *tokens, size_t bytes)
{
    int rc;
    if (!v || (!tokens && bytes) || bytes >= BLOOD_PHRASE_BYTES) return -1;
    if (bytes) memcpy(v->input, tokens, bytes);
    v->input[bytes] = 0xff;
    v->end_requested = v->yielded = 0;
    rc = blood_upcom_scan(v, 1);
    v->active = v->yielded != 0;
    return rc;
}
int blood_upcom_next(BloodUpcom *v)
{
    int rc;
    if (!v) return -1;
    v->end_requested = v->yielded = 0;
    rc = blood_upcom_scan(v, !v->active);
    v->active = v->yielded != 0;
    return rc;
}
