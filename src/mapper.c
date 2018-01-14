/*
 * This file is part of the TuxNES project codebase.
 *
 * Please see the README and COPYING files for more information regarding
 * this project.
 *
 * Description: This file controls the mapper subsystem for the emulator.
 * See the HACKING file (and examine existing mapper code) for information
 * on how to add new mappers.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "consts.h"
#include "globals.h"
#include "mapper.h"

extern void MAPPER_NONE;
extern void MAPPER_MMC1;
extern void MAPPER_UNROM;
extern void MAPPER_CNROM;
extern void MAPPER_MMC3;
extern void MAPPER_MMC5;
extern void MAPPER_MMC2;
extern void MAPPER_MMC4;
extern void MAPPER_CLRDRMS;
extern void MAPPER_AOROM;
extern void MAPPER_CPROM;
extern void MAPPER_100IN1;
extern void MAPPER_NAMCOT106;
extern void MAPPER_VRC2_A;
extern void MAPPER_VRC2_B;
extern void MAPPER_G101;
extern void MAPPER_TAITO_TC0190;
extern void MAPPER_GNROM;
extern void MAPPER_TENGEN_RAMBO1;
extern void MAPPER_SUNSOFT4;
extern void MAPPER_FME7;
extern void MAPPER_CAMERICA;
extern void MAPPER_IREM_74HC161_32;
extern void MAPPER_VS;
extern void MAPPER_SUPERVISION;
extern void MAPPER_NINA7;

void (*MapperInit[MAXMAPPER + 1])(void);
void *Mapper[MAXMAPPER + 1];
char MapperName[MAXMAPPER + 1][30];

static int	commandregister;
static void	init_none(void);
static int	MapRom(int, unsigned, unsigned);

/* ROM mapper table (pointers) */
u_char *MAPTABLE[17];

/* some globals */
extern unsigned char	*VROM_BASE;
extern unsigned int	 ROM_PAGES;
extern unsigned int	 VROM_PAGES;
extern unsigned int	 ROM_MASK;
extern unsigned int	 VROM_MASK;
extern unsigned int	 VROM_MASK_1k;
extern unsigned int	 mapmirror;
extern unsigned int	 irqflag;

void	InitMapperSubsystem(void);
void	m100in1(int, int);
void	aorom(int, int);
void	camerica(int, int);
void	clrdrms(int, int);
void	cnrom(int, int);
void	cprom(int, int);
void	(*drawimage)(int);
void	fme7(int, int);
void	g101(int, int);
void	gnrom(int, int);
void	irem_74hc161_32(int, int);
void	mmc1(int, int);
void	mmc2(int, int);
void	mmc3(int, int);
void	mmc5(int, int);
void	namcot106(int, int);
void	nina7(int, int);
void	sunsoft4(int, int);
void	supervision(int, int);
void	taito_tc0190(int, int);
void	tengen_rambo1(int, int);
void	unrom(int, int);
void	vrc2_a(int, int);
void	vrc2_b(int, int);
void	vs(int, int);

static void	init_100in1(void);
static void	init_aorom(void);
static void	init_camerica(void);
static void	init_clrdrms(void);
static void	init_cnrom(void);
static void	init_cprom(void);
static void	init_fme7(void);
static void	init_g101(void);
static void	init_gnrom(void);
static void	init_irem_74hc161_32(void);
static void	init_mmc1(void);
static void	init_mmc2(void);
static void	init_mmc3(void);
static void	init_mmc5(void);
static void	init_namcot106(void);
static void	init_nina7(void);
static void	init_sunsoft4(void);
static void	init_supervision(void);
static void	init_taito_tc0190(void);
static void	init_tengen_rambo1(void);
static void	init_unrom(void);
static void	init_vrc2_a(void);
static void	init_vrc2_b(void);
static void	init_vs(void);
void	mmc2_4_latch(int);
void	mmc2_4_latchspr(int);

#define LAST_PAGE (ROM_PAGES-1)
#define LAST_HALF_PAGE ((ROM_PAGES<<1)-1)

static int prgmask;
static int chrmask;

/* #define DEBUG_MAPPER 1 */

/****************************************************************************/

/*
   This function maps a chunk of ROM beginning loc bytes from the
   ROM_BASE which is size bytes large into the MAPTABLE array starting
   at page. Constraints:
   8 <= page <= 15  (because those are the ROM regions)
   m = 8, 16, or 32 kilobytes
   m/4K <= (16 - page)  (so it doesn't try to map outside of table)
   If any of these conditions are not met, function returns -1; returns
   0 otherwise.
 */

/* convenience constants to make function calls a little more intuitive */
#define SIZE_8K  8192
#define SIZE_16K 16384
#define SIZE_32K 32768

#define PAGE_8000 0x8
#define PAGE_9000 0x9
#define PAGE_A000 0xA
#define PAGE_B000 0xB
#define PAGE_C000 0xC
#define PAGE_D000 0xD
#define PAGE_E000 0xE
#define PAGE_F000 0xF

int
MapRom(int page, unsigned loc, unsigned size)
{
  int x;

  if ((page < 8) || (page > 15))
    {
      return -1;
    }

  if (size % 4096 != 0)
    {
      return -1;
    }

  if ((size / 4096) > (16 - page))
    {
      return -1;
    }

  for (x = page; x < page + (size / 4096); x++)
    {
      MAPTABLE[x] = ROM_BASE + loc - (page * 4096);
    }

  return 0;
}

/****************************************************************************/

static void
init_none(void)
{
  if (ROM_PAGES < 2)
    {
      MapRom (PAGE_C000, 0, SIZE_16K);
    }
  else
    {
      MapRom (PAGE_8000, 0, SIZE_32K);
    }

  mapmirror = 0;
}

/****************************************************************************/

/* defaults for MMC1 (Zelda, etc) */
static void
init_mmc1(void)
{
  /* First page is mapped to first page of rom */
  MapRom (PAGE_8000, 0, SIZE_16K);

  /* Last page is mapped to last page of rom */
  MapRom (PAGE_C000, LAST_PAGE * 16384, SIZE_16K);

  mapmirror = 1;
}

void
mmc1(int addr, int val)
{
  static int mmc1reg[4];
  static int mmc1shc[4];
  val &= 0xff;
  /*printf("Mapper MMC1:%4x,%2x\n",addr,val); */
  /*printf("Mapper: %4x,%2x stack at %x, shift %d,%d,%d,%d\n",addr,val,STACKPTR,
     mmc1shc[0],mmc1shc[1],mmc1shc[2],mmc1shc[3]); */

  if (val & 0x80)
    {
      /* Reset Mapper */
      mmc1reg[0] |= 12;
      mmc1shc[0] = 0;
      /* When the mapper is reset, does it affect just one register or all?
         I think just one, but I am unsure of the exact effect of a reset. */
      /*mmc1reg[1]=0; */ mmc1shc[1] = 0;
      /*mmc1reg[2]=0; */ mmc1shc[2] = 0;
      /*mmc1reg[3]=0; */ mmc1shc[3] = 0;
      /*mmc1reg[1]=mmc1reg[2]=mmc1reg[3]=0; */
      /* MMC1 always has mirroring */
      nomirror = 0;
      /*printf("Mapper: MMC1 reset\n"); */
    }
  else
    {
      mmc1reg[(addr >> 13) & 3] >>= 1;
      mmc1reg[(addr >> 13) & 3] |= (val & 1) << 4;
      mmc1shc[(addr >> 13) & 3]++;
      if (mmc1shc[(addr >> 13) & 3] % 5 == 0)
        {
          if (((addr >> 13) & 3) == 0)
            {
              drawimage (CLOCK * 3);
              hvmirror = mmc1reg[0] & 1;
              osmirror = (~mmc1reg[0] & 2) >> 1;
            }
          else if (VROM_PAGES)
            {
              drawimage (CLOCK * 3);
              if (((addr >> 13) & 3) == 1)
                memcpy (VRAM, VROM_BASE + (mmc1reg[1] & 31) * 0x1000, 4096
                        << ((mmc1reg[0] & 16) == 0));
              if (((addr >> 13) & 3) == 2 && (mmc1reg[0] & 16) == 16)
                memcpy (VRAM + 0x1000, VROM_BASE + (mmc1reg[2] & 31) *
                        0x1000, 4096);
            }

          /* Set Map Table */

          if (mmc1reg[0] & 8)   /* Swap 16K rom */
            {
              if (mmc1reg[0] & 4)
                {               /* Swap $8000 */
                  MAPTABLE[8] = MAPTABLE[9] = MAPTABLE[10] = MAPTABLE[11] =
                    ROM_BASE + ((mmc1reg[3] & ROM_MASK) << 14) - 0x8000;
                  MAPTABLE[12] = MAPTABLE[13] = MAPTABLE[14] = MAPTABLE[15]
                    = ROM_BASE + (ROM_PAGES << 14) - 0x10000;   /* Last Page hardwired */
                  if (ROM_PAGES > 16)   /* 512K roms */
                    {
                      MAPTABLE[8] = MAPTABLE[9] = MAPTABLE[10] =
                        MAPTABLE[11] = ROM_BASE + (mmc1reg[3] << 14) +
                        ((mmc1reg[1]
                          &
                          16)
                         <<
                         14) - 0x8000;
                      MAPTABLE[12] = MAPTABLE[13] = MAPTABLE[14] =
                        MAPTABLE[15] = ROM_BASE + 262144 + ((mmc1reg[1] &
                                                       16) << 14) - 0x10000;    /* Last Page semi-hardwired */
                    }
                }
              else
                {               /* Swap $C000 */
                  MAPTABLE[8] = MAPTABLE[9] = MAPTABLE[10] = MAPTABLE[11] =
                    ROM_BASE - 0x8000;  /* First page hardwired */
                  MAPTABLE[12] = MAPTABLE[13] = MAPTABLE[14] = MAPTABLE[15]
                    = ROM_BASE + ((mmc1reg[3] & ROM_MASK) << 14)
		    + (((mmc1reg[1] & 16) << 14) & ((ROM_PAGES > 16) << 18))
		    - 0xC000;
                }
            }
          else
            /* Swap 32K rom */
            {
              MAPTABLE[8] = MAPTABLE[9] = MAPTABLE[10] = MAPTABLE[11] =
                MAPTABLE[12] = MAPTABLE[13] = MAPTABLE[14] = MAPTABLE[15] =
                ROM_BASE + ((mmc1reg[3] & ROM_MASK & (~1)) << 14)
		+ (((mmc1reg[1] & 16) << 14) & ((ROM_PAGES > 16) << 18))
                - 0x8000;
            }
        }
    }
}

