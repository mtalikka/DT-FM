/* SPDX-License-Identifier: MIT */
/* Digitakt Mk1 OS 1.53 and 1.54 adapter for the four-operator FM engine: a machine for elekloader's core
 * 3.0, whose SRC page the machine-pages mod draws from ds_page. */
#include "sophie.h"
#include "digitakt-mk1/core3.h"
#ifdef OS154                        /* the Digitakt mk1 1.54 (mod.json's port) */
#include "os154.h"
#else                               /* the Digitakt mk1 1.53 */
#include "os153.h"
#endif

typedef unsigned char u8;
typedef unsigned short u16;
typedef signed short s16;
typedef signed long s32;
typedef unsigned long u32;

#define DS_MACHINE 9
#define TRACKS 8
#define MACH(t) (*(volatile const u8 *)(unsigned long)(OS_MACH + (u32)(t)))
#define VP(t, o) (*(volatile const s16 *)(unsigned long)(OS_VP + 106u * (u32)(t) + (u32)(o)))
#define NOTE(t) (*(volatile const s32 *)(unsigned long)(OS_NOTE + 4u * (u32)(t)))
#define VEL(t) (*(volatile const s16 *)(unsigned long)(OS_VEL + 2u * (u32)(t)))
#define TRIG_BITS (*(volatile const u32 *)(unsigned long)OS_TRIG_BITS)
#define AMP_LEVEL(t) (*(volatile const s32 *)(unsigned long)(OS_AMP_LEVEL + 12u * (u32)(t)))
#define AMP_PHASE(t) (*(volatile const s32 *)(unsigned long)(OS_AMP_PHASE + 12u * (u32)(t)))
#define PITCH_TAB ((const u32 *)(unsigned long)OS_PITCH_TAB)
/* The render's live sound per track: s16 params at +20 + 2*slot, machine at +126. */
#define SOUND(t) (*(u8 *volatile const *)(unsigned long)(OS_SOUND + 4u * (u32)(t)))
#define S_PARAM(s, slot) (*(volatile s16 *)((s) + 20 + 2 * (slot)))

/* SLICE's persistent SRC slots: A..H.  Core makes those values recallable,
 * lockable and reachable via MIDI CC/NRPN for custom machines. */
#define P_TUNE 0
#define P_ALGO 2
#define P_CHAR 14
#define S_OP 0x14
#define DS_OP_INIT {{16, 127, 0, 127}, {16, 80, 0, 90}, {36, 40, 0, 70}, {16, 30, 0, 60}}

static struct ds_voice ds_voices[TRACKS];
/* Knobs C, E, F, G show one operator; the other three ride in the sound's
 * slots 0x2e-0x34, which no parameter uses and the OS neither saves nor
 * restores (it zeroes them on load), so whole-sound copies carry them.
 * Byte 0 is HID_MARK | the knobs' operator, then 4 bytes per other op. */
#define HID 112
#define HID_MARK 0xd0u
#define KITS 128
#define LIVE_PROJECT ((const u8 *)OS_PROJECT)
#define KIT0 ((u8 *)(OS_PROJECT + 0xf6341cu))
#define KIT_SIZE 2334u
#define SND_SIZE 162u
#define LIVE_KIT (*(const u8 *volatile const *)OS_LIVE_KIT)
#define KIT_OPS (TRACKS * DS_OPS * 4)
static const u8 ds_op_slot[4] = {0x13, 0x15, 0x16, 0x17};
static const u8 ds_op_init[DS_OPS][4] = DS_OP_INIT;

static u32 ds_s7(const u8 *s, u32 slot)
{ return ((u32)(u16)S_PARAM(s, slot) >> 8) & 0x7fu; }

static u32 ds_op_sel(const u8 *s)
{
    u32 sel = ds_s7(s, S_OP) >> 3;
    return sel > 3u ? 3u : sel;
}

/* All four operators; an unmarked (new or pool-loaded) sound's hidden ones
 * are the defaults. */
static void ds_sound_ops(const u8 *s, u8 o[DS_OPS][4])
{
    const u8 *h = s + HID;
    u32 marked = (h[0] & 0xfcu) == HID_MARK;
    u32 sel = marked ? h[0] & 3u : ds_op_sel(s), i, j;
    for (i = 0; i < DS_OPS; ++i)
        for (j = 0; j < 4u; ++j)
            o[i][j] = i == sel ? (u8)ds_s7(s, ds_op_slot[j])
                : marked ? h[1 + 4 * (i - (i > sel)) + j] : ds_op_init[i][j];
}

