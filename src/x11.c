// SPDX-FileCopyrightText: Authors of TuxNES
// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Description: This file handles the I/O subsystem for the X window
 * system.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/* if you'd like defocusing to pause emulation, define this */
#undef PAUSE_ON_DEFOCUS

#include <hqx.h>

#include <errno.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/time.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef HAVE_MACHINE_ENDIAN_H
#include <machine/endian.h>
#endif
#ifdef HAVE_SYS_ENDIAN_H
#include <sys/endian.h>
#endif
#ifdef HAVE_ENDIAN_H
#include <endian.h>
#endif

#include "consts.h"
#include "controller.h"
#include "globals.h"
#include "joystick.h"
#include "renderer.h"
#include "screenshot.h"

#ifdef HAVE_X

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/XKBlib.h>
#include <X11/keysym.h>
#include <X11/cursorfont.h>

/*
 * Get this from the xscreensaver package if you want support for virtual
 * root windows
 */
#ifdef HAVE_X11_VROOT_H
#include <X11/vroot.h>
#endif

/* check for XPM support */
#if defined(HAVE_LIBXPM) \
 && defined(HAVE_X11_XPM_H)
#define HAVE_XPM 1
#include <X11/xpm.h>
#endif

/* check for X Extension support */
#if defined(HAVE_LIBXEXT) \
 && defined(HAVE_X11_EXTENSIONS_XEXT_H)
#define HAVE_XEXT 1
#include <X11/extensions/Xext.h>
#endif

/* check for X Shared Memory extension */
#if defined(HAVE_XEXT) \
 && defined(HAVE_SYS_IPC_H) \
 && defined(HAVE_SYS_SHM_H) \
 && defined(HAVE_X11_EXTENSIONS_XSHM_H)
#define HAVE_SHM 1
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>
#ifndef X_ShmAttach
#include <X11/Xmd.h>
#include <X11/extensions/shmproto.h>
#endif
#endif

#if defined(HAVE_LIBXSS) \
 && defined(HAVE_X11_EXTENSIONS_SCRNSAVER_H)
#define HAVE_SCRNSAVER 1
#include <X11/extensions/scrnsaver.h>
#endif

#if defined(HAVE_LIBXRENDER) \
 && defined(HAVE_X11_EXTENSIONS_XRENDER_H)
#define HAVE_XRENDER 1
#include <X11/extensions/Xrender.h>
#endif

/* external and forward declarations */
void    fbinit(void);
void    quit(void);
void    START(void);

/* exported functions */
int     InitDisplayX11(int argc, char **argv);
void    UpdateColorsX11(void);
void    UpdateDisplayX11(void);

/* X11 stuff: */
static Display  *display;
static Window    window;
static unsigned int window_w, window_h;
static int      mapped;
static Colormap colormap;
static GC       gc;
static XImage   *image = NULL;

static unsigned char    *keystate[32];
static unsigned long    colortableX11[25];
static unsigned long    paletteX11[64];

#ifdef HAVE_XRENDER
static Pixmap scalePixmap;
static Picture scalePicture, windowPicture;
#endif

static int shm_incomplete = 0;
#ifdef HAVE_SHM
static XShmSegmentInfo shminfo;
static Status shm_attached = 0;
static int shm_attaching = 0;
static int shm_major_opcode, shm_first_event, shm_first_error;

static int (*oldhandler)(Display *, XErrorEvent *) = NULL;

static int
shm_handler(Display *display, XErrorEvent *ev)
{
	if (shm_attaching
	 && (ev->request_code == shm_major_opcode)
	 && (ev->minor_code == X_ShmAttach)) {
		shm_attached = False;
		return 0;
	}
	if (oldhandler) {
		return oldhandler(display, ev);
	}
	return 0;
}

static void
shm_cleanup(void)
{
	XShmDetach(display, &shminfo);
	shmdt(shminfo.shmaddr);
	image->data = shminfo.shmaddr = NULL;
	XDestroyImage(image);
	image = NULL;
}
#endif

static void
InitScreenshotX11(void)
{
#ifdef HAVE_XPM
	screenshot_init(".xpm");
#endif
}