/****************************************************************************/

static void
init_unrom(void)
{
  /* figure out if the PRG bank should be masked */
  prgmask = 0;
  while (ROM_PAGES - 1 > prgmask)
    {
      prgmask = (prgmask << 1) | 1;
    }

  /* First page is mapped to first page of rom */
  MapRom (PAGE_8000, 0, SIZE_16K);

  /* Last page is mapped to last page of rom */
  MapRom (PAGE_C000, LAST_PAGE * 16384, SIZE_16K);

  mapmirror = 0;
}

void
unrom(int addr, int val)
{
  val &= 0x0f;

#if DEBUG_MAPPER
  if (verbose)
    {
      printf("addr = %04X, val = %02X\n", addr, val);
    }
#endif

  MapRom (PAGE_8000, (val & prgmask) * 16384, SIZE_16K);
}

/****************************************************************************/

static void
init_cnrom(void)
{
  /* figure out if the CHR bank should be masked */
  chrmask = 0;
  while (VROM_PAGES - 1 > chrmask)
    {
      chrmask = (chrmask << 1) | 1;
    }

  init_none();
  memcpy (VRAM, VROM_BASE, 8192);
}

void
cnrom(int addr, int val)
{
  val &= 0xff;

#if DEBUG_MAPPER
  if (verbose)
    {
      printf("addr = %04X, val = %02X\n", addr, val);
    }
#endif

  memcpy (VRAM, VROM_BASE + (val & chrmask) * 8192, 8192);
}

/****************************************************************************/

/* defaults for MMC3 (SMB2, etc) */
static void
init_mmc3(void)
{
  /* this is a quick hack to satisfy a technical purity issue */
  if (ROM_PAGES < 2)
    {
      MapRom (PAGE_C000, 0, SIZE_16K);
      return;
    }

  /* First page is mapped to second-to-last page of rom */
  MAPTABLE[8] = MAPTABLE[9] = ROM_BASE - 0x4000;
  if (ROM_PAGES > 2)
    MAPTABLE[8] = MAPTABLE[9] = ROM_BASE + 0x4000;
  if (ROM_PAGES > 4)
    MAPTABLE[8] = MAPTABLE[9] = ROM_BASE + 0x14000;
  if (ROM_PAGES > 8)
    MAPTABLE[8] = MAPTABLE[9] = ROM_BASE + 0x34000;
  if (ROM_PAGES > 16)
    MAPTABLE[8] = MAPTABLE[9] = ROM_BASE + 0x74000;

  MAPTABLE[10] = MAPTABLE[11] = ROM_BASE + (ROM_PAGES << 14) - 0xE000;

  /* Last page is mapped to last page of rom */
  MAPTABLE[12] = MAPTABLE[13] = MAPTABLE[14] = MAPTABLE[15] = ROM_BASE - 0x4000;
  if (ROM_PAGES > 2)
    MAPTABLE[12] = MAPTABLE[13] = MAPTABLE[14] = MAPTABLE[15] = ROM_BASE;
  if (ROM_PAGES > 4)
    MAPTABLE[12] = MAPTABLE[13] = MAPTABLE[14] = MAPTABLE[15] = ROM_BASE + 0x10000;
  if (ROM_PAGES > 8)
    MAPTABLE[12] = MAPTABLE[13] = MAPTABLE[14] = MAPTABLE[15] = ROM_BASE + 0x30000;
  if (ROM_PAGES > 16)
    MAPTABLE[12] = MAPTABLE[13] = MAPTABLE[14] = MAPTABLE[15] = ROM_BASE + 0x70000;

  mapmirror = 1;
}

void
mmc3(int addr, int val)
{
  static unsigned char mmc3cmd;
  static unsigned char irqval;
  static unsigned char irqenabled = 0;

  val &= 0xff;

#if DEBUG_MAPPER
  if (verbose)
    {
      printf("addr = %04X, val = %02X\n", addr, val);
    }
#endif

  if (addr == 0x8000)
    mmc3cmd = val;
  if (addr == 0x8001)
    {
      if ((mmc3cmd & 7) < 6)
        drawimage (CLOCK * 3);

      if ((mmc3cmd & 0x87) == 0)        /* Switch first 2k video ROM segment */
        memcpy (VRAM, VROM_BASE + (val & VROM_MASK_1k & (~1)) * 1024, 2048);
      if ((mmc3cmd & 0x87) == 1)        /* Switch second 2k video ROM segment */
        memcpy (VRAM + 0x800, VROM_BASE + (val & VROM_MASK_1k & (~1)) * 1024, 2048);
      if ((mmc3cmd & 0x87) == 0x80)     /* Switch first 2k video ROM segment to alternate address */
        memcpy (VRAM + 0x1000, VROM_BASE + (val & VROM_MASK_1k & (~1)) * 1024, 2048);
      if ((mmc3cmd & 0x87) == 0x81)     /* Switch second 2k video ROM segment to alternate address */
        memcpy (VRAM + 0x1800, VROM_BASE + (val & VROM_MASK_1k & (~1)) * 1024, 2048);

      if ((mmc3cmd & 0x87) == 2)        /* Switch first 1k video ROM segment */
        memcpy (VRAM + 4096, VROM_BASE + (val & VROM_MASK_1k) * 1024, 1024);
      if ((mmc3cmd & 0x87) == 3)        /* Switch second 1k video ROM segment */
        memcpy (VRAM + 4096 + 1024, VROM_BASE + (val & VROM_MASK_1k) * 1024, 1024);
      if ((mmc3cmd & 0x87) == 4)        /* Switch third 1k video ROM segment */
        memcpy (VRAM + 4096 + 2048, VROM_BASE + (val & VROM_MASK_1k) * 1024, 1024);
      if ((mmc3cmd & 0x87) == 5)        /* Switch fourth 1k video ROM segment */
        memcpy (VRAM + 4096 + 3072, VROM_BASE + (val & VROM_MASK_1k) * 1024, 1024);
      if ((mmc3cmd & 0x87) == 0x82)     /* Switch first 1k video ROM segment to alt addr */
        memcpy (VRAM, VROM_BASE + (val & VROM_MASK_1k) * 1024, 1024);
      if ((mmc3cmd & 0x87) == 0x83)     /* Switch second 1k video ROM segment to alt addr */
        memcpy (VRAM + 1024, VROM_BASE + (val & VROM_MASK_1k) * 1024, 1024);
      if ((mmc3cmd & 0x87) == 0x84)     /* Switch third 1k video ROM segment to alt addr */
        memcpy (VRAM + 2048, VROM_BASE + (val & VROM_MASK_1k) * 1024, 1024);
      if ((mmc3cmd & 0x87) == 0x85)     /* Switch fourth 1k video ROM segment to alt addr */
        memcpy (VRAM + 3072, VROM_BASE + (val & VROM_MASK_1k) * 1024, 1024);

      if ((mmc3cmd & 0x47) == 0x06)     /* Switch $8000 */
        MAPTABLE[8] = MAPTABLE[9] = ROM_BASE + 8192 * (val & LAST_HALF_PAGE)
          - 0x8000;
      if ((mmc3cmd & 0x47) == 0x46)     /* Switch $C000 */
        MAPTABLE[12] = MAPTABLE[13] = ROM_BASE + 8192 * (val &
                                                   LAST_HALF_PAGE) - 0xC000;

      if ((mmc3cmd & 7) == 7)   /* Switch $A000 */
        MAPTABLE[10] = MAPTABLE[11] = ROM_BASE + 8192 * (val &
                                                   LAST_HALF_PAGE) - 0xA000;

    }
  if (addr == 0xA000)
    {
      hvmirror = val & 1;
    }
  if (addr == 0xC000)
    {
      irqval = val;
      if (irqenabled)
        {
          if (CLOCK >= VBL)
            {
              irqflag = 1;
              CTNI = -((PPF - CLOCK * 3 + HCYCLES * (irqval)) / 3);
            }
          else
            {
              if ((CLOCK * 3 + (irqval + 1) * HCYCLES) < PBL)
                {
                  irqflag = 1;
                  CTNI = -((HCYCLES * (irqval + 1) - ((CLOCK * 3) %
                                                      HCYCLES)) / 3);
                }
              /*printf("clock %d irqval %d set\n",CLOCK,irqval); */
            }
        }
    }

  if (addr == 0xE000)
    {
      irqenabled = 0;
      irqflag = 0;
      CTNI = -((VBL - CLOCK + CPF) % CPF);
    }

  if (addr == 0xE001)
    {
      irqenabled = 1;
      if (CLOCK >= VBL)
        {
          irqflag = 1;
          CTNI = -((PPF - CLOCK * 3 + HCYCLES * (irqval)) / 3);
          /*printf("%d %d \n",CLOCK,CTNI); */
        }
      else
        {
          if ((CLOCK * 3 + (irqval + 1) * HCYCLES) < PBL)
            {
              irqflag = 1;
              CTNI = -((HCYCLES * (irqval + 1) - ((CLOCK * 3) % HCYCLES)) / 3);
              /*printf("%d %d \n",CLOCK,CTNI); */
            }
        }
    }
}