static void ds_sound_hide(u8 *s, const u8 o[DS_OPS][4], u32 sel)
{
    u8 *h = s + HID;
    u32 i, j;
    for (i = 0; i < DS_OPS; ++i)
        if (i != sel)
            for (j = 0; j < 4u; ++j) h[1 + 4 * (i - (i > sel)) + j] = o[i][j] & 0x7fu;
    h[13] = 0;
    h[0] = (u8)(HID_MARK | sel);
}

static int32_t ds_fmt_ratio(char *out, int32_t value, int32_t ctx, int32_t machine)
{
    u32 c = ds_ratio_centi[(((u32)value >> 8) & 0x7fu) >> 2];
    (void)ctx;
    (void)machine;
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
    return 1;
}

static int32_t ds_fmt_op(char *out, int32_t value, int32_t ctx, int32_t machine)
{
    u32 n = (((u32)value >> 8) & 0x7fu) >> 3;
    (void)ctx;
    (void)machine;
    out[0] = (char)('1' + (n > 3u ? 3u : n));
    out[1] = 0;
    return 1;
}

static int32_t ds_fmt_algo(char *out, int32_t value, int32_t ctx, int32_t machine)
{
    u32 n = ((u32)value >> 8) & 0x7fu;
    (void)ctx;
    (void)machine;
    out[0] = (char)('1' + (n >> 4));
    out[1] = 0;
    return 1;
}

static int32_t ds_fmt_u7(char *out, int32_t value, int32_t ctx, int32_t machine)
{
    u32 n = ((u32)value >> 8) & 0x7fu;
    char *p = out;
    (void)ctx;
    (void)machine;
    if (n >= 100) { *p++ = '1'; n -= 100; *p++ = (char)('0' + n / 10); }
    else if (n >= 10) *p++ = (char)('0' + n / 10);
    *p++ = (char)('0' + n % 10);
    *p = 0;
    return 1;
}

static int32_t ds_fmt_decay(char *out, int32_t value, int32_t ctx, int32_t machine)
{
    if ((((u32)value >> 8) & 0x7fu) == 127u) {
        out[0] = 'I';
        out[1] = 'N';
        out[2] = 'F';
        out[3] = 0;
        return 1;
    }
    return ds_fmt_u7(out, value, ctx, machine);
}

/* ALGO's 17x17 diagrams as {x0, row0, x1, row1} boxes, row 0 on top:
 * modulators above their targets, carriers standing on the output bar. */
static const u8 ds_algo_box[][4] = {
    {7, 0, 9, 2}, {7, 4, 9, 6}, {7, 8, 9, 10}, {7, 12, 9, 14}, {8, 3, 8, 15},
    {5, 16, 11, 16},
    {4, 2, 6, 4}, {10, 2, 12, 4}, {5, 5, 11, 5}, {7, 6, 9, 8}, {7, 10, 9, 12},
    {8, 9, 8, 13}, {5, 14, 11, 14},
    {10, 2, 12, 4}, {11, 5, 11, 5}, {4, 6, 6, 8}, {10, 6, 12, 8}, {5, 9, 11, 9},
    {7, 10, 9, 12}, {8, 13, 8, 13}, {5, 14, 11, 14},
    {2, 4, 4, 6}, {7, 4, 9, 6}, {12, 4, 14, 6}, {3, 7, 13, 7}, {7, 8, 9, 10},
    {8, 11, 8, 11}, {5, 12, 11, 12},
    {4, 4, 6, 6}, {10, 4, 12, 6}, {4, 8, 6, 10}, {10, 8, 12, 10}, {5, 7, 5, 11},
    {11, 7, 11, 11}, {4, 12, 12, 12},
    {7, 4, 9, 6}, {3, 7, 13, 7}, {2, 8, 4, 10}, {7, 8, 9, 10}, {12, 8, 14, 10},
    {3, 11, 3, 11}, {8, 11, 8, 11}, {13, 11, 13, 11}, {2, 12, 14, 12},
    {12, 4, 14, 6}, {13, 7, 13, 7}, {2, 8, 4, 10}, {7, 8, 9, 10}, {12, 8, 14, 10},
    {3, 11, 3, 11}, {8, 11, 8, 11}, {13, 11, 13, 11}, {2, 12, 14, 12},
    {1, 6, 3, 8}, {5, 6, 7, 8}, {9, 6, 11, 8}, {13, 6, 15, 8}, {2, 9, 2, 9},
    {6, 9, 6, 9}, {10, 9, 10, 9}, {14, 9, 14, 9}, {1, 10, 15, 10}
};
static const u8 ds_algo_at[9] = {0, 6, 13, 21, 28, 35, 44, 53, 62};

