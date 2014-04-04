/*
 * This file is part of the TuxNES project codebase.
 *
 * Please see the README and COPYING files for more information regarding
 * this project.
 *
 * Description: comptbl.c is a separate support program used in the TuxNES
 * build process. When run, the comptbl executable builds file 'compdata'
 * out of table.x86 which is later turned into table.o.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#ifdef HAVE_FEATURES_H
#include <features.h>
#endif /* HAVE_FEATURES_H */

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define ALLOC_SIZE 0x48d801     /* approximately 4.69MB */
#define TBL_BASE ((unsigned char *)0x10000000)
#define TBL_BASE2 ((unsigned char *)0x20000000)

unsigned char srcseq[256];      /* source (6502) code sequence */
unsigned char srcmask[256];     /* source mask */
unsigned char objseq[256];      /* object (native) code sequence */
unsigned char objmod[256];      /* object code modifiers/relocators */
int blocksalloc = 1;            /* next block of memory to allocate following TBL_BASE */
/* source data flag, and-mask flag, low-nybble flag, full-byte flag,
   source length flag, dest macro flag, source byte number, dest byte number,
   current input ptr, line number, current decoded byte, obj mod char,
   obj mod offset, obj mod len, source seq len, dest seq len: */
int sdf = 1, amf = 0, lnf = 0, fbf = 0, slf = 0, dmf = 0, sbn = 0, dbn = 0,
  cip = 0, cln = 0, cdb = 0, omc = 0, omo = 0, oml = 0, ssl = 0, dsl = 0;
unsigned char *datap;

static int	do_tree(int, int*);
static void	memory_error(void);

