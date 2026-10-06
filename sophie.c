/* SPDX-License-Identifier: MIT
 *
 * Four-operator FM voice for Digitakt Mk1. Audio enters Digitakt's stock
 * AMP/filter/mixer path; this source block only renders the oscillators.
 */
#include "sophie.h"

#define Q15 32767
#define ENV_ONE (1u << 24)

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

/* Per 6 kHz control tick: attack 1 ms..2 s to full scale (0 is instant);
 * decay time constant 3 ms..3 s as a Q20 fraction (127 holds). */
static const uint32_t ds_attack_step[128] = {
	16777216, 2796203, 2632510, 2478400, 2333311, 2196717, 2068118, 1947048,
	1833066, 1725756, 1624728, 1529615, 1440070, 1355766, 1276398, 1201676,
	1131329, 1065100, 1002747, 944046, 888780, 836750, 787766, 741649,
	698232, 657357, 618874, 582645, 548536, 516424, 486192, 457730,
	430934, 405706, 381956, 359596, 338545, 318726, 300067, 282501,
	265963, 250393, 235735, 221935, 208942, 196711, 185195, 174354,
	164147, 154537, 145491, 136973, 128955, 121406, 114298, 107607,
	101308, 95377, 89794, 84537, 79588, 74929, 70543, 66413,
	62525, 58865, 55419, 52174, 49120, 46245, 43537, 40989,
	38589, 36330, 34203, 32201, 30316, 28541, 26870, 25297,
	23816, 22422, 21109, 19874, 18710, 17615, 16584, 15613,
	14699, 13838, 13028, 12266, 11548, 10872, 10235, 9636,
	9072, 8541, 8041, 7570, 7127, 6710, 6317, 5947,
	5599, 5271, 4963, 4672, 4399, 4141, 3899, 3670,
	3456, 3253, 3063, 2884, 2715, 2556, 2406, 2265,
	2133, 2008, 1890, 1780, 1675, 1577, 1485, 1398
};
static const uint16_t ds_decay_coef[128] = {
	58254, 55146, 52205, 49420, 46783, 44287, 41925, 39688, 37571, 35567, 33669, 31873,
	30173, 28563, 27039, 25597, 24231, 22939, 21715, 20556, 19460, 18422, 17439, 16509,
	15628, 14794, 14005, 13258, 12550, 11881, 11247, 10647, 10079, 9541, 9032, 8551,
	8094, 7663, 7254, 6867, 6500, 6154, 5825, 5515, 5220, 4942, 4678, 4429,
	4192, 3969, 3757, 3557, 3367, 3187, 3017, 2856, 2704, 2560, 2423, 2294,
	2171, 2056, 1946, 1842, 1744, 1651, 1563, 1479, 1400, 1326, 1255, 1188,
	1125, 1065, 1008, 954, 903, 855, 809, 766, 725, 687, 650, 615,
	583, 551, 522, 494, 468, 443, 419, 397, 376, 356, 337, 319,
	302, 286, 270, 256, 242, 229, 217, 206, 195, 184, 174, 165,
	156, 148, 140, 133, 126, 119, 112, 106, 101, 95, 90, 86,
	81, 77, 73, 69, 65, 62, 58, 0
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
	uint32_t op_inc[DS_OPS];
	int32_t feedback_amt = 0;
	int32_t tone_rate = 0;
	uint32_t i, k;

	for (k = 0; k < DS_OPS; ++k) {
		ratio_target[k] = ds_ratio_q12(p->op[k].ratio);
		level_target[k] = ds_level_q15(p->op[k].level);
		op_inc[k] = 0;
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
		for (k = 0; k < DS_OPS; ++k)
			v->phase[k] = v->env[k] = 0;
		v->rising = (1u << DS_OPS) - 1u;
		v->fb_z1 = v->fb_z2 = 0;
		v->tone_z = 0;
		v->sleeping = v->quiet_blocks = v->fade_left = 0;
		v->active = 1;
	}

	for (i = 0; i < n; i += 2) {
		int32_t o[DS_OPS];
		int32_t mix = 0;
		int32_t y;

		if (!v->active) {
			out[i] = 0;
			if (i + 1 < n) out[i + 1] = 0;
			continue;
		}

		if ((i & 7u) == 0) {
			for (k = 0; k < DS_OPS; ++k) {
				uint32_t env = v->env[k];
				v->ratio_s[k] = ds_slew8(v->ratio_s[k], ratio_target[k]);
				v->level_s[k] = ds_slew8(v->level_s[k], level_target[k]);
				op_inc[k] = (inc * (uint32_t)v->ratio_s[k]) << 5;
				if (v->rising & (1u << k)) {
					env += ds_attack_step[p->op[k].attack & 0x7fu];
					if (env >= ENV_ONE) {
						env = ENV_ONE;
						v->rising &= (uint8_t)~(1u << k);
					}
				} else {
					env -= ((env >> 8) * ds_decay_coef[p->op[k].decay & 0x7fu]) >> 12;
				}
				v->env[k] = env;
				v->amp[k] = ((int32_t)(env >> 9) * v->level_s[k]) >> 15;
			}
			v->feedback_s = ds_slew8(v->feedback_s, feedback_target);
			v->tone_s = ds_slew8(v->tone_s, tone_target);
			v->velocity_s = ds_slew8(v->velocity_s, velocity_target);
			feedback_amt = ds_mul(v->feedback_s, v->feedback_s);
			tone_rate = 2048 + ((v->tone_s * 30719) >> 15);
		}

		for (k = DS_OPS; k-- > 0;) {
			uint32_t m = algo->mods[k];
			int32_t mod = 0;
			if (k == DS_OPS - 1u)
				mod = (((v->fb_z1 + v->fb_z2) >> 1) * feedback_amt) >> 16;
			if (m & 2u) mod += o[1];
			if (m & 4u) mod += o[2];
			if (m & 8u) mod += o[3];
			v->phase[k] += op_inc[k];
			/* A full-scale modulator deviates the phase by two cycles. */
			o[k] = (ds_sin(v->phase[k] + ((uint32_t)mod << 18)) * v->amp[k]) >> 15;
			if (algo->carriers & (1u << k)) mix += o[k];
		}
		v->fb_z2 = v->fb_z1;
		v->fb_z1 = o[DS_OPS - 1];
		mix = (mix * (int32_t)algo->gain_q15) >> 15;

		v->tone_z += ((mix - v->tone_z) * tone_rate) >> 15;
		y = ds_soft_clip(ds_mul(v->tone_z, v->velocity_s));

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