/* Knob B's graphic: the algorithm's diagram, in the knob's 17x17 area. */
static int32_t ds_draw_algo(void *bmp, int32_t x, int32_t y, int32_t value, int32_t flag)
{
    u32 a = (((u32)value >> 8) & 0x7fu) >> 4, i;
    (void)flag;
    for (i = ds_algo_at[a]; i < ds_algo_at[a + 1]; ++i) {
        const u8 *b = ds_algo_box[i];
        fw_fillrect(bmp, x + 1 + b[0], y + 16 - b[3], x + 1 + b[2], y + 16 - b[1], 1);
    }
    return 1;
}

/* SAMP's framed number: (fn, value 8.8, bmp, x, y, flag). */
#define NUM_BOX ((void (*)(s32, s32, void *, s32, s32, s32))OS_NUM_BOX)

/* Knob D's graphic: the operator's number, 1-4, in SAMP's frame. */
static int32_t ds_draw_op(void *bmp, int32_t x, int32_t y, int32_t value, int32_t flag)
{
    u32 n = (((u32)value >> 8) & 0x7fu) >> 3;
    NUM_BOX(0, (s32)((n > 3u ? 3u : n) + 1u) << 8, bmp, x, y, flag);
    return 1;
}

/* The machine menu's icon: an 11 x 7 Bitmap, as the stock icons are. */
static const u32 ds_icon_px[11] = {
    0x10400000, 0x28a00000, 0x55400000, 0xaa800000, 0x55400000, 0x28a00000, 0x10400000
};
static const u32 ds_icon_mask[11] = {
    0xfe000000, 0xfe000000, 0xfe000000, 0xfe000000, 0xfe000000, 0xfe000000, 0xfe000000
};
static const struct { const void *vt; u32 w, h, one; const u32 *px, *mask; u32 end; } ds_icon = {
    fw_bitmap_vt, 11, 7, 1, ds_icon_px, ds_icon_mask, 0
};

/* The SRC page machine-pages draws: SLICE's, so the knobs keep SLICE's parameter slots (locks, MIDI and
 * the LFOs reach them), and every knob but TUNE turns as BR's (0x86) does, over its own range. */
static const struct cm_ui ds_page = {
    .abi = cm_ui_v31, .page_from = 3, .group = "DTFM",
    .knob = {
        { .name = "TUNE", .lname = "Tune" },
        { .name = "ALGO", .lname = "Algorithm", .look = 0x86, .flags = CM_RANGE | CM_DEFAULT | CM_DRAW,
          .max = 0x7f00, .def = 0, .fmt = ds_fmt_algo, .draw = ds_draw_algo },
        { .name = "RATIO", .lname = "Ratio", .look = 0x86, .flags = CM_RANGE | CM_DEFAULT,
          .max = 0x7f00, .def = 0x1000, .fmt = ds_fmt_ratio },
        { .name = "OP", .lname = "Operator", .look = 0x86,
          .flags = CM_RANGE | CM_DEFAULT | CM_NOT_SAMPLE | CM_DRAW,
          .max = 0x1f00, .def = 0, .fmt = ds_fmt_op, .draw = ds_draw_op },
        { .name = "LEVEL", .lname = "Level", .look = 0x86, .flags = CM_RANGE | CM_DEFAULT,
          .max = 0x7f00, .def = 0x7f00, .fmt = ds_fmt_u7 },
        { .name = "ATTK", .lname = "Attack", .look = 0x86, .flags = CM_RANGE | CM_DEFAULT,
          .max = 0x7f00, .def = 0, .fmt = ds_fmt_u7 },
        { .name = "DECAY", .lname = "Decay", .look = 0x86, .flags = CM_RANGE | CM_DEFAULT,
          .max = 0x7f00, .def = 0x7f00, .fmt = ds_fmt_decay },
        { .name = "CHAR", .lname = "Character", .look = 0x86, .flags = CM_RANGE | CM_DEFAULT,
          .max = 0x7f00, .def = 0x4000, .fmt = ds_fmt_u7 },
    },
};

