/* SPDX-License-Identifier: MIT
 *
 * Four-operator FM voice for Digitakt Mk1. Audio enters Digitakt's stock
 * AMP/filter/mixer path; this source block only renders the oscillators.
 */
#include "sophie.h"

#define Q15 32767
#define ENV_ONE (1u << 24)
#define DS_CHUNK (DS_BLOCK_SIZE / 2)	/* synth samples per control step */

static int32_t ds_clamp(int32_t x, int32_t lo, int32_t hi)
{ return x < lo ? lo : (x > hi ? hi : x); }

static int32_t ds_mul(int32_t a, int32_t b)
{ return (a * b) >> 15; }

static int32_t ds_slew2(int32_t value, int32_t target)
{
	int32_t delta = target - value;
	return value + (delta > 0 ? (delta + 1) >> 1 : -((-delta + 1) >> 1));
}

/* 1024-point sine in Q15, one full turn. */
static const int16_t ds_sine_full[1024] = {
#include "sophie_sin_table.inc"
};

static int32_t ds_sin(uint32_t phase)
{
	return ds_sine_full[phase >> 22];
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

const uint16_t ds_ratio_centi[DS_RATIOS] = {
	25, 50, 71, 75, 100, 125, 141, 150, 173, 200, 224, 250, 283, 300, 314, 350,
	400, 424, 450, 500, 550, 600, 700, 800, 900, 1000, 1100, 1200, 1300, 1400,
	1500, 1600
};

/* Per control step (1.5 kHz): attack 1 ms..2 s to full scale (0 is instant);
 * decay time constant 3 ms..3 s as a Q16 fraction (127 holds). */
static const uint32_t ds_attack_step[128] = {
	16777216, 11184811, 10530039, 9913599, 9333245, 8786867, 8272473, 7788193,
	7332264, 6903025, 6498914, 6118460, 5760278, 5423065, 5105593, 4806706,
	4525316, 4260399, 4010990, 3776182, 3555120, 3346999, 3151062, 2966595,
	2792928, 2629426, 2475497, 2330578, 2194144, 2065696, 1944768, 1830919,
	1723735, 1622825, 1527823, 1438383, 1354178, 1274903, 1200269, 1130004,
	1063852, 1001573, 942940, 887739, 835770, 786843, 740780, 697414,
	656587, 618149, 581962, 547893, 515819, 485622, 457194, 430429,
	405231, 381508, 359175, 338148, 318352, 299716, 282170, 265652,
	250100, 235459, 221675, 208698, 196480, 184978, 174149, 163954,
	154356, 145320, 136813, 128804, 121263, 114165, 107481, 101189,
	95265, 89688, 84438, 79495, 74841, 70460, 66335, 62452,
	58796, 55354, 52113, 49063, 46190, 43486, 40941, 38544,
	36287, 34163, 32163, 30280, 28508, 26839, 25268, 23788,
	22396, 21085, 19850, 18688, 17594, 16564, 15595, 14682,
	13822, 13013, 12251, 11534, 10859, 10223, 9625, 9061,
	8531, 8031, 7561, 7119, 6702, 6310, 5940, 5592
};
static const uint16_t ds_decay_coef[128] = {
	13059, 12433, 11834, 11260, 10712, 10187, 9686, 9207, 8751, 8315, 7899, 7503,
	7125, 6765, 6423, 6097, 5786, 5491, 5210, 4943, 4689, 4447, 4218, 4000,
	3793, 3596, 3409, 3232, 3064, 2904, 2752, 2608, 2472, 2342, 2220, 2103,
	1993, 1888, 1789, 1694, 1605, 1521, 1440, 1364, 1292, 1224, 1159, 1098,
	1040, 985, 933, 883, 836, 792, 750, 710, 673, 637, 603, 571,
	541, 512, 485, 459, 435, 411, 390, 369, 349, 331, 313, 296,
	281, 266, 251, 238, 225, 213, 202, 191, 181, 171, 162, 154,
	145, 138, 130, 123, 117, 111, 105, 99, 94, 89, 84, 80,
	75, 71, 68, 64, 61, 57, 54, 51, 49, 46, 44, 41,
	39, 37, 35, 33, 31, 30, 28, 27, 25, 24, 23, 21,
	20, 19, 18, 17, 16, 15, 15, 0
};

struct ds_algo {
	uint8_t mods[DS_OPS];	/* bit j: operator j+1 modulates this one */
	uint8_t carriers;
	uint16_t gain_q15;
};

/* Operator 4 has feedback. Every modulator has a higher index than its
 * target, so one pass from operator 4 down to 1 routes each algorithm. */
static const struct ds_algo ds_algo_table[8] = {
	{{0x2, 0x4, 0x8, 0}, 0x1, 32767},	/* 4>3>2>1 */
	{{0x2, 0xc, 0, 0}, 0x1, 32767},		/* (3+4)>2>1 */
	{{0x6, 0, 0x8, 0}, 0x1, 32767},		/* (2 + 4>3)>1 */
	{{0xe, 0, 0, 0}, 0x1, 32767},		/* (2+3+4)>1 */
	{{0x2, 0, 0x8, 0}, 0x5, 16384},		/* 2>1, 4>3 */
	{{0x8, 0x8, 0x8, 0}, 0x7, 10923},	/* 4>(1, 2, 3) */
	{{0, 0, 0x8, 0}, 0x7, 10923},		/* 1, 2, 4>3 */
	{{0, 0, 0, 0}, 0xf, 8192}		/* 1, 2, 3, 4 */
};

static int32_t ds_ratio_q12(uint8_t knob)
{
	/* centi * 4096 / 100 */
	return ((int32_t)ds_ratio_centi[(knob & 0x7fu) >> 2] * 41943 + 512) >> 10;
}

static int32_t ds_level_q15(uint8_t knob)
{
	int32_t q = ds_u7_q15(knob & 0x7fu);
	return ds_mul(q, q);
}

/* A full-scale modulator deviates the phase by two cycles. */
static uint32_t ds_op_chunk(uint32_t ph, uint32_t inc, int32_t amp, int32_t step,
			    const int32_t *mod, int32_t *o, uint32_t len)
{
	uint32_t s;
	if (mod) {
		for (s = 0; s < len; ++s) {
			ph += inc;
			o[s] = (ds_sin(ph + ((uint32_t)mod[s] << 18)) * amp) >> 15;
			amp += step;
		}
	} else {
		for (s = 0; s < len; ++s) {
			ph += inc;
			o[s] = (ds_sin(ph) * amp) >> 15;
			amp += step;
		}
	}
	return ph;
}

static uint32_t ds_fb_chunk(struct ds_voice *v, uint32_t ph, uint32_t inc,
			    int32_t amp, int32_t step, int32_t fb_amt,
			    int32_t *o, uint32_t len)
{
	int32_t z1 = v->fb_z1, z2 = v->fb_z2;
	uint32_t s;
	for (s = 0; s < len; ++s) {
		int32_t fb = (((z1 + z2) >> 1) * fb_amt) >> 16;
		ph += inc;
		z2 = z1;
		z1 = (ds_sin(ph + ((uint32_t)fb << 18)) * amp) >> 15;
		o[s] = z1;
		amp += step;
	}
	v->fb_z1 = z1;
	v->fb_z2 = z2;
	return ph;
}

void ds_voice_init(struct ds_voice *v)
{
	uint32_t i;
	for (i = 0; i < DS_OPS; ++i) {
		v->phase[i] = v->env[i] = 0;
		v->ratio_s[i] = v->level_s[i] = v->amp[i] = 0;
	}
	v->fb_z1 = v->fb_z2 = 0;
	v->feedback_s = v->tone_s = v->velocity_s = 0;
	v->tone_z = 0;
	v->last = v->tail = 0;
	v->rising = v->transition = 0;
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
	const struct ds_algo *algo = &ds_algo_table[p->algo & 7u];
	uint32_t inc = (uint32_t)ds_clamp((int32_t)p->phase_inc, 8, 32767);
	int32_t ratio_target[DS_OPS], level_target[DS_OPS];
	int32_t feedback_target = ds_u7_q15(p->feedback);
	int32_t tone_target = ds_u7_q15(p->tone);
	int32_t velocity_target = ds_u7_q15(p->velocity);
	uint32_t i, k;

	for (k = 0; k < DS_OPS; ++k) {
		ratio_target[k] = ds_ratio_q12(p->op[k].ratio);
		level_target[k] = ds_level_q15(p->op[k].level);
	}

	if (trigger) {
		if (v->active) {
			v->tail = v->last;
			v->transition = 64;
		} else {
			for (k = 0; k < DS_OPS; ++k) {
				v->ratio_s[k] = ratio_target[k];
				v->level_s[k] = level_target[k];
			}
			v->feedback_s = feedback_target;
			v->tone_s = tone_target;
			v->velocity_s = velocity_target;
			v->tail = 0;
			v->transition = 0;
		}
		for (k = 0; k < DS_OPS; ++k) {
			v->phase[k] = v->env[k] = 0;
			v->amp[k] = 0;
		}
		v->rising = (1u << DS_OPS) - 1u;
		v->fb_z1 = v->fb_z2 = 0;
		v->tone_z = 0;
		v->sleeping = v->quiet_blocks = v->fade_left = 0;
		v->active = 1;
	}

	for (i = 0; i < n; i += 2 * DS_CHUNK) {
		static const int32_t zero[DS_CHUNK];
		int32_t buf[DS_OPS][DS_CHUNK], sum[DS_CHUNK];
		int32_t *o = out + i, *acc = 0;
		const int32_t *mix;
		uint32_t rem = n - i;
		uint32_t len = rem < 2 * DS_CHUNK ? (rem + 1) / 2 : DS_CHUNK;
		uint32_t live = 0, s, j, tr, fade;
		int32_t feedback_amt, tone_rate, cgain, z, last;

		if (!v->active) {
			for (s = 0; s < rem && s < 2 * DS_CHUNK; ++s) o[s] = 0;
			continue;
		}

		v->feedback_s = ds_slew2(v->feedback_s, feedback_target);
		v->tone_s = ds_slew2(v->tone_s, tone_target);
		v->velocity_s = ds_slew2(v->velocity_s, velocity_target);
		feedback_amt = ds_mul(v->feedback_s, v->feedback_s);
		tone_rate = 2048 + ((v->tone_s * 30719) >> 15);
		cgain = ((int32_t)algo->gain_q15 * v->velocity_s) >> 15;

		/* Operator 4 first: its output feeds the operators below it. Levels
		 * ramp across the block; silent operators only advance their phase. */
		for (k = DS_OPS; k-- > 0;) {
			uint32_t bit = 1u << k, env = v->env[k], op_inc;
			int32_t a0 = v->amp[k], a1, step;
			const int32_t *in = 0;

			v->ratio_s[k] = ds_slew2(v->ratio_s[k], ratio_target[k]);
			v->level_s[k] = ds_slew2(v->level_s[k], level_target[k]);
			op_inc = (inc * (uint32_t)v->ratio_s[k]) << 5;
			if (v->rising & bit) {
				env += ds_attack_step[p->op[k].attack & 0x7fu];
				if (env >= ENV_ONE) {
					env = ENV_ONE;
					v->rising &= (uint8_t)~bit;
				}
			} else {
				env -= ((env >> 8) * ds_decay_coef[p->op[k].decay & 0x7fu]) >> 8;
				if (env < 8192u) env = 0;
			}
			v->env[k] = env;
			a1 = ((int32_t)(env >> 9) * v->level_s[k]) >> 15;
			if (algo->carriers & bit) a1 = (a1 * cgain) >> 15;
			v->amp[k] = a1;

			if (!a0 && !a1) {
				v->phase[k] += op_inc * len;
				if (k == DS_OPS - 1u) v->fb_z1 = v->fb_z2 = 0;
				continue;
			}
			step = (a1 - a0) / DS_CHUNK;
			live |= bit;
			if (k == DS_OPS - 1u) {
				v->phase[k] = ds_fb_chunk(v, v->phase[k], op_inc, a0, step,
							  feedback_amt, buf[k], len);
			} else {
				for (j = k + 1; j < DS_OPS; ++j) {
					const int32_t *a = in;
					if (!(algo->mods[k] & live & (1u << j))) continue;
					if (!a) {
						in = buf[j];
						continue;
					}
					for (s = 0; s < len; ++s) sum[s] = a[s] + buf[j][s];
					in = sum;
				}
				v->phase[k] = ds_op_chunk(v->phase[k], op_inc, a0, step,
							  in, buf[k], len);
			}
			if (algo->carriers & bit) {
				if (!acc) acc = buf[k];
				else for (s = 0; s < len; ++s) acc[s] += buf[k][s];
			}
		}
		mix = acc ? acc : zero;

		z = v->tone_z;
		last = v->last;
		tr = v->transition;
		fade = v->fade_left;
		for (s = 0; s < len; ++s) {
			int32_t y;
			z += ((mix[s] - z) * tone_rate) >> 15;
			y = ds_soft_clip(z);
			if (tr) {
				y = (y * (int32_t)(64 - tr) + v->tail * (int32_t)tr) / 64;
				tr = tr > 2 ? tr - 2 : 0;
			}
			if (fade) {
				y = (y * (int32_t)fade) / 128;
				if (--fade == 0) {
					v->sleeping = 1;
					v->tail = 0;
					tr = 0;
				}
			}
			o[2 * s] = ((last + y) / 2) * 65536;
			if (2 * s + 1 < rem) o[2 * s + 1] = y * 65536;
			last = v->sleeping ? 0 : y;
		}
		v->tone_z = z;
		v->last = last;
		v->transition = (uint8_t)tr;
		v->fade_left = (uint8_t)fade;
	}
}
