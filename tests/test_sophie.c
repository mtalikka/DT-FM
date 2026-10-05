/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../sophie.h"

static uint64_t energy(const int32_t *x, unsigned n)
{
    unsigned i;
    uint64_t total = 0;
    for (i = 0; i < n; ++i) {
        int32_t s = x[i] >> 16;
        total += (uint32_t)(s < 0 ? -s : s);
    }
    return total;
}

static uint64_t difference(const int32_t *a, const int32_t *b, unsigned n)
{
    unsigned i;
    uint64_t total = 0;
    for (i = 0; i < n; ++i) {
        int32_t d = (a[i] >> 16) - (b[i] >> 16);
        total += (uint32_t)(d < 0 ? -d : d);
    }
    return total;
}

int main(void)
{
    enum { N = 48000 };
    static int32_t a[N], b[N], c[N];
    struct ds_voice va, vb;
    struct ds_params p = {180, 64, 64, 16, 80, 40, 64, 127};
    unsigned i, control, positive = 0, negative = 0;

    for (control = 0; control < 128; ++control)
        assert(ds_u7_q15((uint8_t)control) == (int32_t)(control * 32767u / 127u));

    ds_voice_init(&va);
    ds_voice_render(&va, &p, 1, a, N);
    assert(energy(a, 12000) > 50000);
    assert(energy(a + 36000, 12000) > 50000);

    for (i = 0; i < 4096; ++i) {
        if (a[i] > 0) ++positive;
        if (a[i] < 0) ++negative;
    }
    assert(positive > 500 && negative > 500);

    ds_voice_init(&vb);
    ds_voice_render(&vb, &p, 1, b, N);
    assert(!memcmp(a, b, sizeof a));

    p.ratio = 8;
    ds_voice_init(&va);
    ds_voice_render(&va, &p, 1, a, 4096);
    p.ratio = 120;
    ds_voice_init(&vb);
    ds_voice_render(&vb, &p, 1, b, 4096);
    assert(difference(a, b, 4096) > 80000);

    p = (struct ds_params){180, 64, 0, 16, 80, 40, 64, 127};
    ds_voice_init(&va);
    ds_voice_render(&va, &p, 1, a, 4096);
    p.index = 127;
    ds_voice_init(&vb);
    ds_voice_render(&vb, &p, 1, b, 4096);
    assert(difference(a, b, 4096) > 200000);

    p = (struct ds_params){180, 64, 90, 0, 80, 40, 64, 127};
    ds_voice_init(&va);
    ds_voice_render(&va, &p, 1, a, 2048);
    p.attack = 127;
    ds_voice_init(&vb);
    ds_voice_render(&vb, &p, 1, b, 2048);
    assert(energy(a, 256) > energy(b, 256));

    p = (struct ds_params){180, 64, 90, 16, 0, 40, 64, 127};
    ds_voice_init(&va);
    ds_voice_render(&va, &p, 1, a, 4096);
    p.decay = 127;
    ds_voice_init(&vb);
    ds_voice_render(&vb, &p, 1, b, 4096);
    assert(difference(a + 2048, b + 2048, 2048) > 50000);

    p = (struct ds_params){180, 64, 90, 16, 80, 0, 64, 127};
    ds_voice_init(&va);
    ds_voice_render(&va, &p, 1, a, 4096);
    p.feedback = 127;
    ds_voice_init(&vb);
    ds_voice_render(&vb, &p, 1, b, 4096);
    assert(difference(a, b, 4096) > 80000);

    p = (struct ds_params){180, 64, 90, 16, 80, 40, 0, 127};
    ds_voice_init(&va);
    ds_voice_render(&va, &p, 1, a, 4096);
    p.tone = 127;
    ds_voice_init(&vb);
    ds_voice_render(&vb, &p, 1, b, 4096);
    assert(difference(a, b, 4096) > 80000);

    /* Upper-note tracking must keep advancing: very high phase increments
     * must not collapse to one clamped pitch. */
    p = (struct ds_params){25000, 64, 48, 16, 90, 24, 64, 127};
    ds_voice_init(&va);
    ds_voice_render(&va, &p, 1, a, 4096);
    p.phase_inc = 32000;
    ds_voice_init(&vb);
    ds_voice_render(&vb, &p, 1, b, 4096);
    assert(difference(a, b, 4096) > 20000);

    for (i = 0; i < 4096; ++i) {
        int32_t s = b[i] / 65536;
        assert(s < 32768 && s > -32769);
    }

    p = (struct ds_params){68, 64, 90, 16, 80, 32, 64, 127};
    ds_voice_init(&va);
    ds_voice_render(&va, &p, 1, a, N);

    ds_voice_init(&vb);
    for (i = 0; i < N; i += DS_BLOCK_SIZE)
        ds_voice_render(&vb, &p, i == 0, b + i, DS_BLOCK_SIZE);
    assert(!memcmp(a, b, sizeof a));

    {
        int32_t last = b[N - 1];
        p.velocity = 40;
        ds_voice_render(&vb, &p, 1, b, DS_BLOCK_SIZE);
        assert(b[0] == last);
    }

    p = (struct ds_params){68, 64, 90, 16, 80, 32, 64, 127};
    ds_voice_init(&vb);
    ds_voice_render(&vb, &p, 1, b, DS_BLOCK_SIZE);

    for (i = 0; i < 64; ++i)
        ds_voice_gate(&vb, 1 << 23, 3);
    assert(!vb.sleeping && vb.quiet_blocks == 0);

    for (i = 0; i < 64; ++i)
        ds_voice_gate(&vb, 0, 3);
    assert(!vb.sleeping && vb.quiet_blocks == 0);

    for (i = 0; i < 31; ++i)
        ds_voice_gate(&vb, 0, 0);
    assert(!vb.sleeping);

    ds_voice_gate(&vb, 0, 0);
    assert(!vb.sleeping && vb.fade_left == 128);

    for (i = 0; i < 8; ++i)
        ds_voice_render(&vb, &p, 0, c, DS_BLOCK_SIZE);
    assert(vb.sleeping && vb.active && vb.last == 0);

    ds_voice_render(&vb, &p, 1, c, DS_BLOCK_SIZE);
    assert(!vb.sleeping && c[0] == 0 && energy(c, DS_BLOCK_SIZE) > 0);

    puts("ok: FM2OP fixed-point engine");
    return 0;
}