static void
SaveScreenshotX11(void)
{
#ifdef HAVE_XPM
	screenshot_new();
	int status = XpmWriteFileFromImage(display, screenshotfile, image, NULL, NULL);
	if (status != XpmSuccess) {
		fprintf(stderr, "%s: %s\n", screenshotfile, XpmGetErrorString(status));
	} else if (verbose) {
		fprintf(stderr, "Wrote screenshot to %s\n", screenshotfile);
	}
#else
	fprintf(stderr,
	        "cannot capture screenshots; please install Xpm library and then recompile\n");
#endif /* HAVE_XPM */
}

static bool
XRenderSupported(void)
{
#ifdef HAVE_XRENDER
	static bool checked = false, enabled = false;
	if (!checked) {
		int eventBase, errorBase;
		if (XRenderQueryExtension(display, &eventBase, &errorBase))
			enabled = true;
		checked = true;
	}
	return enabled;
#else
	return false;
#endif
}

int
InitDisplayX11(int argc, char **argv)
{
	display = XOpenDisplay(renderer_config.display_id);
	if (!display) {
		fprintf(stderr,
		        "%s: [%s] Can't open display: %s\n",
		        argv[0],
		        renderer->name,
		        XDisplayName(renderer_config.display_id));
		exit(EXIT_FAILURE);
	}
	int screen = DefaultScreen(display);
	unsigned int depth = DefaultDepth(display, screen);
	Visual *visual = DefaultVisual(display, screen);
	colormap = DefaultColormap(display, screen);
	gc = DefaultGC(display, screen);

	XGCValues GCValues;
	GCValues.foreground = WhitePixel(display, screen);
	GCValues.background = BlackPixel(display, screen);
	XChangeGC(display, gc, GCForeground | GCBackground, &GCValues);

	if ((visual->class & 1) && renderer_config.indexedcolor) {
		if (XAllocColorCells(display, colormap, 0, 0, 0, colortableX11, 25) == 0) {
			fprintf(stderr, "%s: [%s] Can't allocate colors!\n",
			        *argv, renderer->name);
			exit(EXIT_FAILURE);
		}
		/* Pre-initialize the colormap to known values */
		unsigned int palcolor = NES_palette[0];
		XColor color;
		color.flags = DoRed | DoGreen | DoBlue;
		color.red    = palcolor >> 16 & 0xff;
		color.green  = palcolor >>  8 & 0xff;
		color.blue   = palcolor       & 0xff;
		color.red   |= color.red   << 8;
		color.green |= color.green << 8;
		color.blue  |= color.blue  << 8;
		for (int x = 0; x <= 24; x++) {
			color.pixel = colortableX11[x];
			palette[x] = color.pixel;
			XStoreColor(display, colormap, &color);
		}
	} else {
		renderer_config.indexedcolor = 0;
		/* convert palette to local color format */
		for (int x = 0; x < 64; x++) {
			unsigned int palcolor = NES_palette[x];
			XColor color;
			color.red    = palcolor >> 16 & 0xff;
			color.green  = palcolor >>  8 & 0xff;
			color.blue   = palcolor       & 0xff;
			color.red   |= color.red   << 8;
			color.green |= color.green << 8;
			color.blue  |= color.blue  << 8;
			if (XAllocColor(display, colormap, &color) == 0) {
				fprintf(stderr, "%s: [%s] Can't allocate colors!\n",
				        *argv, renderer->name);
				exit(EXIT_FAILURE);
			}
			paletteX11[x] = color.pixel;
		}
	}

	/* magstep determines minimum window size */
	if (renderer_config.magstep <= renderer_config.scaler_magstep) {
		renderer_config.magstep = renderer_config.scaler_magstep;
	} else if (!XRenderSupported()) {
		fprintf(stderr, "[%s] Enlargement requires XRENDER support\n",
		        renderer->name);
		exit(EXIT_FAILURE);
	}

	/* allocate window (server-side drawable) */
	Window rootwindow = RootWindow(display, screen);
	unsigned int border_width = 0;
	if (renderer_config.inroot) {
		window = rootwindow;
		mapped = 1;
	} else {
		int user_x, user_y;
		int user_w, user_h;
		int gravity;
		XSizeHints sizehints;
		sizehints.flags = PMinSize | PBaseSize;
		sizehints.min_width = 256 * renderer_config.magstep;
		sizehints.min_height = 240 * renderer_config.magstep;
		sizehints.base_width = 0;
		sizehints.base_height = 0;
		int geometry_mask = XWMGeometry(display, screen,
		                                renderer_config.geometry, NULL,
		                                border_width, &sizehints,
		                                &user_x, &user_y, &user_w, &user_h, &gravity);
		if (geometry_mask & (XValue | YValue)) {
			sizehints.flags |= USPosition;
			sizehints.x = user_x;
			sizehints.y = user_y;
		}
		if (geometry_mask & (WidthValue | HeightValue)) {
			sizehints.flags |= USSize;
			sizehints.width = user_w;
			sizehints.height = user_h;
		}
		sizehints.flags |= PWinGravity;
		sizehints.win_gravity = gravity;

		XSetWindowAttributes attrs;
		attrs.background_pixel = BlackPixel(display, screen);
		attrs.backing_store = WhenMapped;
		attrs.cursor = XCreateFontCursor(display, XC_crosshair);
		window = XCreateWindow(display, rootwindow,
		                       user_x, user_y, user_w, user_h, border_width,
		                       /* depth */ CopyFromParent,
		                       /* class */ InputOutput,
		                       /* visual */ visual,
		                       /* valuemask */ CWBackPixel | CWBackingStore | CWCursor,
		                       /* attributes */ &attrs);

		/* set aspect ratio */
		XSetWMNormalHints(display, window, &sizehints);

		/* set window manager hints */
		XWMHints wmhints;
		wmhints.flags = InputHint | StateHint;
		wmhints.input = True;
		wmhints.initial_state = NormalState;
		XSetWMHints(display, window, &wmhints);

		/* set window title */
		XClassHint classhints;
		classhints.res_class = PACKAGE_NAME;
		classhints.res_name = PACKAGE;
		XSetClassHint(display, window, &classhints);
		XStoreName(display, window, PACKAGE_NAME);
		XSetIconName(display, window, PACKAGE);

		/* pass the command line to the window system */
		XSetCommand(display, window, argv, argc);
	}
	int x = 0;
	int y = 0;
	XGetGeometry(display, window, &rootwindow,
	             &x, &y, &window_w, &window_h, &border_width, &depth);

	XSelectInput(display, window, ExposureMask | StructureNotifyMask
#ifdef PAUSE_ON_DEFOCUS
	                            | FocusChangeMask
#endif
	                            | KeyPressMask | KeyReleaseMask | KeymapStateMask);
	XMapWindow(display, window);
	XFlush(display);

	struct timeval time;
	gettimeofday(&time, NULL);
	renderer_data.basetime = time.tv_sec;

	/* scaler_magstep determines XImage size */
	unsigned int w = 256 * renderer_config.scaler_magstep;
	unsigned int h = 240 * renderer_config.scaler_magstep;

	/* transfer to a separate drawable when server-side scaling */
#ifdef HAVE_XRENDER
	if (renderer_config.magstep > renderer_config.scaler_magstep) {
		scalePixmap = XCreatePixmap(display, window, w, h, depth);

		XRenderPictFormat *fmt = XRenderFindVisualFormat(display, visual);
		XRenderPictureAttributes attrs = {0};

		windowPicture = XRenderCreatePicture(display, window, fmt, 0, &attrs);
		scalePicture = XRenderCreatePicture(display, scalePixmap, fmt, 0, &attrs);

		double inv_xscale = (double)renderer_config.scaler_magstep / (double)renderer_config.magstep, inv_yscale = inv_xscale;
		XTransform transform = {
			{
				{ XDoubleToFixed(inv_xscale), XDoubleToFixed(0), XDoubleToFixed(0) },
				{ XDoubleToFixed(0), XDoubleToFixed(inv_yscale), XDoubleToFixed(0) },
				{ XDoubleToFixed(0), XDoubleToFixed(0), XDoubleToFixed(1.0) }
			}
		};
		XRenderSetPictureTransform(display, scalePicture, &transform);
	}
#endif

	/* allocate XImage (client-side framebuffer) */
#ifdef HAVE_SHM
	if (XQueryExtension(display, "MIT-SHM", &shm_major_opcode, &shm_first_event, &shm_first_error)) {
		image = XShmCreateImage(display, visual, depth,
		                        depth == 1 ? XYBitmap : ZPixmap, NULL, &shminfo,
		                        w, h);
		if (image) {
			shminfo.shmid = shmget(IPC_PRIVATE, image->bytes_per_line * image->height, IPC_CREAT | 0600);
			if (shminfo.shmid == -1) {
				perror("shmget");
				goto shm_err1;
			}
			shminfo.shmaddr = shmat(shminfo.shmid, NULL, 0);
			if (shminfo.shmaddr == (void *)-1) {
				perror("shmat");
				goto shm_err1;
			}
			shminfo.readOnly = False;
			oldhandler = XSetErrorHandler(shm_handler);
			XSync(display, False);
			shm_attaching = 1;
			shm_attached = XShmAttach(display, &shminfo);
			XSync(display, False);
			shm_attaching = 0;
			if (!shm_attached) {
				fprintf(stderr,
					"[%s] XShmAttach failed\n",
					renderer->name);
				goto shm_err2;
			}
			image->data = shminfo.shmaddr;
			atexit(shm_cleanup);
			goto shm_done;
shm_err2:
			shmdt(shminfo.shmaddr);
shm_err1:
			XDestroyImage(image);
			image = NULL;
shm_done:
			if (shminfo.shmid != -1)
				shmctl(shminfo.shmid, IPC_RMID, NULL);
		}
	}
	if (!image)
#endif
	{
		image = XCreateImage(display, visual, depth,
		                     depth == 1 ? XYBitmap : ZPixmap, 0, NULL,
		                     w, h, BitmapPad(display), 0);
		if (!image) {
			fprintf(stderr, "[%s] Can't allocate image!\n",
			        renderer->name);
			exit(EXIT_FAILURE);
		}
		if (!(image->data = malloc(image->bytes_per_line * image->height))) {
			perror("malloc");
			exit(EXIT_FAILURE);
		}
	}

	if (verbose) {
		fprintf(stderr,
		        "[%s] %s visual, depth %d, %dbpp, %s colors, %s %s\n",
		        renderer->name,
		        (visual->class == StaticGray) ? "StaticGray" :
		        (visual->class == GrayScale) ? "GrayScale" :
		        (visual->class == StaticColor) ? "StaticColor" :
		        (visual->class == PseudoColor) ? "PseudoColor" :
		        (visual->class == TrueColor) ? "TrueColor" :
		        (visual->class == DirectColor) ? "DirectColor" :
		        "Unknown",
		        image->depth, image->bits_per_pixel,
		        renderer_config.indexedcolor ? "dynamic" : "static",
#ifdef HAVE_SHM
		        shm_attached ? "shared-memory" :
#endif
		        "plain",
		        "XImage");
	}

	/* set globals for fbinit/drawimage */
	rfb = fb = image->data;
	bytes_per_line = image->bytes_per_line;
	bpp = image->bits_per_pixel;
	bpu = image->bitmap_unit;
	lsb_first = image->bitmap_bit_order == LSBFirst;
	lsn_first = lsb_first; /* who knows? packed 4bpp is really an obscure case */
	pix_swab = 0;
#if BYTE_ORDER == BIG_ENDIAN
	if (image->byte_order == LSBFirst) {
		pix_swab = 1;
	} else if (image->byte_order != MSBFirst)
#elif BYTE_ORDER == LITTLE_ENDIAN
	if (image->byte_order == MSBFirst) {
		pix_swab = 1;
	} else if (image->byte_order != LSBFirst)
#endif
	{
		fprintf(stderr,
		        "======================================================\n"
		        "Warning: Mixed-endian host or pixel format detected\n"
		        "\n"
		        "This host may not render properly.\n"
		        "======================================================\n");
	}

	/* render to a separate unscaled framebuffer when client-side scaling */
	if (renderer_config.scaler_magstep > 1) {
		if (bpp != 32) {
			fprintf(stderr, "[%s] HQX will only work at 32bpp\n",
				renderer->name);
			exit(EXIT_FAILURE);
		}
		bytes_per_line = 256 * bpp / 8;
		if (!(rfb = fb = malloc(bytes_per_line * 240))) {
			perror("malloc");
			exit(EXIT_FAILURE);
		}
		hqxInit();
	}
	InitScreenshotX11();
	fbinit();
	return 0;
}

