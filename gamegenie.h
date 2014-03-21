/*
 * This file is part of the TuxNES project codebase.
 *
 * Please see the README and COPYING files for more information regarding
 * this project.
 *
 * $Id: gamegenie.h,v 1.6 2001/04/11 21:45:47 tmmm Exp $
 *
 * Description: Constants for the Game Genie translator functions
 */

#ifndef _GAMEGENIE_H_
#define _GAMEGENIE_H_

#define GAME_GENIE_BAD_CODE 0
#define GAME_GENIE_6_CHAR 1
#define GAME_GENIE_8_CHAR 2

int DecodeGameGenieCode (char *ggcode, int *address, int *data, int *compare);

#endif /* _GAMEGENIE_H_ */
