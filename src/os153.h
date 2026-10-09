/* SPDX-License-Identifier: MIT */
/* Digitakt mk1 MAIN OS 1.53: the stock data and code digitakt.c uses (os153.h
 * and os154.h, one per OS; digitakt.c includes the one OS154 picks). */
#define OS_MACH        0x800018bcu   /* the render's machine byte per track */
#define OS_VP          0x80002794u   /* the render's voice parameters, 106 bytes a track */
#define OS_NOTE        0x80001f28u   /* the trig's note per track, 16.16 */
#define OS_VEL         0x80001f18u   /* its velocity, 8.8 */
#define OS_TRIG_BITS   0x80001228u   /* this block's trigs, one bit per track */
#define OS_SOUND       0x800019b4u   /* the render's sound per track */
#define OS_VOICE_SND   0x80001502u   /* the voice's copy of its sound's slots, with its trig's locks, 106 bytes a track */
#define OS_LOCKS       0x4399db14u   /* the slots the trig's locks hold, 64 bits a track */
#define OS_LIVE_KIT    0x800019acu   /* the render's kit */
#define OS_AMP_LEVEL   0x4199df58u   /* the AMP envelope's level, 12 bytes a track */
#define OS_AMP_PHASE   0x4199df54u   /* the AMP envelope's phase, 12 bytes a track */
#define OS_PITCH_TAB   0x4019b1c0u   /* the firmware's pitch table */
#define OS_PROJECT     0x409babfcu   /* the project in RAM; its 128 kits at +0xf6341c */
#define OS_WC_DATA     0x406481f8u   /* the power-up working copy's project block */
#define OS_NUM_BOX     0x400607fau   /* SAMP's knob graphic: its number in a frame */