static void
HandleKeyboardX11(const XEvent *ev)
{
	KeySym keysym = XkbKeycodeToKeysym(display, ev->xkey.keycode, 0, 0);
	bool press = ev->type == KeyPress;

	if (press && keysym == XK_Escape)
		quit();                    /* ESC */

	/* the coin and dipswitch inputs work only in VS UniSystem mode */
	if (unisystem)
		switch (keysym) {
		case XK_bracketleft:
		case XK_braceleft:
			ctl_coinslot(0x01, press);
			break;
		case XK_bracketright:
		case XK_braceright:
			ctl_coinslot(0x02, press);
			break;
		case XK_backslash:
		case XK_bar:
			ctl_coinslot(0x04, press);
			break;
		case XK_Q:
		case XK_q:
			ctl_dipswitch(0x01, press);
			break;
		case XK_W:
		case XK_w:
			ctl_dipswitch(0x02, press);
			break;
		case XK_E:
		case XK_e:
			ctl_dipswitch(0x04, press);
			break;
		case XK_R:
		case XK_r:
			ctl_dipswitch(0x08, press);
			break;
		case XK_T:
		case XK_t:
			ctl_dipswitch(0x10, press);
			break;
		case XK_Y:
		case XK_y:
			ctl_dipswitch(0x20, press);
			break;
		case XK_U:
		case XK_u:
			ctl_dipswitch(0x40, press);
			break;
		case XK_I:
		case XK_i:
			ctl_dipswitch(0x80, press);
			break;
		}

	switch (keysym) {
		/* controller 1 keyboard mapping */
	case XK_Return:
	case XK_KP_Enter:
		ctl_keypress(0, STARTBUTTON, press);
		break;
	case XK_Tab:
	case XK_ISO_Left_Tab:
	case XK_KP_Add:
		ctl_keypress(0, SELECTBUTTON, press);
		break;
	case XK_Up:
	case XK_KP_Up:
	case XK_KP_8:
	case XK_Pointer_Up:
		ctl_keypress(0, UP, press);
		break;
	case XK_Down:
	case XK_KP_Down:
	case XK_KP_2:
	case XK_Pointer_Down:
		ctl_keypress(0, DOWN, press);
		break;
	case XK_Left:
	case XK_KP_Left:
	case XK_KP_4:
	case XK_Pointer_Left:
		ctl_keypress(0, LEFT, press);
		break;
	case XK_Right:
	case XK_KP_Right:
	case XK_KP_6:
	case XK_Pointer_Right:
		ctl_keypress(0, RIGHT, press);
		break;
	case XK_Pointer_UpLeft:
	case XK_Home:
	case XK_KP_Home:
	case XK_KP_7:
		ctl_keypress_diag(0, UP | LEFT, press);
		break;
	case XK_Pointer_UpRight:
	case XK_Prior:
	case XK_KP_Prior:
	case XK_KP_9:
		ctl_keypress_diag(0, UP | RIGHT, press);
		break;
	case XK_Pointer_DownLeft:
	case XK_End:
	case XK_KP_End:
	case XK_KP_1:
		ctl_keypress_diag(0, DOWN | LEFT, press);
		break;
	case XK_Pointer_DownRight:
	case XK_Next:
	case XK_KP_Next:
	case XK_KP_3:
		ctl_keypress_diag(0, DOWN | RIGHT, press);
		break;
	case XK_Z:
	case XK_z:
	case XK_X:
	case XK_x:
	case XK_D:
	case XK_d:
	case XK_Insert:
	case XK_KP_Insert:
	case XK_KP_0:
	case XK_Shift_L:
	case XK_Shift_R:
	case XK_Eisu_Shift:
	case XK_Control_L:
	case XK_Alt_R:
		ctl_keypress(0, BUTTONB, press);
		break;
	case XK_A:
	case XK_a:
	case XK_C:
	case XK_c:
	case XK_space:
	case XK_KP_Space:
	case XK_Clear:
	case XK_Delete:
	case XK_KP_Delete:
	case XK_KP_Begin:
	case XK_KP_5:
	case XK_KP_Decimal:
	case XK_Alt_L:
	case XK_Control_R:
		ctl_keypress(0, BUTTONA, press);
		break;

		/* controller 2 keyboard mapping */
	case XK_G:
	case XK_g:
		ctl_keypress(1, STARTBUTTON, press);
		break;
	case XK_F:
	case XK_f:
		ctl_keypress(1, SELECTBUTTON, press);
		break;
	case XK_K:
	case XK_k:
		ctl_keypress(1, UP, press);
		break;
	case XK_J:
	case XK_j:
		ctl_keypress(1, DOWN, press);
		break;
	case XK_H:
	case XK_h:
		ctl_keypress(1, LEFT, press);
		break;
	case XK_L:
	case XK_l:
		ctl_keypress(1, RIGHT, press);
		break;
	case XK_V:
	case XK_v:
		ctl_keypress(1, BUTTONB, press);
		break;
	case XK_B:
	case XK_b:
		ctl_keypress(1, BUTTONA, press);
		break;
	}

	/* emulator keys */
	if (press)
		switch (keysym) {
		case XK_F7:
		case XK_Print:
		case XK_S:
		case XK_s:
			SaveScreenshotX11();
			break;
		case XK_BackSpace:
			START();
			break;
		case XK_Pause:
		case XK_P:
		case XK_p:
			renderer_data.desync = 1;
			renderer_data.pause_display = !renderer_data.pause_display;
			break;
		case XK_grave:
			renderer_data.desync = 1;
			renderer_data.halfspeed = 1;
			renderer_data.doublespeed = 0;
			renderer_data.pause_display = 0;
			break;
		case XK_1:
			renderer_data.desync = 1;
			renderer_data.halfspeed = 0;
			renderer_data.doublespeed = 0;
			renderer_data.pause_display = 0;
			break;
		case XK_2:
			renderer_data.desync = 1;
			renderer_data.halfspeed = 0;
			renderer_data.doublespeed = 2;
			renderer_data.pause_display = 0;
			break;
		case XK_3:
			renderer_data.desync = 1;
			renderer_data.halfspeed = 0;
			renderer_data.doublespeed = 3;
			renderer_data.pause_display = 0;
			break;
		case XK_4:
			renderer_data.desync = 1;
			renderer_data.halfspeed = 0;
			renderer_data.doublespeed = 4;
			renderer_data.pause_display = 0;
			break;
		case XK_5:
			renderer_data.desync = 1;
			renderer_data.halfspeed = 0;
			renderer_data.doublespeed = 5;
			renderer_data.pause_display = 0;
			break;
		case XK_6:
			renderer_data.desync = 1;
			renderer_data.halfspeed = 0;
			renderer_data.doublespeed = 6;
			renderer_data.pause_display = 0;
			break;
		case XK_7:
			renderer_data.desync = 1;
			renderer_data.halfspeed = 0;
			renderer_data.doublespeed = 7;
			renderer_data.pause_display = 0;
			break;
		case XK_8:
			renderer_data.desync = 1;
			renderer_data.halfspeed = 0;
			renderer_data.doublespeed = 8;
			renderer_data.pause_display = 0;
			break;
		case XK_0:
			renderer_data.desync = 1;
			renderer_data.pause_display = 1;
			break;
		}
}

