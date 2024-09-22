// SPDX-FileCopyrightText: Authors of TuxNES
// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Description: This file is largely responsible for mixing the waveform and
 * pumping it out the audio device.
 */

/*
 *
 * New Sound code ideas based on sound.c in DarcNES by Alastair Bridgewater
 * Noise Shift Register from nosefart
 *
 * Channel function based on NESSOUND.txt and DMC.txt by Brad Taylor.
 *
 * Revision History:
 *
 * 2001/02/04 - Paul Zaremba
 *           Noise Volume sweep fixed
 *
 * 2001/01/28 - Paul Zaremba
 *           Default rate changed to 44100
 *
 * 2001/01/14 - Paul Zaremba
 *           Change to make the noise channel less harsh sounding
 *
 * 2000/11/16 - Paul Zaremba
 *           DMC frequency implemented properly.  Sounds correct on Zelda.
 *           Frequency corrected for noise channel.
 *           Volume envelope decay fixed (sq?).
 *           Triangle channel sounds better (note length appears to be fixed)
 *
 * 2000/11/02 - Paul Zaremba
 *        Fixed most of the channel start/stop popping noises
 *        Removed hard tabs so sound.c now fits the project standard
 *
 * 2000/10/23 - Paul Zaremba, corrections by Eric Jacobs integrated
 *        Duty cycle fixed for square channels         (Eric)
 *        Fixed 120/240hz timing problems
 *            for rates > 44khz                        (Paul)
 *        Initial (proper) noise rate-conversion
 *            implemented                                (Paul)
 *
 * 2000/10/07 - Paul Zaremba, patch by Eric Jacobs integrated, corrections
 *                               by Ben Sittler integrated
 *     DMC now accesses proper memory location,
 *         and other DMC fixes                               (Eric)
 *     Off-By-One error for sq1/2, noi fixed                 (Ben, Eric, Paul)
 *     Proper volume register implementation (related to
 *         off-by-one error)                                 (Ben, Eric, Paul)
 *     Noise Shift Register                                  (Ben, Eric)
 *     Sweep Carry implemented                               (Eric, Paul)
 *     Misc. sweep bugs fixed                                (Ben, Eric, Paul)
 *     Default Frame skip set to 0.33                        (Paul)
 *     Frame Skip Implemented                                (Ben, Paul)
 *     Double/Triple.. speed implemented
 *         - through Frame Skip
 *
 * 2000/10/04 - Paul Zaremba
 *     Volume adjusted so no additional calculations for volume are necessary
 *        to switch between signed and unsigned rendering
 *     Rendering is 32 bit -> 16 bit or 32(16 used) bit -> 8 bit
 *     16 bit samples supported (only signed tested)
 *     Signed Samples supported (only 16 bit tested)
 *     Triangle length problems partially fixed
 *     Default set to 48Khz - 8 bit
 *
 * 2000/10/02 - Paul Zaremba
 *     Speed increases in square channels
 *     DMC Partially implemented, but silenced.  Interrupt works
 *
 * 2000/09/26 - Paul Zaremba
 *     Initial diff to the mailing list
 *     New sound layout
 *     Fixed sound delay bug on Linux (buffer size bug)
 *     Implemented:
 *                     Triangle Channel  (Fully Implemented.. I think)
 *                     SQ1/SQ2  Channels Partially Implemented, Missing Freq. sweep
 *                     Noise    Channel  Partially Implemented
 *                     DMC      Interrupt only (always on)
 * */

/*
 * Things that need to be fixed:
 *
 * * Help needs updating
 * * Noise channel fades in signed 16 bit at rates > 22100 KHz -
 *                 verify in ZELDA (story line drum)
 * * Endian-ness implemented.  Needs testing.
 * * I need to know how to interrupt the cpu
 * * Reverb not yet supported
 * * Half speed is not supported
 * * Stereo is not supported
 * * Format AFMT_MU_LAW is still unsupported/untested
 * * Need *BSD patches.  sound.c does not compile properly under FreeBSD.
 * * DMC pops periodically.
 *
 * */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>

#ifdef HAVE_MACHINE_ENDIAN_H
#include <machine/endian.h>
#endif
#ifdef HAVE_SYS_ENDIAN_H
#include <sys/endian.h>
#endif
#ifdef HAVE_ENDIAN_H
#include <endian.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifdef HAVE_LINUX_SOUNDCARD_H
#include <linux/soundcard.h>
#endif /* HAVE_LINUX_SOUNDCARD_H */

#ifdef HAVE_SYS_SOUNDCARD_H
#include <sys/soundcard.h>
#endif /* HAVE_SYS_SOUNDCARD_H */

#include "consts.h"
#include "globals.h"
#include "mapper.h"
#include "sound.h"

/* local functions */

static char     shift_register15(unsigned char);

/* global sound parameters */
struct SoundConfig sound_config = {
	.audiofile = NULL,
	.audiorate = 44100,
	.audiostereo = 0,
	.max_sound_delay = 0.33,
	.reverb = 0,
};

/* sample format numbers */
#ifndef AFMT_MU_LAW
#  define AFMT_MU_LAW 1
#endif
#ifndef AFMT_U8
#  define AFMT_U8 8
#endif
#ifndef AFMT_S16_LE
#  define AFMT_S16_LE 16
#endif
#ifndef AFMT_S16_BE
#  define AFMT_S16_BE 32
#endif
#ifndef AFMT_S8
#  define AFMT_S8 64
#endif
#ifndef AFMT_U16_LE
#  define AFMT_U16_LE 128
#endif
#ifndef AFMT_U16_BE
#  define AFMT_U16_BE 256
#endif

/* the currently selected sample format */
const struct SampleFormat *sample_format = NULL;