/****************************************************************************/

/* MMC5 */

/* possible PRG bank sizes */
#define PRG_32K 0
#define PRG_16K 1
#define PRG_8K 2

/* possible CHR bank sizes */
#define CHR_8K 0
#define CHR_4K 1
#define CHR_2K 2
#define CHR_1K 3

static int prgbanksize;
static int chrbanksize;
static int exramselect;
static int nametableselect;
static char *blankbank;

static int prgmask8;
static int prgmask16;
static int prgmask32;

static void
init_mmc5(void)
{
  int i;

  prgbanksize = 2;

  /* figure out if the PRG bank should be masked */
  prgmask = 0;
  while (ROM_PAGES - 1 > prgmask)
    {
      prgmask = (prgmask << 1) | 1;
    }

  prgmask8 = prgmask << 2;
  prgmask16 = prgmask;
  prgmask32 = prgmask >> 2;

  blankbank = malloc(4096);
  for (i = 0; i < 4096; i++)
    {
      blankbank[i] = 0xFF;
    }

  MapRom (PAGE_8000, LAST_PAGE * 16384 + 8192, SIZE_8K);
  MapRom (PAGE_A000, LAST_PAGE * 16384 + 8192, SIZE_8K);
  MapRom (PAGE_C000, LAST_PAGE * 16384 + 8192, SIZE_8K);
  MapRom (PAGE_E000, LAST_PAGE * 16384 + 8192, SIZE_8K);
  mapmirror = 0;
}

void
mmc5(int addr, int val)
{
  val &= 0xff;

#if DEBUG_MAPPER
  if (verbose)
    {
      printf ("address = %04X, value = %02X\n", addr, val);
    }
#endif

  switch (addr)
    {
    /* mapper registers */
    case 0x5100:
      prgbanksize = val & 0x03;
      break;
    case 0x5101:
      chrbanksize = val & 0x03;
      break;
    case 0x5102:
      break;
    case 0x5103:
      break;
    case 0x5104:
      exramselect = val & 0x03;
      break;
    case 0x5105:
      nametableselect = val;
      break;
    case 0x5106:
      break;
    case 0x5107:
      break;

    case 0x5113:
      break;

    /* PRG bank switching */
    case 0x5114:
      if (prgbanksize >= PRG_8K)
        {
          MapRom(PAGE_8000, (val & prgmask8) * 8192, SIZE_8K);
        }
      break;
    case 0x5115:
      if (prgbanksize >= PRG_8K)
        {
          MapRom(PAGE_A000, (val & prgmask8) * 8192, SIZE_8K);
        }
      else if (prgbanksize == PRG_16K)
        {
          MapRom(PAGE_8000, ((val >> 1) & prgmask16) * 16384, SIZE_16K);
        }
      break;
    case 0x5116:
      if (prgbanksize >= PRG_8K)
        {
          MapRom(PAGE_C000, (val & prgmask8) * 8192, SIZE_8K);
        }
      break;
    case 0x5117:
      if (prgbanksize >= PRG_8K)
        {
          MapRom(PAGE_E000, (val & prgmask8) * 8192, SIZE_8K);
        }
      else if (prgbanksize == PRG_16K)
        {
          MapRom(PAGE_8000, ((val >> 1) & prgmask16) * 16384, SIZE_16K);
        }
      else
        {
          MapRom(PAGE_8000, ((val >> 2) & prgmask32) * 32768, SIZE_32K);
        }
      break;

    /* CHR bank switching for sprites */
    case 0x5120:
      if (chrbanksize == CHR_1K)
        {
          memcpy (VRAM + 0x0000, VROM_BASE + val * 1024, 1024);
        }
      break;
    case 0x5121:
      if (chrbanksize == CHR_1K)
        {
          memcpy (VRAM + 0x0400, VROM_BASE + val * 1024, 1024);
        }
      else if (chrbanksize == CHR_2K)
        {
          memcpy (VRAM + 0x0000, VROM_BASE + val * 2048, 2048);
        }
      break;
    case 0x5122:
      if (chrbanksize == CHR_1K)
        {
          memcpy (VRAM + 0x0800, VROM_BASE + val * 1024, 1024);
        }
      break;
    case 0x5123:
      if (chrbanksize == CHR_1K)
        {
          memcpy (VRAM + 0x0C00, VROM_BASE + val * 1024, 1024);
        }
      else if (chrbanksize == CHR_2K)
        {
          memcpy (VRAM + 0x0800, VROM_BASE + val * 2048, 2048);
        }
      else if (chrbanksize == CHR_4K)
        {
          memcpy (VRAM + 0x0000, VROM_BASE + val * 4096, 4096);
        }
      break;
    case 0x5124:
      if (chrbanksize == CHR_1K)
        {
          memcpy (VRAM + 0x1000, VROM_BASE + val * 1024, 1024);
        }
      break;
    case 0x5125:
      if (chrbanksize == CHR_1K)
        {
          memcpy (VRAM + 0x1400, VROM_BASE + val * 1024, 1024);
        }
      else if (chrbanksize == CHR_2K)
        {
          memcpy (VRAM + 0x1000, VROM_BASE + val * 2048, 2048);
        }
      break;
    case 0x5126:
      if (chrbanksize == CHR_1K)
        {
          memcpy (VRAM + 0x1800, VROM_BASE + val * 1024, 1024);
        }
      break;
    case 0x5127:
      if (chrbanksize == CHR_1K)
        {
          memcpy (VRAM + 0x1C00, VROM_BASE + val * 1024, 1024);
        }
      else if (chrbanksize == CHR_2K)
        {
          memcpy (VRAM + 0x1800, VROM_BASE + val * 2048, 2048);
        }
      else if (chrbanksize == CHR_4K)
        {
          memcpy (VRAM + 0x1000, VROM_BASE + val * 4096, 4096);
        }
      else if (chrbanksize == CHR_8K)
        {
          memcpy (VRAM + 0x1000, VROM_BASE + val * 4096, 4096);
        }
      break;

      /* CHR bank switching for nametables */
      case 0x5128:
        if ((exramselect & 0x01) == 0)
          {
            memcpy (VRAM + 0x0000, VROM_BASE + val * 2048, 2048);
          }
        break;
      case 0x5129:
        if ((exramselect & 0x01) == 0)
          {
            memcpy (VRAM + 0x0800, VROM_BASE + val * 2048, 2048);
          }
        break;
      case 0x512A:
        if ((exramselect & 0x01) == 0)
          {
            memcpy (VRAM + 0x1000, VROM_BASE + val * 2048, 2048);
          }
        break;
      case 0x512B:
        if ((exramselect & 0x01) == 0)
          {
            memcpy (VRAM + 0x1800, VROM_BASE + val * 2048, 2048);
          }
        break;
    }
}