static void
HandleEventX11(const XEvent *ev)
{
#if 0
	printf("event %d\n", ev->type);
#endif
	if (ev->type == Expose) {
		const XExposeEvent *xev = (const XExposeEvent *)&ev;

		if (!xev->count) {
			renderer_data.needsrefresh = 1;
		}
	} else if (ev->type == ConfigureNotify) {
		const XConfigureEvent *ce = (const XConfigureEvent *)ev;

		if (window_w != ce->width
		 || window_h != ce->height) {
			window_w = ce->width;
			window_h = ce->height;
			XClearWindow(display, window);
		}
	} else if (ev->type == DestroyNotify) {
		quit();
	} else if (ev->type == MapNotify) {
		mapped = 1;
	} else if (ev->type == UnmapNotify) {
		mapped = 0;
#ifdef PAUSE_ON_DEFOCUS
	} else if (ev->type == FocusIn) {
		renderer_data.desync = 1;
		renderer_data.pause_display = 0;
	} else if (ev->type == FocusOut) {
		renderer_data.desync = 1;
		renderer_data.pause_display = 1;
#endif
	} else if (ev->type == KeyPress || ev->type == KeyRelease) {
		HandleKeyboardX11(ev);
	} else if (ev->type == KeymapNotify) {
		controller[0] = controller[1] = 0;
		controllerd[0] = controllerd[1] = 0;
#ifdef HAVE_SHM
	} else if (shm_attached && ev->type == shm_first_event + ShmCompletion) {
		shm_incomplete = 0;
#endif
	}
}

