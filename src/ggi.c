/*
 * This file is part of the TuxNES project codebase.
 *
 * Please see the README and COPYING files for more information regarding
 * this project.
 *
 * Description: This file handles the I/O subsystem for GGI.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_FEATURES_H
#include <features.h>
#endif /* HAVE_FEATRES_H */

#include <sys/time.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "consts.h"
#include "controller.h"
#include "globals.h"
#include "joystick.h"
#include "mapper.h"
#include "renderer.h"
#include "screenshot.h"
#include "sound.h"

/* imports */
void    quit(void);
void    START(void);
void    fbinit(void);

#ifdef HAVE_GGI
#include <ggi/gii.h>
#include <ggi/ggi.h>

#ifdef HAVE_LIBPPM
#ifdef HAVE_PPM_H
#include <ppm.h>

#define HAVE_PPM 1

static pixel **pixels = 0;
static ggi_color *cols = 0;

#endif
#endif

/* exported functions */
int     InitDisplayGGI(int argc, char **argv);
void    UpdateColorsGGI(void);
void    UpdateDisplayGGI(void);

static void     InitScreenshotGGI(void);
static void     SaveScreenshotGGI(void);
static void     HandleKeyboardGGI(ggi_event ev);

static ggi_visual_t visualGGI;
static const ggi_pixelformat *formatGGI;
static const ggi_directbuffer *bufferGGI[2] = {0, 0};
static int directGGI;
static int numbuffersGGI;
static int frameno;
static ggi_mode modeGGI;
static ggi_pixel whitepixel, blackpixel;
static ggi_pixel paletteGGI[64], palette2GGI[64];
static ggi_pixel basepixel;
static ggi_color colormapGGI[27];

#endif

#ifdef HAVE_GGI

static void
InitScreenshotGGI(void)
{
#ifdef HAVE_PPM
	int                   ppm_argc = 1;
	char                  *ppm_argv[] = {
		PACKAGE_NAME
	};

	/* Initialize PPM */
	ppm_init(&ppm_argc, ppm_argv);

	screenshot_init(".ppm");

	if (!(pixels = ppm_allocarray(256 * magstep, 240 * magstep))) {
		perror("ppm_allocarray");
		fprintf(stderr, "GGI: screenshots are disabled\n");
	} else {
		if (!(cols = malloc(256 * magstep * sizeof *cols))) {
			perror("malloc");
			pixels = 0;
		}
	}
#endif
}

static void
SaveScreenshotGGI(void)
{
#ifdef HAVE_PPM
	int x, y;
	int w, h;
	FILE *f = 0;

	if (!pixels) {
		fprintf(stderr, "GGI: screenshots are disabled\n");
		return;
	}

	screenshot_new();
	w = 256 * magstep;
	h = 240 * magstep;
	/* default (fast-ish) color decoding */
	if (bpp > 1) {
		for (y = 0; y < h; y++) {
			if (ggiUnpackPixels(visualGGI, (void *)(rfb + y * bytes_per_line), cols, w)) {
				/* indicates ggiUnpackPixels failed */
				break;
			}
			for (x = 0; x < w; x++) {
				PPM_ASSIGN(pixels[y][x],
				           cols[x].r >> 8, cols[x].g >> 8, cols[x].b >> 8);
			}
		}
	} else {
		y = 0;
	}
	/* fallback for crappy visuals */
	if (y < h) {
		int x0, y0;
		int x1, y1;
		int w0, h0;

		if ((modeGGI.frames > 1)
		 && (ggiGetReadFrame(visualGGI) != frameno)
		 && ggiSetReadFrame(visualGGI, frameno)) {
			fprintf(stderr,
			        "GGI: ggiSetReadFrame failed for frame %d, no screenshot\n",
			        frameno);
			return;
		}
		w0 = modeGGI.visible.x;
		h0 = modeGGI.visible.y;
		if (w0 < w) {
			x1 = 0;
		} else {
			x1 = (w0 - w) / 2;
		}
		if (h0 < h) {
			y1 = 0;
		} else {
			y1 = (h0 - h) / 2;
		}
		for (y0 = y; y0 < h; y0 += h0)
			for (x0 = 0; x0 < w; x0 += w0) {
				if ((h > h0) || (w > w0)) {
					ggiPutBox(visualGGI,
					          x1 - x0, y1 - y0,
					          256 * magstep, 240 * magstep,
					          fb);
					ggiFlush(visualGGI);
				}
				for (y = 0; (y < h0) && ((y + y0) < h); y++)
					for (x = 0; (x < w0) && ((x + x0) < w); x++) {
						ggi_pixel pix;

						cols[x + x0].r = cols[x + x0].b = 0xFFFF;
						cols[x + x0].g = 0x0000;
						if (ggiGetPixel(visualGGI, x + x1, y + y1, &pix)) {
							if (verbose)
								fprintf(stderr,
								        "GGI: can't get pixel at (%d,%d)!\n",
								        x + x1, y + y1);
						} else if (ggiUnmapPixel(visualGGI, pix, cols + x + x0)) {
							if (verbose)
								fprintf(stderr,
								        "GGI: can't unmap color %d!\n",
								        pix);
						}
						PPM_ASSIGN(pixels[y + y0][x + x0],
						           cols[x + x0].r >> 8,
						           cols[x + x0].g >> 8,
						           cols[x + x0].b >> 8);
					}
			}
	}
	f = fopen(screenshotfile, "wb");
	if (f) {
		ppm_writeppm(f, pixels, w, h, 255, 0);
		if (verbose) {
			fprintf(stderr, "Wrote screenshot to %s\n", screenshotfile);
		}
		fclose(f);
	} else {
		perror(screenshotfile);
	}
#else
	fprintf(stderr,
	        "cannot capture screenshots; please install ppm library and then recompile\n");
#endif /* HAVE_PPM */
}

