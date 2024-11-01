// SPDX-FileCopyrightText: Authors of TuxNES
// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Description: comptbl.c is a separate support program used in the TuxNES
 * build process. When run, the comptbl executable builds file 'compdata'
 * out of table.x86 which is later turned into table.o.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BLOCK_SIZE (256 * sizeof (uintptr_t *))
#define TREE_SIZE (4662 * BLOCK_SIZE)
#define DATA_SIZE (9624)

static uintptr_t *tree;
static unsigned char *data, *datap;
static unsigned char srcseq[256];      /* source (6502) code sequence */
static unsigned char srcmask[256];     /* source mask */
static unsigned char objseq[256];      /* object (native) code sequence */
static unsigned char objmod[256];      /* object code modifiers/relocators */
static int blocksalloc = 1;            /* next block of memory to allocate following tree */
/* source data flag, and-mask flag, low-nybble flag, full-byte flag,
   source length flag, dest macro flag, source byte number, dest byte number,
   current input ptr, line number, current decoded byte, obj mod char,
   obj mod offset, obj mod len, source seq len, dest seq len: */
static int sdf = 1, amf = 0, lnf = 0, fbf = 0, slf = 0, dmf = 0, sbn = 0, dbn = 0,
           cip = 0, cln = 0, cdb = 0, omc = 0, omo = 0, oml = 0, ssl = 0, dsl = 0;

#define align8(x) (((uintptr_t)(x) + (uintptr_t)7) & ~(uintptr_t)7)

static void do_tree(int, int, uintptr_t *);