static void
RedrawImageX11(void)
{
	drawimage(PBL);

	switch (renderer_config.scaler_magstep) {
	case 2:
		hq2x_32_rb((uint32_t *)fb, bytes_per_line, (uint32_t *)image->data, image->bytes_per_line, 256, 240);
		break;
	case 3:
		hq3x_32_rb((uint32_t *)fb, bytes_per_line, (uint32_t *)image->data, image->bytes_per_line, 256, 240);
		break;
	case 4:
		hq4x_32_rb((uint32_t *)fb, bytes_per_line, (uint32_t *)image->data, image->bytes_per_line, 256, 240);
		break;
	}

	renderer_data.needsrefresh = 1;
	renderer_data.redrawbackground = 0;
	renderer_data.redrawall = 0;
}

static void
RefreshImageX11(void)
{
	Drawable target;
	int sx = 0, sy = 0;
	int dx = 0, dy = 0;
	unsigned int w = image->width, h = image->height;

#ifdef HAVE_XRENDER
	if (renderer_config.magstep > renderer_config.scaler_magstep) {
		target = scalePixmap;
	} else
#endif
	{
		target = window;
		dx = (window_w - w) / 2;
		dy = (window_h - h) / 2;
	}

#ifdef HAVE_SHM
	if (shm_attached) {
		XShmPutImage(display, target, gc, image, sx, sy, dx, dy, w, h, True);
		/* hang the event loop until we get a ShmCompletion */
		shm_incomplete = 1;
	} else
#endif
	{
		XPutImage(display, target, gc, image, sx, sy, dx, dy, w, h);
		XFlush(display);
	}

#ifdef HAVE_XRENDER
	if (renderer_config.magstep > renderer_config.scaler_magstep) {
		w = 256 * renderer_config.magstep;
		h = 240 * renderer_config.magstep;
		dx = (window_w - w) / 2;
		dy = (window_h - h) / 2;
		XRenderComposite(display, PictOpSrc, scalePicture, None, windowPicture, 0,0, 0,0, dx, dy, w, h);
	}
#endif

	renderer_data.needsrefresh = 0;
}

