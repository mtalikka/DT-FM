/* SPDX-License-Identifier: MIT */
#ifndef DIGISOPHIE_SOPHIE_H
#define DIGISOPHIE_SOPHIE_H

#include <stdint.h>

#define DS_BLOCK_SIZE 32
#define DS_OPS 4
#define DS_RATIOS 32
/* Exact 0..127 to Q15 mapping without a ColdFire signed divide:
 * 32767 = 127*258 + 1. */
static inline int32_t ds_u7_q15(uint8_t x)
{ return ((int32_t)x << 8) + ((int32_t)x << 1) + (x == 127); }

/* RATIO steps (knob value >> 2) in hundredths, shared with the display. */
extern const uint16_t ds_ratio_centi[DS_RATIOS];

/* One operator's 0..127 controls; DECAY 127 holds. */
struct ds_op_params {
    uint8_t ratio;
    uint8_t level;
    uint8_t attack;
    uint8_t decay;
};

/* SRC values are decoded by the Digitakt adapter into these native domains. */
struct ds_params {
    uint16_t phase_inc;
    uint8_t algo;
    uint8_t feedback;
    uint8_t tone;
    uint8_t velocity;
    struct ds_op_params op[DS_OPS];
};

struct ds_voice {
    uint32_t phase[DS_OPS];
    uint32_t env[DS_OPS];
    int32_t ratio_s[DS_OPS], level_s[DS_OPS], amp[DS_OPS];
    int32_t fb_z1, fb_z2;
    int32_t feedback_s, tone_s, velocity_s;
    int32_t tone_z;
    int32_t last, tail;
    uint8_t rising, transition;
    uint8_t active, sleeping, quiet_blocks;
    uint8_t fade_left;
};

void ds_voice_init(struct ds_voice *voice);
void ds_voice_gate(struct ds_voice *voice, int32_t amp_level,
                   int32_t amp_phase);
void ds_voice_render(struct ds_voice *voice, const struct ds_params *params,
                     int trigger, int32_t *output, uint32_t size);

#endif
