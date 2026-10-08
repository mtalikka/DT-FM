/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../src/sophie.h"

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

/* Algorithm 1 (4>3>2>1) with every operator audible. */
static struct ds_params patch(void)
{
    struct ds_params p;
    memset(&p, 0, sizeof p);
    p.phase_inc = 180;
    p.feedback = 16;
    p.tone = 64;
    p.velocity = 127;
    p.op[0] = (struct ds_op_params){16, 127, 0, 127};
    p.op[1] = (struct ds_op_params){16, 80, 0, 90};
    p.op[2] = (struct ds_op_params){36, 40, 0, 70};
    p.op[3] = (struct ds_op_params){16, 30, 0, 60};
    return p;
}

static void render(const struct ds_params *p, int32_t *out, unsigned n)
{
    struct ds_voice v;
    ds_voice_init(&v);
    ds_voice_render(&v, p, 1, out, n);
}

int main(void)
{
    enum { N = 48000 };
    static int32_t a[N], b[N], c[N];
    struct ds_voice vb;
    struct ds_params p = patch();
    unsigned i, k, control, positive = 0, negative = 0;

    for (control = 0; control < 128; ++control)
        assert(ds_u7_q15((uint8_t)control) == (int32_t)(control * 32767u / 127u));

    render(&p, a, N);
    assert(energy(a, 12000) > 50000);
    assert(energy(a + 36000, 12000) > 50000);

    for (i = 0; i < 4096; ++i) {
        if (a[i] > 0) ++positive;
        if (a[i] < 0) ++negative;
    }
    assert(positive > 500 && negative > 500);

    render(&p, b, N);
    assert(!memcmp(a, b, sizeof a));

    /* Each operator's controls act on it alone: in algorithm 8 every
     * operator is a carrier, so solo each one in turn. */
    for (k = 0; k < DS_OPS; ++k) {
        p = patch();
        p.algo = 7;
        for (i = 0; i < DS_OPS; ++i)
            p.op[i].level = i == k ? 127 : 0;
        render(&p, a, 4096);
        assert(energy(a, 4096) > 50000);
        p.op[k].ratio = 64;
        render(&p, b, 4096);
        assert(difference(a, b, 4096) > 80000);
        p.op[(k + 1) % DS_OPS].ratio = 100;
        render(&p, c, 4096);
        assert(!memcmp(b, c, 4096 * sizeof c[0]));
    }

    p = patch();
    p.op[1].level = 0;
    render(&p, a, 4096);
    p.op[1].level = 127;
    render(&p, b, 4096);
    assert(difference(a, b, 4096) > 200000);

    p = patch();
    render(&p, a, 2048);
    p.op[0].attack = 127;
    render(&p, b, 2048);
    assert(energy(a, 256) > energy(b, 256));

    p = patch();
    p.op[0].decay = 10;
    render(&p, a, 8192);
    p.op[0].decay = 127;
    render(&p, b, 8192);
    assert(energy(b + 4096, 4096) > 4 * energy(a + 4096, 4096));

    p = patch();
    p.op[1].level = 127;
    p.op[1].decay = 20;
    render(&p, a, 8192);
    p.op[1].decay = 127;
    render(&p, b, 8192);
    assert(difference(a + 4096, b + 4096, 4096) > 50000);

    p = patch();
    p.feedback = 0;
    p.op[2].level = 100;
    p.op[3].level = 100;
    p.op[3].decay = 127;
    render(&p, a, 4096);
    p.feedback = 127;
    render(&p, b, 4096);
    assert(difference(a, b, 4096) > 80000);

    p = patch();
    p.tone = 0;
    render(&p, a, 4096);
    p.tone = 127;
    render(&p, b, 4096);
    assert(difference(a, b, 4096) > 80000);

    p = patch();
    render(&p, a, 4096);
    p.algo = 5;
    render(&p, b, 4096);
    assert(difference(a, b, 4096) > 20000);

    /* Upper-note tracking must keep advancing: very high phase increments
     * must not collapse to one clamped pitch. */
    p = patch();
    p.phase_inc = 25000;
    render(&p, a, 4096);
    p.phase_inc = 32000;
    render(&p, b, 4096);
    assert(difference(a, b, 4096) > 20000);

    for (i = 0; i < 4096; ++i) {
        int32_t s = b[i] / 65536;
        assert(s < 32768 && s > -32769);
    }

    p = patch();
    p.phase_inc = 68;
    render(&p, a, N);

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

    p = patch();
    p.phase_inc = 68;
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

    puts("ok: DT-FM fixed-point engine");
    return 0;
}