int
main(int argc, char *argv[])
{
	int                    fd;
	char                   input[1024];

	fd = open("compdata", O_RDWR | O_TRUNC | O_CREAT, 0666);
	if (fd < 0)
		exit(EXIT_FAILURE);
	ftruncate(fd, TREE_SIZE);
	tree = mmap(NULL, TREE_SIZE,
	            PROT_READ | PROT_WRITE,
	            MAP_SHARED,
	            fd, 0);
	if (tree == MAP_FAILED) {
		perror("mmap");
		exit(EXIT_FAILURE);
	}
	data = mmap(NULL, DATA_SIZE,
	            PROT_READ | PROT_WRITE,
	            MAP_PRIVATE | MAP_ANONYMOUS,
	            -1, 0);
	if (data == MAP_FAILED) {
		perror("mmap");
		exit(EXIT_FAILURE);
	}
	memset(tree, 0, BLOCK_SIZE);
	datap = data;
	while (fgets(input, sizeof input, stdin)) {
		cln++;
		for (cip = 0; (input[cip] != 0) && (input[cip] != '#') && (input[cip] != '\n'); cip++) {
			char i = input[cip];
			/* Decode hex digits */
			if (!slf && !dmf) {
				if ((i >= '0' && i <= '9')
				 || ((i | 32) >= 'a' && (i | 32) <= 'f')) {
					if (fbf)
						goto parse_error;   /* one byte at a time only */
					if (lnf) {
						cdb |= (i | 32) - ((i | 32) >= 'a') * ('a' - 0xa - '0') - '0';
						lnf = 0;
						fbf = 1;
					} else {
						cdb = ((i | 32) - ((i | 32) >= 'a') * ('a' - 0xa - '0') - '0') << 4;
						lnf = 1;
					}
				} else {
					if (lnf)
						goto parse_error;   /* missing low nybble */
				}
			} else {
				if (slf) {
					/* Decode source length */
					if (i < '0' || i > '9')
						goto parse_error;   /* Not a number! */
					ssl = i - '0';
					slf = 0;
				}
			}

			/* Parse source data */
			if (sdf) {
				if ((lnf | fbf) == 0 && i == '/') {
					amf = 1;
				}
				if ((lnf | fbf) == 0 && i == ',') {
					amf = 0;
					slf = 1;
				}
				if (fbf) {
					if (amf) {
						srcmask[sbn - 1] = cdb;
						amf = 0;
					} else {
						srcmask[sbn] = 0xff;
						srcseq[sbn++] = cdb;
					}
					fbf = 0;
				}
				if (i == ':') {
					sdf = 0;
					if (ssl < sbn)
						goto parse_error;   /* Bytes read greater than length! */
				}
			} else {
				/* Parse target data */
				if (fbf) {
					objseq[dbn++] = cdb;
					fbf = 0;
				}
				if (i == '[') {
					dmf = 1;
					omc = 0;
					omo = 0;
				} else if (dmf) {
					if (i == 'R' || i == 'S' || i == 'T' || i == 'B'
					 || i == 'D' || i == 'E' || i == 'C' || i == 'Z'
					 || i == 'V' || i == 'F' || i == 'U' || i == 'N'
					 || i == 'M' || i == 'P' || i == 'A' || i == 'J'
					 || i == 'I' || i == 'W' || i == 'X' || i == 'O'
					 || i == 'Y' || i == 'L' || i == '!' || i == '>'
					 || i == '^')
						omc = i;
					if (i >= '0' && i <= '9')
						omo = i - '0';
					if (i == ']') {
						dmf = 0;
						if (omc == 0)
							goto parse_error;
						objmod[oml++] = omc;
						objmod[oml++] = dbn;
						objmod[oml++] = omo;
						if (omc == 'B' || omc == 'C' || omc == '^')
							objseq[dbn++] = 0;
						if (omc == 'W') {
							objseq[dbn++] = 0;
							objseq[dbn++] = 0;
						}
						if (omc == 'S' || omc == 'T' || omc == 'D'
						 || omc == 'Z' || omc == 'V' || omc == 'E'
						 || omc == 'R' || omc == 'F' || omc == 'P'
						 || omc == 'M' || omc == 'A' || omc == 'I'
						 || omc == 'J' || omc == 'U' || omc == 'X'
						 || omc == 'O' || omc == 'Y' || omc == 'L'
						 || omc == 'N') {
							objseq[dbn++] = 0;
							objseq[dbn++] = 0;
							objseq[dbn++] = 0;
							objseq[dbn++] = 0;
						}
					}
				}
				if (i == '/') {
					if (lnf | dmf)
						goto parse_error;
					/* done, insert into table */
#if 0
					printf("\ns:");
					for (int x = 0; x < sbn; x++) printf(" %02x", srcseq[x]);
					printf("\nm:");
					for (int x = 0; x < sbn; x++) printf(" %02x", srcmask[x]);
					printf("\nd:");
					for (int x = 0; x < dbn; x++) printf(" %02x", objseq[x]);
					printf("\n");
#endif
					dsl = dbn;
					do_tree(0, sbn, tree);

					if (align8((datap - data) + dsl + oml + 3) > DATA_SIZE) {
						printf("%s:%d: Buffer memory exceeded, increase %s and recompile\n", __FILE__, __LINE__, "DATA_SIZE");
						exit(EXIT_FAILURE);
					}
					*datap++ = ssl;
					*datap++ = dsl;
					for (int x = 0; x < dsl; x++)
						*datap++ = objseq[x];
					for (int x = 0; x < oml; x++)
						*datap++ = objmod[x];
					*datap++ = 0;
					datap = (unsigned char *)align8(datap);
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
	for (uintptr_t *ptr = tree, x = 256 * blocksalloc; x--; ptr++) {
		if (*ptr) {
			if (*ptr & 1) {
				*ptr -= (uintptr_t)data;
				*ptr += blocksalloc * BLOCK_SIZE;
			} else {
				*ptr -= (uintptr_t)tree;
			}
		}
	}
	if (fsync(fd) != 0)
		exit(EXIT_FAILURE);
	lseek(fd, blocksalloc * BLOCK_SIZE, SEEK_SET);
	ftruncate(fd, blocksalloc * BLOCK_SIZE);
	write(fd, data, datap - data);
	exit(EXIT_SUCCESS);

parse_error:
	printf("Parse error at line %d:\n", cln);
	printf("%s", input);
	for (int x = 0; x < cip; x++)
		putchar(input[x] == '\t' ? input[x] : ' ');
	printf("^\n");
	exit(EXIT_FAILURE);
}


/* Recursively build binary search tree */
static void
do_tree(int sbn, int len, uintptr_t *blockp)
{
	for (int x = 0; x < 256; x++) {
		if ((x & srcmask[sbn]) == srcseq[sbn]) {
			if (sbn + 1 == len) {
				blockp[x] = (uintptr_t)datap | 1;  /* Leaf node */
			} else {
				uintptr_t *nblockp;
				if (blockp[x] == 0 || blockp[x] & 1) {
					/* grow tree and copy data to new node */
#if 0
					printf("allocated block %d\n", blocksalloc);
#endif
					nblockp = &tree[256 * blocksalloc++];
					if ((blocksalloc * BLOCK_SIZE) > TREE_SIZE) {
						printf("%s:%d: Buffer memory exceeded, increase %s and recompile\n", __FILE__, __LINE__, "TREE_SIZE");
						exit(EXIT_FAILURE);
					}
					for (int y = 0; y < 256; y++)
						nblockp[y] = blockp[x];
					blockp[x] = (uintptr_t)nblockp;
				} else {
					/* traverse existing branch */
#if 0
					printf("following ptr...%02x %02x/%02x\n", srcseq[sbn], srcseq[sbn+1], srcmask[sbn+1]);
#endif
					nblockp = (uintptr_t *)blockp[x];
				}

				do_tree(sbn + 1, len, nblockp);
			}
		}
	}
}
