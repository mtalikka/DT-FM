/* SPDX-License-Identifier: MIT */
/* Digitakt Mk1 OS 1.53 adapter for the four-operator FM engine. */
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
/* The render's live sound per track: s16 params at +20 + 2*slot, machine at +126. */
#define SOUND(t) (*(u8 *volatile const *)(unsigned long)(0x800019b4u + 4u * (u32)(t)))
#define S_PARAM(s, slot) (*(volatile s16 *)((s) + 20 + 2 * (slot)))

/* SLICE's persistent SRC slots: A..H.  Core makes those values recallable,
 * lockable and reachable via MIDI CC/NRPN for custom machines. */
#define P_TUNE 0
#define P_ALGO 2
#define P_CHAR 14
#define S_OP 0x14
#define DS_OP_INIT {{16, 127, 0, 127}, {16, 80, 0, 90}, {36, 40, 0, 70}, {16, 30, 0, 60}}

static struct ds_voice ds_voices[TRACKS];
/* Knobs C, E, F, G edit the selected operator; ds_op keeps all four per track. */
static const u8 ds_op_slot[4] = {0x13, 0x15, 0x16, 0x17};
static const u8 ds_op_init[DS_OPS][4] = DS_OP_INIT;
static u8 ds_op[TRACKS][DS_OPS][4] = {
    DS_OP_INIT, DS_OP_INIT, DS_OP_INIT, DS_OP_INIT,
    DS_OP_INIT, DS_OP_INIT, DS_OP_INIT, DS_OP_INIT
};
static u8 *ds_snd[TRACKS];
static u8 ds_sel[TRACKS];
extern volatile u8 ds_d_turned;

char *ds_fmt_ratio(char *out, s32 value)
{
    u32 c = ds_ratio_centi[(((u32)value >> 8) & 0x7fu) >> 2];
    if (c >= 1000u) {
        out[0] = (char)('0' + c / 1000u);
        out[1] = (char)('0' + c / 100u % 10u);
        out[2] = '.';
        out[3] = (char)('0' + c / 10u % 10u);
    } else {
        out[0] = (char)('0' + c / 100u);
        out[1] = '.';
        out[2] = (char)('0' + c / 10u % 10u);
        out[3] = (char)('0' + c % 10u);
    }
    out[4] = 0;
    return out;
}

char *ds_fmt_op(char *out, s32 value)
{
    u32 n = (((u32)value >> 8) & 0x7fu) >> 3;
    out[0] = (char)('1' + (n > 3u ? 3u : n));
    out[1] = 0;
    return out;
}

char *ds_fmt_algo(char *out, s32 value)
{
    u32 n = ((u32)value >> 8) & 0x7fu;
    out[0] = (char)('1' + (n >> 4));
    out[1] = 0;
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

char *ds_fmt_decay(char *out, s32 value)
{
    if ((((u32)value >> 8) & 0x7fu) == 127u) {
        out[0] = 'I';
        out[1] = 'N';
        out[2] = 'F';
        out[3] = 0;
        return out;
    }
    return ds_fmt_u7(out, value);
}

static u32 ds_u7(s32 track, s32 offset)
{ return ((u32)(u16)VP(track, offset) >> 8) & 0x7fu; }

static u32 ds_mul_u32_q15_sat(u32 value, u32 mul_q15)
{
    u32 integer = mul_q15 >> 15;      /* 0..2 for our semitone table */
    u32 fraction = mul_q15 & 0x7fffu; /* 0..32767 */
    u32 result = 0;

    if (integer == 1u) {
        result = value;
    } else if (integer == 2u) {
        if (value > 0x7fffffffu) return 0xffffffffu;
        result = value << 1;
    }

    if (fraction) {
        /* ((value * fraction) + 16384) >> 15 without 64-bit multiply. */
        u32 hi = value >> 15;
        u32 lo = value & 0x7fffu;
        u32 term = hi * fraction + ((lo * fraction + 16384u) >> 15);
        if (result > 0xffffffffu - term) return 0xffffffffu;
        result += term;
    }
    return result;
}

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
        u32 mul = lo + (((hi - lo) * frac + 32768u) >> 16);
        ratio = ds_mul_u32_q15_sat(ratio, mul);
    }
    return ratio;
}