/****************************************************************************/

/* mapper 7: Rare */
static void
init_aorom(void)
{
  mapmirror = osmirror = 1;
  MapRom (PAGE_8000, 0, SIZE_32K);
}

void
aorom(int addr, int val)
{
  static int mirrorbit = 0;
  char temp[1024];
  /*val&=0xff;printf("Mapper AOROM:%4x,%2x (%d)\n",addr,val,CLOCK); */
  val &= 0x1f;
  if ((val >> 4) != mirrorbit)
    {
      drawimage (CLOCK * 3);
      memcpy (temp, VRAM + 0x2000, 1024);
      memcpy (VRAM + 0x2000, VRAM + 0x2400, 1024);
      memcpy (VRAM + 0x2400, temp, 1024);
      mirrorbit = (val >> 4);
    }
  val &= 0x0f;
  val &= ROM_PAGES - 1;
  MapRom (PAGE_8000, val << 15, SIZE_32K);
}

/****************************************************************************/

/* MMC2 for PunchOut contributed by Kristoffer Brånemyr */
static int mmc2_4_latch1;
static int mmc2_4_latch1hi;
static int mmc2_4_latch1low;
static int mmc2_4_latch2;
static int mmc2_4_latch2hi;
static int mmc2_4_latch2low;
static int mmc2_4_init = 0;
static void
init_mmc2(void)
{
  /* the first page is at the start of the rom */
  MAPTABLE[8] = MAPTABLE[9] = ROM_BASE - 0x8000;
  /* the last three pages are at the end of the rom */
  MAPTABLE[10] = MAPTABLE[11] =
    MAPTABLE[12] = MAPTABLE[13] =
    MAPTABLE[14] = MAPTABLE[15] = ROM_BASE + 0x10000;

  mmc2_4_latch1 = 1;
  mmc2_4_latch1low = 0;
  mmc2_4_latch1hi = 0;
  mmc2_4_latch2 = 1;
  mmc2_4_latch2low = 0;
  mmc2_4_latch2hi = 0;
  mmc2_4_init = 1;
}

static void
init_mmc4(void)
{
  MapRom (PAGE_8000, 0, SIZE_16K);
  MapRom (PAGE_C000, LAST_PAGE * 16384, SIZE_16K);

  mmc2_4_latch1 = 1;
  mmc2_4_latch1low = 0;
  mmc2_4_latch1hi = 0;
  mmc2_4_latch2 = 1;
  mmc2_4_latch2low = 0;
  mmc2_4_latch2hi = 0;
  mmc2_4_init = 1;
}

void
mmc2_4_latch(int addr)
{
  if (mmc2_4_init == 0)
    return;
  addr = addr & 0xfff;
  if (addr >= 0xfd0 && addr <= 0xfdf)
    {
      mmc2_4_latch1 = 0;
      memcpy (VRAM + 0x1000, VROM_BASE + (mmc2_4_latch1low * 0x1000), 0x1000);
    }
  else if (addr >= 0xfe0 && addr <= 0xfef)
    {
      mmc2_4_latch1 = 1;
      memcpy (VRAM + 0x1000, VROM_BASE + (mmc2_4_latch1hi * 0x1000), 0x1000);
    }
}

void
mmc2_4_latchspr(int tile)
{
  if (mmc2_4_init == 0)
    return;
  if (tile == 0xfd)
    {
      mmc2_4_latch2 = 0;
      memcpy (VRAM, VROM_BASE + (mmc2_4_latch2low * 0x1000), 0x1000);
    }
  else if (tile == 0xfe)
    {
      mmc2_4_latch2 = 1;
      memcpy (VRAM, VROM_BASE + (mmc2_4_latch2hi * 0x1000), 0x1000);
    }
}

void
mmc2(int addr, int val)
{
  if (addr >= 0xA000 && addr <= 0xAFFF)
    {
      /* switch $8000 */
      MAPTABLE[8] = MAPTABLE[9] = ROM_BASE - 0x8000 + (val * 0x2000);
      return;
    }
  if (addr >= 0xB000 && addr <= 0xBFFF)
    {
      /* switch ppu $0000 */
      mmc2_4_latch2low = val;   /* #1 */
    }
  if (addr >= 0xC000 && addr <= 0xCFFF)
    {
      /* switch ppu $0000 */
      mmc2_4_latch2hi = val;    /* #2 */
    }
  if (addr >= 0xB000 && addr <= 0xCFFF)
    {
      /* switch ppu $0000 */
      if (!mmc2_4_latch2) {
        memcpy (VRAM, VROM_BASE + (mmc2_4_latch2low * 0x1000), 0x1000);
      } else {
        memcpy (VRAM, VROM_BASE + (mmc2_4_latch2hi * 0x1000), 0x1000);
      }
    }
  if (addr >= 0xD000 && addr <= 0xDFFF)
    {
      /* switch ppu $1000 */
      mmc2_4_latch1low = val;   /* #1 */
    }
  if (addr >= 0xE000 && addr <= 0xEFFF)
    {
      /* switch ppu $1000 */
      mmc2_4_latch1hi = val;    /* #2 */
    }
  if (addr >= 0xd000 && addr <= 0xefff)
    {
      if (!mmc2_4_latch1) {
        memcpy (VRAM + 0x1000, VROM_BASE + (mmc2_4_latch1low * 0x1000), 0x1000);
      } else {
        memcpy (VRAM + 0x1000, VROM_BASE + (mmc2_4_latch1hi * 0x1000), 0x1000);
      }
    }

  if (addr >= 0xF000 && addr <= 0xFFFF)
    {
      if (val == 0)
        hvmirror = 0;
      else
        hvmirror = 1;
    }
}

void
mmc4(int addr, int val)
{
  if (addr >= 0xA000 && addr <= 0xAFFF)
    {
      /* switch $8000 */
      MapRom (PAGE_8000, val * 16384, SIZE_16K);
      return;
    }
  if (addr >= 0xB000 && addr <= 0xBFFF)
    {
      /* switch ppu $0000 */
      mmc2_4_latch2low = val;   /* #1 */
    }
  if (addr >= 0xC000 && addr <= 0xCFFF)
    {
      /* switch ppu $0000 */
      mmc2_4_latch2hi = val;    /* #2 */
    }
  if (addr >= 0xB000 && addr <= 0xCFFF)
    {
      /* switch ppu $0000 */
      if (!mmc2_4_latch2) {
        memcpy (VRAM, VROM_BASE + (mmc2_4_latch2low * 0x1000), 0x1000);
      } else {
        memcpy (VRAM, VROM_BASE + (mmc2_4_latch2hi * 0x1000), 0x1000);
      }
    }
  if (addr >= 0xD000 && addr <= 0xDFFF)
    {
      /* switch ppu $1000 */
      mmc2_4_latch1low = val;   /* #1 */
    }
  if (addr >= 0xE000 && addr <= 0xEFFF)
    {
      /* switch ppu $1000 */
      mmc2_4_latch1hi = val;    /* #2 */
    }
  if (addr >= 0xd000 && addr <= 0xefff)
    {
      if (!mmc2_4_latch1) {
        memcpy (VRAM + 0x1000, VROM_BASE + (mmc2_4_latch1low * 0x1000), 0x1000);
      } else {
        memcpy (VRAM + 0x1000, VROM_BASE + (mmc2_4_latch1hi * 0x1000), 0x1000);
      }
    }

  if (addr >= 0xF000 && addr <= 0xFFFF)
    {
      if (val == 0)
        hvmirror = 0;
      else
        hvmirror = 1;
    }
}

/****************************************************************************/

/* Mapper 11 (Colordreams) */
static void
init_clrdrms(void)
{
  /* figure out if the PRG bank should be masked */
  prgmask = 0;
  while (ROM_PAGES - 1 > prgmask)
    {
      prgmask = (prgmask << 1) | 1;
    }
  /* this mapper uses 32K pages, so shift once more */
  prgmask >>= 1;

  /* figure out if the CHR bank should be masked */
  chrmask = 0;
  while (VROM_PAGES - 1 > chrmask)
    {
      chrmask = (chrmask << 1) | 1;
    }

  MapRom (PAGE_8000, 0, SIZE_32K);
}

