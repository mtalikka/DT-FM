/* SPDX-License-Identifier: MIT
 *
 * Four-operator FM scaffold for Digitakt Mk1. Audio enters Digitakt's stock
 * AMP/filter/mixer path; this source block only renders the oscillator core.
 * Milestone 1 keeps Algorithm 0 equivalent to the previous two-operator tone.
 */
#include "sophie.h"

#define Q15 32767

static int32_t ds_clamp(int32_t x, int32_t lo, int32_t hi)
{ return x < lo ? lo : (x > hi ? hi : x); }

static int32_t ds_mul(int32_t a, int32_t b)
{ return (a * b) >> 15; }

static int32_t ds_slew8(int32_t value, int32_t target)
{
	int32_t delta = target - value;
	return value + (delta > 0 ? (delta + 7) / 8 : -((-delta + 7) / 8));
}

/* 1024-point sine in Q15, one full turn. */
static const int16_t ds_sine_full[1024] = {
#include "sophie_sin_table.inc"
};

static int32_t ds_sin(uint32_t phase)
{
	return ds_sine_full[phase >> 22];
}

static uint32_t ds_phase_from_radians_q15(int32_t radians)
{
	/* radians(Q15) * 65536 / (2*pi), returned in high-word units. */
	return (uint32_t)((radians * 5215) >> 14) << 16;
}

static uint32_t ds_index_phase(int32_t index_q12, int32_t mod_q15,
							   int32_t factor_q15)
{
	int32_t scaled = ds_mul(mod_q15, factor_q15);
	int32_t n = (scaled >> 4) * index_q12;
	uint32_t a = n < 0 ? 0u - (uint32_t)n : (uint32_t)n;
	/* 163/131072 approximates 1/804 and avoids a division in the sample loop. */
	uint32_t q = (((a >> 9) * 163u) + (((a & 511u) * 163u) >> 9)) >> 8;
	return (uint32_t)(n < 0 ? -(int32_t)q : (int32_t)q) << 16;
}

/* Soft clip lookup with linear interpolation. */
static const uint16_t ds_clip_table[65] = {
	0, 992, 1927, 2808, 3640, 4428, 5173, 5881, 6553,
	7192, 7801, 8382, 8936, 9466, 9972, 10457, 10922, 11368,
	11796, 12207, 12602, 12983, 13349, 13702, 14043, 14371, 14688,
	14995, 15291, 15578, 15855, 16123, 16383, 16635, 16880, 17117,
	17347, 17570, 17788, 17999, 18204, 18403, 18597, 18786, 18970,
	19149, 19324, 19494, 19660, 19822, 19980, 20134, 20284, 20431,
	20574, 20715, 20851, 20985, 21116, 21244, 21370, 21492, 21612,
	21729, 21844
};

static int32_t ds_soft_clip(int32_t x)
{
	uint32_t a = x < 0 ? 0u - (uint32_t)x : (uint32_t)x;
	int32_t y;
	if (a >= 65536u) {
		if (a > 131072u) a = 131072u;
		y = (int32_t)(((a >> 1) * Q15) / ((Q15 + a) >> 1));
	} else {
		uint32_t pos = a >> 10;
		int32_t lo = ds_clip_table[pos];
		y = lo + ((ds_clip_table[pos + 1] - lo) * (int32_t)(a & 1023u) >> 10);
	}
	return x < 0 ? -y : y;
}

static int32_t ds_ratio_q12(int32_t ratio_q15)
{
	/* 0.25x .. 4.0x mapped to Q12 ratio units (1024..16384). */
	return 1024 + ((ratio_q15 * 15360) >> 15);
}

static int32_t ds_attack_step(int32_t attack_q15)
{
	/* 0: near-instant, 127: slower rise. */
	return 8 + (((Q15 - attack_q15) * 2040) >> 15);
}

static int32_t ds_decay_step(int32_t decay_q15)
{
	/* 0: short burst, 127: long decay. */
	return 2 + (((Q15 - decay_q15) * 510) >> 15);
}

struct ds_algo {
	uint8_t carrier_op;
	uint8_t mod_op;
	uint8_t feedback_op;
	uint16_t ratio_mul_q15;
	uint16_t index_mul_q15;
};

/* Algorithm slots currently pick carrier/modulator pairings and apply small
 * ratio/index scalings so ALGO audibly changes timbre before full M2 routing. */
