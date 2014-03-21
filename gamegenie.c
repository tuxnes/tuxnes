/*
 * This file is part of the TuxNES project codebase.
 *
 * Please see the README and COPYING files for more information regarding
 * this project.
 *
 * $Id: gamegenie.c,v 1.7 2001/04/11 21:45:47 tmmm Exp $
 *
 * Description: Routines for translating Game Genie codes into useful
 * addresses and data
 */

#include <ctype.h>
#include <stdio.h>

#include "gamegenie.h"

static int	ggtable(char);

int
ggtable(char lookup)
{
  switch (toupper (lookup))
    {
    case 'A':
      return 0x0;
      break;
    case 'P':
      return 0x1;
      break;
    case 'Z':
      return 0x2;
      break;
    case 'L':
      return 0x3;
      break;
    case 'G':
      return 0x4;
      break;
    case 'I':
      return 0x5;
      break;
    case 'T':
      return 0x6;
      break;
    case 'Y':
      return 0x7;
      break;
    case 'E':
      return 0x8;
      break;
    case 'O':
      return 0x9;
      break;
    case 'X':
      return 0xA;
      break;
    case 'U':
      return 0xB;
      break;
    case 'K':
      return 0xC;
      break;
    case 'S':
      return 0xD;
      break;
    case 'V':
      return 0xE;
      break;
    case 'N':
      return 0xF;
      break;
    default:
      return -1;
      break;
    }
}

int
DecodeGameGenieCode (char *ggcode, int *address, int *data, int *compare)
{
  int codelen;
  int i;

  /* make sure the code is either 6 or 8 characters */
  codelen = strlen (ggcode);
  if ((codelen != 6) && (codelen != 8))
    {
/*    printf("invalid number of Game Genie character: %d\n", strlen(ggcode)); */
      return GAME_GENIE_BAD_CODE;
    }

  /* make sure that all of the characters are valid */
  for (i = 0; i < codelen; i++)
    {
      if (ggtable (ggcode[i]) == -1)
        {
/*      printf("bad character: %c\n", ggcode[i]); */
          return GAME_GENIE_BAD_CODE;
        }
    }

  /* get the data */
  if (codelen == 6)
    {
      *data =
        ((ggtable (ggcode[1]) & 7) << 4) | ((ggtable (ggcode[0]) & 8) << 4)
        | (ggtable (ggcode[0]) & 7) | (ggtable (ggcode[5]) & 8);
    }
  else
    {
      *data =
        ((ggtable (ggcode[1]) & 7) << 4) | ((ggtable (ggcode[0]) & 8) << 4)
        | (ggtable (ggcode[0]) & 7) | (ggtable (ggcode[7]) & 8);
    }

  /* get the address */
  *address = 0x8000 +
    (((ggtable(ggcode[3]) & 7) << 12)
    | ((ggtable(ggcode[5]) & 7) << 8) | ((ggtable(ggcode[4]) & 8) << 8)
    | ((ggtable(ggcode[2]) & 7) << 4) | ((ggtable(ggcode[1]) & 8) << 4)
    | (ggtable(ggcode[4]) & 7) | (ggtable(ggcode[3]) & 8));

  /* get the compare value if it's an 8-char code */
  if (codelen == 8)
    {
      *compare =
        ((ggtable (ggcode[7]) & 7) << 4) | ((ggtable (ggcode[6]) & 8) << 4)
        | (ggtable (ggcode[6]) & 7) | (ggtable (ggcode[5]) & 8);
      return GAME_GENIE_8_CHAR;
    }
  else
    {
      return GAME_GENIE_6_CHAR;
    }

  return 0;
}