/* table of sample formats, terminated by { 0, 0, 0 } */
const struct SampleFormat sample_formats[] = {
	{ "mu8",    "8-bit Mu-Law encoded *tested, imperfect",
	  AFMT_MU_LAW },
	{ "8",      "8-bit unsigned",
	  AFMT_U8 },
	{ "8s",     "8-bit signed *untested",
	  AFMT_S8 },
	{ "16",     "16-bit signed",
#if BYTE_ORDER == BIG_ENDIAN
	  AFMT_S16_BE
#else
	  AFMT_S16_LE
#endif
	},
	{ "16u",    "16-bit unsigned *untested",
#if BYTE_ORDER == BIG_ENDIAN
	  AFMT_U16_BE
#else
	  AFMT_U16_LE
#endif
	},
	{ "le16",   "16-bit signed (little-endian)",
	  AFMT_S16_LE },
	{ "le16u",  "16-bit unsigned (little-endian) *untested",
	  AFMT_U16_LE },
	{ "be16",   "16-bit signed (big-endian) *untested",
	  AFMT_S16_BE },
	{ "be16u",  "16-bit unsigned (big-endian) *untested",
	  AFMT_U16_BE },
	{ 0, 0, 0 }
};

/* NTSC frequency multiplier */
#define BASE_FREQ 111860.78
/* NTSC sound update rate (== vertical retrace rate) */
#define UPDATE_FREQ 60.0
#define INTERNAL_FREQ 60.0
/* PAL frequency multiplier */
/* #define BASE_FREQ 110840.47 */
/* PAL sound update rate (== vertical retrace rate) */
/* #define UPDATE_FREQ 50.0 */

#define SND_BUF_SIZE         20000
#define SAMPLES_PER_SECOND   44100.0f
#define STEPS_PER_VSYNC      BASE_FREQ
#define CYCLES_PER_SAMPLE    41 /* 44100Hz */

/* dmc values - this is in actual cpu time
 * */
static const unsigned short dmc_samples_wait[0x10] = {
	0x0D60, 0x0BE0, 0x0AA0, 0x0A00,
	0x08F0, 0x07F0, 0x0710, 0x06B0,
	0x05F0, 0x0500, 0x0470, 0x0400,
	0x0350, 0x02A0, 0x0240, 0x01B0,
};

/* when the sound should be played, when step <= the value */
static const unsigned long squ_duty[0x04] = {
	0x04000000, 0x08000000, 0x10000000, 0x18000000,
};

static short triangle_50[0x20] = {
	0 /* these are calculated */
};

/* as in NESSOUND.txt */
static const unsigned char length_precalc[0x20] = {
	0x05, 0x7F, 0x0A, 0x01, 0x14, 0x02, 0x28, 0x03,
	0x50, 0x04, 0x1E, 0x05, 0x07, 0x06, 0x0E, 0x07,
	0x06, 0x08, 0x0C, 0x09, 0x18, 0x0A, 0x30, 0x0B,
	0x60, 0x0C, 0x24, 0x0D, 0x08, 0x0E, 0x10, 0x0F,
};

/* as in NESSOUND.txt */
static const unsigned short noi_wavelen[0x10] = {
	0x002, 0x004, 0x008, 0x010,
	0x020, 0x030, 0x040, 0x050,
	0x065, 0x07F, 0x0BE, 0x0FE,
	0x17D, 0x1FC, 0x3F9, 0x7F2,
};

/* These are the variables for 60, 120, and 240 hz cycles
 * They trigger statements that effect counters in the channels
 * They will be consistent no matter what the update rate is.
 * */
static unsigned short hz_60 = 1;
static unsigned short hz_60_set;
static unsigned short hz_120;
static unsigned short hz_120_set;
static unsigned short hz_240;
static unsigned short hz_240_set;

static int audiofd = -1;
static int buf_size;
static int sample_format_number;
static int bytes_per_sample = 1;
static int signed_samples   = 0;

static struct snd_event {
	unsigned long        addr;                /* address of write */
	unsigned int         count;                /* cycle count */
	unsigned char        value;                /* value of write */
} snd_event_buf[SND_BUF_SIZE];

static float         magic_adjust;      /* 44100 / rate */
static unsigned int  cycles_per_sample;
static unsigned int  samples_per_vsync;
static unsigned char *audio_buffer;

