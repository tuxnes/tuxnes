// SPDX-FileCopyrightText: Authors of TuxNES
// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Description: Routines for translating Game Genie codes into useful
 * addresses and data
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gamegenie.h"

#include <ctype.h>
/* #include <stdio.h> */
#include <string.h>

static int
ggtable(char lookup)
{
	switch (toupper(lookup)) {
	case 'A': return 0x0;
	case 'P': return 0x1;
	case 'Z': return 0x2;
	case 'L': return 0x3;
	case 'G': return 0x4;
	case 'I': return 0x5;
	case 'T': return 0x6;
	case 'Y': return 0x7;
	case 'E': return 0x8;
	case 'O': return 0x9;
	case 'X': return 0xA;
	case 'U': return 0xB;
	case 'K': return 0xC;
	case 'S': return 0xD;
	case 'V': return 0xE;
	case 'N': return 0xF;
	default:  return -1;
	}
}

int
DecodeGameGenieCode(const char *ggcode, int *address, int *data, int *compare)
{
	int decode[9];

	/* make sure the code is either 6 or 8 characters */
	size_t codelen = strlen(ggcode);
	if (codelen != 6
	 && codelen != 8) {
		/* printf("invalid number of Game Genie character: %zu\n", codelen); */
		return GAME_GENIE_BAD_CODE;
	}

	/* make sure that all of the characters are valid */
	for (size_t i = 0; i < codelen; ++i) {
		decode[i+1] = ggtable(ggcode[i]);
		if (decode[i+1] == -1) {
			/* printf("bad character: %c\n", ggcode[i]); */
			return GAME_GENIE_BAD_CODE;
		}
	}

	/* rotate the high bit from each code letter to the next */
	decode[0] = decode[codelen] & 8;
	for (size_t i = 0; i < codelen; ++i) {
		decode[i] = (decode[i]   & 8)
		          | (decode[i+1] & 7);
	}

	/* get the address */
	*address = 0x8000
	         | decode[3] << 12
	         | decode[5] <<  8
	         | decode[2] <<  4
	         | decode[4];

	/* get the data */
	*data = decode[1] << 4
	      | decode[0];

	/* get the compare value if it's an 8-char code */
	if (codelen == 8) {
		*compare = decode[7] << 4
		         | decode[6];
		return GAME_GENIE_8_CHAR;
	} else {
		return GAME_GENIE_6_CHAR;
	}
}