/* Params 3: SLICE's eight parameters; render DS_MACHINE: its own voice window, which ds_inject fills. */
const struct cm_machine ds_machine = {
    DS_MACHINE, "DT-FM", "DTFM", &ds_icon, 3, DS_MACHINE, CM_UI_TAG, &ds_page
};

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
    const u8 *s = SOUND(track);
    u8 ops[DS_OPS][4];
    if (s) ds_sound_ops(s, ops);
    else for (k = 0; k < sizeof ops; ++k) (&ops[0][0])[k] = (&ds_op_init[0][0])[k];
    if (phase_inc < 8) phase_inc = 8;
    if (phase_inc > 32767u) phase_inc = 32767u;
    p->phase_inc = (u16)phase_inc; /* 50 Hz is 68 phase units/sample. */
    p->algo = (u8)(ds_u7(track, P_ALGO) >> 4);         /* B: 8 zones */
    p->feedback = p->tone = (u8)ds_u7(track, P_CHAR);  /* H: CHAR macro */
    p->velocity = (u8)(((u32)(u16)VEL(track) >> 8) & 0x7fu);
    for (k = 0; k < DS_OPS; ++k) {
        const u8 *o = ops[k];
        p->op[k].ratio = o[0];
        p->op[k].level = o[1];
        p->op[k].attack = o[2];
        p->op[k].decay = o[3];
    }
}

/* machine-pages' ev_render_voices, after playback has written every track's block. */
void ds_inject(int32_t *blocks)
{
    u32 triggers = TRIG_BITS;
    s32 track;
    for (track = 0; track < TRACKS; ++track) {
        int32_t *out = blocks + DS_BLOCK_SIZE * track;
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
            for (i = 0; i < DS_BLOCK_SIZE; ++i) out[i] = 0;
            continue;
        }
        ds_read_params(track, &params);
        ds_voice_render(&ds_voices[track], &params, trigger, out, DS_BLOCK_SIZE);
    }
}

static void ds_wc_sync(void);

void ds_tick(void *ctrl)
{
    u32 t, sel, redraw = 0;
    for (t = 0; t < TRACKS; ++t) {
        u8 *s = SOUND(t);
        u8 o[DS_OPS][4];
        if (!s || s[126] != DS_MACHINE) continue;
        sel = ds_op_sel(s);
        if ((s[HID] & 0xfcu) == HID_MARK && (s[HID] & 3u) == sel) continue;
        ds_sound_ops(s, o);
        if ((s[HID] & 0xfcu) == HID_MARK) {
            /* OP turned: its operator onto the knobs, the old one hidden. */
            u32 j;
            for (j = 0; j < 4u; ++j) S_PARAM(s, ds_op_slot[j]) = (s16)(o[sel][j] << 8);
            redraw = 1;
        }
        ds_sound_hide(s, o, sel);
    }
    if (redraw) *((volatile u8 *)ctrl + 0x20) = 1;
    ds_wc_sync();
}

/* Project block +0x20..+0x200, skipped by SERIALIZE and DESERIALIZE: where
 * the operators went per track before they rode in the sounds, in core-dn1's
 * projdata layout ('ELKP', tag, size, bytes, zero tag). Now only read. */
#define PROJ_GAP 0x20
#define PROJ_MAGIC 0x454c4b50u
#define PROJ_TAG 0x464d324fu
#define PROJ_HEAD 0xbeefbaceu
/* Stored kits are 0xa00 bytes, of which the OS uses 0..0x8f3; the tail holds
 * 'DTFM' and all four operators of each track. A project has 128 of them
 * from +0x310200. */
#define SKIT0 0x310200u
#define SKIT_SIZE 0xa00u
#define SKIT_GAP 0x8f4u
/* The power-up working copy: dumped to NAND at power-off and DESERIALIZEd
 * at boot, but re-serialized only on project load, so ds_wc_sync keeps our
 * kit tails in it current. The COKI checksum covers only its header. */
#define WC_DATA ((u8 *)OS_WC_DATA)
/* A stored kit's sounds: 160 bytes each from +36, SRC slot s at +28 + 2s. */
#define SSND(rec, t) ((rec) + 36 + 160u * (t))

static u8 ds_live;     /* the project's sounds came from (or went to) storage */
static u8 ds_kit_seen[KITS / 8];   /* last load found a tagged tail */
static u32 ds_wc_next;

static s32 ds_kit_index(const u8 *kit)
{
    u32 off = (u32)kit - (u32)KIT0;
    if (off >= KITS * KIT_SIZE || off % KIT_SIZE) return -1;
    return (s32)(off / KIT_SIZE);
}

static int ds_tagged(const u8 *b)
{ return b[0] == 'D' && b[1] == 'T' && b[2] == 'F' && b[3] == 'M'; }