static unsigned int   head       = 0;
static unsigned int   tail       = 0;
static unsigned short volume_max = 0x78; /* 120 is max */
static unsigned short volume_adjust = 0x08;
/* channel enabled */
static unsigned char sq1_enabled = 0;
static unsigned char sq2_enabled = 0;
static unsigned char tri_enabled = 0;
static unsigned char noi_enabled = 0;
static unsigned char dmc_enabled = 0;
/* current position in precalc waves */
static unsigned long sq1_index    = 0;
static unsigned long sq2_index    = 0;
static unsigned long tri_index    = 0;
static unsigned long noi_index    = 0;
static unsigned long dmc_index    = 0;
static unsigned long dmcs_index   = 0;
/* current position increment in precalculated waves */
static unsigned long step_sq1    = 0;
static unsigned long step_sq2    = 0;
static unsigned long step_tri    = 0;
static unsigned long step_noi    = 0;
static unsigned long step_dmc    = 0;
static unsigned long step_dmcs   = 0;
/* new pulse variables */
/* pulse (square) 1 variables */
static unsigned long  sq1_duty_cycle      = 0; /* 500 */
static          short wavelen_sq1         = 0;
static unsigned short sq1_volume_reg      = 0;
static unsigned short sq1_env_dec_volume  = 0;
static unsigned char  sq1_len_counter     = 0;
static unsigned char  sq1_len_cnt_ld_reg  = 0;
static unsigned char  sq1_env_dec_cycle   = 0;
static unsigned char  sq1_env_dec_counter = 0;
static unsigned char  sq1_env_dec_disable = 0;
static unsigned char  sq1_lcc_disable     = 0;
static unsigned char  sq1_right_shift     = 0;
static unsigned char  sq1_inc_dec_wavelen = 0; /* 0 = increase */
static unsigned char  sq1_sw_counter      = 0;
static unsigned char  sq1_sw_update_rate  = 0;
static unsigned char  sq1_sweep_enabled   = 0;
static unsigned char  sq1_sw_no_carry     = 1;
/* pulse (square) 2 variables */
static unsigned long  sq2_duty_cycle      = 0; /* 500 */
static          short wavelen_sq2         = 0;
static unsigned short sq2_volume_reg      = 0;
static unsigned short sq2_env_dec_volume  = 0;
static unsigned char  sq2_len_counter     = 0;
static unsigned char  sq2_len_cnt_ld_reg  = 0;
static unsigned char  sq2_env_dec_cycle   = 0;
static unsigned char  sq2_env_dec_counter = 0;
static unsigned char  sq2_env_dec_disable = 0;
static unsigned char  sq2_lcc_disable     = 0;
static unsigned char  sq2_right_shift     = 0;
static unsigned char  sq2_inc_dec_wavelen = 0; /* 0 = increase */
static unsigned char  sq2_sw_counter      = 0;
static unsigned char  sq2_sw_update_rate  = 0;
static unsigned char  sq2_sweep_enabled   = 0;
static unsigned char  sq2_sw_no_carry     = 1;
/* triangle variables */
static unsigned char  tri_lin_counter     = 0;
static unsigned char  tri_lin_cnt_ld_reg  = 0;
static unsigned char  tri_lin_cnt_strt    = 0; /* also length counter disable */
static unsigned char  tri_len_cnt_ld_reg  = 0;
static unsigned char  tri_len_counter     = 0;
static unsigned char  tri_mode_count      = 0;
static unsigned char  tri_will_count      = 0;
static          short wavelen_tri         = 0;
static          int   tri_count_delay     = 0; /* hack inspired by nosefart
                                                  (pez) */
static          int   tri_count_delay_max = 0;
/* noise variables */
static unsigned char  noi_sample_rate     = 0;
static unsigned char  noi_number_type     = 0;
static unsigned char  noi_len_counter     = 0;
static unsigned char  noi_env_dec_cycle   = 0;
static unsigned char  noi_env_dec_counter = 0;
static unsigned short noi_env_dec_volume;
static unsigned short noi_volume_reg      = 0;
static unsigned char  noi_env_dec_disable = 0;
static unsigned char  noi_lcc_disable     = 0;
static unsigned char  noi_len_cnt_ld_reg  = 0;
static unsigned char  noi_state_reg       = 1;
/* DMC variables */
#define DMC_INTERRUPT_FALSE               0x00
#define DMC_INTERRUPT_TRUE                0x07
static unsigned short dmc_load_register = 0;
static unsigned short dmc_dta_ftch_addr = 0;
static unsigned short dmc_len_freq      = 0;
static unsigned short dmc_len_counter   = 0;
static unsigned short dmc_clk_register  = 0;
static unsigned short dmc_clk_for_fetch = 0;
static unsigned short dmc_delta         = 0;
static unsigned char  dmc_interrupt     = DMC_INTERRUPT_FALSE;
static unsigned char  dmc_gen_interrupt = 0;
static unsigned char  dmc_loop_sample   = 0;
/* buffers for frequency calculations.  Total footprint: 2K 32B */
static unsigned long freq_buffer_squ[2048];
static unsigned long freq_buffer_tri[2048];
static unsigned long freq_buffer_noi[16];
static unsigned long freq_buffer_dmc[16];

#define MAGIC_squ ((unsigned long)0x1FFFFFF)
#define MAGIC_tri ((unsigned long)0xFFFFFF)
#define MAGIC_dmc ((unsigned long)0x1FFFFF)
#define MAGIC_noi ((unsigned long long)0xFFFFFFF)