void
UpdateDisplayX11(void)
{
	struct timeval time;
	static unsigned int frame;
	unsigned int timeframe;
#ifdef HAVE_SCRNSAVER
	static int sssuspend = 0;
#endif

	if (!frameskip) {
		RedrawImageX11();
		if (mapped) {
			RefreshImageX11();
		}
	}

	/* Check the time.  If we're getting behind, skip next frame to stay in sync. */
	gettimeofday(&time, NULL);
	timeframe = (time.tv_sec - renderer_data.basetime) * 50 + time.tv_usec / 20000;     /* PAL */
	timeframe = (time.tv_sec - renderer_data.basetime) * 60 + time.tv_usec / 16666;     /* NTSC */
	frame++;
	if (renderer_data.halfspeed)
		timeframe >>= 1;
	else if (renderer_data.doublespeed)
		timeframe *= renderer_data.doublespeed;
	if (renderer_data.desync) {
		renderer_data.desync = 0;
		frame = timeframe;
	}
	frameskip = frame < timeframe;
	if (frameskip && frame % 20 == 0) {
		frameskip = 0;
		if (frame < timeframe - 20) {
			/* If we're more than 20 frames behind, might as well stop counting. */
			renderer_data.desync = 1;
		}
	}

	/* Slow down if we're getting ahead */
	if (frame > timeframe + 1) {
		usleep(16666 * (frame - timeframe - 1));
	}

	/* Input loop */
	struct pollfd fds[] = {
		{ .fd = jsfd[0], .events = POLLIN, },
		{ .fd = jsfd[1], .events = POLLIN, },
		{ .fd = ConnectionNumber(display), .events = POLLIN, },
	};
	int nready;
	do {
		if (XPending(display)) {
			fds[0].revents = 0;
			fds[1].revents = 0;
			fds[2].revents = POLLIN;
			nready = 1;
		} else {
			nready = poll(fds, 3, renderer_data.pause_display || shm_incomplete ? -1 : 0);
		}
		if (nready < 0) {
			if (errno != EINTR && errno != EAGAIN) {
				perror("poll");
				exit(EXIT_FAILURE);
			}
		} else if (nready > 0) {
			if (fds[0].revents)
				fds[0].fd = js_handle_input(0);
			if (fds[1].revents)
				fds[1].fd = js_handle_input(1);
			if (fds[2].revents) {
				static XEvent ev;
				XNextEvent(display, &ev);
				HandleEventX11(&ev);
			}
		}

		if (renderer_data.needsrefresh && renderer_data.pause_display && !shm_incomplete && mapped) {
			RefreshImageX11();
		}

#ifdef HAVE_SCRNSAVER
		if (sssuspend && (!mapped || renderer_data.pause_display)) {
			sssuspend = 0;
			XScreenSaverSuspend(display, 0);
		}
#endif
	} while (nready);

#ifdef HAVE_SCRNSAVER
	if (!sssuspend && mapped) {
		sssuspend = 1;
		XScreenSaverSuspend(display, 1);
	}
#endif
}