static const struct ds_algo ds_algo_table[8] = {
	{0, 1, 1, 32767, 32767}, {0, 2, 2, 23170, 32767},
	{0, 3, 3, 46341, 32767}, {1, 2, 2, 16384, 24576},
	{1, 3, 3, 49152, 28672}, {2, 3, 3, 32767, 49152},
	{3, 0, 0, 8192, 57344},  {2, 0, 0, 65535, 20480}
};

void ds_voice_init(struct ds_voice *v)
{
	uint32_t i;
	for (i = 0; i < DS_OPS; ++i) {
		v->phase[i] = 0;
		v->op_env[i] = 0;
		v->op_feedback_z[i] = 0;
		v->ratio_s[i] = 0;
		v->index_s[i] = 0;
		v->attack_s[i] = 0;
		v->decay_s[i] = 0;
		v->feedback_s[i] = 0;
	}
	v->amp_env = 0;
	v->tone_z = 0;
	v->tone_s = v->velocity_s = 0;
	v->last = v->tail = 0;
	v->transition = 0;
	v->env_rise = 0;
	v->op_current = 1;
	v->sleeping = v->quiet_blocks = v->fade_left = 0;
	v->active = 0;
}

/* Follow stock AMP phase, then fade and sleep only after a quiet release. */
void ds_voice_gate(struct ds_voice *v, int32_t amp_level, int32_t amp_phase)
{
	uint32_t mag;
	if (!v->active) return;
	mag = amp_level < 0 ? 0u - (uint32_t)amp_level : (uint32_t)amp_level;
	if (amp_phase != 0 || mag > (1u << 19)) {
		v->quiet_blocks = 0;
		if (v->fade_left) {
			v->tail = v->last;
			v->transition = 64;
			v->fade_left = 0;
		}
		if (v->sleeping) {
			v->sleeping = 0;
			v->tail = 0;
			v->transition = 64;
		}
	} else if (v->quiet_blocks < 32) {
		if (++v->quiet_blocks == 32) {
			v->fade_left = 128; /* ~5.3 ms at 48 kHz */
		}
	}
}

