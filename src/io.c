/*
 * This file is part of the TuxNES project codebase.
 *
 * Please see the README and COPYING files for more information regarding
 * this project.
 *
 * Description: This file consists of functions which are called by the
 * dynamically recompiled code to do I/O operations. Whenever the 6502 code
 * reads from or writes to a memory location known to be a NES register,
 * a function in this file is called.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#ifdef HAVE_FEATURES_H
#include <features.h>
#endif /* HAVE_FEATURES_H */

#include <stdio.h>
#include <string.h>

#include "consts.h"
#include "globals.h"
#include "mapper.h"
#include "renderer.h"
#include "sound.h"

int	vbl = 0;
int	hvscroll = 0;
int     vramlatch = 0;
int	hscrollval, vscrollval;
int	sprite0hit;
int     swap_inputs = 0;

/* forward and external declarations */
void	vs(int, int);

/* Called by x86.S */
int	donmi(void);
void	input(int);
void	output(int, int);
void	trace(int);

/* Declaration of global variables */
unsigned char	vram[16384];
/* primary controller status vectors */
unsigned int	controller[2] = {0, 0};
/* secondary (diagonal key and joystick "hat") controller status vectprs */
unsigned int	controllerd[2] = {0, 0};
unsigned int	coinslot = 0, dipswitches = 0;
unsigned char	spriteram[256];
unsigned int	hvmirror = 0;
unsigned int	nomirror = 0;
unsigned int	osmirror = 0;
unsigned short int	hscroll[240];
unsigned short int	vscroll[240];
unsigned char	linereg[240];
unsigned int	CLOCK;             /* Current scanline position */
unsigned char	hscrollreg, vscrollreg;

static int last_clock; /* For vblank bit */