void
clrdrms(int addr, int val)
{
  val &= 0xFF;

#if DEBUG_MAPPER
  if (verbose)
    {
      printf("addr = %04X, val = %02X\n", addr, val);
    }
#endif

  MapRom (PAGE_8000, (val & prgmask) * 32768, SIZE_32K);
  memcpy (VRAM, VROM_BASE + (((val >> 4) & chrmask) * 8192), 8192);
}

/****************************************************************************/

/* Mapper 13 (CPROM) */
static void
init_cprom(void)
{
  MapRom (PAGE_8000, 0, SIZE_32K);
  memcpy (VRAM, VROM_BASE, 4096);
}

void
cprom(int addr, int val)
{
  if (addr & 0x8000)
    {
      MapRom(PAGE_8000, (val >> 4) & 0x03, SIZE_32K);
      memcpy (VRAM, VROM_BASE + (4096 * (val & 0x03)), 4096);
    }
}

/****************************************************************************/

/* 100-in-1 mapper */
static void
init_100in1(void)
{
  mapmirror = 0;
  MapRom (PAGE_8000, 0, SIZE_32K);
}

static int loc8000;
static int locA000;
static int locC000;
static int locE000;

void
m100in1(int addr, int val)
{
/*  static int locswap; */

  val &= 0xff;
#if DEBUG_MAPPER
  if (verbose)
    {
      printf("addr = %04X, val = %02X\n", addr, val);
    }
#endif
  if (addr == 0x8000)
    {
      hvmirror = (~val & 0x40) >> 6;
      MapRom (PAGE_8000, 16384 * (val & 0x1F), SIZE_32K);

      loc8000 = 16384 * (val & 0x1F);
      locA000 = loc8000 + 0x2000;
      locC000 = locA000 + 0x2000;
      locE000 = locC000 + 0x2000;

/*
   if (val & 0x80)
   {
   MapRom(PAGE_C000, locE000, SIZE_8K);
   MapRom(PAGE_E000, locC000, SIZE_8K);
   locswap = locC000;
   locC000 = locE000;
   locE000 = locswap;
   }
   else
   {
   MapRom(PAGE_8000, locA000, SIZE_8K);
   MapRom(PAGE_A000, loc8000, SIZE_8K);
   locswap = loc8000;
   loc8000 = locA000;
   locA000 = locswap;
   }
*/
    }
  else if (addr == 0x8001)
    {
      MapRom (PAGE_C000, val & 0x1F, SIZE_16K);
      locC000 = 16384 * (val & 0x1F);
      locE000 = locE000 + 0x2000;
/*
   if (val & 0x80)
   {
   MapRom(PAGE_C000, locE000, SIZE_8K);
   MapRom(PAGE_E000, locC000, SIZE_8K);
   locswap = locC000;
   locC000 = locE000;
   locE000 = locswap;
   }
*/
    }
  else if (addr == 0x8002)
    {
      MapRom (PAGE_8000, (val & 0x3F) * 16384 + (val >> 7) * 8192, SIZE_8K);
      MapRom (PAGE_A000, (val & 0x3F) * 16384 + (val >> 7) * 8192, SIZE_8K);
      MapRom (PAGE_C000, (val & 0x3F) * 16384 + (val >> 7) * 8192, SIZE_8K);
      MapRom (PAGE_E000, (val & 0x3F) * 16384 + (val >> 7) * 8192, SIZE_8K);
    }
  else if (addr == 0x8003)
    {
      hvmirror = (~val & 0x40) >> 6;
      MapRom (PAGE_C000, val & 0x1F, SIZE_16K);
      locC000 = 16384 * (val & 0x1F);
      locE000 = locE000 + 0x2000;
/*
   if (val & 0x80)
   {
   MapRom(PAGE_C000, locE000, SIZE_8K);
   MapRom(PAGE_E000, locC000, SIZE_8K);
   locswap = locC000;
   locC000 = locE000;
   locE000 = locswap;
   }
*/
    }
}

/****************************************************************************/

/* Mapper 19, Namcot 106 */
static void
init_namcot106(void)
{
  /* figure out if the PRG bank should be masked */
  prgmask = 0;
  while (ROM_PAGES - 1 > prgmask)
    {
      prgmask = (prgmask << 1) | 1;
    }
  /* this mapper uses 8K pages, so shift once more */
  prgmask = (prgmask << 1) | 1;

  MapRom (PAGE_8000, 0, SIZE_16K);
  MapRom (PAGE_C000, LAST_PAGE * 16384, SIZE_16K);
  memcpy (VRAM, VROM_BASE + (VROM_PAGES - 1) * 8192, 8192);
}

void
namcot106(int addr, int val)
{
  val &= 0xFF;

#if DEBUG_MAPPER
  if (verbose)
    {
      printf("addr = %04X, val = %02X\n", addr, val);
    }
#endif
  switch (addr & 0xF800)
    {
      case 0x5000:
      printf("addr = %04X, val = %02X\n", addr, val);
        break;
      case 0x5800:
      printf("addr = %04X, val = %02X\n", addr, val);
        break;
      case 0x8000:
        memcpy (VRAM + 0x0000, VROM_BASE + (val * 1024), 1024);
        break;
      case 0x8800:
        memcpy (VRAM + 0x0400, VROM_BASE + (val * 1024), 1024);
        break;
      case 0x9000:
        memcpy (VRAM + 0x0800, VROM_BASE + (val * 1024), 1024);
        break;
      case 0x9800:
        memcpy (VRAM + 0x0C00, VROM_BASE + (val * 1024), 1024);
        break;
      case 0xA000:
        memcpy (VRAM + 0x1000, VROM_BASE + (val * 1024), 1024);
        break;
      case 0xA800:
        memcpy (VRAM + 0x1400, VROM_BASE + (val * 1024), 1024);
        break;
      case 0xB000:
        memcpy (VRAM + 0x1800, VROM_BASE + (val * 1024), 1024);
        break;
      case 0xB800:
        memcpy (VRAM + 0x1C00, VROM_BASE + (val * 1024), 1024);
        break;
      case 0xC000:
        if (val < 0xE0)
          memcpy (VRAM + 0x2000, VROM_BASE + (val * 1024), 1024);
        break;
      case 0xC800:
        if (val < 0xE0)
          memcpy (VRAM + 0x2400, VROM_BASE + (val * 1024), 1024);
        break;
      case 0xD000:
        if (val < 0xE0)
          memcpy (VRAM + 0x2800, VROM_BASE + (val * 1024), 1024);
        break;
      case 0xD800:
        if (val < 0xE0)
          memcpy (VRAM + 0x2C00, VROM_BASE + (val * 1024), 1024);
        break;
      case 0xE000:
        MapRom (PAGE_8000, (val & prgmask) * 8192, SIZE_8K);
        break;
      case 0xE800:
        MapRom (PAGE_A000, (val & prgmask) * 8192, SIZE_8K);
        break;
      case 0xF000:
        MapRom (PAGE_C000, (val & prgmask) * 8192, SIZE_8K);
        break;
    }
}

/****************************************************************************/

/* Konami VRC2 type A */
static void
init_vrc2_a(void)
{
  MapRom (PAGE_8000, 0, SIZE_16K);
  MapRom (PAGE_C000, LAST_PAGE * 16384, SIZE_16K);
/*  memcpy (VRAM, VROM_BASE, 8192); */
  mapmirror = 0;
}

