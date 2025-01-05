// SPDX-FileCopyrightText: Authors of TuxNES
// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Description: sound interface
 */

/* exports */

extern int              InitAudio(void);
extern void             SoundEvent(long addr, unsigned char value);
extern unsigned char    SoundGetLengthReg(void);
extern void             UpdateAudio(void);

struct SampleFormat {
	const char *name, *fullname;
	int number;
};

/* the currently selected sample format */
extern const struct SampleFormat *sample_format;

/* table of sample formats, terminated by { 0, 0, 0 } */
extern const struct SampleFormat sample_formats[];

/* global sound parameters */
extern struct SoundConfig {
	const char *audiofile;
	int        audiorate;
	int        audiostereo;
	float      max_sound_delay;
	int        reverb;
} sound_config;