int
InitAudio(void)
{
	/* Open an audio stream */
	if (sound_config.audiofile) {
		if ((audiofd = open(sound_config.audiofile, O_WRONLY | O_APPEND)) < 0) {
			perror(sound_config.audiofile);
			return 1;
		} else {
			int desired_fragmentsize = 0;
#ifdef SNDCTL_DSP_RESET
			if (!ioctl(audiofd, SNDCTL_DSP_RESET)) {
				const struct SampleFormat *desired_sample_format = sample_format;

				int desired_audiorate = sound_config.audiorate;
				if (ioctl(audiofd, SNDCTL_DSP_SPEED, &sound_config.audiorate))
					perror(sound_config.audiofile);


				if (sound_config.audiorate != desired_audiorate) {
					fprintf(stderr, "%s: wanted %d Hz, got %d Hz\n",
					        sound_config.audiofile,
					        desired_audiorate,
					        sound_config.audiorate);
				}
				int desired_audiostereo = sound_config.audiostereo;
				if (desired_audiostereo == 1) {
					fprintf(stderr, "Stereo desired, but unsupported.  Mono forced. - pez\n");
					desired_audiostereo = 0;
				}
				if (ioctl(audiofd, SNDCTL_DSP_STEREO, &sound_config.audiostereo))
					perror(sound_config.audiofile);
				if (sound_config.audiostereo != desired_audiostereo)
					fprintf(stderr, "%s: wanted %s, got %s (expect failure)\n", sound_config.audiofile,
					        desired_audiostereo ? "stereo" : "monaural",
					        sound_config.audiostereo ? "stereo" : "monaural");
				sample_format_number = sample_format->number;
				if (ioctl(audiofd, SNDCTL_DSP_SAMPLESIZE, &sample_format_number))
					perror(sound_config.audiofile);

				/* set bytes_per_sample */
				if (sample_format_number == AFMT_U16_LE
				 || sample_format_number == AFMT_U16_BE
				 || sample_format_number == AFMT_S16_LE
				 || sample_format_number == AFMT_S16_BE)
					bytes_per_sample = 2;
				/* set signed */
				if (sample_format_number == AFMT_S8
				 || sample_format_number == AFMT_S16_LE
				 || sample_format_number == AFMT_S16_BE)
					signed_samples = 1;

				if (sample_format_number != sample_format->number) {
					for (sample_format = sample_formats; sample_format->name; sample_format++) {
						if (sample_format->number == sample_format_number)
							break;
					}
					fprintf(stderr, "%s: wanted %s samples [%d], got %s samples [%d]\n",
					        sound_config.audiofile,
					        desired_sample_format->fullname
					        ? desired_sample_format->fullname
					        : "(unknown sample format)",
					        desired_sample_format->number,
					        sample_format->fullname
					        ? sample_format->fullname
					        : "(unknown sample format)",
					        sample_format_number);
				}
			}
#endif /* SNDCTL_DSP_RESET */
#ifdef SNDCTL_DSP_SETFRAGMENT
			/* this is necessary to get the sound out fast enough so that
			 * the sound is in sync with the game
			 * for performance issues, I believe a < 1/30 second delay is
			 * very good, this will be tweaked for optimum performance
			 */
			/* using samples_per_vsync as a temp */
			samples_per_vsync = sound_config.audiorate * bytes_per_sample / 30;
			while (samples_per_vsync) {
				++desired_fragmentsize;
				samples_per_vsync >>= 1;
			}
#if 0
			printf("fs: %u\n", desired_fragmentsize);
#endif
			if (ioctl(audiofd, SNDCTL_DSP_SETFRAGMENT, &desired_fragmentsize))
				perror(sound_config.audiofile);
			if (verbose) {
				ioctl(audiofd, SNDCTL_DSP_GETBLKSIZE, &buf_size);
				fprintf(stderr, "BufSize: %u bytes\n",
				        (1 << desired_fragmentsize));
			}
#endif /* SNDCTL_DSP_SETFRAGMENT */
			if (verbose)
				fprintf(stderr,
				        "Appending %d Hz %s samples to %s\n",
				        sound_config.audiorate,
				        sample_format->fullname
				        ? sample_format->fullname
				        : "(unknown sample format)",
				        sound_config.audiofile);
		}
	}



	/* set up sound buffer */
	magic_adjust = SAMPLES_PER_SECOND / sound_config.audiorate;
	cycles_per_sample = (STEPS_PER_VSYNC * UPDATE_FREQ)/ sound_config.audiorate;
	samples_per_vsync = sound_config.audiorate * bytes_per_sample / UPDATE_FREQ;
	audio_buffer = malloc(samples_per_vsync);

	if (audio_buffer == NULL) {
		fprintf(stderr, "Error allocating sound buffer.");
		audiofd = -1;
		return 1;
	}
	memset(audio_buffer, 0, samples_per_vsync);

	/* set up the triangle precalc
	 * triangle_50[0] stays 0, don't change it
	 * */
	for (int i = 1; i < 32; ++i) {
		if (i < 16) {
			triangle_50[i] = (volume_adjust) * i;
		} else {
			triangle_50[i] = triangle_50[31 - i];
		}
#if 0
		fprintf(stderr, "Test(0x%X): 0x%X\n", i, triangle_50[i]);
#endif
	}

	/* adjust the values if bytes_per_sample != 1 */
	if (bytes_per_sample != 1) {
		volume_max    |= (volume_max << 8);
		volume_adjust |= (volume_adjust << 8);
		/* adjust the triangle */
		for (int i = 1; i < 32; ++i) {
			triangle_50[i] |= (triangle_50[i] << 8);
		}
	}
	/* set up initial values */
	sq1_duty_cycle = squ_duty[1]; /* 50/50 is the default */
	sq2_duty_cycle = squ_duty[1];
	sq1_env_dec_volume =
	sq2_env_dec_volume =
	noi_env_dec_volume = volume_max;

	/* hz_60 is already set
	 * these must be set to the default value, +1 so the first decrement
	 * happens properly
	 * */
	hz_60_set  = sound_config.audiorate / (INTERNAL_FREQ);
	hz_120_set = hz_60_set / 2 + hz_60_set % 2;
	hz_240_set = hz_60_set / 4 + hz_60_set % 4;

	/* precalculate square wave steps */
	for (int i = 1; i < 2048; ++i) {
		freq_buffer_squ[i] = (MAGIC_squ * CYCLES_PER_SAMPLE * magic_adjust) / i;
		freq_buffer_tri[i] = (MAGIC_tri * CYCLES_PER_SAMPLE * magic_adjust) / i;
	}

	/* set up triangle stuff */
	tri_count_delay_max = (CYCLES_PER_SAMPLE * magic_adjust * samples_per_vsync
	                         / 2); /* wait a half frame */

	return 0;
}


inline void
SoundEvent(long addr, unsigned char value)
{
	snd_event_buf[tail].count = CLOCK;
	snd_event_buf[tail].addr = addr;
	snd_event_buf[tail].value = value;
	if (++tail == SND_BUF_SIZE) tail = 0;
}