/* Update the colors on the screen if the palette changed */
void
UpdateColorsX11(void)
{
	static unsigned char palette_cache[32];

	if (renderer_config.indexedcolor) {
		/* Background color */
		if (VRAM[0x3f00] != palette_cache[0]) {
			unsigned int palcolor = NES_palette[VRAM[0x3f00] & 0x3f];
			XColor color;
			color.pixel = colortableX11[24];
			color.flags = DoRed | DoGreen | DoBlue;
			color.red    = palcolor >> 16 & 0xff;
			color.green  = palcolor >>  8 & 0xff;
			color.blue   = palcolor       & 0xff;
			color.red   |= color.red   << 8;
			color.green |= color.green << 8;
			color.blue  |= color.blue  << 8;
			XStoreColor(display, colormap, &color);
		}

		/* Tile colors */
		for (int x = 0; x < 24; x++) {
			if (VRAM[0x3f01 + x + (x / 3)] != palette_cache[1 + x + (x / 3)]) {
				unsigned int palcolor = NES_palette[VRAM[0x3f01 + x + (x / 3)] & 0x3f];
				XColor color;
				color.pixel = colortableX11[x];
				color.flags = DoRed | DoGreen | DoBlue;
				color.red    = palcolor >> 16 & 0xff;
				color.green  = palcolor >>  8 & 0xff;
				color.blue   = palcolor       & 0xff;
				color.red   |= color.red   << 8;
				color.green |= color.green << 8;
				color.blue  |= color.blue  << 8;
				XStoreColor(display, colormap, &color);
#if 0
				printf("color %d (%lu) = %06x\n", x, color.pixel, palcolor);
#endif
			}
		}
	} else /* truecolor */ {
		if (VRAM[0x3f00] != palette_cache[0]) {
			renderer_data.redrawbackground = 1;
		}

		/* Set palette tables */
		palette[24] = paletteX11[VRAM[0x3f00] & 0x3f];
		for (int x = 0; x < 24; x++) {
			palette[x] = paletteX11[VRAM[0x3f01 + x + (x / 3)] & 0x3f];
		}
	}

	memcpy(palette_cache, &VRAM[0x3f00], 32);
}

#endif /* HAVE_X */
