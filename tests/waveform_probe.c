/* SPDX-License-Identifier: MIT */
/* Standalone continuity diagnostic for the fixed-point FM2OP renderer. */
#include <stdint.h>
#include <stdio.h>
#include "../sophie.h"

#define N 24000
static int32_t pcm[N];

static void probe(const char *name, struct ds_params p)
{
    struct ds_voice v;
    uint32_t i, large = 0, at_blocks = 0, clipped = 0;
    int32_t prev = 0, max_jump = 0, peak = 0;
    uint64_t sum_jump = 0, sum_square = 0;

    ds_voice_init(&v);
    for (i = 0; i < N; i += DS_BLOCK_SIZE)
        ds_voice_render(&v, &p, i == 0, pcm + i, DS_BLOCK_SIZE);

    for (i = 0; i < N; ++i) {
        int32_t s = pcm[i] / 65536;
        int32_t a = s < 0 ? -s : s;
        int32_t jump = s - prev;
        if (jump < 0) jump = -jump;
        if (a > peak) peak = a;
        if (a >= 32766) ++clipped;
        if (i && jump > 8192) {
            ++large;
            if (!(i % DS_BLOCK_SIZE)) ++at_blocks;
            if (large <= 8)
                printf("  jump sample=%u time_ms=%.2f from=%d to=%d delta=%d\n",
                       i, i / 48.0, prev, s, jump);
        }
        if (jump > max_jump) max_jump = jump;
        sum_jump += (uint32_t)jump;
        sum_square += (uint64_t)(s * (int64_t)s);
        prev = s;
    }

    printf("%s: peak=%d rms2=%llu mean_jump=%llu max_jump=%d jumps>8192=%u block_jumps=%u clipped=%u\n",
           name,
           peak,
           (unsigned long long)(sum_square / N),
           (unsigned long long)(sum_jump / N),
           max_jump,
           large,
           at_blocks,
           clipped);
}

int main(void)
{
    struct ds_params p = {0};
    p.phase_inc = 180;
    p.feedback = 32;
    p.tone = 64;
    p.velocity = 127;
    p.op[0] = (struct ds_op_params){16, 127, 0, 127};
    p.op[1] = (struct ds_op_params){16, 64, 0, 90};
    p.op[2] = (struct ds_op_params){36, 40, 0, 70};
    p.op[3] = (struct ds_op_params){16, 30, 0, 60};

    probe("neutral", p);

    p.op[1].ratio = 24;
    p.op[1].level = 96;
    probe("low-ratio bell", p);

    p.op[1].ratio = 112;
    p.op[1].level = 112;
    p.feedback = 96;
    probe("bright noisy", p);

    p.op[1].ratio = 80;
    p.op[1].level = 100;
    p.op[0].attack = 127;
    p.op[0].decay = 32;
    p.tone = 24;
    probe("slow attack dark", p);

    p.op[1].ratio = 32;
    p.op[1].level = 72;
    p.op[0].attack = 8;
    p.op[0].decay = 112;
    p.feedback = 12;
    p.tone = 112;
    probe("pluck bright", p);

    return 0;
}