void
vrc2_a(int addr, int val)
{
  static int reg0000;
  static int reg0400;
  static int reg0800;
  static int reg0C00;
  static int reg1000;
  static int reg1400;
  static int reg1800;
  static int reg1C00;

  val &= 0xFF;

#if DEBUG_MAPPER
  if (verbose)
    {
      printf("addr = %04X, val = %02X\n", addr, val);
    }
#endif
  switch (addr)
    {
    case 0x8000:
      MapRom (PAGE_8000, (val & 0x0F) * 8192, SIZE_8K);
      break;
    case 0x9000:
      switch (val & 0x03)
        {
        case 0:
          hvmirror = 1;
          break;
        case 1:
          hvmirror = 0;
          break;
        case 2:
          mapmirror = 0;
          break;
        case 3:
          mapmirror = 1;
          break;
        }
      break;
    case 0xA000:
      MapRom (PAGE_A000, (val & 0x0F) * 8192, SIZE_8K);
      break;

    case 0xB000:
      reg0000 = val & 0x0F;
      break;
    case 0xB002:
      reg0000 |= val << 4;
      memcpy (VRAM + 0x0000, VROM_BASE + (reg0000 >> 1) * 1024, 1024);
#if DEBUG_MAPPER
printf("VROM page 0x%02X loaded at 0x0000\n", reg0000 >> 1);
#endif
      break;

    case 0xB001:
      reg0400 = val & 0x0F;
      break;
    case 0xB003:
      reg0400 |= val << 4;
      memcpy (VRAM + 0x0400, VROM_BASE + (reg0400 >> 1) * 1024, 1024);
#if DEBUG_MAPPER
printf("VROM page 0x%02X loaded at 0x0400\n", reg0400 >> 1);
#endif
      break;

    case 0xC000:
      reg0800 = val & 0x0F;
      break;
    case 0xC002:
      reg0800 |= val << 4;
      memcpy (VRAM + 0x0800, VROM_BASE + (reg0800 >> 1) * 1024, 1024);
#if DEBUG_MAPPER
printf("VROM page 0x%02X loaded at 0x0800\n", reg0800 >> 1);
#endif
      break;

    case 0xC001:
      reg0C00 = val & 0x0F;
      break;
    case 0xC003:
      reg0C00 |= val << 4;
      memcpy (VRAM + 0x0C00, VROM_BASE + (reg0C00 >> 1) * 1024, 1024);
#if DEBUG_MAPPER
printf("VROM page 0x%02X loaded at 0x0C00\n", reg0C00 >> 1);
#endif
      break;

    case 0xD000:
      reg1000 = val & 0x0F;
      break;
    case 0xD002:
      reg1000 |= val << 4;
      memcpy (VRAM + 0x1000, VROM_BASE + (reg1000 >> 1) * 1024, 1024);
#if DEBUG_MAPPER
printf("VROM page 0x%02X loaded at 0x1000\n", reg1000 >> 1);
#endif
      break;

    case 0xD001:
      reg1400 = val & 0x0F;
      break;
    case 0xD003:
      reg1400 |= val << 4;
      memcpy (VRAM + 0x1400, VROM_BASE + (reg1400 >> 1) * 1024, 1024);
#if DEBUG_MAPPER
printf("VROM page 0x%02X loaded at 0x1400\n", reg1400 >> 1);
#endif
      break;

    case 0xE000:
      reg1800 = val & 0x0F;
      break;
    case 0xE002:
      reg1800 |= val << 4;
      memcpy (VRAM + 0x1800, VROM_BASE + (reg1800 >> 1) * 1024, 1024);
#if DEBUG_MAPPER
printf("VROM page 0x%02X loaded at 0x1800\n", reg1800 >> 1);
#endif
      break;

    case 0xE001:
      reg1C00 = val & 0x0F;
      break;
    case 0xE003:
      reg1C00 |= val << 4;
      memcpy (VRAM + 0x1C00, VROM_BASE + (reg1C00 >> 1) * 1024, 1024);
#if DEBUG_MAPPER
printf("VROM page 0x%02X loaded at 0x1C00\n", reg1C00 >> 1);
#endif
      break;
    }
}

/****************************************************************************/
/* Konami VRC2 type B */
static void
init_vrc2_b(void)
{
  MapRom (PAGE_8000, 0, SIZE_16K);
  MapRom (PAGE_C000, LAST_PAGE * 16384, SIZE_16K);
  mapmirror = 0;
}

void
vrc2_b(int addr, int val)
{
  static int reg0000;
  static int reg0400;
  static int reg0800;
  static int reg0C00;
  static int reg1000;
  static int reg1400;
  static int reg1800;
  static int reg1C00;

  val &= 0xFF;
  switch (addr)
    {
    case 0x8000:
      MapRom (PAGE_8000, (val & 0x0F) * 8192, SIZE_8K);
      break;
    case 0x9000:
      switch (val & 0x03)
        {
        case 0:
          hvmirror = 1;
          break;
        case 1:
          hvmirror = 0;
          break;
        case 2:
          mapmirror = 0;
          break;
        case 3:
          mapmirror = 1;
          break;
        }
      break;
    case 0xA000:
      MapRom (PAGE_A000, (val & 0x0F) * 8192, SIZE_8K);
      break;

    case 0xB000:
      reg0000 = val & 0x0F;
      break;
    case 0xB001:
      reg0000 = val << 4;
      memcpy (VRAM + 0x0000, VROM_BASE + (reg0000 >> 1) * 1024, 1024);
      break;

    case 0xB002:
      reg0400 = val & 0x0F;
      break;
    case 0xB003:
      reg0400 |= val << 4;
      memcpy (VRAM + 0x0400, VROM_BASE + (reg0400 >> 1) * 1024, 1024);
      break;

    case 0xC000:
      reg0800 = val & 0x0F;
      break;
    case 0xC001:
      reg0800 |= val << 4;
      memcpy (VRAM + 0x0800, VROM_BASE + (reg0800 >> 1) * 1024, 1024);
      break;

    case 0xC002:
      reg0C00 = val & 0x0F;
      break;
    case 0xC003:
      reg0C00 |= val << 4;
      memcpy (VRAM + 0x0C00, VROM_BASE + (reg0C00 >> 1) * 1024, 1024);
      break;

    case 0xD000:
      reg1000 = val & 0x0F;
      break;
    case 0xD001:
      reg1000 |= val << 4;
      memcpy (VRAM + 0x1000, VROM_BASE + (reg1000 >> 1) * 1024, 1024);
      break;

    case 0xD002:
      reg1400 = val & 0x0F;
      break;
    case 0xD003:
      reg1400 |= val << 4;
      memcpy (VRAM + 0x1400, VROM_BASE + (reg1400 >> 1) * 1024, 1024);
      break;

    case 0xE000:
      reg1800 = val & 0x0F;
      break;
    case 0xE001:
      reg1800 |= val << 4;
      memcpy (VRAM + 0x1800, VROM_BASE + (reg1800 >> 1) * 1024, 1024);
      break;

    case 0xE002:
      reg1C00 = val & 0x0F;
      break;
    case 0xE003:
      reg1C00 |= val << 4;
      memcpy (VRAM + 0x1C00, VROM_BASE + (reg1C00 >> 1) * 1024, 1024);
      break;
    }
}

/****************************************************************************/

/* Irem G-101 */
static void
init_g101(void)
{
  MapRom (PAGE_8000, 0, SIZE_16K);
  MapRom (PAGE_C000, LAST_PAGE * 16384, SIZE_16K);
  mapmirror = 0;
}

void
g101(int addr, int val)
{
  static int switchmode = 0;

  switch (addr)
    {
    case 0x8FFF:
      if (switchmode)
        {
          MapRom (PAGE_C000, val * 8192, SIZE_8K);
        }
      else
        {
          MapRom (PAGE_8000, val * 8192, SIZE_8K);
        }
      break;
    case 0x9FFF:
      switchmode = (val & 0x02) >> 1;
      hvmirror = val * 0x01;
      break;
    case 0xAFFF:
      MapRom (PAGE_A000, val * 8192, SIZE_8K);
      break;
    case 0xBFF0:
      memcpy (VRAM + 0x0000, VROM_BASE + val * 1024, 1024);
      break;
    case 0xBFF1:
      memcpy (VRAM + 0x0400, VROM_BASE + val * 1024, 1024);
      break;
    case 0xBFF2:
      memcpy (VRAM + 0x0800, VROM_BASE + val * 1024, 1024);
      break;
    case 0xBFF3:
      memcpy (VRAM + 0x0C00, VROM_BASE + val * 1024, 1024);
      break;
    case 0xBFF4:
      memcpy (VRAM + 0x1000, VROM_BASE + val * 1024, 1024);
      break;
    case 0xBFF5:
      memcpy (VRAM + 0x1400, VROM_BASE + val * 1024, 1024);
      break;
    case 0xBFF6:
      memcpy (VRAM + 0x1800, VROM_BASE + val * 1024, 1024);
      break;
    case 0xBFF7:
      memcpy (VRAM + 0x1C00, VROM_BASE + val * 1024, 1024);
      break;
    }
}

/****************************************************************************/

/* Taito TC0190 */
static void
init_taito_tc0190(void)
{
  /* figure out if the PRG bank should be masked */
  prgmask = 0;
  while (ROM_PAGES - 1 > prgmask)
    {
      prgmask = (prgmask << 1) | 1;
    }
  /* this mapper uses 8K pages, so shift once more */
  prgmask = (prgmask << 1) | 1;

  MapRom (PAGE_8000, 0, SIZE_16K);
  MapRom (PAGE_C000, LAST_PAGE * 16384, SIZE_16K);
  mapmirror = 0;
}