static void
HandleKeyboardGGI(ggi_event ev)
{
	_Bool press = ev.any.type == evKeyPress;

	if (press && ev.key.sym == GIIUC_Escape) {
		ggiClose(visualGGI);
		ggiExit();
		quit();                    /* ESC */
	}

	/* the coin and dipswitch inputs work only in VS UniSystem mode */
	if (unisystem)
		switch (ev.key.sym) {
		case '[':
		case '{':
			ctl_coinslot(0x01, press);
			break;
		case ']':
		case '}':
			ctl_coinslot(0x02, press);
			break;
		case '\\':
		case '|':
			ctl_coinslot(0x04, press);
			break;
		case GIIUC_Q:
		case GIIUC_q:
			ctl_dipswitch(0x01, press);
			break;
		case GIIUC_W:
		case GIIUC_w:
			ctl_dipswitch(0x02, press);
			break;
		case GIIUC_E:
		case GIIUC_e:
			ctl_dipswitch(0x04, press);
			break;
		case GIIUC_R:
		case GIIUC_r:
			ctl_dipswitch(0x08, press);
			break;
		case GIIUC_T:
		case GIIUC_t:
			ctl_dipswitch(0x10, press);
			break;
		case GIIUC_Y:
		case GIIUC_y:
			ctl_dipswitch(0x20, press);
			break;
		case GIIUC_U:
		case GIIUC_u:
			ctl_dipswitch(0x40, press);
			break;
		case GIIUC_I:
		case GIIUC_i:
			ctl_dipswitch(0x80, press);
			break;
		}

	switch (ev.key.sym) {
		/* controller 1 keyboard mapping */
	case GIIUC_Return:
	case GIIK_PEnter:
		ctl_keypress(0, STARTBUTTON, press);
		break;
	case GIIUC_Tab:
	case GIIK_PTab:
	case GIIK_PPlus:
		ctl_keypress(0, SELECTBUTTON, press);
		break;
	case GIIK_Up:
	case GIIK_P8:
		ctl_keypress(0, UP, press);
		break;
	case GIIK_Down:
	case GIIK_P2:
		ctl_keypress(0, DOWN, press);
		break;
	case GIIK_Left:
	case GIIK_P4:
		ctl_keypress(0, LEFT, press);
		break;
	case GIIK_Right:
	case GIIK_P6:
		ctl_keypress(0, RIGHT, press);
		break;
	case GIIK_Home:
	case GIIK_P7:
		ctl_keypress_diag(0, UP | LEFT, press);
		break;
	case GIIK_Prior:
	case GIIK_P9:
		ctl_keypress_diag(0, UP | RIGHT, press);
		break;
	case GIIK_End:
	case GIIK_P1:
		ctl_keypress_diag(0, DOWN | LEFT, press);
		break;
	case GIIK_Next:
	case GIIK_P3:
		ctl_keypress_diag(0, DOWN | RIGHT, press);
		break;
	case GIIUC_Z:
	case GIIUC_z:
	case GIIUC_X:
	case GIIUC_x:
	case GIIUC_D:
	case GIIUC_d:
	case GIIK_Insert:
	case GIIK_P0:
	case GIIK_ShiftL:
	case GIIK_ShiftR:
	case GIIK_CtrlL:
	case GIIK_AltR:
		ctl_keypress(0, BUTTONB, press);
		break;
	case GIIUC_A:
	case GIIUC_a:
	case GIIUC_C:
	case GIIUC_c:
	case GIIUC_Space:
	case GIIK_PSpace:
	case GIIK_Clear:
	case GIIUC_Delete:
	case GIIK_PBegin:
	case GIIK_P5:
	case GIIK_PDecimal:
	case GIIK_AltL:
	case GIIK_CtrlR:
		ctl_keypress(0, BUTTONA, press);
		break;

		/* controller 2 keyboard mapping */
	case GIIUC_G:
	case GIIUC_g:
		ctl_keypress(1, STARTBUTTON, press);
		break;
	case GIIUC_F:
	case GIIUC_f:
		ctl_keypress(1, SELECTBUTTON, press);
		break;
	case GIIUC_K:
	case GIIUC_k:
		ctl_keypress(1, UP, press);
		break;
	case GIIUC_J:
	case GIIUC_j:
		ctl_keypress(1, DOWN, press);
		break;
	case GIIUC_H:
	case GIIUC_h:
		ctl_keypress(1, LEFT, press);
		break;
	case GIIUC_L:
	case GIIUC_l:
		ctl_keypress(1, RIGHT, press);
		break;
	case GIIUC_V:
	case GIIUC_v:
		ctl_keypress(1, BUTTONB, press);
		break;
	case GIIUC_B:
	case GIIUC_b:
		ctl_keypress(1, BUTTONA, press);
		break;
	}

	/* emulator keys */
	if (press)
		switch (ev.key.sym) {
/*		case GIIK_F1: */
/*			debug_bgoff = 1; */
/*			break; */
/*		case GIIK_F2: */
/*			debug_bgoff = 0; */
/*			break; */
/*		case GIIK_F3: */
/*			debug_spritesoff = 1; */
/*			break; */
/*		case GIIK_F4: */
/*			debug_spritesoff = 0; */
/*			break; */
/*		case GIIK_F5: */
/*			debug_switchtable = 1; */
/*			break; */
/*		case GIIK_F6: */
/*			debug_switchtable = 0; */
/*			break; */
		case GIIK_F7:
		case GIIK_PrintScreen:
		case GIIUC_S:
		case GIIUC_s:
			SaveScreenshotGGI();
			break;
/*		case GIIK_F8: */
/*			memset(displaycachevalid, 0, sizeof displaycachevalid); */
/*			memset(bitplanecachevalid, 0, sizeof bitplanecachevalid); */
/*			break; */
		case GIIUC_BackSpace:
			START();
			break;
		case GIIK_Pause:
		case GIIUC_P:
		case GIIUC_p:
			renderer_data.pause_display = !renderer_data.pause_display;
			desync = 1;
			break;
		case GIIUC_Grave:
			desync = 1;
			halfspeed = 1;
			doublespeed = 0;
			renderer_data.pause_display = 0;
			break;
		case GIIUC_1:
			desync = 1;
			halfspeed = 0;
			doublespeed = 0;
			renderer_data.pause_display = 0;
			break;
		case GIIUC_2:
			desync = 1;
			halfspeed = 0;
			doublespeed = 2;
			renderer_data.pause_display = 0;
			break;
		case GIIUC_3:
			desync = 1;
			halfspeed = 0;
			doublespeed = 3;
			renderer_data.pause_display = 0;
			break;
		case GIIUC_4:
			desync = 1;
			halfspeed = 0;
			doublespeed = 4;
			renderer_data.pause_display = 0;
			break;
		case GIIUC_5:
			desync = 1;
			halfspeed = 0;
			doublespeed = 5;
			renderer_data.pause_display = 0;
			break;
		case GIIUC_6:
			desync = 1;
			halfspeed = 0;
			doublespeed = 6;
			renderer_data.pause_display = 0;
			break;
		case GIIUC_7:
			desync = 1;
			halfspeed = 0;
			doublespeed = 7;
			renderer_data.pause_display = 0;
			break;
		case GIIUC_8:
			desync = 1;
			halfspeed = 0;
			doublespeed = 8;
			renderer_data.pause_display = 0;
			break;
		case GIIUC_0:
			desync = 1;
			renderer_data.pause_display = 1;
			break;
		}
}

