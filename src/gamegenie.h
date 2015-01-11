/*
 * This file is part of the TuxNES project codebase.
 *
 * Please see the README and COPYING files for more information regarding
 * this project.
 *
 * Description: Constants for the Game Genie translator functions
 */

#ifndef GAMEGENIE_H
#define GAMEGENIE_H

#define GAME_GENIE_BAD_CODE 0
#define GAME_GENIE_6_CHAR 1
#define GAME_GENIE_8_CHAR 2

int DecodeGameGenieCode(const char *ggcode, int *address, int *data, int *compare);

#endif