void ds_voice_render(struct ds_voice *v, const struct ds_params *p,
					 int trigger, int32_t *out, uint32_t n)
{
	uint32_t i;
	uint32_t j;
	const struct ds_algo *algo = &ds_algo_table[p->algo & 7u];
	uint32_t pair_shift = (uint32_t)p->op_select & 3u;
	uint32_t carrier_op = (algo->carrier_op + pair_shift) & 3u;
	uint32_t mod_op = (algo->mod_op + pair_shift) & 3u;
	uint32_t feedback_op = mod_op;
	int32_t ratio_target = ds_u7_q15(p->ratio);
	int32_t index_target = ds_u7_q15(p->index);
	int32_t attack_target = ds_u7_q15(p->attack);
	int32_t decay_target = ds_u7_q15(p->decay);
	int32_t feedback_target = ds_u7_q15(p->feedback);
	int32_t tone_target = ds_u7_q15(p->tone);
	int32_t velocity_target = ds_u7_q15(p->velocity);

	int32_t ratio_q12 = 4096;
	int32_t feedback_amt = 0;
	int32_t attack_step = 1024;
	int32_t decay_step = 64;
	int32_t tone_rate = 8192;

	if (trigger) {
		if (v->active) {
			v->tail = v->last;
			v->transition = 64;
		}
		if (!v->active) {
			for (j = 0; j < DS_OPS; ++j) {
				v->ratio_s[j] = ratio_target;
				v->index_s[j] = index_target;
				v->attack_s[j] = attack_target;
				v->decay_s[j] = decay_target;
				v->feedback_s[j] = feedback_target;
			}
			v->tone_s = tone_target;
			v->velocity_s = velocity_target;
			v->tail = 0;
			v->transition = 0;
		}
		for (j = 0; j < DS_OPS; ++j) {
			v->phase[j] = 0;
			v->op_env[j] = 0;
			v->op_feedback_z[j] = 0;
		}
		v->amp_env = Q15;
		v->op_env[carrier_op] = Q15;
		v->tone_z = 0;
		v->env_rise = 1;
		v->op_current = (uint8_t)mod_op;
		v->sleeping = v->quiet_blocks = v->fade_left = 0;
		v->active = 1;
	} else if (v->active && v->op_current != (uint8_t)mod_op) {
		v->tail = v->last;
		v->transition = 64;
		v->op_current = (uint8_t)mod_op;
		v->env_rise = 1;
	}

	for (i = 0; i < n; i += 2) {
		int32_t y;
		uint32_t control_tick = (i & 7u) == 0;

		if (!v->active) {
			out[i] = 0;
			if (i + 1 < n) out[i + 1] = 0;
			continue;
		}

		if (control_tick) {
			v->ratio_s[mod_op] = ds_slew8(v->ratio_s[mod_op], ratio_target);
			v->index_s[mod_op] = ds_slew8(v->index_s[mod_op], index_target);
			v->attack_s[mod_op] = ds_slew8(v->attack_s[mod_op], attack_target);
			v->decay_s[mod_op] = ds_slew8(v->decay_s[mod_op], decay_target);
			v->feedback_s[mod_op] = ds_slew8(v->feedback_s[mod_op], feedback_target);
			v->tone_s = ds_slew8(v->tone_s, tone_target);
			v->velocity_s = ds_slew8(v->velocity_s, velocity_target);

			ratio_q12 = ds_ratio_q12(v->ratio_s[mod_op]);
			ratio_q12 = (int32_t)(((int64_t)ratio_q12 * algo->ratio_mul_q15 + 16384) >> 15);
			if (ratio_q12 < 256) ratio_q12 = 256;
			if (ratio_q12 > 32767) ratio_q12 = 32767;
			feedback_amt = ds_mul(v->feedback_s[mod_op], v->feedback_s[mod_op]);
			attack_step = ds_attack_step(v->attack_s[mod_op]);
			decay_step = ds_decay_step(v->decay_s[mod_op]);
			tone_rate = 1024 + ((v->tone_s * 14336) >> 15);
		}

		if (v->env_rise) {
			v->op_env[mod_op] += attack_step;
			if (v->op_env[mod_op] >= Q15) {
				v->op_env[mod_op] = Q15;
				v->env_rise = 0;
			}
		} else if (v->op_env[mod_op] > 0) {
			v->op_env[mod_op] -= decay_step;
			if (v->op_env[mod_op] < 0) v->op_env[mod_op] = 0;
		}
		if (v->amp_env > 0) {
			v->amp_env -= (v->amp_env * 14 + 32767) >> 15;
			if (v->amp_env < 0) v->amp_env = 0;
		}

		{
			int32_t inc = ds_clamp((int32_t)p->phase_inc, 8, 32767);
			int32_t mod_step;
			int32_t fb;
			int32_t mod;
			int32_t index_q12;
			uint32_t phase_mod;
			int32_t car;
			int32_t bright;
			int32_t raw;
			int32_t tone_mix;

			v->phase[carrier_op] += (uint32_t)inc << 17;
			mod_step = (inc * ratio_q12) >> 11;
			if (mod_step < 1) mod_step = 1;
			if (mod_step > 49152) mod_step = 49152;
			v->phase[mod_op] += (uint32_t)mod_step << 16;

			fb = ds_mul(v->op_feedback_z[feedback_op], feedback_amt);
			fb = (fb * (8192 + ((v->index_s[mod_op] * 24576) >> 15))) >> 12;
			mod = ds_sin(v->phase[mod_op] + ds_phase_from_radians_q15(fb));

			index_q12 = (int32_t)(((int64_t)v->index_s[mod_op] * algo->index_mul_q15 + 16384) >> 15);
			index_q12 = (index_q12 * (1024 + ((v->op_env[mod_op] * 7168) >> 15))) >> 15;
			phase_mod = ds_index_phase(index_q12, mod, Q15);

			car = ds_sin(v->phase[carrier_op] + phase_mod);
			bright = ds_sin((v->phase[carrier_op] << 1) + (phase_mod >> 1));
			tone_mix = v->tone_s >> 1;
			raw = ds_mul(car, Q15 - tone_mix) + ds_mul(bright, tone_mix);
			raw = ds_mul(raw, 16384 + (v->amp_env >> 1));

			v->op_feedback_z[feedback_op] += (mod - v->op_feedback_z[feedback_op]) >> 2;
			v->tone_z += ((raw - v->tone_z) * tone_rate) >> 15;

			y = ds_mul(v->tone_z, v->velocity_s);
			y = ds_soft_clip(y);
		}

		if (v->transition) {
			y = (y * (64 - v->transition) + v->tail * v->transition) / 64;
			v->transition = v->transition > 2 ? v->transition - 2 : 0;
		}
		if (v->fade_left) {
			y = (y * v->fade_left) / 128;
			if (--v->fade_left == 0) {
				v->sleeping = 1;
				v->tail = 0;
				v->transition = 0;
			}
		}

		out[i] = ((v->last + y) / 2) * 65536;
		if (i + 1 < n) out[i + 1] = y * 65536;
		v->last = v->sleeping ? 0 : y;
	}
}