int
InitDisplayGGI(int argc, char **argv)
{
	ggi_color color;
	struct timeval time;

	if (magstep < 1) {
		magstep = 1;
	}
	if (magstep > maxsize) {
		fprintf(stderr, "Warning: Enlargement factor %d is too large!\n",
		        magstep);
		magstep = maxsize;
	}
	if (ggiInit()) {
		fprintf(stderr, "%s: GGI initialization failed\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	if (!(visualGGI = ggiOpen(renderer_config.display_id, 0))) {
		fprintf(stderr, "%s: Can't open visual on %s%s%s target\n",
		        argv[0],
		        renderer_config.display_id ? "\"" : "",
		        renderer_config.display_id ? renderer_config.display_id : "default",
		        renderer_config.display_id ? "\"" : "");
		exit(EXIT_FAILURE);
	}
	ggiParseMode(renderer_config.geometry ? renderer_config.geometry : "", &modeGGI);
	if (modeGGI.visible.x == GGI_AUTO)
		modeGGI.visible.x = 256 * magstep;
	if (modeGGI.visible.y == GGI_AUTO)
		modeGGI.visible.y = 240 * magstep;
	if (modeGGI.virt.x == GGI_AUTO)
		modeGGI.virt.x = modeGGI.visible.x;
	if (modeGGI.virt.y == GGI_AUTO)
		modeGGI.virt.y = modeGGI.visible.y;
	if (modeGGI.virt.x < 240 * magstep)
		modeGGI.virt.x = 256 * magstep;
	if (modeGGI.virt.y < 240 * magstep)
		modeGGI.virt.y = 240 * magstep;
	if (modeGGI.frames == GGI_AUTO)
		modeGGI.frames = 2;
	ggiSetFlags(visualGGI, GGIFLAG_ASYNC);
	ggiCheckMode(visualGGI, &modeGGI);
	if (ggiSetMode(visualGGI, &modeGGI) < 0) {
		if (verbose) {
			fprintf(stderr, "GGI: ggiSetMode failed, trying ggiSetSimpleMode.\n");
		}
		if (ggiSetSimpleMode(visualGGI,
		                     modeGGI.visible.x,
		                     modeGGI.visible.y,
		                     modeGGI.frames,
		                     GT_AUTO) < 0) {
			ggiClose(visualGGI);
			ggiExit();
			fprintf(stderr, "%s: requested GGI mode refused.\n", argv[0]);
			exit(EXIT_FAILURE);
		}
	}
	ggiGetMode(visualGGI, &modeGGI);
	if (verbose) {
		fprintf(stderr, "GGI mode: \"");
		ggiFPrintMode(stderr, &modeGGI);
		fprintf(stderr, "\"\n");
	}
	if ((GT_SCHEME(modeGGI.graphtype) == GT_PALETTE) && indexedcolor) {
		colormapGGI[0].r = colormapGGI[0].g = colormapGGI[0].b = 0x0000;
		colormapGGI[1].r = colormapGGI[1].g = colormapGGI[1].b = 0xFFFF;
		if ((basepixel = ggiSetPalette(visualGGI, GGI_PALETTE_DONTCARE, 27,
		                               colormapGGI)) < 0) {
			ggiClose(visualGGI);
			ggiExit();
			fprintf(stderr, "Can't allocate colors!\n");
			exit(EXIT_FAILURE);
		}
		/* Pre-initialize the colormap to known values */
		blackpixel = basepixel;
		whitepixel = basepixel + 1;
		oldbgcolor = currentbgcolor = NES_palette[0];
		color.r = ((NES_palette[0] & 0xFF0000) >> 8);
		color.g = (NES_palette[0] & 0xFF00);
		color.b = ((NES_palette[0] & 0xFF) << 8);
		for (int x = 0; x <= 24; x++) {
			palette[x] = x + basepixel + 2;
			colormapGGI[x + 2] = color;
			palette2[x] = blackpixel;
		}
		if (ggiSetPalette(visualGGI, basepixel, 27, colormapGGI) < 0) {
			fprintf(stderr,
			        "======================================================\n"
			        "GGI: ggiSetPalette failed in GT_PALETTE mode!\n"
			        "\n"
			        "To work around this problem, run " PACKAGE_NAME " with the\n"
			        "--static-color option.\n"
			        "\n"
			        "There is probably a bug in your GGI display driver.\n"
			        "As a permanent solution, consider fixing libggi and\n"
			        "submitting your fix back to the GGI maintainers.\n"
			        "======================================================\n");
			indexedcolor = 0;
		}
		if (scanlines && (scanlines != 100)) {
			fprintf(stderr, "Warning: Scanline intensity is ignored in indexed-color modes!\n");
			scanlines = 0;
		}
	}
	if ((GT_SCHEME(modeGGI.graphtype) != GT_PALETTE) || !indexedcolor) {
		indexedcolor = 0;
		if (GT_SCHEME(modeGGI.graphtype) == GT_PALETTE) {
			/* setup GGI palette -- should handle indexed-color visuals */
			ggiSetColorfulPalette(visualGGI);
		}
		color.r = color.g = color.b = 0x0000;
		blackpixel = ggiMapColor(visualGGI, &color);
		color.r = color.g = color.b = 0xFFFF;
		whitepixel = ggiMapColor(visualGGI, &color);
		for (int x=0; x < 24; x++) {
			palette[x] = palette2[x] = blackpixel;
		}
		for (int x=0; x < 64; x++) {
			color.r = ((NES_palette[x] & 0xFF0000) >> 8);
			color.g = (NES_palette[x] & 0xFF00);
			color.b = ((NES_palette[x] & 0xFF) << 8);
			paletteGGI[x] = ggiMapColor(visualGGI, &color);
			palette2GGI[x] = blackpixel;
		}
		if (scanlines && (scanlines != 100))
			for (int x = 0; x < 64; x++) {
				unsigned long r, g, b;

				r = ((NES_palette[x] & 0xFF0000) >> 8) * (scanlines / 100.0);
				if (r > 0xFFFF)
					r = 0xFFFF;
				color.r = r;
				g = (NES_palette[x] & 0xFF00) * (scanlines / 100.0);
				if (g > 0xFFFF)
					g = 0xFFFF;
				color.g = g;
				b = ((NES_palette[x] & 0xFF) << 8) * (scanlines / 100.0);
				if (b > 0xFFFF)
					b = 0xFFFF;
				color.b = b;
				palette2GGI[x] = ggiMapColor(visualGGI, &color);
			}
	}
	frameno = 0;
	directGGI = 0;
	formatGGI = ggiGetPixelFormat(visualGGI);
	numbuffersGGI = ggiDBGetNumBuffers(visualGGI);
	if (numbuffersGGI > 2)
		numbuffersGGI = 2;
	if ((numbuffersGGI > 0)
	 && (modeGGI.visible.x >= 256 * magstep)
	 && (modeGGI.visible.x <= 400 * magstep)
	 && (modeGGI.visible.y >= 240 * magstep)
	 && (modeGGI.visible.y <= 300 * magstep)) {
		int buffer;

		frameno = 0;
		for (buffer = 0; buffer < numbuffersGGI; buffer++) {
			if ((bufferGGI[frameno] = ggiDBGetBuffer(visualGGI, buffer))) {
				if (bufferGGI[frameno]->type & GGI_DB_SIMPLE_PLB) {
					frameno++;
				}
			}
		}
		numbuffersGGI = frameno;
		frameno = 0;
		if (ggiResourceAcquire(bufferGGI[frameno]->resource,
		                       GGI_ACTYPE_READ | GGI_ACTYPE_WRITE)) {
			fprintf(stderr, "GGI: failed to acquire directbuffer\n");
		} else {
			if (bufferGGI[frameno]->write && bufferGGI[frameno]->read) {
				formatGGI = bufferGGI[frameno]->buffer.plb.pixelformat;
				bytes_per_line = bufferGGI[frameno]->buffer.plb.stride;
				bpp = formatGGI->size;
				if (!bpp) {
					bpp = GT_SIZE(modeGGI.graphtype);
				}
				rfb = (char *)bufferGGI[frameno]->read
				    + bytes_per_line * ((240 * magstep - modeGGI.visible.y) / -2)
				    + (bpp * ((256 * magstep - modeGGI.visible.x) / -2)) / 8;
				fb = (char *)bufferGGI[frameno]->write
				   + bytes_per_line * ((240 * magstep - modeGGI.visible.y) / -2)
				   + (bpp * ((256 * magstep - modeGGI.visible.x) / -2)) / 8;
				directGGI = 1;
			} else
				ggiResourceRelease(bufferGGI[frameno]->resource);
		}
	}
	pix_swab = formatGGI->flags & GGI_PF_REVERSE_ENDIAN;
	lsb_first = formatGGI->flags & GGI_PF_HIGHBIT_RIGHT;
	lsn_first = lsb_first; /* who knows? packed 4bpp is really an obscure case */
	bpu = 8;
	if (!fb) {
		bpp = formatGGI->size;
		if (!bpp) {
			bpp = GT_SIZE(modeGGI.graphtype);
		}
	}
	depth = formatGGI->depth;
	if (!depth) {
		depth = GT_DEPTH(modeGGI.graphtype);
	}
	if (verbose) {
		fprintf(stderr,
		        "GGI: %s scheme, depth %d, %dbpp, %s colors, %d %s%s\n",
		        (GT_SCHEME(modeGGI.graphtype) == GT_TEXT) ? "Text" :
		        (GT_SCHEME(modeGGI.graphtype) == GT_TRUECOLOR) ? "Truecolor" :
		        (GT_SCHEME(modeGGI.graphtype) == GT_GREYSCALE) ? "Greyscale" :
		        (GT_SCHEME(modeGGI.graphtype) == GT_PALETTE) ? "Palette" :
		        (GT_SCHEME(modeGGI.graphtype) == GT_STATIC_PALETTE) ?
		        "Static Palette" : "Unknown",
		        depth,
		        bpp,
		        indexedcolor ? "dynamic" : "static",
		        directGGI ? numbuffersGGI : modeGGI.frames,
		        directGGI ? "direct buffer" : "frame",
		        ((directGGI ? numbuffersGGI : modeGGI.frames) == 1) ? "" : "s");
	}
	if (!fb) {
		bytes_per_line = 256 / 8 * magstep * bpp;
		if (!(rfb = (fb = malloc(bytes_per_line * 240 * magstep)))) {
			ggiClose(visualGGI);
			ggiExit();
			perror("malloc");
			exit(EXIT_FAILURE);
		}
	}
	InitScreenshotGGI();
	fbinit();
	if ((modeGGI.frames > 1)
	 && (ggiGetWriteFrame(visualGGI) != (directGGI ? bufferGGI[frameno]->frame : frameno))
	 && ggiSetWriteFrame(visualGGI, directGGI ? bufferGGI[frameno]->frame : frameno)) {
		ggiClose(visualGGI);
		ggiExit();
		fprintf(stderr,
		        "GGI: ggiSetWriteFrame failed for frame %d\n",
		        directGGI ? bufferGGI[frameno]->frame : frameno);
		exit(EXIT_FAILURE);
	}
	ggiSetGCForeground(visualGGI, blackpixel);
	ggiFillscreen(visualGGI);
	ggiSetGCForeground(visualGGI, whitepixel);
	ggiFlush(visualGGI);
	gettimeofday(&time, NULL);
	renderer_data.basetime = time.tv_sec;
	return 0;
}

/* Update the colors on the screen if the palette changed */
void
UpdateColorsGGI(void)
{
	/* Set Background color */
	oldbgcolor = currentbgcolor;
	if (indexedcolor) {
		currentbgcolor = NES_palette[VRAM[0x3f00] & 63];
		if (currentbgcolor != oldbgcolor) {
			colormapGGI[24 + 2].r = ((currentbgcolor & 0xFF0000) >> 8);
			colormapGGI[24 + 2].g = (currentbgcolor & 0xFF00);
			colormapGGI[24 + 2].b = ((currentbgcolor & 0xFF) << 8);
			if (ggiSetPalette(visualGGI, 24 + basepixel + 2, 1,
			                  colormapGGI + 24 + 2) < 0) {
				fprintf(stderr,
				        "GGI: ggiSetPalette failed in GT_PALETTE mode!\n");
			}
		}
	} else /* truecolor */ {
		palette[24] = currentbgcolor = paletteGGI[VRAM[0x3f00] & 63];
		if (scanlines && (scanlines != 100))
			palette2[24] = palette2GGI[VRAM[0x3f00] & 63];
		if (oldbgcolor != currentbgcolor /*||currentbgcolor!=bgcolor[currentcache] */) {
			redrawbackground = 1;
			needsredraw = 1;
		}
	}

	/* Tile colors */
	if (indexedcolor) {
		for (int x = 0; x < 24; x++) {
			if (VRAM[0x3f01 + x + (x / 3)] != palette_cache[0][1 + x + (x / 3)]) {
				colormapGGI[x+2].r = ((NES_palette[VRAM[0x3f01 + x + (x / 3)] & 63] & 0xFF0000) >> 8);
				colormapGGI[x+2].g = (NES_palette[VRAM[0x3f01 + x + (x / 3)] & 63] & 0xFF00);
				colormapGGI[x+2].b = ((NES_palette[VRAM[0x3f01 + x + (x / 3)] & 63] & 0xFF) << 8);
				if (ggiSetPalette(visualGGI, x + basepixel + 2, 1, colormapGGI + x + 2) < 0) {
					ggiClose(visualGGI);
					ggiExit();
					fprintf(stderr,
					        "GGI: ggiSetPalette failed in GT_PALETTE mode!\n");
					exit(EXIT_FAILURE);
				}
			}
		}
		memcpy(palette_cache[0], VRAM + 0x3f00, 32);
	}

	/* Set palette tables */
	if (indexedcolor) {
		/* Already done in InitDisplayGGI */
	} else /* truecolor */ {
		for (int x = 0; x < 24; x++) {
			palette[x] = paletteGGI[VRAM[0x3f01 + x + (x / 3)] & 63];
			if (scanlines && (scanlines != 100))
				palette2[x] = palette2GGI[VRAM[0x3f01 + x + (x / 3)] & 63];
		}
	}
}

void
UpdateDisplayGGI(void)
{
	struct timeval time;
	static unsigned int frame;
	unsigned int timeframe;
	static int nodisplay = 0;

	/* do audio update */
	UpdateAudio();

	/* Check the time.  If we're getting behind, skip a frame to stay in sync. */
	gettimeofday(&time, NULL);
	timeframe = (time.tv_sec - renderer_data.basetime) * 50 + time.tv_usec / 20000;     /* PAL */
	timeframe = (time.tv_sec - renderer_data.basetime) * 60 + time.tv_usec / 16666;     /* NTSC */
	frame++;
	if (halfspeed)
		timeframe >>= 1;
	else if (doublespeed)
		timeframe *= doublespeed;
	if (desync)
		frame = timeframe;
	desync = 0;
	if (frame < timeframe - 20 && frame % 20 == 0)
		desync = 1;                 /* If we're more than 20 frames behind, might as well stop counting. */

	if (!nodisplay) {
		drawimage(PBL);
		if (!frameskip) {
			UpdateColorsGGI();
			if (directGGI) {
				ggiResourceRelease(bufferGGI[frameno]->resource);
				ggiFlush(visualGGI);
				if ((modeGGI.frames > 1)
				 && (ggiGetDisplayFrame(visualGGI) != (directGGI ? bufferGGI[frameno]->frame : frameno))
				 && ggiSetDisplayFrame(visualGGI, bufferGGI[frameno]->frame)) {
					ggiClose(visualGGI);
					ggiExit();
					fprintf(stderr,
					        "GGI: ggiSetDisplayFrame failed for frame %d\n",
					        bufferGGI[frameno]->frame);
					exit(EXIT_FAILURE);
				}
				frameno = (frameno + 1) % numbuffersGGI;
				if (ggiResourceAcquire(bufferGGI[frameno]->resource,
				                       GGI_ACTYPE_READ | GGI_ACTYPE_WRITE)) {
					fprintf(stderr, "GGI: failed to acquire directbuffer\n");
					ggiClose(visualGGI);
					ggiExit();
					exit(EXIT_FAILURE);
				} else {
					if (bufferGGI[frameno]->write && bufferGGI[frameno]->read) {
						formatGGI = bufferGGI[frameno]->buffer.plb.pixelformat;
						bytes_per_line = bufferGGI[frameno]->buffer.plb.stride;
						bpp = formatGGI->size;
						if (!bpp) {
							bpp = GT_SIZE(modeGGI.graphtype);
						}
						rfb = (char *)bufferGGI[frameno]->read
						    + bytes_per_line * ((240 * magstep - modeGGI.visible.y) / -2)
						    + (bpp * ((256 * magstep - modeGGI.visible.x) / -2)) / 8;
						fb = (char *)bufferGGI[frameno]->write
						   + bytes_per_line * ((240 * magstep - modeGGI.visible.y) / -2)
						   + (bpp * ((256 * magstep - modeGGI.visible.x) / -2)) / 8;
						pix_swab = formatGGI->flags & GGI_PF_REVERSE_ENDIAN;
						lsb_first = formatGGI->flags & GGI_PF_HIGHBIT_RIGHT;
						bpu = 8;
						if (formatGGI->depth) {
							depth = formatGGI->depth;
						}
						fbinit();
						if ((modeGGI.frames > 1)
						 && (ggiGetWriteFrame(visualGGI) != (directGGI ? bufferGGI[frameno]->frame : frameno))
						 && ggiSetWriteFrame(visualGGI, bufferGGI[frameno]->frame)) {
							ggiClose(visualGGI);
							ggiExit();
							fprintf(stderr,
							        "GGI: ggiSetWriteFrame failed for frame %d\n",
							        bufferGGI[frameno]->frame);
							exit(EXIT_FAILURE);
						}
					} else {
						fprintf(stderr, "GGI: bad directbuffer pointers\n");
						ggiClose(visualGGI);
						ggiExit();
						exit(EXIT_FAILURE);
					}
				}
			} else {
				ggiPutBox(visualGGI,
				          (256 * magstep - modeGGI.visible.x) / -2,
				          (240 * magstep - modeGGI.visible.y) / -2,
				          256 * magstep, 240 * magstep,
				          fb);
				ggiFlush(visualGGI);
				if ((modeGGI.frames > 1)
				 && (ggiGetDisplayFrame(visualGGI) != (directGGI ? bufferGGI[frameno]->frame : frameno))
				 && ggiSetDisplayFrame(visualGGI, frameno)) {
					ggiClose(visualGGI);
					ggiExit();
					fprintf(stderr,
					        "GGI: ggiSetDisplayFrame failed for frame %d\n",
					        frameno);
					exit(EXIT_FAILURE);
				}
				frameno = (frameno + 1) % modeGGI.frames;
				while ((modeGGI.frames > 1)
				 && (ggiGetWriteFrame(visualGGI) != (directGGI ? bufferGGI[frameno]->frame : frameno))
				 && ggiSetWriteFrame(visualGGI, frameno)) {
					frameno = (frameno + 1) % modeGGI.frames;
				}
			}
			redrawall = needsredraw = 0;
		}
	}

	needsredraw = 0;
	redrawbackground = 0;
	redrawall = 0;

	/* Slow down if we're getting ahead */
	if (frame > timeframe + 1 && frameskip == 0) {
		usleep(16666 * (frame - timeframe - 1));
	}

	/* Input loop */
	do {
		struct timeval tv = { 0, 0 };

		/* Handle joystick input */
		js_handle_input();

		/* Handle GGI input */
		while (ggiEventPoll(visualGGI, emKeyPress | emKeyRelease, &tv)) {
			ggi_event ev;

			ggiEventRead(visualGGI, &ev, emKeyPress | emKeyRelease);
			HandleKeyboardGGI(ev);
		}
	} while (renderer_data.pause_display);

	/* Check the time.  If we're getting behind, skip next frame to stay in sync. */
	gettimeofday(&time, NULL);
	timeframe = (time.tv_sec - renderer_data.basetime) * 60 + time.tv_usec / 16666;     /* NTSC */
	if (halfspeed)
		timeframe >>= 1;
	else if (doublespeed)
		timeframe *= doublespeed;
	if (frame >= timeframe || frame % 20 == 0)
		frameskip = 0;
	else
		frameskip = 1;
}

#endif