/* This is called whenever the game reads from 2xxx or 4xxx */
void
input(int addr)
{
  int x;
  static signed char vram_read;
  unsigned char *ptr;
  INRET = 0;

  /* Read PPU status register */
  if (addr == 0x2002)
    {
      /* +40 is to account for the latency of the PPU; from the time it reads
         the background tile to the time the flag is set is about 40 cycles,
         give or take a few. */
      sprite0hit = spriteram[0] * HCYCLES + HCYCLES + 85 + spriteram[3] + 40;
      if (RAM[0x2000] & 0x20)
        ptr = VRAM + ((spriteram[1] & 0xFE) << 4) + ((spriteram[1] & 1) << 12);         /* 8x16 sprites */
      else
        ptr = VRAM + (spriteram[1] << 4) + ((RAM[0x2000] & 0x08) << 9);         /* 8x8 sprites */
      if ((RAM[0x2000] & 0x20) && (((long long *) ptr)[0] | ((long long *)
                                                             ptr)[8]) == 0)
        {
          sprite0hit += 8 * HCYCLES;
          ptr += 16;
        }
      while ((ptr[0] | ptr[8]) == 0 && sprite0hit < spriteram[0] * HCYCLES + 5797)
        {
          sprite0hit += HCYCLES;
          ptr++;
        }
      /* I'm sure this isn't really accurate.  The vblank flag is set
         before the NMI is triggered, but the timing here is just a
         guess; also a read should always clear the flag. */
      /*        if ((vbl && CLOCK >= VBL && CLOCK < 29695) | */
      /*  	  (CLOCK < VBL && CLOCK > 27393)) */
      /*          { */
      /*            INRET |= (signed char) 0x80; */
      /*            vbl--; */
      /*          } */

      if ((CLOCK >= 27393) &&
	  (last_clock >= CLOCK || last_clock <= 27393))
	{
	  vbl=1;
	}
      last_clock=CLOCK;
      if (vbl &&
	  (CLOCK > 27393))
	{
	  INRET |= (signed char) 0x80;
	  vbl--;
	}

      /*       if ((CLOCK * 3 >= sprite0hit) && (CLOCK < 29695)) */
      /*         INRET |= (signed char) 0x40; */

      /* When does sprite0hit go off? */
      /*if((CLOCK*3>=sprite0hit)&&(CLOCK<VBL)) INRET|=(signed char)0x40; */
      
      /* Pick a number, any number... hmm... this one seems to work
         okay, I think I'll use it. */
      if ((CLOCK * 3 >= sprite0hit) && (CLOCK < 27742))
	INRET|=(signed char)0x40;
      if (CLOCK > VBL && !(RAM[0x2000] & 0x80))
	{
	  /* This is totally wierd, but SMB and Zelda depend on it. */
	  RAM[0x2000] &= 0xFE;
	  for (x = 0; x < 240; x++)
	    hscroll[x] = hscrollval & 255;
	  for (x = 0; x < 240; x++)
	    linereg[x] = RAM[0x2000];
	  /*printf("Read: %4x=%2x (scan %d)\n",addr,INRET&0xff,CLOCK);*/
	}
      hvscroll = 0;
      /*printf("Read: %4x=%2x (scan %d)\n",addr,INRET&0xff,CLOCK);*/
    }

  /* Read from VRAM */
  if (addr == 0x2007)
    {
      INRET = vram_read;
      VRAMPTR &= 0x3fff;
      if (osmirror && VRAMPTR >= 0x2000 && VRAMPTR <= 0x2FFF)
        vram_read = VRAM[VRAMPTR & 0x23FF];     /* one screen */
      else if (VRAMPTR > 0x3f00)
        vram_read = VRAM[VRAMPTR & 0x3f1f];
      else if (!nomirror && hvmirror && VRAMPTR >= 0x2400 && VRAMPTR < 0x2c00)
        vram_read = VRAM[VRAMPTR - 0x400];
      else
        vram_read = VRAM[VRAMPTR];
      VRAMPTR += 1 << (((*((unsigned char *) REG1) & 4) >> 2) * 5);     /* bit 2 of $2000 controls increment */
      if (VRAMPTR > 0x3FFF)
        VRAMPTR &= 0x3fff;
      /*printf("VRAM Read: %4x=%2x (scan %d)\n",VRAMPTR,INRET,CLOCK); */

      /*
       * This is the so-called mid-hblank update.  If an address is written
       * into 2006 followed by reads of 2007 during refresh, then the
       * address is loaded into the PPU and used as the start address of the
       * next scanline.  This is used to scroll the background vertically
       * on a portion of the screen.  We need to convert the scanline
       * address into horizontal/vertical offsets.
       */
      if (CLOCK < VBL && (RAM[0x2001] & 8) != 0)
        {
          for (x = (CLOCK * 3 / HCYCLES) + 1; x < 240; x++)
            {
              hscroll[x] = (hscroll[x] & 0xff) | ((VRAMPTR & 0x400) >> 2);
              vscroll[x] = (480 + 480 + (((VRAMPTR & 0x800) >> 11) * 240) +
                            ((VRAMPTR
                              &
                              0x3e0)
                             >>
                             2) - ((CLOCK * 3 / HCYCLES) + 1)) % 480;
            }

          drawimage(CLOCK * 3);
          vline = (VRAMPTR & 0x3e0) >> 5;
          vscan = 0;

          /*printf("Read: %4x (scan %d, sprite0 %d) 2000=%2x 2001=%2x vbl=%d\n",addr,CLOCK,sprite0hit*HCYCLES,RAM[0x2000],RAM[0x2001],vbl); */
          /* For debugging */
          /*printf("vram read during refresh! (%4x)\n",VRAMPTR); */
          /*printf("v=%d oldv=%d\n",vscroll[CLOCK/HCYCLES/8],vscroll[CLOCK/HCYCLES/8-1]); */
        }
    }

  /* Read from pAPU registers */
    if ( addr == 0x4015 ) {
	INRET = SoundGetLengthReg();
    }
#if 0
    if ((addr >= 0x4000) && (addr <= 0x4015))
      {
        INRET = RAM[addr];
      /*       fprintf (stderr, "Read from pAPU register 0x%2.2x: 0x%4.4x\n", */
      /*                addr - 0x4000, INRET); */
      }
#endif

  /* Read joypad controller, dipswitches and coinslot */
  if (addr == 0x4016)
    {
      INRET = (RAM[0x4016] & 1) | ((coinslot & 3) << 5) | ((dipswitches & 3) << 3) |
	(coinslot & 4);
      RAM[0x4016] >>= 1;
    }
  if (addr == 0x4017)
    {
      INRET = (RAM[0x4017] & 1) | (dipswitches & ~3);
      RAM[0x4017] >>= 1;
    }

  /* debug stuff: */
/*
   printf("Read: %4x=%2x (scan %d)\n",addr,INRET&0xff,CLOCK);
   printf("\nstack: %x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x\n",
   (&addr)[1],(&addr)[2],(&addr)[3],(&addr)[4],(&addr)[5],(&addr)[6],
   (&addr)[7],(&addr)[8],(&addr)[9],(&addr)[10],(&addr)[11],(&addr)[12] );
 */
}

