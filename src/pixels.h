/*
 * This file is part of the TuxNES project codebase.
 *
 * Please see the README and COPYING files for more information regarding
 * this project.
 *
 * Description: This file is included several times with different bpps
 * and enlargement/scanline combinations defined. See fb.c.
 */

#include "consts.h"

#undef pixel_t

#undef endian_fix

#if (BPP==1)
#define endian_fix(x) (x)
#define pixel_t unsigned char
#if DOUBLE
#if SCANLINES
void    drawimage1s(int);
#else
void    drawimage1d(int);
#endif
#else
void    drawimage1(int);
#endif
#endif

#if (BPP==4)
#define endian_fix(x) ((x) | ((x) << 4))
#define pixel_t unsigned char
#if DOUBLE
#if SCANLINES
void    drawimage4s(int);
#else
void    drawimage4d(int);
#endif
#else
void    drawimage4(int);
#endif
#endif

#if (BPP==8)
#define endian_fix(x) (x)
#define pixel_t unsigned char
#if DOUBLE
#if SCANLINES
void    drawimage8s(int);
#else
void    drawimage8d(int);
#endif
#else
void    drawimage8(int);
#endif
#endif

#if (BPP==16)
#define endian_fix(x) \
        (pix_swab \
         ? ((((x) & 0xFF) << 8) | ((x) >> 8)) \
         : (x))
#define pixel_t unsigned short int
#if DOUBLE
#if SCANLINES
void    drawimage16s(int);
#else
void    drawimage16d(int);
#endif
#else
void    drawimage16(int);
#endif
#endif

#if (BPP==24)
#define endian_fix(x) \
        (pix_swab \
         ? ((((x) & 0xFF) << 16) | ((x) & 0xFF00) | ((x) >> 16)) \
         : (x))
#define pixel_t unsigned char
#if DOUBLE
#if SCANLINES
void    drawimage24s(int);
#else
void    drawimage24d(int);
#endif
#else
void    drawimage24(int);
#endif
#endif

#if (BPP==32)
#define endian_fix(x) \
        (pix_swab \
         ? ((((x) & 0xFF) << 24) \
            | (((x) & 0xFF00) << 8) \
            | (((x) & 0xFF0000) >> 8) \
            | ((x) >> 24)) \
         : (x))
#define pixel_t unsigned int
#if DOUBLE
#if SCANLINES
void    drawimage32s(int);
#else
void    drawimage32d(int);
#endif
#else
void    drawimage32(int);
#endif
#endif

#undef rpixmap
#define rpixmap ((pixel_t *)rfb)

#undef pixmap
#define pixmap ((pixel_t *)fb)