static void ds_read_params(s32 track, struct ds_params *p)
{
    u32 k;
    u32 ratio = ds_pitch_ratio(track);
    u32 phase_inc = ratio >> 23;
    if (phase_inc < 8) phase_inc = 8;
    if (phase_inc > 32767u) phase_inc = 32767u;
    p->phase_inc = (u16)phase_inc; /* 50 Hz is 68 phase units/sample. */
    p->algo = (u8)(ds_u7(track, P_ALGO) >> 4);         /* B: 8 zones */
    p->feedback = p->tone = (u8)ds_u7(track, P_CHAR);  /* H: CHAR macro */
    p->velocity = (u8)(((u32)(u16)VEL(track) >> 8) & 0x7fu);
    for (k = 0; k < DS_OPS; ++k) {
        const u8 *o = ds_op[track][k];
        p->op[k].ratio = o[0];
        p->op[k].level = o[1];
        p->op[k].attack = o[2];
        p->op[k].decay = o[3];
    }
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

static u32 ds_s7(u8 *s, u32 slot)
{ return ((u32)(u16)S_PARAM(s, slot) >> 8) & 0x7fu; }

static void ds_wc_sync(void);

void ds_tick(void *ctrl)
{
    u32 t, j, sel, redraw = 0;
    for (t = 0; t < TRACKS; ++t) {
        u8 *s = SOUND(t);
        u8 cur[4];
        if (!s || s[126] != DS_MACHINE) continue;
        sel = ds_s7(s, S_OP) >> 3;
        if (sel > 3u) sel = 3u;
        for (j = 0; j < 4u; ++j) cur[j] = (u8)ds_s7(s, ds_op_slot[j]);
        /* Swap only on a D turn: a new pattern or loaded sound brings its own op. */
        if (sel != ds_sel[t] && s == ds_snd[t] && ds_d_turned) {
            for (j = 0; j < 4u; ++j) {
                ds_op[t][ds_sel[t]][j] = cur[j];
                cur[j] = ds_op[t][sel][j];
                S_PARAM(s, ds_op_slot[j]) = (s16)(cur[j] << 8);
            }
            redraw = 1;
        }
        for (j = 0; j < 4u; ++j) ds_op[t][sel][j] = cur[j];
        ds_sel[t] = (u8)sel;
        ds_snd[t] = s;
    }
    ds_d_turned = 0;
    if (redraw) *((volatile u8 *)ctrl + 0x20) = 1;
    ds_wc_sync();
}

/* Project block +0x20..+0x200, skipped by SERIALIZE and DESERIALIZE, in
 * core-dn1's projdata layout: 'ELKP', then tag, size, bytes, zero tag. */
#define PROJ_GAP 0x20
#define PROJ_MAGIC 0x454c4b50u
#define PROJ_TAG 0x464d324fu
#define PROJ_HEAD 0xbeefbaceu
/* The power-up working copy: dumped to NAND at power-off and DESERIALIZEd
 * at boot, but re-serialized only on project load, so ds_wc_sync keeps our
 * gap in it current. The COKI checksum covers only its header. */
#define WC_DATA ((u8 *)0x406481f8)

static u8 ds_live;     /* ds_op came from (or went to) a project */

void ds_proj_put(u8 *data)
{
    u32 *w = (u32 *)(data + PROJ_GAP);
    const u8 *src = &ds_op[0][0][0];
    u8 *dst = (u8 *)(w + 3);
    u32 i;
    ds_live = 1;
    w[0] = PROJ_MAGIC;
    w[1] = PROJ_TAG;
    w[2] = sizeof ds_op;
    for (i = 0; i < sizeof ds_op; ++i) dst[i] = src[i];
    w[3 + sizeof ds_op / 4] = 0;
}

void ds_proj_get(const u8 *data)
{
    const u32 *w = (const u32 *)(data + PROJ_GAP);
    const u8 *src = (const u8 *)(w + 3);
    u8 *dst = &ds_op[0][0][0];
    u32 i;
    ds_live = 1;
    if (w[0] == PROJ_MAGIC && w[1] == PROJ_TAG && w[2] == sizeof ds_op)
        for (i = 0; i < sizeof ds_op; ++i) dst[i] = src[i];
    else
        for (i = 0; i < sizeof ds_op; ++i) dst[i] = (&ds_op_init[0][0])[i & 15u];
}

/* Not before the boot restore has run (ds_live), so defaults never
 * overwrite the persisted block. */
static void ds_wc_sync(void)
{
    const u32 *w = (const u32 *)(WC_DATA + PROJ_GAP);
    const u8 *a = (const u8 *)(w + 3), *b = &ds_op[0][0][0];
    u32 i;
    if (!ds_live || *(const u32 *)WC_DATA != PROJ_HEAD) return;
    if (w[0] == PROJ_MAGIC && w[1] == PROJ_TAG && w[2] == sizeof ds_op) {
        for (i = 0; i < sizeof ds_op && a[i] == b[i]; ++i) {}
        if (i == sizeof ds_op) return;
    }
    ds_proj_put(WC_DATA);
}