/* This is called whenever the game writes to 2xxx or 4xxx */
void
output(int addr, int val)
{
  int x;
  int scanline;
  static int spriteaddr;

  val &= 0xFF;
  if ((scanline = (CLOCK * 3 / HCYCLES) + 1) >= 240)
    scanline = 0;

  /* Select pattern table */
  if (addr == 0x2000)
    {
      drawimage (CLOCK * 3);

      /* Ugly kludge - bit 1 of 2000 does not take effect until the
         next frame so this supresses it using the vertical wraparound
         bit.  This could be done a better way. */
      if (hvmirror || nomirror)
	vwrap ^= ((val ^ RAM[0x2000]) >> 1) & 1;
      RAM[0x2000] = val;
      hscrollval = ((RAM[0x2000] & 1) << 8) | (hscrollval & 255);
      vscrollval = (RAM[0x2000] & 2) * 120 + (vscrollval % 240);
      /*printf("vrom base:0x%4x\n",0x2000+((RAM[0x2000]&3)<<10)); */
      for (x = scanline; x < 240; x++)
        hscroll[x] = hscrollval;
      for (x = scanline; x < 240; x++)
        linereg[x] = RAM[0x2000];
      /*printf("Write: %4x,%2x (scan %d)\n",addr,val,CLOCK); */
    }

  if (addr == 0x2001)
    {
      drawimage (CLOCK * 3);
      RAM[0x2001] = val;
    }

  /* Set horizontal/vertical scroll */
  if (addr == 0x2005)
    {
      if (hvscroll ^= 1)
        {
          drawimage (CLOCK * 3);
          hscrollreg = val;
          hscrollval = ((RAM[0x2000] & 1) << 8) + val;
          for (x = scanline; x < 240; x++)
            hscroll[x] = hscrollval;
          /*printf("hscroll: %d\n",hscrollval); */
        }
      else
        {
          vscrollreg = val;
          /* A little kludge here... It appears that the PPU will actually
             accept bogus vscroll values above 240, wrapping around to 0
             when it reaches 256, without the usual page flip at line 240.
             The val+224 hack below is to compensate for this, so that the
             rest of the lines end up where they're supposed to be. */
          vscrollval = ((RAM[0x2000] & 2) * 120 + ((val < 240) ? val : val +
                                                   224)) % 480;
          if (scanline < 1)
            for (x = scanline; x < 240; x++)
              vscroll[x] = vscrollval;
          /* Note: Unlike the h-scroll, the v-scroll register only gets read
             on the first scanline.  To create split-screen vertical scrolling,
             2006/2007 must be used to directly update the PPU registers. */

	  /* More weirdness */
	  vramlatch = (vramlatch & 0x3c00) | (val << 2);

          /*printf("vscroll: %d\n",vscrollval); */
        }
    }

  /* Load VRAM target address */
  if (addr == 0x2006)
    {
      drawimage (CLOCK * 3);
      /* VRAMPTR = ((VRAMPTR & 0x3f) << 8) | (val & 0xff); */

      /* It appears that h/v scroll and the VRAM address registers share
	 a common toggle-bit which deterines which byte is written to. */
      if (hvscroll ^= 1)
	{
	  vramlatch = (vramlatch & 0xFF) | ((val & 0xff) << 8);
	}
      else
	{
	  vramlatch = (vramlatch & 0xFF00) | (val & 0xff);
	}

      /* For mid-hblank updates: */
      /* Note: When the VRAM address register is loaded, the scanline number
         is derived from the top bits of the address.  When the address
         register load is followed by a read of 2005, then the scanline
         number is reset to zero and the scan starts with the top of the
         current char/tile line. (see above) */
      if (hvscroll)
	{
	  /* Set page only on first write */
	  /* This is guesswork, but seems to function correctly. */
	  RAM[0x2000] = (RAM[0x2000] & 0xFC) | ((vramlatch & 0xC00) >> 10);
	  vscan = vramlatch >> 12;
	  vwrap = 0;
	  /*vscrollreg=(vscrollreg&0x3F)|((VRAMPTR&3)<<6);*/
	}
      else
	{
	  /* Set offset on second write */
	  hscrollreg = (hscrollreg & 7) | ((vramlatch & 31) << 3);
	  vline = (vramlatch & 0x3e0) >> 5;
	  vscan = vramlatch >> 12;
	  vwrap = 0;
	}
      hscrollval = ((RAM[0x2000] & 1) << 8) + hscrollreg;
      for (x = scanline; x < 240; x++)
	hscroll[x] = hscrollval;

      /*if(CLOCK<VBL)printf("hbl update: %4x %d\n",VRAMPTR,CLOCK); */

      /*if(CLOCK<VBL)printf("hbl update: %4x %d /%d /%d \n",VRAMPTR,CLOCK,vramlatch,hvscroll); */
      /*VRAMPTR&=0x3fff; */
      VRAMPTR = vramlatch & 0x3fff;
    }
  /* Argh... This 2006 shit is complicated.  It seems that writing to
     2006 alters the low two bits of 2000, and furthermore makes it so
     that the next write to 2005 will set the horizontal scroll.  This
     is in addition to the direct effect it has on the horizontal
     scroll.  The effect on 2000 happens even during vblank, such that
     accessing the vram during vblank will cause the next frame to
     start at whatever vram page was selected, unless the program
     writes to register 2000 to change it.  I probably don't have this
     completely correct. */
  
  /* Write VRAM */
  if (addr == 0x2007)
    {
      /* A little bit about VRAM:

         The NES has two pages of internal VRAM.  The emulator always stores
         the data internally at 2000 and 2400, however the page at 2400 may
         be mapped to 2800 in the PPU address space, with the page at 2400
         mirroring 2000, or vice versa.

         0000-1FFF is mapped in from the game cartridge, and may be RAM or ROM.

         3Fxx holds the color palette.
       */

      VRAMPTR &= 0x3fff;
      /*mmc2_latch(VRAMPTR); */
      /* For debugging */
      /*if(CLOCK<VBL&&(RAM[0x2001]&8)) printf("vram write during refresh! "); */
      /*printf("VRAM: %4x=%2x (+%d) scan %d\n",VRAMPTR,val,1<<(((*((unsigned char *)REG1)&4)>>2)*5),CLOCK); */
      if ((VRAMPTR & 0x3f0f) == 0x3f00)
        VRAM[0x3f00] = VRAM[0x3f10] = (unsigned char) val;      /* Background color is mirrored between palettes */
      else if ((VRAMPTR & 0x3f00) == 0x3f00)
	{
	  /* Write to color palette */

	  /* FIXME: when CLOCK<VBL we should switch into static-color mode */
	  VRAM[VRAMPTR & 0x3f1f] = (unsigned char) val;

	  /* FIXME - This might flicker on palettized displays; see
           * above for suggested fix, or use the "--static-color"
           * command-line parameter as a workaround.
	   */
	  renderer->UpdateColors ();
	}
      else if (VRAMPTR >= 0x2000 && VRAMPTR < 0x3000)
        {
          if (nomirror)
            VRAM[VRAMPTR] = (unsigned char) val;
          else if (osmirror)
            VRAM[VRAMPTR & 0x23FF] = (unsigned char) val;       /* One-Screen Mirroring */
          else if (!hvmirror)
            VRAM[VRAMPTR] = VRAM[VRAMPTR ^ 0x800] = (unsigned char) val;
          else if (hvmirror)
            {
              if (VRAMPTR >= 0x2000 && VRAMPTR < 0x2400)
                VRAM[VRAMPTR] = VRAM[VRAMPTR ^ 0x800] = (unsigned char) val;
              if (VRAMPTR >= 0x2400 && VRAMPTR < 0x2800)
                VRAM[VRAMPTR - 0x400] = VRAM[VRAMPTR + 0x400] = (unsigned
                                                                 char) val;
              if (VRAMPTR >= 0x2800 && VRAMPTR < 0x2c00)
                VRAM[VRAMPTR - 0x400] = VRAM[VRAMPTR + 0x400] = (unsigned
                                                                 char) val;
              if (VRAMPTR >= 0x2c00 && VRAMPTR < 0x3000)
                VRAM[VRAMPTR] = VRAM[VRAMPTR ^ 0x800] = (unsigned char) val;
            }
        }
      else
        VRAM[VRAMPTR] = (unsigned char) val;

      VRAMPTR += 1 << (((*((unsigned char *) REG1) & 4) >> 2) * 5);     /* bit 2 of $2000 controls increment */
      if (VRAMPTR > 0x3FFF)
        {                       /* printf ("help - vramptr >0x3fff!\n"); exit(1); */
          VRAMPTR &= 0x3fff;
        }
    }

  /* Write to pAPU registers */
  if ((addr >= 0x4000) && (addr <= 0x4015))
    {
      /* Sprite DMA */
      if (addr == 0x4014)
        /* I'm not entirely sure of this, but it seems accurate. */
        {
          drawimage (CLOCK * 3);
          if (val < 0x80)
            memcpy (spriteram, RAM + (val << 8), 256);
          else
            memcpy (spriteram, MAPTABLE[val >> 4] + (val << 8), 256);
          CLOCK += 514;
          CTNI += 514;
        }
      else
        {
	  /* SOUND WRITE */
          SoundEvent( addr, val );
        }
    }

  if (addr == 0x2003)
    spriteaddr = val;
  if (addr == 0x2004)
    spriteram[spriteaddr] = val;

  /* VS UniSystem CHR rom bank switch */
  if ((MAPPERNUMBER == 99) && (addr == 0x4016))
	vs(addr, val);

  /* Reset controller */
  if ((addr | 1) == 0x4017)
    {
      if (swap_inputs)
	{
	  RAM[0x4016] = controller[1] | controllerd[1];
	  RAM[0x4017] = controller[0] | controllerd[0];
	}
      else
	{
	  RAM[0x4016] = controller[0] | controllerd[0];
	  RAM[0x4017] = controller[1] | controllerd[1];
	}
    }

  /* more debugging stuff: */
/*
   printf("Write: %4x,%2x (scan %d)\n",addr,val,CLOCK);
   printf("stack: %x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x\n",
   (&addr)[1],(&addr)[2],(&addr)[3],(&addr)[4],(&addr)[5],(&addr)[6],
   (&addr)[7],(&addr)[8],(&addr)[9],(&addr)[10],(&addr)[11],(&addr)[12] );
   * */

}

