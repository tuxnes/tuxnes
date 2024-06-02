// SPDX-FileCopyrightText: Authors of TuxNES
// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Description: Constants for the Game Genie translator functions
 */

#ifndef GAMEGENIE_H
#define GAMEGENIE_H

#define GAME_GENIE_BAD_CODE 0
#define GAME_GENIE_6_CHAR 1
#define GAME_GENIE_8_CHAR 2

int DecodeGameGenieCode(const char *ggcode, int *address, int *data, int *compare);

#endif
