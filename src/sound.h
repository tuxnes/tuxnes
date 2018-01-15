/*
 * This file is part of the TuxNES project codebase.
 *
 * Please see the README and COPYING files for more information regarding
 * this project.
 *
 * Description: sound interface
 */

/* exports */

extern int              InitAudio(int argc, char **argv);
extern void             SoundEvent(long addr, unsigned char value);
extern char             SoundGetLengthReg(void);
extern void             UpdateAudio(void);

struct SampleFormat {
	const char *name, *fullname;
	int number;
};

/* the currently selected sample format */
extern struct SampleFormat *sample_format;

/* table of sample formats, terminated by { 0, 0, 0, 0 } */
extern struct SampleFormat sample_formats[];

/* lookup table for 12bit->Mu-Law sample conversion */
extern unsigned char ulaw [0x4000];

/* global sound parameters */
extern struct SoundConfig {
	const char *audiofile;
	int        audiorate;
	int        audiostereo;
	float      max_sound_delay;
	int        reverb;
} sound_config;