void
taito_tc0190(int addr, int val)
{
  val &= 0xFF;

#if DEBUG_MAPPER
  if (verbose)
    {
      printf("addr = %04X, val = %02X\n", addr, val);
    }
#endif
  switch (addr)
    {
    case 0x8000:
      MapRom (PAGE_8000, (val & prgmask) * 8192, SIZE_8K);
      break;
    case 0x8001:
      MapRom (PAGE_A000, (val & prgmask) * 8192, SIZE_8K);
      break;
    case 0x8002:
      memcpy (VRAM + 0x0000, VROM_BASE + val * 2048, 2048);
      break;
    case 0x8003:
      memcpy (VRAM + 0x0800, VROM_BASE + val * 2048, 2048);
      break;
    case 0xA000:
      memcpy (VRAM + 0x1000, VROM_BASE + val * 1024, 1024);
      break;
    case 0xA001:
      memcpy (VRAM + 0x1400, VROM_BASE + val * 1024, 1024);
      break;
    case 0xA002:
      memcpy (VRAM + 0x1800, VROM_BASE + val * 1024, 1024);
      break;
    case 0xA003:
      memcpy (VRAM + 0x1C00, VROM_BASE + val * 1024, 1024);
      break;
    case 0xC000:
      /* unknown */
      break;
    case 0xC001:
      /* unknown */
      break;
    case 0xE000:
      /* unknown */
      break;
    }
}

/****************************************************************************/

/* Tengen RAMBO-1 */
static void
init_tengen_rambo1(void)
{
  MapRom (PAGE_8000, (ROM_PAGES - 1) * 16384 + 8192, SIZE_8K);
  MapRom (PAGE_A000, (ROM_PAGES - 1) * 16384 + 8192, SIZE_8K);
  MapRom (PAGE_C000, (ROM_PAGES - 1) * 16384 + 8192, SIZE_8K);
  MapRom (PAGE_E000, (ROM_PAGES - 1) * 16384 + 8192, SIZE_8K);

  if (VROM_PAGES)
    {
      memcpy (VRAM, VROM_BASE, 8192);
    }

  mapmirror = 0;
}

void tengen_rambo1(int addr, int val)
{
  val &= 0xFF;

#if DEBUG_MAPPER
  if (verbose)
    {
      printf("addr = %04X, val = %02X\n", addr, val);
    }
#endif
  if (addr == 0x8000)
    {
      commandregister = val;
    }
  else if (addr == 0x8001)
    {
      switch (commandregister & 0x0F)
        {
        case 0:
          memcpy (VRAM + (0x0000 ^ (val & 0x80 << 5)),
            VROM_BASE + val * 1024, 2048);
          break;
        case 1:
          memcpy (VRAM + (0x0800 ^ (val & 0x80 << 5)),
            VROM_BASE + val * 1024, 2048);
          break;
        case 2:
          memcpy (VRAM + (0x1000 ^ (val & 0x80 << 5)),
            VROM_BASE + val * 1024, 1024);
          break;
        case 3:
          memcpy (VRAM + (0x1400 ^ (val & 0x80 << 5)),
            VROM_BASE + val * 1024, 1024);
          break;
        case 4:
          memcpy (VRAM + (0x1800 ^ (val & 0x80 << 5)),
            VROM_BASE + val * 1024, 1024);
          break;
        case 5:
          memcpy (VRAM + (0x1C00 ^ (val & 0x80 << 5)),
            VROM_BASE + val * 1024, 1024);
          break;
        case 6:
          if (commandregister & 0x40)
            {
              MapRom (PAGE_A000, val * 8192, SIZE_8K);
            }
          else
            {
              MapRom (PAGE_8000, val * 8192, SIZE_8K);
            }
          break;
        case 7:
          if (commandregister & 0x40)
            {
              MapRom (PAGE_C000, val * 8192, SIZE_8K);
            }
          else
            {
              MapRom (PAGE_A000, val * 8192, SIZE_8K);
            }
          break;
        case 8:
          memcpy (VRAM + 0x0400,
            VROM_BASE + val * 1024, 1024);
          break;
        case 9:
          memcpy (VRAM + 0x0C00,
            VROM_BASE + val * 1024, 1024);
          break;
        case 15:
          if (commandregister & 0x40)
            {
              MapRom (PAGE_8000, val * 8192, SIZE_8K);
            }
          else
            {
              MapRom (PAGE_C000, val * 8192, SIZE_8K);
            }
          break;
        default:
          break;
        }
    }
  else if (addr == 0xA000)
    {
      hvmirror = val & 0x01;
    }
}

/****************************************************************************/

/* GNROM */
static void
init_gnrom(void)
{
  MapRom (PAGE_8000, 0, SIZE_32K);
  memcpy (VRAM, VROM_BASE, 8192);
  mapmirror = 0;
}

void
gnrom(int addr, int val)
{
  if (addr > 0x8000)
    {
      MapRom (PAGE_8000, (val >> 4 & 0x03) * 32768, SIZE_32K);
      memcpy (VRAM, VROM_BASE + (val & 0x03) * 8192, 8192);
    }
}

/****************************************************************************/

/* Sunsoft Mapper 4 */
static void
init_sunsoft4(void)
{
  MapRom (PAGE_8000, 0, SIZE_16K);
  MapRom (PAGE_C000, LAST_PAGE * 16384, SIZE_16K);
  mapmirror = 0;
}

void
sunsoft4(int addr, int val)
{
  val &= 0xFF;

#if DEBUG_MAPPER
  if (verbose)
    {
      printf("addr = %04X, val = %02X\n", addr, val);
    }
#endif

  switch (addr)
    {
    case 0x8000:
      memcpy (VRAM + 0x0000, VROM_BASE + (val * 2048), 8192);
      break;
    case 0x9000:
      memcpy (VRAM + 0x0800, VROM_BASE + (val * 2048), 8192);
      break;
    case 0xA000:
      memcpy (VRAM + 0x1000, VROM_BASE + (val * 2048), 8192);
      break;
    case 0xB000:
      memcpy (VRAM + 0x1800, VROM_BASE + (val * 2048), 8192);
      break;
    case 0xE000:
      switch (val & 0x03)
        {
        case 0:
          osmirror = 0;
          hvmirror = 0;
          break;
        case 1:
          osmirror = 0;
          hvmirror = 1;
          break;
        case 2:
          osmirror = 1;
	  /* osmirrorpage = 0x2000; */
          break;
        case 3:
          osmirror = 1;
          /* osmirrorpage = 0x2400; */
          break;
        }
      break;
    case 0xF000:
#if DEBUG_MAPPER
  if (verbose)
    {
      printf("addr = %04X, val = %02X\n", addr, val);
    }
#endif
      MapRom (PAGE_8000, val * 16384, SIZE_16K);
      break;
    }
}

/****************************************************************************/

/* FME-7 */
static void
init_fme7(void)
{
  MapRom (PAGE_8000, 0, SIZE_16K);
  MapRom (PAGE_C000, 16384, SIZE_8K);
  MapRom (PAGE_E000, LAST_PAGE * 16384 + 8192, SIZE_8K);
}

void
fme7(int addr, int val)
{
  val &= 0xFF;
  if (addr == 0x8000)
    {
      commandregister = val;
    }
  else if (addr == 0xA000)
    {
      switch (commandregister)
        {
        case 0:
          memcpy (VRAM + 0x0000, VROM_BASE + val * 1024, 1024);
          break;
        case 1:
          memcpy (VRAM + 0x0400, VROM_BASE + val * 1024, 1024);
          break;
        case 2:
          memcpy (VRAM + 0x0800, VROM_BASE + val * 1024, 1024);
          break;
        case 3:
          memcpy (VRAM + 0x0C00, VROM_BASE + val * 1024, 1024);
          break;
        case 4:
          memcpy (VRAM + 0x1000, VROM_BASE + val * 1024, 1024);
          break;
        case 5:
          memcpy (VRAM + 0x1400, VROM_BASE + val * 1024, 1024);
          break;
        case 6:
          memcpy (VRAM + 0x1800, VROM_BASE + val * 1024, 1024);
          break;
        case 7:
          memcpy (VRAM + 0x1C00, VROM_BASE + val * 1024, 1024);
          break;
        case 8:
          break;
        case 9:
          MapRom (PAGE_8000, val * 8192, SIZE_8K);
          break;
        case 10:
          MapRom (PAGE_A000, val * 8192, SIZE_8K);
          break;
        case 11:
          MapRom (PAGE_C000, val * 8192, SIZE_8K);
          break;
        case 12:
          break;
        case 13:
          break;
        case 14:
          break;
        case 15:
          break;
        }
    }
}

/****************************************************************************/

/* Camerica */
static void
init_camerica(void)
{
  MapRom (PAGE_8000, 0, SIZE_16K);
  MapRom (PAGE_C000, LAST_PAGE * 16384, SIZE_16K);
  mapmirror = 0;
}

void
camerica(int addr, int val)
{
  val &= 0xFF;

#if DEBUG_MAPPER
  if (verbose)
    {
      printf("addr = %04X, val = %02X\n", addr, val);
    }
#endif

  if (addr > 0xC000)
    {
      MapRom (PAGE_8000, val * 16384, SIZE_16K);
    }
}