inline char
SoundGetLengthReg(void)
{
	return (sq1_enabled && sq1_len_counter ? 0x01 : 0x00)
	     | (sq2_enabled && sq2_len_counter ? 0x02 : 0x00)
	     | (tri_enabled && tri_len_counter ? 0x04 : 0x00)
	     | (noi_enabled && noi_len_counter ? 0x08 : 0x00)
	     | (dmc_len_counter ? 0x10 : 0x00)
	     |  dmc_interrupt;
}


/* emulation of the 15-bit shift register the
 * NES uses to generate pseudo-random series
 * for the white noise channel
 * From Nosefart
 * */

static inline char
shift_register15(unsigned char xor_tap)
{
	static unsigned int sreg = 0x4000;
	int bit0, tap, bit14;
	bit0 = sreg & 1;
	tap = (sreg & xor_tap) ? 1 : 0;
	bit14 = (bit0 ^ tap);
	sreg >>= 1;
	sreg |= (bit14 << 14);
	return bit0 ^ 1;
}

void
UpdateAudio(void) /* called freq times a sec */
{
	/* #define is faster than assigning to a var */
#define CUR_EVENT snd_event_buf[head]
	         long  samp_temp;
	unsigned int   count = 0;
	unsigned int   sample = 0;
	unsigned char  dmc_shift = 0;
	static   unsigned char dmc_shiftcnt = 0;
	static   unsigned char skip_count = 0;

	if (audiofd < 0)
		return;


	while (sample < samples_per_vsync) {
		/* do counter checks */
		/* first, decrement counters */
		--hz_60; --hz_120; --hz_240;

		/* then, check 60hz */
		if (hz_60 == 0) {
#if 0
			printf("hz_60:  %u\n", sample);
#endif
			hz_60  = hz_60_set;
			hz_120 = 0; /* 120hz hits on 60hz too */

			if (!sq1_lcc_disable  && sq1_len_counter) --sq1_len_counter;
			if (!sq2_lcc_disable  && sq2_len_counter) --sq2_len_counter;
			if (!tri_lin_cnt_strt && tri_len_counter) --tri_len_counter;
			if (!noi_lcc_disable  && noi_len_counter) --noi_len_counter;
		}

		/* do 120hz stuff */
		if (hz_120 == 0) {
#if 0
			printf("hz_120: %u\n", sample);
#endif
			hz_120 = hz_120_set; /* 240hz hits on 120hz (and on 60hz) */
			hz_240 = 0;
			/* frequency sweep sq1 : wavelen_sq1 */
			if (sq1_sweep_enabled
			 && sq1_len_counter
			 && sq1_right_shift
			 && sq1_sw_no_carry && wavelen_sq1 > 0x07) {
				--sq1_sw_counter;
				if (sq1_sw_counter == 0) {
					sq1_sw_counter = sq1_sw_update_rate;
					/* update the frequency */
					if (sq1_inc_dec_wavelen) {
						wavelen_sq1 -= ((wavelen_sq1 >> sq1_right_shift) + 1);
						if (wavelen_sq1 < 0) wavelen_sq1 = 0;
					} else {
						short temp_wavelen = wavelen_sq1 + (wavelen_sq1 >> sq1_right_shift);
						if (temp_wavelen >= 0x0800) {
							sq1_sw_no_carry = 0;
						} else {
							wavelen_sq1 = temp_wavelen;
						}
					}

					step_sq1 = freq_buffer_squ[wavelen_sq1];
				}
			}

			/* frequency sweep sq2 : wavelen_sq2 */
			if (sq2_sweep_enabled
			 && sq2_len_counter
			 && sq2_right_shift
			 && sq2_sw_no_carry && wavelen_sq2 > 0x07) {
				--sq2_sw_counter;
				if (sq2_sw_counter == 0) {
					sq2_sw_counter = sq2_sw_update_rate;
					/* update the frequency */
					if (sq2_inc_dec_wavelen) {
						wavelen_sq2 -= (wavelen_sq2 >> sq2_right_shift);
						if (wavelen_sq2 < 0) wavelen_sq2 = 0;
					} else {
						short temp_wavelen = wavelen_sq2 + (wavelen_sq2 >> sq2_right_shift);
						if (temp_wavelen >= 0x0800) {
							sq2_sw_no_carry = 0;
						} else {
							wavelen_sq2 = temp_wavelen;
						}
					}

					step_sq2 = freq_buffer_squ[wavelen_sq2];
				}
			}

			/* frequency sweep noi : wavelen_noi */
#if 0
			if (noi_sweep_enabled
			 && noi_len_counter
			 && noi_right_shift
			 && noi_sw_no_carry
			 && wavelen_noi > 0x07) {
				--noi_sw_counter;
				if (noi_sw_counter == 0) {
					noi_sw_counter = noi_sw_update_rate;
					/* update the frequency */
					if (noi_inc_dec_wavelen) {
						wavelen_noi -= (wavelen_noi >> noi_right_shift);
						if (wavelen_noi < 0) wavelen_noi = 0;
					} else {
						short temp_wavelen = wavelen_noi + (wavelen_noi >> noi_right_shift);
						if (temp_wavelen >= 0x0800) {
							noi_sw_no_carry = 0;
						} else {
							wavelen_noi = temp_wavelen;
						}
					}

					step_noi = freq_buffer_squ[wavelen_noi];
				}
			}
#endif
		}

		/* now, do 240hz stuff */
		if (hz_240 == 0) {
#if 0
			printf("hz_240: %u\n", sample);
#endif
			hz_240 = hz_240_set;
			if (!sq1_env_dec_disable) --sq1_env_dec_counter;
			if (!sq2_env_dec_disable) --sq2_env_dec_counter;
			if (!noi_env_dec_disable) --noi_env_dec_counter;

			if (!sq1_env_dec_counter) {
				sq1_env_dec_counter = sq1_env_dec_cycle;
				if (sq1_env_dec_volume) {
					sq1_env_dec_volume -= volume_adjust;
				} else if (sq1_lcc_disable) {
					sq1_env_dec_volume = volume_max;
				}
			}
			if (!sq2_env_dec_counter) {
				sq2_env_dec_counter = sq2_env_dec_cycle;
				if (sq2_env_dec_volume) {
					sq2_env_dec_volume -= volume_adjust;
				} else if (sq2_lcc_disable) {
					sq2_env_dec_volume = volume_max;
				}
			}
			if (!noi_env_dec_counter) {
				noi_env_dec_counter = noi_env_dec_cycle;
				if (noi_env_dec_volume) {
					noi_env_dec_volume -= volume_adjust;
				} else if (noi_lcc_disable) {
					noi_env_dec_volume = volume_max;
				}
			}
			/* this is optimized from the docs */
			if (tri_mode_count
			 && tri_lin_counter
			 && !tri_lin_cnt_strt)
				--tri_lin_counter;
		}

		/* set up audio values for this cycle */
		while (head != tail && CUR_EVENT.count < count) {
			switch (CUR_EVENT.addr) {
				/* pulse one */
				case 0x4000:
					sq1_env_dec_disable = (CUR_EVENT.value & 0x10);
					sq1_lcc_disable = (CUR_EVENT.value & 0x20);
					sq1_duty_cycle = squ_duty[(CUR_EVENT.value >> 6)];
					if (sq1_env_dec_disable) {
						sq1_env_dec_volume =
						sq1_volume_reg = volume_adjust * (CUR_EVENT.value & 0x0F);
					} else {
						sq1_env_dec_counter =
						sq1_env_dec_cycle = (CUR_EVENT.value & 0x0F) + 1;
					}
					break;

				case 0x4001:
					sq1_right_shift = (CUR_EVENT.value & 0x07);
					sq1_inc_dec_wavelen = (CUR_EVENT.value & 0x08);
					sq1_sw_counter =
					sq1_sw_update_rate  = ((CUR_EVENT.value >> 4) & 0x07) + 1;
					sq1_sweep_enabled   = (CUR_EVENT.value & 0x80);
					sq1_sw_no_carry = 1;
					break;

				case 0x4002:
					wavelen_sq1 = ((wavelen_sq1 & 0x0700) | CUR_EVENT.value);
					sq1_sw_no_carry = 1;
					step_sq1 = freq_buffer_squ[wavelen_sq1];
					break;

				case 0x4003:
					wavelen_sq1 = ((wavelen_sq1 & 0x00FF) | ((CUR_EVENT.value & 0x07) << 8));
					sq1_sw_no_carry = 1;
					sq1_len_cnt_ld_reg = (CUR_EVENT.value >> 3);
					sq1_len_counter = length_precalc[ sq1_len_cnt_ld_reg ];
					sq1_env_dec_volume = volume_max;
					step_sq1 = freq_buffer_squ[wavelen_sq1];
					break;

				/* pulse two */
				case 0x4004:
					sq2_env_dec_disable = (CUR_EVENT.value & 0x10);
					sq2_lcc_disable = (CUR_EVENT.value & 0x20);
					sq2_duty_cycle  = squ_duty[(CUR_EVENT.value >> 6)];
					if (sq2_env_dec_disable) {
						sq2_env_dec_volume =
						sq2_volume_reg = volume_adjust * (CUR_EVENT.value & 0x0F);
					} else {
						sq2_env_dec_counter =
						sq2_env_dec_cycle = (CUR_EVENT.value & 0x0F) + 1;
					}
					break;

				case 0x4005:
					sq2_right_shift = (CUR_EVENT.value & 0x07);
					sq2_inc_dec_wavelen = (CUR_EVENT.value & 0x08);
					sq2_sw_counter =
					sq2_sw_update_rate  = ((CUR_EVENT.value >> 4) & 0x07) + 1;
					sq2_sweep_enabled   = (CUR_EVENT.value & 0x80);
					sq2_sw_no_carry = 1;
					break;

				case 0x4006:
					wavelen_sq2 = ((wavelen_sq2 & 0x0700) | CUR_EVENT.value);
					sq2_sw_no_carry = 1;
					step_sq2 = freq_buffer_squ[wavelen_sq2];
					break;

				case 0x4007:
					wavelen_sq2 = ((wavelen_sq2 & 0x00FF) | ((CUR_EVENT.value & 0x07) << 8));
					sq2_len_cnt_ld_reg = (CUR_EVENT.value >> 3);
					sq2_len_counter = length_precalc[ sq2_len_cnt_ld_reg ];
					sq2_env_dec_volume = volume_max;
					sq2_sw_no_carry = 1;
					step_sq2 = freq_buffer_squ[wavelen_sq2];
					break;

				/* triangle channel */
				case 0x4008:
					tri_lin_cnt_ld_reg = (CUR_EVENT.value & 0x7F);
					if (!tri_mode_count) {
						tri_lin_counter = tri_lin_cnt_ld_reg;
					}

					if (tri_lin_cnt_strt == 0) {
						tri_mode_count = 1;
						tri_will_count = /* save a write later */
						tri_count_delay = 0;

					} else {
						if (!(CUR_EVENT.value & 0x80)) {
							tri_will_count = 1;
							tri_count_delay = tri_count_delay_max;
						} else {
							tri_will_count =
							tri_count_delay = 0;
						}
					}
					tri_lin_cnt_strt = (CUR_EVENT.value & 0x80);
					break;
				case 0x4009: /* unused, don't waste cycles on it */
					break;
				case 0x400A:
					wavelen_tri = ((wavelen_tri & 0x0700) | CUR_EVENT.value);
					step_tri = freq_buffer_tri[wavelen_tri];
					break;
				case 0x400B:
					wavelen_tri = ((wavelen_tri & 0x00FF) | ((CUR_EVENT.value & 0x07) << 8));
					tri_len_cnt_ld_reg = CUR_EVENT.value >> 3;
					tri_len_counter = length_precalc[ tri_len_cnt_ld_reg ];

					if (tri_lin_cnt_strt) {
						tri_will_count =
						tri_count_delay = 0;
					} else {
						tri_will_count = 1;
						tri_count_delay = tri_count_delay_max;
					}
					tri_mode_count = 0;
					tri_lin_counter = tri_lin_cnt_ld_reg;
					step_tri = freq_buffer_tri[wavelen_tri];
					break;

				/* Noise Channel */
				case 0x400C:
					noi_env_dec_disable = (CUR_EVENT.value & 0x10);
					noi_lcc_disable = (CUR_EVENT.value & 0x20);
					if (noi_env_dec_disable) {
						noi_env_dec_volume =
						noi_volume_reg = volume_adjust * (CUR_EVENT.value & 0x0F);
					} else {
						noi_env_dec_counter =
						noi_env_dec_cycle = (CUR_EVENT.value & 0x0F) + 1;
					}
					break;
				case 0x400D:
					/* unused, don't waste cycles */
					break;
				case 0x400E:
					noi_sample_rate = (CUR_EVENT.value & 0x0F);
					noi_number_type = (CUR_EVENT.value & 0x80);
					if (freq_buffer_noi[noi_sample_rate] == 0) {
						freq_buffer_noi[noi_sample_rate] =
						    (MAGIC_noi * CYCLES_PER_SAMPLE * magic_adjust) /
						    noi_wavelen[noi_sample_rate];
					}
					step_noi = freq_buffer_noi[noi_sample_rate];
					break;
				case 0x400F:
					noi_len_cnt_ld_reg = (CUR_EVENT.value >> 3);
					noi_len_counter = length_precalc[ noi_len_cnt_ld_reg ];
					noi_env_dec_volume = volume_max;
					break;

				/* DMC channel */
				case 0x4010:
					dmc_loop_sample   = (CUR_EVENT.value & 0x40);
					dmc_gen_interrupt = (dmc_loop_sample ? 0 : CUR_EVENT.value & 0x80);
					if (!dmc_gen_interrupt)
						dmc_interrupt = DMC_INTERRUPT_FALSE;

					dmc_clk_for_fetch = 1;
					dmc_clk_register  = dmc_samples_wait[(CUR_EVENT.value & 0x0F)];
					if (freq_buffer_dmc[(CUR_EVENT.value & 0x0F)] == 0) {
						freq_buffer_dmc[(CUR_EVENT.value & 0x0F)] =
						    (MAGIC_dmc * CYCLES_PER_SAMPLE * magic_adjust) /
						    dmc_samples_wait[(CUR_EVENT.value & 0x0F)];
					}
					step_dmc = freq_buffer_dmc[(CUR_EVENT.value & 0x0F)];
					step_dmcs = step_dmc << 3; /* shift count is 8x freq */
					break;
				case 0x4011:
					dmc_delta = ((CUR_EVENT.value & 0x7E) >> 1);
					break;
				case 0x4012:
					dmc_dta_ftch_addr =
					dmc_load_register = (CUR_EVENT.value << 6) | 0xc000;
					break;
				case 0x4013: /* length is in bytes */
					dmc_len_counter =
					dmc_len_freq    = (CUR_EVENT.value << 4);
					break;
				case 0x4015: /* write = channel enable */
					sq1_enabled = CUR_EVENT.value & 0x01;
					sq2_enabled = CUR_EVENT.value & 0x02;
					tri_enabled = CUR_EVENT.value & 0x04;
					noi_enabled = CUR_EVENT.value & 0x08;
					dmc_enabled = CUR_EVENT.value & 0x10;
					if (!sq1_enabled) {
						sq1_len_counter = 0;
					}

					if (!sq2_enabled) {
						sq2_len_counter = 0;
					}

					if (!tri_enabled) {
						tri_len_counter = 0;
					}

					if (!noi_enabled) {
						noi_len_counter = 0;
					}
					dmc_interrupt = DMC_INTERRUPT_FALSE;
					break;
				/* default = break */
				default:
					if (verbose)
						fprintf(stderr, "Sound Write: 0x%lX (0x%X)\n",
						        CUR_EVENT.addr, CUR_EVENT.value);
			} /* switch */

			++head;
			if (head == SND_BUF_SIZE) head = 0;
		} /* while head != tail && CUR_EVENT.count <= count */

		/* create this sample */
		/* start with the triangle channel, why? I want to. (pez) */
		if (tri_count_delay > 0) {
			tri_count_delay -= CYCLES_PER_SAMPLE * magic_adjust;
		} else if (tri_will_count) {
			tri_mode_count = 1;
			tri_will_count = 0;
		}
		if (tri_mode_count == 0) {
			tri_lin_counter = tri_lin_cnt_ld_reg;
		}
		if ((tri_enabled
		  && tri_len_counter
		  && tri_lin_counter)
		 || tri_index > step_tri) {
			tri_index += step_tri;
			tri_index &= 0x1FFFFFFF;
			samp_temp = triangle_50[tri_index >> 24];
		} else {
			samp_temp = 0;
		}

		/* next do sq1 */
		if ((sq1_enabled
		  && sq1_len_counter
		  && wavelen_sq1 > 0x07
		  && sq1_sw_no_carry)
		 || sq1_index > step_sq1) {
			sq1_index += step_sq1;
			sq1_index &= 0x1FFFFFFF; /* fast modulus of a power of 2 */

			if (sq1_index <= sq1_duty_cycle) {
				samp_temp += (sq1_env_dec_disable ?
				              sq1_volume_reg :
				              sq1_env_dec_volume);

			} else if (signed_samples) {
				samp_temp -= (sq1_env_dec_disable ?
				              sq1_volume_reg :
				              sq1_env_dec_volume);
			}
		}

		/* do sq2 */
		if ((sq2_enabled
		  && sq2_len_counter
		  && wavelen_sq2 > 0x07
		  && sq2_sw_no_carry)
		 || sq2_index > step_sq2) {
			sq2_index += step_sq2;
			sq2_index &= 0x1FFFFFFF; /* fast modulus of a power of 2 */
			if (sq2_index <= sq2_duty_cycle) {
				samp_temp += (sq2_env_dec_disable ?
				              sq2_volume_reg :
				              sq2_env_dec_volume);
			} else if (signed_samples) {
				samp_temp -= (sq2_env_dec_disable ?
				              sq2_volume_reg :
				              sq2_env_dec_volume);
			}
		}

		/* do noi */
		if (noi_enabled) {
			noi_index += step_noi;
			if (noi_index > 0x1FFFFFFF) {
				noi_state_reg = shift_register15(noi_number_type ? 0x40 : 0x02);
				noi_index &= 0x1FFFFFFF;
			}


			if (noi_len_counter) {
				if (noi_state_reg) {
					samp_temp +=
					    (noi_env_dec_disable ?
					     noi_volume_reg      :
					     noi_env_dec_volume);
				} else if (signed_samples) {
					samp_temp -=
					    (noi_env_dec_disable ?
					     noi_volume_reg      :
					     noi_env_dec_volume);
				}
			}
		}

		/* do everything dmc here */
		if (dmc_enabled && /*dmc_clk_for_fetch &&*/ dmc_len_counter) {
			if (dmc_index >= MAGIC_dmc) {
				dmc_shift = MAPTABLE[dmc_dta_ftch_addr >> 12][dmc_dta_ftch_addr];
				dmc_dta_ftch_addr++;
				if (dmc_dta_ftch_addr == 0x10000) {
					dmc_dta_ftch_addr = 0x8000;
				}

				dmc_index &= MAGIC_dmc;
				dmcs_index = 0;
				dmc_shiftcnt = 8;
				--dmc_len_counter;
			}
			dmc_index  += step_dmc;
			dmcs_index += step_dmcs;

			if (dmc_len_counter == 0) {
				if (dmc_loop_sample) {
					dmc_len_counter = dmc_len_freq;
					dmc_dta_ftch_addr = dmc_load_register;
				} else if (dmc_gen_interrupt) {
					dmc_interrupt = DMC_INTERRUPT_TRUE;
				}
			}

			while (dmc_shiftcnt > 0
			    && (dmcs_index >= MAGIC_dmc
			     || dmc_index >= MAGIC_dmc)) {
				if (dmc_shift & 1) {
					if (dmc_delta != 0x3F) {
						++dmc_delta;
					}
				} else if (dmc_delta) {
					--dmc_delta;
				}
				dmc_shift >>= 1;
				dmcs_index -= MAGIC_dmc;
				--dmc_shiftcnt;
			}

			samp_temp += (dmc_delta << (bytes_per_sample == 1 ? 3 : 11));
		}


		if (bytes_per_sample == 2) {
			*((short *)(audio_buffer + sample)) = (samp_temp / 5);
#if BYTE_ORDER == BIG_ENDIAN
			/* swap order */
			if (sample_format_number == AFMT_U16_BE
			 || sample_format_number == AFMT_S16_BE) {
				audio_buffer[sample]     ^= audio_buffer[sample + 1];
				audio_buffer[sample + 1] ^= audio_buffer[sample];
				audio_buffer[sample]     ^= audio_buffer[sample + 1];
			}
#endif /* BYTE_ORDER == BIG_ENDIAN */
		} else {
			audio_buffer[sample] = (samp_temp / 5); /* average the waves */
		}

		count += cycles_per_sample;
		sample += bytes_per_sample;

	} /* while sample < samples_per_vsync */

	/* This is used to check sync every 0x0F frames instead of every frame.
	 * Checking it every frame really slows the emulator down.
	 * */
	if (skip_count) {
		--skip_count;
		return;
	}
#ifdef SNDCTL_DSP_GETODELAY
	if (sound_config.max_sound_delay > 0.0) {
		static unsigned long frame_count = 0;
		static int maxdelay = 0;
		       int odelay;

		++frame_count;

		if ((frame_count & 0x0F) == 0x0F  /* check it every few frames */
		 && !ioctl (audiofd, SNDCTL_DSP_GETODELAY, &odelay)) {
			if (odelay > maxdelay) {
				maxdelay = odelay;
			}
			if (odelay * 1.0 / (sound_config.audiorate * bytes_per_sample) >=
			    sound_config.max_sound_delay) {
				skip_count = odelay * UPDATE_FREQ / (sound_config.audiorate * bytes_per_sample) + 1;

				if (verbose) {
					fprintf(stderr,
					      "Warning: %.3f sec sound delay, resynchronizing\n",
					      odelay * 1.0 / (sound_config.audiorate
					      * bytes_per_sample));
					fprintf(stderr, "Skipping %u frames.\n", skip_count);
				}
				return;
			}
		}
	}
#endif /* SNDCTL_DSP_GETODELAY */

	write(audiofd, audio_buffer, sample);

#undef CUR_EVENT
}