static void ds_kit_ops(const u8 *kit, u8 *b)
{
    u32 t, i;
    for (t = 0; t < TRACKS; ++t) {
        const u8 *s = kit + 32 + SND_SIZE * t;
        u8 *o = b + 16 * t;
        if (s[126] == DS_MACHINE) ds_sound_ops(s, (u8 (*)[4])o);
        else for (i = 0; i < 16u; ++i) o[i] = (&ds_op_init[0][0])[i];
    }
}

/* After the OS saves a kit (rec, kit), to a project, a file or a copy. */
void ds_kit_put(u8 *rec, const u8 *kit)
{
    u8 *b = rec + SKIT_GAP;
    b[0] = 'D';
    b[1] = 'T';
    b[2] = 'F';
    b[3] = 'M';
    ds_kit_ops(kit, b + 4);
}

static void ds_kit_restore(u8 *kit, const u8 *ops)
{
    u32 t;
    for (t = 0; t < TRACKS; ++t) {
        u8 *s = kit + 32 + SND_SIZE * t;
        if (s[126] == DS_MACHINE)
            ds_sound_hide(s, (const u8 (*)[4])(ops + 16 * t), ds_op_sel(s));
    }
}

/* SERIALIZE(data, project, ..., flags, ...): flags bit 1 writes the kits. */
void ds_proj_put(u8 *data, const u8 *project, u32 flags)
{
    if (data && project == LIVE_PROJECT && (flags & 2u)) ds_live = 1;
}

/* After the OS loads a stored kit into RAM, from a project, a file or a copy. */
void ds_kit_get(u8 *kit, const u8 *rec)
{
    s32 k = ds_kit_index(kit);
    const u8 *b = rec + SKIT_GAP;
    u32 tagged = ds_tagged(b);
    if (tagged) ds_kit_restore(kit, b + 4);
    if (k < 0) return;
    if (tagged) ds_kit_seen[k >> 3] |= (u8)(1u << (k & 7));
    else ds_kit_seen[k >> 3] &= (u8)~(1u << (k & 7));
}

/* After DESERIALIZE: an older project's per-track operators seed every kit
 * that had none of its own, as they applied to every pattern then. */
void ds_proj_get(const u8 *project, const u8 *data)
{
    const u32 *w = (const u32 *)(data + PROJ_GAP);
    u32 k;
    if (project != LIVE_PROJECT) return;
    ds_live = 1;
    if (w[0] != PROJ_MAGIC || w[1] != PROJ_TAG || w[2] != KIT_OPS) return;
    for (k = 0; k < KITS; ++k)
        if (!(ds_kit_seen[k >> 3] & (1u << (k & 7))))
            ds_kit_restore(KIT0 + KIT_SIZE * k, (const u8 *)(w + 3));
}

static void ds_wc_kit(u32 k)
{
    u8 *rec = WC_DATA + SKIT0 + SKIT_SIZE * k, *kit = KIT0 + KIT_SIZE * k;
    u8 *b = rec + SKIT_GAP, cur[KIT_OPS];
    u32 i, t;
    /* An OP swap writes the knobs behind the OS, which never records them. */
    for (t = 0; t < TRACKS; ++t) {
        const u8 *s = kit + 32 + SND_SIZE * t;
        u8 *srec = SSND(rec, t);
        if (s[126] != DS_MACHINE || srec[124] != DS_MACHINE) continue;
        for (i = 0; i < 4u; ++i) {
            s16 *v = (s16 *)(srec + 28 + 2 * ds_op_slot[i]);
            if (*v != S_PARAM(s, ds_op_slot[i])) *v = S_PARAM(s, ds_op_slot[i]);
        }
    }
    ds_kit_ops(kit, cur);
    if (ds_tagged(b)) {
        for (i = 0; i < KIT_OPS && b[4 + i] == cur[i]; ++i) {}
        if (i == KIT_OPS) return;
    }
    ds_kit_put(rec, kit);
}

/* Not before the boot restore has run (ds_live), so defaults never
 * overwrite the persisted tails. The live kit, then one more in turn. */
static void ds_wc_sync(void)
{
    s32 k;
    if (!ds_live || *(const u32 *)WC_DATA != PROJ_HEAD) return;
    k = ds_kit_index(LIVE_KIT);
    if (k >= 0) ds_wc_kit((u32)k);
    ds_wc_kit(ds_wc_next);
    ds_wc_next = (ds_wc_next + 1u) & (KITS - 1u);
}