int
main(int argc, char *argv[])
{
  int			 x;
  int			 fd;
  char			 input[1024], i;
  int			*ptr;

  fd = open ("compdata", O_RDWR | O_TRUNC | O_CREAT, 0666);
  if (fd < 0)
    exit (1);
  lseek (fd, 0x1000000, SEEK_SET);
  ftruncate (fd, 0x1000000);
  if (mmap (TBL_BASE, ALLOC_SIZE, PROT_READ | PROT_WRITE, MAP_FIXED |
            MAP_SHARED, fd, 0) != (void *) TBL_BASE)
    {
      printf ("Can't allocate memory!\n");
      exit (1);
    }
  if (mmap (TBL_BASE2, ALLOC_SIZE, PROT_READ | PROT_WRITE, MAP_FIXED |
            MAP_ANON | MAP_PRIVATE, -1, 0) != (void *) TBL_BASE2)
    {
      printf ("Can't allocate memory!\n");
      exit (1);
    }
  memset (TBL_BASE, 0, 1024);
  datap = TBL_BASE2;
  while (fgets (input, 1024, stdin))
    {
      cln++;
      for (cip = 0; (input[cip] != 0) && (input[cip] != '#') && (input[cip]
                                                                 != 10);
           cip++)
        {
          i = input[cip];
          /* Decode hex digits */
          if ((!slf) && (!dmf))
            {
              if ((i >= '0' && i <= '9') || ((i | 32) >= 'a' && (i | 32) <= 'f'))
                {
                  if (fbf)
                    goto parse_error;   /* one byte at a time only */
                  if (lnf)
                    {
                      cdb |= (i | 32) - ((i | 32) > 96) * 39 - 48;
                      lnf = 0;
                      fbf = 1;
                    }
                  else
                    {
                      cdb = ((i | 32) - ((i | 32) > 96) * 39 - 48) << 4;
                      lnf = 1;
                    }
                }
              else
                {
                  if (lnf)
                    goto parse_error;   /* missing low nybble */
                }
            }
          else
            {
              if (slf)          /* Decode source length */
                {
                  if (i < '0' || i > '9')
                    goto parse_error;   /* Not a number! */
                  ssl = i - 48;
                  slf = 0;
                }
            }

          /* Parse source data */
          if (sdf)
            {
              if ((lnf | fbf) == 0 && i == '/')
                {
                  amf = 1;
                }
              if ((lnf | fbf) == 0 && i == ',')
                {
                  amf = 0;
                  slf = 1;
                }
              if (fbf)
                {
                  if (amf)
                    {
                      srcmask[sbn - 1] = cdb;
                      amf = 0;
                    }
                  else
                    {
                      srcmask[sbn] = 0xff;
                      srcseq[sbn++] = cdb;
                    }
                  fbf = 0;
                }
              if (i == ':')
                {
                  sdf = 0;
                  if (ssl < sbn)
                    goto parse_error;   /* Bytes read greater than length! */
                  while (sbn <= ssl)
                    {
                      srcmask[sbn] = 0;
                      srcseq[sbn] = 0xff;
                      sbn++;
                    }
                }
            }
          else
            /* Parse target data */
            {
              if (fbf)
                {
                  objseq[dbn++] = cdb;
                  fbf = 0;
                }
              if (i == '[')
                {
                  dmf = 1;
                  omc = 0;
                  omo = 0;
                }
              else if (dmf)
                {
                  if (i == 'R' || i == 'S' || i == 'T' || i == 'B' ||
		      i == 'D' || i == 'E' || i == 'C' || i == 'Z' ||
		      i == 'V' || i == 'F' || i == 'U' || i == 'N' ||
		      i == 'M' || i == 'P' || i == 'A' || i == 'J' ||
		      i == 'I' || i == 'W' || i == 'X' || i == 'O' ||
		      i == 'Y' || i == 'L' || i == '!' || i == '>' ||
		      i == '^')
                    omc = i;
                  if (i >= '0' && i <= '9')
                    omo = i - 48;
                  if (i == ']')
                    {
                      dmf = 0;
                      if (omc == 0)
                        goto parse_error;
                      objmod[oml++] = omc;
                      objmod[oml++] = dbn;
                      objmod[oml++] = omo;
                      if (omc == 'B' || omc == 'C' || omc == '^')
                        objseq[dbn++] = 0;
                      if (omc == 'W')
                        {
                          objseq[dbn++] = 0;
                          objseq[dbn++] = 0;
                        }
                      if (omc == 'S' || omc == 'T' || omc == 'D' || omc ==
                          'Z' || omc == 'V' || omc == 'E' ||
                          omc == 'R' || omc == 'F' || omc == 'P' || omc ==
                          'M' || omc == 'A' || omc == 'I' ||
                          omc == 'J' || omc == 'U' || omc == 'X' || omc ==
                          'O' || omc == 'Y' || omc == 'L' ||
                          omc == 'N')
                        {
                          objseq[dbn++] = 0;
                          objseq[dbn++] = 0;
                          objseq[dbn++] = 0;
                          objseq[dbn++] = 0;
                        }
                    }
                }
              if (i == '/')
                {
                  if (lnf | dmf)
                    goto parse_error;
                  /* done, insert into table */
                  dsl = dbn;
                  /*
                     printf("\ns: ");
                     for(x=0;x<ssl;x++) printf("%2x",srcseq[x]);
                     printf("\nm: ");
                     for(x=0;x<ssl;x++) printf("%2x",srcmask[x]);
                     printf("\nd: ");
                     for(x=0;x<dsl;x++) printf("%2x",objseq[x]);
                     printf("\n");
                   */
                  sbn = 0;
                  dbn = 0;
                  do_tree(sbn, (int *)TBL_BASE);

                  if ((datap - TBL_BASE2) + dsl + oml + 3 > ALLOC_SIZE)
                    memory_error();
                  *(datap++) = ssl;
                  *(datap++) = dsl;
                  for (x = 0; x < dsl; x++)
                    *(datap++) = objseq[x];
                  for (x = 0; x < oml; x++)
                    *(datap++) = objmod[x];
                  *(datap++) = 0;
                  datap = (unsigned char *) (((unsigned int) datap + 7) & 0xfffffff8);
                  sdf = 1;
                  sbn = 0;
                  dbn = 0;
                  ssl = 0;
                  dsl = 0;
                  oml = 0;
                  break;
                }
            }

        }
    }
  /* relocate pointers */
  for (ptr = (int *) TBL_BASE, x = blocksalloc * 256; x--; ptr++)
    {
      if (*ptr)
        {
          if (*ptr & 1)
            *ptr -= (TBL_BASE2 - TBL_BASE) - blocksalloc * 1024;
          *ptr -= (int) TBL_BASE;
        }
    }
  if (fsync (fd) != 0)
    exit (1);
  lseek (fd, blocksalloc * 1024, SEEK_SET);
  ftruncate (fd, blocksalloc * 1024);
  write (fd, TBL_BASE2, datap - TBL_BASE2);
  exit (0);

parse_error:
  printf ("Parse error at line %d:\n", cln);
  printf ("%s", input);
  for (x = 0; x < cip; x++)
    if (input[x] == 9)
      printf ("%c", 9);
    else
      printf (" ");
  printf ("^\n");
  exit (1);
}


/* Recursively build binary search tree */
int
do_tree(int sbn, int *blockp)
{
  int			*nblockp;
  int			 x, y;

  for (x = 0; x < 256; x++)
    if ((x & srcmask[sbn]) == srcseq[sbn])
      {
        if (blockp[x] == 0 || (srcseq[sbn + 1] != 0 && srcmask[sbn + 1] == 0))
          blockp[x] = (unsigned int) datap | 1;         /* Leaf node */
        else
          {
            if (blockp[x] & 1)
              {
                /* grow tree and copy data to new node */
                /*printf("allocated block %d\n",blocksalloc); */
                nblockp = (int *) (TBL_BASE + (blocksalloc++) * 1024);
                if ((blocksalloc << 10) >= ALLOC_SIZE)
                  memory_error();
                for (y = 0; y < 256; y++)
                  nblockp[y] = blockp[x];
                blockp[x] = (int) nblockp;
              }
            else
              {
                /* traverse existing branch */
                /*printf ("following ptr...%x %x %x\n",srcseq[sbn],srcseq[sbn+1],srcmask[sbn+1]); */
                nblockp = (int *) blockp[x];
              }

            do_tree(sbn + 1, nblockp);
          }
      }
  return (0);
}

void
memory_error(void)
{
  printf ("Buffer memory exceeded in comptbl\n");
  printf (__FILE__ ":%d: increase ALLOC_SIZE and recompile\n", __LINE__);
  exit (1);
}