#if DOUBLE
#if SCANLINES
#if (BPP==1)
void
drawimage1s(int endclock)
#endif
#if (BPP==4)
void
drawimage4s(int endclock)
#endif
#if (BPP==8)
void
drawimage8s(int endclock)
#endif
#if (BPP==16)
void
drawimage16s(int endclock)
#endif
#if (BPP==24)
void
drawimage24s(int endclock)
#endif
#if (BPP==32)
void
drawimage32s(int endclock)
#endif
#else
#if (BPP==1)
void
drawimage1d(int endclock)
#endif
#if (BPP==4)
void
drawimage4d(int endclock)
#endif
#if (BPP==8)
void
drawimage8d(int endclock)
#endif
#if (BPP==16)
void
drawimage16d(int endclock)
#endif
#if (BPP==24)
void
drawimage24d(int endclock)
#endif
#if (BPP==32)
void
drawimage32d(int endclock)
#endif
#endif
#else
#if (BPP==1)
void
drawimage1(int endclock)
#endif
#if (BPP==4)
void
drawimage4(int endclock)
#endif
#if (BPP==8)
void
drawimage8(int endclock)
#endif
#if (BPP==16)
void
drawimage16(int endclock)
#endif
#if (BPP==24)
void
drawimage24(int endclock)
#endif
#if (BPP==32)
void
drawimage32(int endclock)
#endif
#endif
{
	static unsigned int lastclock = 0;
	static unsigned int curhscroll = 0;
	unsigned int curclock = lastclock;
	unsigned int baseaddr = ((RAM[0x2000] & 0x10) << 8);  /* 0 or 0x1000 */
	unsigned int currentline = lastclock / HCYCLES;
	unsigned int hposition = lastclock % HCYCLES;
	unsigned int x;
	int s;
	int hflip, vflip, behind;
	int d1, d2;
	int spritebase = (RAM[0x2000] & 0x08) << 9;   /* 0x0 or 0x1000 */
	int spritesize = 8 << ((RAM[0x2000] & 0x20) >> 5);    /* 8 or 16 */
	int spritetile;
	unsigned char linebuffer[256];
	unsigned char bgmask[256];
	static unsigned int tile;
	static int bit;
	static unsigned char byte1, byte2;
#if (BPP==24)
	static unsigned int curpal[4];
#if DOUBLE && SCANLINES
	static unsigned int curpal2[4];
#endif
#else
	static pixel_t curpal[4];
#if DOUBLE && SCANLINES
	static pixel_t curpal2[4];
#endif
#endif
	static unsigned char tilecolor;
	static pixel_t *rptr, *rptr0;
	static pixel_t *ptr, *ptr0;
#if (BPP==1) || (BPP==4)
	static pixel_t pix_mask;
#elif (BPP==24)
	int pix_byte;
#endif

	if (frameskip)
	{
		return;
	}

/* 113 2/3 cpu cycles per scanline */
/* = 341 ppu cycles per scanline */
/* = 81840 ppu cycles per frame (not counting vblank) */
/* = 89342 ppu cycles per frame (including vblank) */

	if (endclock > PBL)
		return;
	if (curclock > PBL)
		curclock = 0;
	if (endclock <= curclock)
		return;
	if (curclock == 0)
	{
		/* Begin new frame */
		vline = vscrollreg >> 3;
		vscan = vscrollreg & 7;
		vwrap = 0;
		if (osmirror)
			scanpage = 0x2000;
		else if (nomirror)
			scanpage = 0x2000 + ((RAM[0x2000] & 3) << 10);
		else if (hvmirror == 0)
			scanpage = 0x2000 + ((RAM[0x2000] & 1) << 10);  /* v-mirror, h-layout */
		else if (hvmirror == 1)
			scanpage = 0x2000 + ((RAM[0x2000] & 2) << 9);   /* h-mirror, v-layout */
		curpal[0] = endian_fix(palette[24]);
#if DOUBLE && SCANLINES
		curpal2[0] = endian_fix(palette2[24]);
#endif
		rptr0 = rptr = rpixmap;
		ptr0 = ptr = pixmap;
#if (BPP==1)
		if (lsb_first)
			{
#if DOUBLE
			pix_mask = 3;
#else
			pix_mask = 1;
#endif
			}
		else
		{
#if DOUBLE
			pix_mask = 0xc0;
#else
			pix_mask = 0x80;
#endif
		}
#endif
#if (BPP==4)
#if DOUBLE
		pix_mask = 0xff;
#else
		if (lsn_first)
		{
			pix_mask = 0x0f;
		}
		else
		{
			pix_mask = 0xf0;
		}
#endif
#endif
	}

	if (hposition >= 85)
	{
		/* In scanline */
		x = hposition - 85 + curhscroll;
	}
	else
	{
		/* In hblank */
		curhscroll = hscrollreg;
		x = curhscroll;
		tile = VRAM[scanpage + ((x & 255) >> 3) + (vline << 5)];
		mmc2_4_latch(baseaddr + (tile << 4) + vscan);
		mmc2_4_latch(baseaddr + (tile << 4) + vscan + 8);
		byte1 = VRAM[baseaddr + (tile << 4) + vscan] << (x & 7);
		byte2 = VRAM[baseaddr + (tile << 4) + vscan + 8] << (x & 7);
		bit = (~x) & 7;
		curclock += 85 - hposition;
		hposition = 85;
		tilecolor = VRAM[scanpage + 0x3C0 + ((x & 255) >> 5) + ((vline & 28)
		                                                        << 1)] >>
		  ((vline
		    &
		    2)
		   << 1);
		if (x & 16)
			tilecolor >>= 2;
		curpal[1] = endian_fix(palette[3 * (tilecolor & 3)]);
		curpal[2] = endian_fix(palette[3 * (tilecolor & 3) + 1]);
		curpal[3] = endian_fix(palette[3 * (tilecolor & 3) + 2]);
#if DOUBLE && SCANLINES
		curpal2[1] = endian_fix(palette2[3 * (tilecolor & 3)]);
		curpal2[2] = endian_fix(palette2[3 * (tilecolor & 3) + 1]);
		curpal2[3] = endian_fix(palette2[3 * (tilecolor & 3) + 2]);
#endif
	}

	while (curclock < endclock)
	{
		while (hposition < HCYCLES && curclock < endclock)
		{
			if (RAM[0x2001] & 8)
			{
				bit--;
#if (BPP==1)
#if (DOUBLE && !SCANLINES)
				ptr[nextline] =
#endif
				  *ptr = (*rptr & ~pix_mask) |
				  ((curpal[bgmask[hposition - 85] = (((byte1 & 0x80) >>
				                                      7) | ((byte2 &
				                                             0x80) >> 6))]) ?
				   pix_mask : 0);
#if (DOUBLE && SCANLINES)
				ptr[nextline] = (rptr[nextline] & ~pix_mask) |
				  ((curpal2[bgmask[hposition - 85]]) ? pix_mask : 0);
#endif
#elif (BPP==4)
#if (DOUBLE && !SCANLINES)
				ptr[nextline] =
#endif
				  *ptr = (*rptr & ~pix_mask) |
				  ((curpal[bgmask[hposition - 85] = (((byte1 & 0x80) >>
				                                      7) | ((byte2 &
				                                             0x80) >> 6))]) &
				   pix_mask);
#if (DOUBLE && SCANLINES)
				ptr[nextline] = (rptr[nextline] & ~pix_mask) |
				  ((curpal2[bgmask[hposition - 85]]) & pix_mask);
#endif
#elif (BPP == 24)
				bgmask[hposition - 85] =
				  (((byte1 & 0x80) >> 7) |
				   ((byte2 & 0x80) >> 6));
				for (pix_byte = 0; pix_byte < 3; pix_byte ++)
				{
#if DOUBLE
#if !SCANLINES
					ptr[nextline + 3 + pix_byte] =
					  ptr[nextline + pix_byte] =
#endif
					  ptr[3 + pix_byte] =
#endif
					  ptr[pix_byte] =
					  (curpal[bgmask[hposition - 85]]) >> (8 * pix_byte);
#if DOUBLE && SCANLINES
					ptr[nextline + 3 + pix_byte] =
					  ptr[nextline + pix_byte] =
					  (curpal2[bgmask[hposition - 85]]) >> (8 * pix_byte);
#endif
				}
#else /* (BPP != 1) && (BPP != 24) */
#if DOUBLE
#if !SCANLINES
				ptr[nextline + 1] =
				  ptr[nextline] =
#endif
				  ptr[1] =
#endif
				  *ptr = curpal[bgmask[hposition - 85] = (((byte1 & 0x80) >>
				                                           7) | ((byte2 &
				                                                  0x80) >> 6))];
#if DOUBLE && SCANLINES
				ptr[nextline + 1] =
				  ptr[nextline] = curpal2[bgmask[hposition - 85]];
#endif
#endif
				byte1 <<= 1;
				byte2 <<= 1;
			}
			else
			{
				/* Blank screen / Background color */
#if (BPP==1)
#if DOUBLE && !SCANLINES
				ptr[nextline] =
#endif
				  *ptr = (*rptr & ~pix_mask) |
				  (*curpal ?
				   pix_mask : 0);
#if DOUBLE && SCANLINES
				ptr[nextline] = (rptr[nextline] & ~pix_mask) |
				  (*curpal2 ?
				   pix_mask : 0);
#endif
#elif (BPP==4)
#if DOUBLE && !SCANLINES
				ptr[nextline] =
#endif
				  *ptr = (*rptr & ~pix_mask) |
				  (*curpal & pix_mask);
#if DOUBLE && SCANLINES
				ptr[nextline] = (rptr[nextline] & ~pix_mask) |
				  (*curpal2 & pix_mask);
#endif
#elif (BPP==24)
				for (pix_byte = 0; pix_byte < 3; pix_byte ++)
				{
#if DOUBLE
#if !SCANLINES
					ptr[nextline + 3 + pix_byte] =
					  ptr[nextline + pix_byte] =
#endif
					  ptr[3 + pix_byte] =
#endif
					  ptr[pix_byte] = (*curpal) >> (8 * pix_byte);
#if DOUBLE && SCANLINES
					ptr[nextline + 3 + pix_byte] =
					  ptr[nextline + pix_byte] =
					  (*curpal2) >> (8 * pix_byte);
#endif
				}
#else /* (BPP != 1) && (BPP != 24) */
#if DOUBLE
#if !SCANLINES
				ptr[nextline + 1] =
				  ptr[nextline] =
#endif
				  ptr[1] =
#endif
				  *ptr = *curpal;
#if DOUBLE && SCANLINES
				ptr[nextline + 1] =
				  ptr[nextline] = *curpal2;
#endif
#endif
			}
#if (BPP==1)
#if DOUBLE
			if (lsb_first)
			{
				if (!(pix_mask <<= 2))
				{
					pix_mask = 3;
					rptr++;
					ptr++;
				}
			}
			else
			{
				if (!(pix_mask >>= 2))
				{
					pix_mask = 0xc0;
					rptr++;
					ptr++;
				}
			}
#else
			if (lsb_first)
			{
				if (!(pix_mask <<= 1))
				{
					pix_mask = 1;
					rptr++;
					ptr++;
				}
			}
			else
			{
				if (!(pix_mask >>= 1))
				{
					pix_mask = 0x80;
					rptr++;
					ptr++;
				}
			}
#endif
#elif (BPP==4)
#if DOUBLE
			rptr++;
			ptr++;
#else
			if (lsb_first)
			{
				if (!(pix_mask <<= 4))
				{
					pix_mask = 0x0f;
					rptr++;
					ptr++;
				}
			}
			else
			{
				if (!(pix_mask >>= 4))
				{
					pix_mask = 0xf0;
					rptr++;
					ptr++;
				}
			}
#endif
#elif (BPP==24)
#if DOUBLE
			rptr+=3;
			ptr+=3;
#endif
			rptr+=3;
			ptr+=3;
#else
#if DOUBLE
			rptr++;
			ptr++;
#endif
			rptr++;
			ptr++;
#endif
			x++;
			hposition++;
			curclock++;
			if (x == 256)
				if ((!osmirror) && (nomirror || !hvmirror))
					scanpage ^= 0x400;        /* bit 8 of x -> bit 10 of addr */
			if (bit < 0)
			{
				tile = VRAM[scanpage + ((x & 255) >> 3) + (vline << 5)];
				mmc2_4_latch(baseaddr + (tile << 4) + vscan);
				mmc2_4_latch(baseaddr + (tile << 4) + vscan + 8);
				byte1 = VRAM[baseaddr + (tile << 4) + vscan];
				byte2 = VRAM[baseaddr + (tile << 4) + vscan + 8];
				bit = 7;
				if ((x & 0xf) == 0)
				{
					if ((x & 0x1f) == 0)
						tilecolor = VRAM[scanpage + 0x3C0 + ((x & 255) >> 5) +
						                 ((vline
						                   &
						                   28)
						                  <<
						                  1)] >> ((vline & 2) << 1);
					else
						tilecolor >>= 2;
					curpal[1] = endian_fix(palette[3 * (tilecolor & 3)]);
					curpal[2] = endian_fix(palette[3 * (tilecolor & 3) + 1]);
					curpal[3] = endian_fix(palette[3 * (tilecolor & 3) + 2]);
#if DOUBLE && SCANLINES
					curpal2[1] = endian_fix(palette2[3 * (tilecolor & 3)]);
					curpal2[2] = endian_fix(palette2[3 * (tilecolor & 3) + 1]);
					curpal2[3] = endian_fix(palette2[3 * (tilecolor & 3) + 2]);
#endif
				}
			}
		}

		if (hposition == HCYCLES)
		{
			if (RAM[0x2001] & 16)
			{
				/* Draw sprites */
				memset(linebuffer, 0, 256);      /* Clear buffer for this scanline */
				for (s = 0; s < 64; s++) {
					if (spriteram[s * 4] < currentline && spriteram[s * 4] <
					  240 && spriteram[s * 4 + 3] < 249
					      && (spriteram[s * 4] + spritesize >= currentline))
					{
						spritetile = spriteram[s * 4 + 1];
						if ((spritetile == 0xfd) || (spritetile == 0xfe)) {
							mmc2_4_latchspr(spritetile);
						}
					}
				}
				for (s = 63; s >= 0; s--)
				{
					if (spriteram[s * 4] < currentline && spriteram[s * 4] <
					    240 && spriteram[s * 4 + 3] < 249)
					{
						if (spriteram[s * 4] + spritesize >= currentline)
						{
							spritetile = spriteram[s * 4 + 1];
							if (spritesize == 16)
								spritebase = (spritetile & 1) << 12;
							behind = spriteram[s * 4 + 2] & 0x20;
							hflip = spriteram[s * 4 + 2] & 0x40;
							vflip = spriteram[s * 4 + 2] & 0x80;

							/* This finds the memory location of the tiles, taking into account
							   that vertically flipped sprites are in reverse order. */
							if (vflip)
							{
								if (spriteram[s << 2] >= ((signed int)
								                          currentline) - 8)       /* 8x8 sprites and first half of 8x16 sprites */
								{
									d1 = VRAM[spritebase + ((spritetile &
									                         (~(spritesize
									                            >>
									                            4))) << 4) +
									          spritesize * 2 - 8 - currentline
									          + spriteram[s * 4]];
									d2 = VRAM[spritebase + ((spritetile &
									                         (~(spritesize
									                            >>
									                            4))) << 4) +
									          spritesize * 2 - currentline +
									          spriteram[s * 4]];
								}
								else
								/* Do second half of 8x16 sprites */
								{
									d1 = VRAM[spritebase + ((spritetile &
									                         (~(spritesize
									                            >>
									                            4))) << 4) +
									          spritesize * 2 - 16 -
									          currentline + spriteram[s * 4]];
									d2 = VRAM[spritebase + ((spritetile &
									                         (~(spritesize
									                            >>
									                            4))) << 4) +
									          spritesize * 2 - 8 - currentline
									          + spriteram[s * 4]];
								}
							}
							else
							{
								if (spriteram[s << 2] >= ((signed int)
								                          currentline) - 8)       /* 8x8 sprites and first half of 8x16 sprites */
								{
									d1 = VRAM[spritebase + ((spritetile &
									                         (~(spritesize
									                            >>
									                            4))) << 4) +
									      currentline - 1 - spriteram[s * 4]];
									d2 = VRAM[spritebase + ((spritetile &
									                         (~(spritesize
									                            >>
									                            4))) << 4) +
									      currentline + 7 - spriteram[s * 4]];
								}
								else
								/* Do second half of 8x16 sprites */
								{
									d1 = VRAM[spritebase + ((spritetile &
									                         (~(spritesize
									                            >>
									                            4))) << 4) +
									      currentline + 7 - spriteram[s * 4]];
									d2 = VRAM[spritebase + ((spritetile &
									                         (~(spritesize
									                            >>
									                            4))) << 4) +
									     currentline + 15 - spriteram[s * 4]];
								}
							}
							for (x = 7 * (!hflip); x < 8 && x >= 0; x += 1 -
							     ((!hflip) << 1))
							{
								if (d1 & d2 & 1)
									linebuffer[spriteram[s * 4 + 3] + x] = 12 +
									  2 + ((spriteram[s * 4 + 2] & 3) * 3);
								else if (d1 & 1)
									linebuffer[spriteram[s * 4 + 3] + x] = 12 +
									  ((spriteram[s
									              *
									              4
									              +
									              2]
									    &
									    3)
									   * 3);
								else if (d2 & 1)
									linebuffer[spriteram[s * 4 + 3] + x] = 12 +
									  1 + ((spriteram[s * 4 + 2] & 3) * 3);
								if (behind && (d1 | d2))
									if (bgmask[spriteram[s * 4 + 3] + x])
										linebuffer[spriteram[s * 4 + 3] + x] = 0;     /* Sprite hidden behind background */
								d1 >>= 1;
								d2 >>= 1;
							}
						}
					}
				}

				for (x = 0; x < 256; x++)
				{
					if (linebuffer[x])
					{
#if (BPP==1)
#if DOUBLE
						unsigned int offset = spriteram[s * 4 + 3] + x;
						unsigned char mask;

						if (lsb_first)
							mask = 3 << ((offset & 3) << 1);
						else
							mask = 0xc0 >> ((offset & 3) << 1);
						offset = (offset >> 2);
#if !SCANLINES
						ptr0[offset + nextline] =
#endif
						  ptr0[offset] = (rptr0[offset] & ~mask) |
						  endian_fix(palette[linebuffer[x]] ? mask : 0);
#if SCANLINES
						ptr0[offset + nextline] =
						  (rptr0[offset + nextline] & ~mask) |
						  endian_fix(palette2[linebuffer[x]] ? mask : 0);
#endif
#else
						unsigned int offset = spriteram[s * 4 + 3] + x;
						unsigned char mask;

						if (lsb_first)
							mask = 1 << (offset & 7);
						else
							mask = 0x80 >> (offset & 7);
						offset = (offset >> 3);
						ptr0[offset] = (rptr0[offset] & ~mask) |
						  endian_fix(palette[linebuffer[x]] ? mask : 0);
#endif
#elif (BPP==4)
#if DOUBLE
#if !SCANLINES
						ptr0[spriteram[s * 4 + 3] + x + nextline] =
#endif
						  ptr0[spriteram[s * 4 + 3] + x] =
						  endian_fix(palette[linebuffer[x]]);
#if SCANLINES
						ptr0[spriteram[s * 4 + 3] + x + nextline] =
						  endian_fix(palette2[linebuffer[x]]);
#endif
#else
						unsigned int offset = spriteram[s * 4 + 3] + x;
						unsigned char mask;

						if (lsb_first)
							mask = 0x0f << ((offset & 1) << 2);
						else
							mask = 0xf0 >> ((offset & 1) << 2);
						offset = (offset >> 1);
						ptr0[offset] = (rptr0[offset] & ~mask) |
						  endian_fix(palette[linebuffer[x]] & mask);
#endif
#elif (BPP==24)
						for (pix_byte = 0; pix_byte < 3; pix_byte ++)
						{
#if DOUBLE
#if !SCANLINES
							ptr0[((spriteram[s * 4 + 3] + x) * 2 + 1) * 3
							    + nextline + pix_byte] =
							  ptr0[((spriteram[s * 4 + 3] + x) * 2) * 3
							      + nextline + pix_byte] =
#endif
							  ptr0[((spriteram[s * 4 + 3] + x) * 2 + 1) * 3
							      + pix_byte] =
							  ptr0[((spriteram[s * 4 + 3] + x) * 2) * 3
							      + pix_byte] =
							  endian_fix(palette[linebuffer[x]]) >> (8 * pix_byte);
#if SCANLINES
							ptr0[((spriteram[s * 4 + 3] + x) * 2 + 1) * 3
							    + nextline + pix_byte] =
							  ptr0[((spriteram[s * 4 + 3] + x) * 2) * 3
							      + nextline + pix_byte] =
							  endian_fix(palette2[linebuffer[x]]) >> (8 * pix_byte);
#endif
#else
							ptr0[(spriteram[s * 4 + 3] + x) * 3
							    + pix_byte] =
							  endian_fix(palette[linebuffer[x]]) >> (8 * pix_byte);
#endif
						}
#else /* (BPP != 1) && (BPP != 24) */
#if DOUBLE
#if !SCANLINES
						ptr0[(spriteram[s * 4 + 3] + x) * 2 + 1 + nextline] =
						  ptr0[(spriteram[s * 4 + 3] + x) * 2 + nextline] =
#endif
						  ptr0[(spriteram[s * 4 + 3] + x) * 2 + 1] =
						  ptr0[(spriteram[s * 4 + 3] + x) * 2] =
						  endian_fix(palette[linebuffer[x]]);
#if SCANLINES
						ptr0[(spriteram[s * 4 + 3] + x) * 2 + 1 + nextline] =
						  ptr0[(spriteram[s * 4 + 3] + x) * 2 + nextline] =
						  endian_fix(palette2[linebuffer[x]]);
#endif
#else
						ptr0[spriteram[s * 4 + 3] + x] =
						  endian_fix(palette[linebuffer[x]]);
#endif
#endif
					}
				}
			}

			/* Next line */
			rptr = rptr0 + nextline;
			ptr = ptr0 + nextline;
#if DOUBLE
			rptr += nextline;
			ptr += nextline;
#endif
			rptr0 = rptr;
			ptr0 = ptr;
			currentline++;
			curhscroll = hscrollreg;
			x = curhscroll;
			vscan++;
			if (vscan >= 8)
			{
				vscan = 0;
				vline++;
				vline &= 31;
				if (vline == 30)
				{
					vline = 0;
					vwrap ^= 1;
				}
			}
			if (osmirror)
				scanpage = 0x2000;
			else if (nomirror)
				scanpage = 0x2000 + (((RAM[0x2000] & 3) << 10) ^ (vwrap << 11));
			else if (hvmirror == 0)
				scanpage = 0x2000 + ((RAM[0x2000] & 1) << 10);      /* v-mirror, h-layout */
			else if (hvmirror == 1)
				scanpage = 0x2000 + (((RAM[0x2000] & 2) << 9) ^ (vwrap << 10));     /* h-mirror, v-layout */
			tile = VRAM[scanpage + ((x & 255) >> 3) + (vline << 5)];
			mmc2_4_latch(baseaddr + (tile << 4) + vscan);
			mmc2_4_latch(baseaddr + (tile << 4) + vscan + 8);
			byte1 = VRAM[baseaddr + (tile << 4) + vscan] << (x & 7);
			byte2 = VRAM[baseaddr + (tile << 4) + vscan + 8] << (x & 7);
			bit = (~x) & 7;
			tilecolor = VRAM[scanpage + 0x3C0 + ((x & 255) >> 5) + ((vline &
			                                                         28) << 1)]
			  >> ((vline & 2) << 1);
			if (x & 16)
				tilecolor >>= 2;
			curpal[1] = endian_fix(palette[3 * (tilecolor & 3)]);
			curpal[2] = endian_fix(palette[3 * (tilecolor & 3) + 1]);
			curpal[3] = endian_fix(palette[3 * (tilecolor & 3) + 2]);
#if DOUBLE && SCANLINES
			curpal2[1] = endian_fix(palette2[3 * (tilecolor & 3)]);
			curpal2[2] = endian_fix(palette2[3 * (tilecolor & 3) + 1]);
			curpal2[3] = endian_fix(palette2[3 * (tilecolor & 3) + 2]);
#endif
			hposition = 85;
			curclock += 85;
		}
	}

	if ((lastclock = endclock) >= PBL)
		lastclock = 0;
}
