/* SPDX-License-Identifier: MIT */
/* Digitakt Mk1 OS 1.53 adapter for the two-operator FM engine. */
#include "sophie.h"

typedef unsigned char u8;
typedef unsigned short u16;
typedef signed short s16;
typedef signed long s32;
typedef unsigned long u32;

#define DS_MACHINE 7
#define TRACKS 8
#define TBUF(t) ((s32 *)(unsigned long)(0x80001a18u + 128u * (u32)(t)))
#define MACH(t) (*(volatile const u8 *)(unsigned long)(0x800018bcu + (u32)(t)))
#define VP(t, o) (*(volatile const s16 *)(unsigned long)(0x80002794u + 106u * (u32)(t) + (u32)(o)))
#define NOTE(t) (*(volatile const s32 *)(unsigned long)(0x80001f28u + 4u * (u32)(t)))
#define VEL(t) (*(volatile const s16 *)(unsigned long)(0x80001f18u + 2u * (u32)(t)))
#define TRIG_BITS (*(volatile const u32 *)(unsigned long)0x80001228u)
#define AMP_LEVEL(t) (*(volatile const s32 *)(unsigned long)(0x4199df58u + 12u * (u32)(t)))
#define AMP_PHASE(t) (*(volatile const s32 *)(unsigned long)(0x4199df54u + 12u * (u32)(t)))
#define PITCH_TAB ((const u32 *)(unsigned long)0x4019b1c0u)

/* SLICE's persistent SRC slots: A..H.  Core makes those values recallable,
 * lockable and reachable via MIDI CC/NRPN for custom machines. */
#define P_TUNE 0
#define P_RATIO 2
#define P_INDEX 4
#define P_ATTACK 8
#define P_DECAY 10
#define P_FEEDBACK 12
#define P_TONE 14

static struct ds_voice ds_voices[TRACKS];

char *ds_fmt_ratio(char *out, s32 value)
{
    u32 n = ((u32)value >> 8) & 0x7fu;
    u32 centi = 25u + ((n * 375u + 63u) / 127u); /* 0.25x .. 4.00x */
    u32 whole = centi / 100u;
    u32 frac = centi % 100u;
    out[0] = (char)('0' + whole);
    out[1] = '.';
    out[2] = (char)('0' + frac / 10u);
    out[3] = (char)('0' + frac % 10u);
    out[4] = 0;
    return out;
}

char *ds_fmt_time(char *out, s32 value)
{
    u32 n = ((u32)value >> 8) & 0x7fu;
    u32 ms = 2u + ((n * 998u + 63u) / 127u);
    char *p = out;
    if (ms >= 1000u) {
        *p++ = '1';
        ms -= 1000u;
        *p++ = (char)('0' + ms / 100u);
    } else if (ms >= 100u) {
        *p++ = (char)('0' + ms / 100u);
        ms %= 100u;
    }
    if (ms >= 10u || p != out) {
        *p++ = (char)('0' + ms / 10u);
        ms %= 10u;
    }
    *p++ = (char)('0' + ms);
    *p = 0;
    return out;
}

char *ds_fmt_u7(char *out, s32 value)
{
    u32 n = ((u32)value >> 8) & 0x7fu;
    char *p = out;
    if (n >= 100) { *p++ = '1'; n -= 100; *p++ = (char)('0' + n / 10); }
    else if (n >= 10) *p++ = (char)('0' + n / 10);
    *p++ = (char)('0' + n % 10);
    *p = 0;
    return out;
}

static u32 ds_u7(s32 track, s32 offset)
{ return ((u32)(u16)VP(track, offset) >> 8) & 0x7fu; }

static u32 ds_pitch_ratio(s32 track)
{
    static const u16 semitone_q15[13] = {
        32768, 34716, 36781, 38968, 41285, 43740, 46341,
        49097, 52016, 55109, 58386, 61858, 65536
    };
    enum { TAB_MAX_PITCH = 87 << 16 };
    s32 pitch = ((s32)VP(track, P_TUNE) - 0x4000) * 256
        + NOTE(track) + (3 << 16);
    u32 ratio;
    if (pitch < 0) pitch = 0;
    if (pitch <= TAB_MAX_PITCH)
        return PITCH_TAB[(u32)pitch / 384u]; /* Q29, as stock playback uses. */

    /* Stock table reads stay in-range; higher notes are extrapolated. */
    ratio = PITCH_TAB[(u32)TAB_MAX_PITCH / 384u];
    pitch -= TAB_MAX_PITCH;
    while (pitch >= (12 << 16) && ratio < 0x80000000u) {
        ratio <<= 1;
        pitch -= 12 << 16;
    }
    if (pitch > 0 && ratio < 0xffffffffu) {
        u32 semitone = (u32)pitch >> 16;
        u32 frac = (u32)pitch & 0xffffu;
        u32 lo = semitone_q15[semitone];
        u32 hi = semitone_q15[semitone + 1u];
        u32 mul = lo + (u32)((((uint64_t)(hi - lo) * frac) + 32768u) >> 16);
        uint64_t scaled = (((uint64_t)ratio * mul) + 16384u) >> 15;
        ratio = scaled > 0xffffffffu ? 0xffffffffu : (u32)scaled;
    }
    return ratio;
}

static void ds_read_params(s32 track, struct ds_params *p)
{
    u32 ratio = ds_pitch_ratio(track);
    u32 phase_inc = ratio >> 23;
    if (phase_inc < 8) phase_inc = 8;
    if (phase_inc > 32767u) phase_inc = 32767u;
    p->phase_inc = (u16)phase_inc; /* 50 Hz is 68 phase units/sample. */
    p->ratio = (u8)ds_u7(track, P_RATIO);
    p->index = (u8)ds_u7(track, P_INDEX);
    p->attack = (u8)ds_u7(track, P_ATTACK);
    p->decay = (u8)ds_u7(track, P_DECAY);
    p->feedback = (u8)ds_u7(track, P_FEEDBACK);
    p->tone = (u8)ds_u7(track, P_TONE);
    p->velocity = (u8)(((u32)(u16)VEL(track) >> 8) & 0x7fu);
}

void ds_inject(void)
{
    u32 triggers = TRIG_BITS;
    s32 track;
    for (track = 0; track < TRACKS; ++track) {
        struct ds_params params;
        u32 i;
        int trigger;
        if (MACH(track) != DS_MACHINE) {
            if (ds_voices[track].active) ds_voice_init(&ds_voices[track]);
            continue;
        }
        trigger = (triggers & (1u << track)) != 0;
        ds_voice_gate(&ds_voices[track], AMP_LEVEL(track), AMP_PHASE(track));
        if (!trigger && (!ds_voices[track].active || ds_voices[track].sleeping)) {
            s32 *out = TBUF(track);
            for (i = 0; i < DS_BLOCK_SIZE; ++i) out[i] = 0;
            continue;
        }
        ds_read_params(track, &params);
        ds_voice_render(&ds_voices[track], &params, trigger,
                        TBUF(track), DS_BLOCK_SIZE);
    }
}