/****************************************************************************/

/* Irem 74HC161_32 */
static void
init_irem_74hc161_32(void)
{
  /* figure out if the PRG bank should be masked */
  prgmask = 0;
  while (ROM_PAGES - 1 > prgmask)
    {
      prgmask = (prgmask << 1) | 1;
    }

  MapRom (PAGE_8000, 0, SIZE_16K);
  MapRom (PAGE_C000, LAST_PAGE * 16384, SIZE_16K);
  memcpy (VRAM, VROM_BASE, 8192);
  mapmirror = 0;
}

void irem_74hc161_32(int addr, int val)
{
  val &= 0xFF;

#if DEBUG_MAPPER
  if (verbose)
    {
      printf("addr = %04X, val = %02X\n", addr, val);
    }
#endif

  if (addr & 0x8000)
    {
      MapRom (PAGE_8000, (val & prgmask) * 16384, SIZE_16K);
      memcpy (VRAM, VROM_BASE + (val >> 4) * 8192, 8192);
    }
}

/****************************************************************************/

/* VS UniSystem */

static int vsreg = 0;

static void
init_vs(void)
{
  MapRom (PAGE_8000, 0, SIZE_32K);
  memcpy (VRAM, VROM_BASE, 8192);
  nomirror = 1;
  mapmirror = 1;
}

void
vs(int addr, int val)
{
  if (addr == 0x4016)
    {
      if (vsreg != (val & 0x04))
        {
          if (vsreg)
            memcpy (VRAM, VROM_BASE, 8192);
          else
            memcpy (VRAM, VROM_BASE + 8192, 8192);
          vsreg = val & 0x04;
        }
    }
}

/****************************************************************************/

/* SuperVision */
static void
init_supervision(void)
{
  /* on power-up, mapper acts as if $8000 has been written */
  MapRom (PAGE_8000, 0, SIZE_32K);
  memcpy (VRAM, VROM_BASE, 8192);
  mapmirror = 0;
}

void supervision(int addr, int val)
{
#if DEBUG_MAPPER
/*   if (verbose)
    {
      printf("addr = %04X, val = %02X\n", addr, val);
    } */
#endif

  if (addr & 0x8000)
    {
      /* take care of the mirroring */
      hvmirror = (addr & 0x2000) > 13;

      /* take care of the PRG switching */
      if (addr & 0x1000)
        {
/* printf("(a) 32kB page = %02X\n", (addr >> 7) & 0x001F); */
          MapRom (PAGE_8000,
            ((addr >> 7) & 0x001F) * 32768 + ((addr >> 6) & 0x0001 * 16384),
            SIZE_16K);
          MapRom (PAGE_C000,
            ((addr >> 7) & 0x001F) * 32768 + ((addr >> 6) & 0x0001 * 16384),
            SIZE_16K);
        }
      else
        {
/* printf("(b) 32kB page = %02X\n", (addr >> 7) & 0x001F); */
          MapRom (PAGE_8000, ((addr >> 7) & 0x001F) * 32768, SIZE_32K);
        }

      /* take care of the CHR switching */
      memcpy (VRAM, VROM_BASE + (addr & 0x003F) * 8192, 8192);
    }
}

/****************************************************************************/

/* NINA-07 */
static void
init_nina7(void)
{
  MapRom (PAGE_8000, (ROM_PAGES / 2 - 1) * 32768, SIZE_32K);
  mapmirror = 0;
}

void nina7(int addr, int val)
{
  val &= 0xFF;

#if DEBUG_MAPPER
  if (verbose)
    {
      printf("addr = %04X, val = %02X\n", addr, val);
    }
#endif

  if (addr & 0x8000)
    {
      MapRom (PAGE_8000, ((val & 0x03) | ((val & 0x80) >> 5)) * 32768, SIZE_32K);
      if (VROM_PAGES)
        {
          memcpy (VRAM, VROM_BASE + ((val & 0x70) >> 4) * 8192, 8192);
        }
    }
}

/****************************************************************************/
/*
  if (verbose)
    {
      printf("addr = %04X, val = %02X\n", addr, val);
    }
*/

/*
   This procedure initializes the MapperInit[], Mapper[], and
   MapperName[] arrays.
 */
void
InitMapperSubsystem(void)
{
  int x;

  /* clear them all to zero */
  for (x = 0; x < MAXMAPPER; x++)
    {
      MapperInit[x] = 0;
      Mapper[x] = 0;
      MapperName[x][0] = '\0';
    }

  MapperInit[0] = &init_none;
  Mapper[0] = &MAPPER_NONE;
  strcpy (MapperName[0], "No mapper");

  MapperInit[1] = &init_mmc1;
  Mapper[1] = &MAPPER_MMC1;
  strcpy (MapperName[1], "MMC1");

  MapperInit[2] = &init_unrom;
  Mapper[2] = &MAPPER_UNROM;
  strcpy (MapperName[2], "UNROM");

  MapperInit[3] = &init_cnrom;
  Mapper[3] = &MAPPER_CNROM;
  strcpy (MapperName[3], "CNROM");

  MapperInit[4] = &init_mmc3;
  Mapper[4] = &MAPPER_MMC3;
  strcpy (MapperName[4], "MMC3");

  MapperInit[5] = &init_mmc5;
  Mapper[5] = &MAPPER_MMC5;
  strcpy (MapperName[5], "MMC5");

  MapperInit[7] = &init_aorom;
  Mapper[7] = &MAPPER_AOROM;
  strcpy (MapperName[7], "AOROM");

  MapperInit[9] = &init_mmc2;
  Mapper[9] = &MAPPER_MMC2;
  strcpy (MapperName[9], "MMC2");

  MapperInit[10] = &init_mmc4;
  Mapper[10] = &MAPPER_MMC4;
  strcpy (MapperName[10], "MMC4");

  MapperInit[11] = &init_clrdrms;
  Mapper[11] = &MAPPER_CLRDRMS;
  strcpy (MapperName[11], "Color Dreams");

  MapperInit[13] = &init_cprom;
  Mapper[13] = &MAPPER_CPROM;
  strcpy (MapperName[13], "CPROM");

  MapperInit[15] = &init_100in1;
  Mapper[15] = &MAPPER_100IN1;
  strcpy (MapperName[15], "100-in-1");

  MapperInit[19] = &init_namcot106;
  Mapper[19] = &MAPPER_NAMCOT106;
  strcpy (MapperName[19], "Namcot 106");

  MapperInit[22] = &init_vrc2_a;
  Mapper[22] = &MAPPER_VRC2_A;
  strcpy (MapperName[22], "Konami VRC2 type A");

  MapperInit[23] = &init_vrc2_b;
  Mapper[23] = &MAPPER_VRC2_B;
  strcpy (MapperName[23], "Konami VRC2 type B");

  MapperInit[32] = &init_g101;
  Mapper[32] = &MAPPER_G101;
  strcpy (MapperName[32], "Irem G-101");

  MapperInit[33] = &init_taito_tc0190;
  Mapper[33] = &MAPPER_TAITO_TC0190;
  strcpy (MapperName[33], "Taito TC0190");

  MapperInit[64] = &init_tengen_rambo1;
  Mapper[64] = &MAPPER_TENGEN_RAMBO1;
  strcpy (MapperName[64], "Tengen RAMBO-1");

  MapperInit[66] = &init_gnrom;
  Mapper[66] = &MAPPER_GNROM;
  strcpy (MapperName[66], "GNROM");

  MapperInit[68] = &init_sunsoft4;
  Mapper[68] = &MAPPER_SUNSOFT4;
  strcpy (MapperName[68], "Sunsoft Mapper #4");

  MapperInit[69] = &init_fme7;
  Mapper[69] = &MAPPER_FME7;
  strcpy (MapperName[69], "Sunsoft FME-7");

  MapperInit[71] = &init_camerica;
  Mapper[71] = &MAPPER_CAMERICA;
  strcpy (MapperName[71], "Camerica");

  MapperInit[78] = &init_irem_74hc161_32;
  Mapper[78] = &MAPPER_IREM_74HC161_32;
  strcpy (MapperName[78], "Irem 74HC161/32");

  MapperInit[99] = &init_vs;
  Mapper[99] = &MAPPER_VS;
  strcpy (MapperName[99], "VS UniSystem");

  MapperInit[225] = &init_supervision;
  Mapper[225] = &MAPPER_SUPERVISION;
  strcpy (MapperName[225], "SuperVision");

  MapperInit[231] = &init_nina7;
  Mapper[231] = &MAPPER_NINA7;
  strcpy (MapperName[231], "NINA-07");
}