/* This determines whether an NMI should occur and returns 1 if so. */
/* This function is called even when interrupts are off, and also */
/* refreshes the screen as necessary. */
int
donmi(void)
{
  int x;

  /*printf("donmi: at %d\n",CLOCK); */

  CLOCK = VBL + 7;              /* 7 cycle interrupt latency */
  CTNI = -CPF + 7;

  if (last_clock >= CLOCK || last_clock <= 27393)
    vbl=1;
  last_clock = CLOCK;

  renderer->UpdateDisplay ();

  /*printf("donmi: stack at %x\n",STACKPTR); */

  /* reset scroll registers */
  /*hvscroll = 0;*/
  for (x = 0; x < 240; x++)
    {
      hscroll[x] = hscrollval;
      vscroll[x] = vscrollval;
      linereg[x] = RAM[0x2000];
    }

  /* Is an NMI to be generated?  Return 1 if so. */
  if (((*((unsigned char *) (REG1))) & 0x80) != 0)
    return 1;
  else
    return 0;
}

/*
 * this is left in here for debugging purposes but is not normally called:
 * FIXME: this makes assumptions about the exact parameter passing
 * implementation!
 */
void
trace(s)
	int		s;
{
  /*
     printf ("branch, stack: %x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x\n",
     (&s)[1], (&s)[2], (&s)[3], (&s)[4], (&s)[5], (&s)[6],
     (&s)[7], (&s)[8], (&s)[9], (&s)[10], (&s)[11], (&s)[12]);
   */
  unsigned int x;
  char hex[17] = "0123456789ABCDEF";
  x = ((int *) (&s))[4];
  printf ("%c%c%c%c\n", hex[x >> 12], hex[(x & 0xf00) >> 8], hex[(x & 0xf0) >> 4], hex[x & 0xf]);
}
