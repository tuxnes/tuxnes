// SPDX-FileCopyrightText: Authors of TuxNES
// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Description: This file handles the I/O subsystem for the X window
 * system.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <hqx.h>

#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <signal.h>
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
#include "mapper.h"
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

/* exported functions */
int     InitDisplayX11(int argc, char **argv);
void    UpdateColorsX11(void);
void    UpdateDisplayX11(void);

/* X11 stuff: */
static Display  *display;
static int      (*oldhandler)(Display *, XErrorEvent *) = 0;
static Window    window;
static unsigned int window_w, window_h;
static Colormap colormap;

static unsigned int paletteX11[64];

static unsigned char    *keystate[32];
static GC       gc, blackgc;
static GC       backgroundgc, solidbggc;
static GC       bgcolorgc;
static unsigned long    colortableX11[25];

#ifdef HAVE_XRENDER
static Pixmap scalePixmap;
static Picture scalePicture, windowPicture;
#endif
static XImage   *image = 0;

/* externel and forward declarations */
void    fbinit(void);
void    quit(void);
void    START(void);

#ifdef HAVE_SHM

static Status shm_status = 0;
static int shm_major, shm_minor;
static Bool shm_pixmaps;
static XShmSegmentInfo shminfo;
static Status shm_attached = 0;
static int shm_attaching = 0;
static XImage *shm_image = 0;

static void
cleanup_shm(void)
{
	if (shminfo.shmid >= 0) {
		if (shm_attached)
			XShmDetach(display, &shminfo);
		if (shm_image) {
			XDestroyImage(shm_image);
			shm_image = 0;
		}
		if (shminfo.shmaddr)
			shmdt(shminfo.shmaddr);
		shmctl(shminfo.shmid, IPC_RMID, 0);
		shminfo.shmid = -1;
	}
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

static int
handler(Display *display, XErrorEvent *ev)
{
#ifdef HAVE_SHM
	if (shm_attaching
	 && (ev->error_code == BadAccess)
	/* && (ev->request_code == Find_the_request_code_for_MIT_SHM) */
	 && (ev->minor_code == X_ShmAttach)) {
		shm_attached = False;
		return 0;
	}
#endif
	if (oldhandler) {
		return oldhandler(display, ev);
	}
	return 0;
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
	switch (renderer_config.scaler_magstep) {
	case 1:
	case 2:
	case 3:
	case 4:
		break;
	default:
		fprintf(stderr,
			"[%s] Scale factor %d is not valid for HQX\n",
			renderer->name, renderer_config.scaler_magstep);
		renderer_config.scaler_magstep = renderer_config.scaler_magstep > 1 ? 2 : 1;
	}
	if (renderer_config.magstep > maxsize) {
		fprintf(stderr,
		        "[%s] Enlargement factor %d is too large!\n",
		        renderer->name, renderer_config.magstep);
		renderer_config.magstep = maxsize;
	}
	display = XOpenDisplay(renderer_config.display_id);
	oldhandler = XSetErrorHandler(handler);
	if (display == NULL) {
		fprintf(stderr,
		        "%s: [%s] Can't open display: %s\n",
		        argv[0],
		        renderer->name,
		        XDisplayName(renderer_config.display_id));
		exit(EXIT_FAILURE);
	}
	int screen = XDefaultScreen(display);
#ifdef HAVE_SHM
	shm_status = XShmQueryVersion(display, &shm_major, &shm_minor, &shm_pixmaps);
#endif /* HAVE_SHM */
	Visual *visual = XDefaultVisual(display, screen);
	Window rootwindow = RootWindow(display, screen);
	colormap = DefaultColormap(display, screen);
	unsigned long black = BlackPixel(display, screen);
	unsigned int depth = DefaultDepth(display, screen);
#if BYTE_ORDER == BIG_ENDIAN
	pix_swab = (ImageByteOrder(display) != MSBFirst);
#else
	pix_swab = (ImageByteOrder(display) == MSBFirst);
#endif
	lsb_first = (BitmapBitOrder(display) == LSBFirst);
	lsn_first = lsb_first; /* who knows? packed 4bpp is really an obscure case */
	bpu = BitmapUnit(display);
	bpp = depth;
	if (depth > 1) {
		int formats = 0;
		XPixmapFormatValues *xpfv = XListPixmapFormats(display, &formats);

		for (int i = 0; i < formats; i++)
			if (xpfv[i].depth == depth) {
				bpp = xpfv[i].bits_per_pixel;
				break;
			}
	}
	if (renderer_config.scaler_magstep != 1 & bpp != 32) {
		fprintf(stderr,
			"[%s] HQX will only work at 32bpp; disabling\n",
			renderer->name);
		renderer_config.scaler_magstep = 1;
	}
	if (renderer_config.scaler_magstep != 1) {
		hqxInit();
	}
	if (renderer_config.scaler_magstep > renderer_config.magstep) {
		renderer_config.magstep = renderer_config.scaler_magstep;
	}
	if ((BYTE_ORDER != BIG_ENDIAN) && (BYTE_ORDER != LITTLE_ENDIAN)
	 && (bpp == 32)) {
		fprintf(stderr,
		        "======================================================\n"
		        "Warning: %s-endian Host Detected\n"
		        "\n"
		        "This host may not handle 32bpp display properly.\n"
		        "======================================================\n",
		        (BYTE_ORDER == PDP_ENDIAN) ? "PDP" : "Obscure");
	}
	if ((visual->class & 1) && renderer_config.indexedcolor) {
		if (XAllocColorCells(display, colormap, 0, 0, 0, colortableX11, 25) == 0) {
			fprintf(stderr, "%s: [%s] Can't allocate colors!\n",
			        *argv, renderer->name);
			exit(EXIT_FAILURE);
		}
		/* Pre-initialize the colormap to known values */
		XColor color;
		color.red   = NES_palette[0] >> 8 & 0xff00;
		color.green = NES_palette[0]      & 0xff00;
		color.blue  = NES_palette[0] << 8 & 0xff00;
		color.flags = DoRed | DoGreen | DoBlue;
		for (int x = 0; x <= 24; x++) {
			color.pixel = colortableX11[x];
			palette[x] = color.pixel;
			XStoreColor(display, colormap, &color);
		}
	} else {
		renderer_config.indexedcolor = 0;
		/* convert palette to local color format */
		for (int x = 0; x < 64; x++) {
			XColor color;
			color.pixel = x;
			color.red   = NES_palette[x] >> 8 & 0xff00;
			color.green = NES_palette[x]      & 0xff00;
			color.blue  = NES_palette[x] << 8 & 0xff00;
			color.flags = DoRed | DoGreen | DoBlue;
			if (XAllocColor(display, colormap, &color) == 0) {
				fprintf(stderr, "%s: [%s] Can't allocate colors!\n",
				        *argv, renderer->name);
				exit(EXIT_FAILURE);
			}
			paletteX11[x] = color.pixel;
		}
	}
	unsigned int w = 256 * renderer_config.scaler_magstep;
	unsigned int h = 240 * renderer_config.scaler_magstep;
	window_w = 256 * renderer_config.magstep;
	window_h = 240 * renderer_config.magstep;
	int x = 0;
	int y = 0;
	unsigned int border_width = 0;
	int geometry_mask = HeightValue | WidthValue;
	if (renderer_config.geometry)
		geometry_mask |= XParseGeometry(renderer_config.geometry, &x, &y, &window_w, &window_h);
	if (renderer_config.inroot) {
		window = rootwindow;
	} else {
		XSetWindowAttributes attrs;

		attrs.backing_store = WhenMapped;
		attrs.cursor = XCreateFontCursor(display, XC_crosshair);
		window = XCreateWindow(display, rootwindow,
		                       x, y, window_w, window_h, border_width,
		                       /* depth */ CopyFromParent,
		                       /* class */ InputOutput,
		                       /* visual */ visual,
		                       /* valuemask */ CWBackingStore | CWCursor,
		                       /* attributes */ &attrs);
	}
	XGetGeometry(display, window, &rootwindow,
	             &x, &y, &window_w, &window_h, &border_width, &depth);
	gc = XCreateGC(display, window, 0, 0);
	blackgc = XCreateGC(display, window, 0, 0);
	XSetForeground(display, gc, ~0UL);
	XSetBackground(display, gc, 0UL);
	XSetForeground(display, blackgc, 0);
	XSetBackground(display, blackgc, 0);
	XGCValues GCValues;
	GCValues.function = GXor;
	bgcolorgc = XCreateGC(display, window, GCFunction, &GCValues);
	XSetBackground(display, bgcolorgc, black);

	if (!renderer_config.inroot) {
		/* set aspect ratio */
		XSizeHints sizehints;
		/* sizehints.flags = PMinSize | PMaxSize | PResizeInc | PAspect | PBaseSize; */
		/*   sizehints.min_width = 256; */
		/*   sizehints.min_height = 240; */
		/*   sizehints.max_width = 256 * maxsize; */
		/*   sizehints.max_height = 240 * maxsize; */
		/*   sizehints.width_inc = 256; */
		/*   sizehints.height_inc = 240; */
		/*   sizehints.min_aspect.x = 256; */
		/*   sizehints.min_aspect.y = 240; */
		/*   sizehints.max_aspect.x = 256; */
		/*   sizehints.max_aspect.y = 240; */
		sizehints.flags = PBaseSize;
		sizehints.base_width = window_w;
		sizehints.base_height = window_h;
		XSetWMNormalHints(display, window, &sizehints);

		/* pass the command line to the window system */
		XSetCommand(display, window, argv, argc);

		/* set window manager hints */
		XWMHints wmhints;
		wmhints.flags = InputHint | StateHint;
		wmhints.input = True;
		wmhints.initial_state = NormalState;
		XSetWMHints(display, window, &wmhints);

		/* set window title */
		XClassHint classhints;
		char *wname[] = {
			PACKAGE_NAME,
			PACKAGE
		};
		XTextProperty name[2];
		classhints.res_class = wname[0];
		classhints.res_name = wname[1];
		XSetClassHint(display, window, &classhints);
		if (!XStringListToTextProperty(wname, 1, name)) {
			fprintf(stderr, "[%s] Can't set window name property\n",
			        renderer->name);
		} else {
			XSetWMName(display, window, name);
		}
		if (!XStringListToTextProperty(wname + 1, 1, name + 1)) {
			fprintf(stderr, "[%s] Can't set icon name property\n",
			        renderer->name);
		} else {
			XSetWMIconName(display, window, name + 1);
		}
	}

	backgroundgc = XCreateGC(display, window, 0, 0);
	XSetBackground(display, backgroundgc, black);
	solidbggc = XCreateGC(display, window, 0, 0);
	XSetBackground(display, solidbggc, black);
	XMapWindow(display, window);
	XSelectInput(display, window, KeyPressMask | KeyReleaseMask | ExposureMask |
	             FocusChangeMask | KeymapStateMask | StructureNotifyMask);
	XFlush(display);

	struct timeval time;
	gettimeofday(&time, NULL);
	renderer_data.basetime = time.tv_sec;
	if (renderer_config.magstep != renderer_config.scaler_magstep) {
		if (!XRenderSupported()) {
			fprintf(stderr, "[%s] Scaling requires XRENDER support\n",
			        renderer->name);
			exit(EXIT_FAILURE);
		}

#ifdef HAVE_XRENDER
		scalePixmap = XCreatePixmap(display, window, w, h, depth);

		XRenderPictFormat *fmt = XRenderFindVisualFormat(display, DefaultVisual(display, DefaultScreen(display)));
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
#endif
	}
#ifdef HAVE_SHM
	if (shm_status == True) {
		shm_image = XShmCreateImage(display, visual, depth,
		                            depth == 1 ? XYBitmap : ZPixmap, NULL, &shminfo,
		                            w, h);
		if (shm_image) {
			shminfo.shmid = shmget(IPC_PRIVATE, shm_image->bytes_per_line * shm_image->height,
			                       IPC_CREAT|0777);
			if (shminfo.shmid < 0) {
				perror("shmget");
			} else {
				shminfo.shmaddr = 0;
				switch (fork()) {
				case -1:
					perror("fork");
					atexit(cleanup_shm);
				case 0:
					break;
				default:
					{
						int status = 0;

						atexit(cleanup_shm);
						while (wait(&status) != -1)
							if (WIFEXITED(status))
								exit(WEXITSTATUS(status));
							else if (WIFSIGNALED(status))
								raise(WTERMSIG(status));
						perror("wait");
						exit(EXIT_FAILURE);
					}
				}
				shminfo.shmaddr =
				shm_image->data = shmat(shminfo.shmid, 0, 0);
				shminfo.readOnly = False;
				XSync(display, False);
				shm_attaching = 1;
				shm_attached = XShmAttach(display, &shminfo);
				XSync(display, False);
				shm_attaching = 0;
				if (shm_attached)
					image = shm_image;
			}
		}
	}
#endif /* HAVE_SHM */
	if (verbose) {
		fprintf(stderr,
		        "[%s] %s visual, depth %u, %ubpp, %s colors, %s %s\n",
		        renderer->name,
		        (visual->class == StaticGray) ? "StaticGray" :
		        (visual->class == GrayScale) ? "GrayScale" :
		        (visual->class == StaticColor) ? "StaticColor" :
		        (visual->class == PseudoColor) ? "PseudoColor" :
		        (visual->class == TrueColor) ? "TrueColor" :
		        (visual->class == DirectColor) ? "DirectColor" :
		        "Unknown",
		        depth, bpp,
		        renderer_config.indexedcolor ? "dynamic" : "static",
		        image ? "shared-memory" : "plain",
		        "XImage");
	}
	if (!image) {
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
	if (renderer_config.scaler_magstep != 1) {
		// Allocate unscaled image buffer
		bytes_per_line = 256 * bpp / 8;
		if (!(rfb = fb = malloc(bytes_per_line * 240))) {
			perror("malloc");
			exit(EXIT_FAILURE);
		}
	} else {
		// No framebuffer scaling (though we might use XRender to upscale)
		// This means fb.c and the XImage share the same buffer.
		bytes_per_line = image->bytes_per_line;
		rfb = fb = image->data;
	}
	XFillRectangle(display, window, blackgc, 0, 0, window_w, window_h);
	InitScreenshotX11();
	fbinit();
	return 0;
}

static void
HandleKeyboardX11(XEvent ev)
{
	KeySym keysym = XkbKeycodeToKeysym(display, ev.xkey.keycode, 0, 0);
	bool press = ev.type == KeyPress;

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
			renderer_data.pause_display = !renderer_data.pause_display;
			break;
		case XK_grave:
			renderer_data.halfspeed = 1;
			renderer_data.doublespeed = 0;
			renderer_data.pause_display = 0;
			break;
		case XK_1:
			renderer_data.halfspeed = 0;
			renderer_data.doublespeed = 0;
			renderer_data.pause_display = 0;
			break;
		case XK_2:
			renderer_data.halfspeed = 0;
			renderer_data.doublespeed = 2;
			renderer_data.pause_display = 0;
			break;
		case XK_3:
			renderer_data.halfspeed = 0;
			renderer_data.doublespeed = 3;
			renderer_data.pause_display = 0;
			break;
		case XK_4:
			renderer_data.halfspeed = 0;
			renderer_data.doublespeed = 4;
			renderer_data.pause_display = 0;
			break;
		case XK_5:
			renderer_data.halfspeed = 0;
			renderer_data.doublespeed = 5;
			renderer_data.pause_display = 0;
			break;
		case XK_6:
			renderer_data.halfspeed = 0;
			renderer_data.doublespeed = 6;
			renderer_data.pause_display = 0;
			break;
		case XK_7:
			renderer_data.halfspeed = 0;
			renderer_data.doublespeed = 7;
			renderer_data.pause_display = 0;
			break;
		case XK_8:
			renderer_data.halfspeed = 0;
			renderer_data.doublespeed = 8;
			renderer_data.pause_display = 0;
			break;
		case XK_0:
			renderer_data.pause_display = 1;
			break;
		}
}

static void
RenderImage(XEvent *ev)
{
	Drawable target;
	int sx = 0, sy = 0;
	int dx = 0, dy = 0;
	unsigned int w = image->width, h = image->height;

#ifdef HAVE_XRENDER
	if (renderer_config.magstep != renderer_config.scaler_magstep) {
		target = scalePixmap;
	} else
#endif
	{
		target = window;
		dx = (window_w - w) / 2;
		dy = (window_h - h) / 2;
	}

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

#ifdef HAVE_SHM
	if (shm_attached) {
		XShmPutImage(display, target, gc, image, sx, sy, dx, dy, w, h, True);
		/* hang the event loop until we get a ShmCompletion */
		ev->type = -1;
	} else
#endif
	{
		XPutImage(display, target, gc, image, sx, sy, dx, dy, w, h);
		XFlush(display);
	}

#ifdef HAVE_XRENDER
	if (renderer_config.magstep != renderer_config.scaler_magstep) {
		w = 256 * renderer_config.magstep;
		h = 240 * renderer_config.magstep;
		dx = (window_w - w) / 2;
		dy = (window_h - h) / 2;
		XRenderComposite(display, PictOpSrc, scalePicture, None, windowPicture, 0,0, 0,0, dx, dy, w, h);
	}
#endif
}

void
UpdateDisplayX11(void)
{
	static XEvent ev;
	struct timeval time;
	static unsigned int frame;
	unsigned int timeframe;
	static int nodisplay = 0;
#ifdef HAVE_SCRNSAVER
	static int sssuspend = 0;
#endif

	/* Check the time.  If we're getting behind, skip a frame to stay in sync. */
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
	} else if (frame < timeframe - 20 && frame % 20 == 0) {
		/* If we're more than 20 frames behind, might as well stop counting. */
		renderer_data.desync = 1;
	}

	if (!nodisplay) {
		drawimage(PBL);
		if (!frameskip) {
			UpdateColorsX11();
			RenderImage(&ev);
			renderer_data.redrawall = renderer_data.needsredraw = 0;
		}
	}

	/* Slow down if we're getting ahead */
	if (frame > timeframe + 1 && frameskip == 0) {
		usleep(16666 * (frame - timeframe - 1));
	}

	/* Input loop */
	do {
		/* Handle joystick input */
		js_handle_input();

		/* Handle X input */
		while (XPending(display) || ev.type == -1) {
			XNextEvent(display, &ev);
			/*printf("event %d\n", ev.type); */
			if (ev.type == DestroyNotify)
				quit();
			/* if you'd like defocusing to pause emulation, define this */
#ifdef PAUSE_ON_DEFOCUS
			if (ev.type == FocusIn)
				renderer_data.pause_display = 0;
			if (ev.type == FocusOut)
				renderer_data.pause_display = 1;
#endif
			if (ev.type == MapNotify) {
				XExposeEvent *xev = (XExposeEvent *)&ev;

				xev->type = Expose;
				xev->count = 0;
				nodisplay = 0;
				renderer_data.needsredraw = renderer_data.redrawall = 1;
			} else if (ev.type == UnmapNotify) {
				nodisplay = 1;
				renderer_data.needsredraw = renderer_data.redrawall = 0;
			}
			/* Handle keyboard input */
			if (ev.type == KeyPress || ev.type == KeyRelease) {
				HandleKeyboardX11(ev);
			}
			if (ev.type == KeymapNotify) {
				memcpy(keystate, ((XKeymapEvent *)&ev)->key_vector, 32);
				controller[0] = controller[1] = 0;
				controllerd[0] = controllerd[1] = 0;
			}
			if (ev.type == ConfigureNotify) {
				XConfigureEvent *ce = (XConfigureEvent *)&ev;

				if (window_w != ce->width
				 || window_h != ce->height) {
					window_w = ce->width;
					window_h = ce->height;
					XFillRectangle(display, window, blackgc, 0, 0, window_w, window_h);
				}
			}
			if (ev.type == Expose) {

				XExposeEvent *xev = (XExposeEvent *)&ev;

				if (!xev->count) {
					RenderImage(&ev);
				}
			}
			if (renderer_data.pause_display && renderer_data.needsredraw) {
				RenderImage(&ev);
				renderer_data.needsredraw = 0;
			}
		}

#ifdef HAVE_SCRNSAVER
		if (renderer_data.pause_display && sssuspend) {
			sssuspend = 0;
			XScreenSaverSuspend(display, 0);
		}
#endif

		if (renderer_data.pause_display) {
			usleep(16666);
			renderer_data.desync = 1;
		}
	} while (renderer_data.pause_display);

#ifdef HAVE_SCRNSAVER
	if (!sssuspend) {
		sssuspend = 1;
		XScreenSaverSuspend(display, 1);
	}
#endif

	renderer_data.needsredraw = 0;
	renderer_data.redrawbackground = 0;
	renderer_data.redrawall = 0;

	/* Check the time.  If we're getting behind, skip next frame to stay in sync. */
	gettimeofday(&time, NULL);
	timeframe = (time.tv_sec - renderer_data.basetime) * 60 + time.tv_usec / 16666;     /* NTSC */
	if (renderer_data.halfspeed)
		timeframe >>= 1;
	else if (renderer_data.doublespeed)
		timeframe *= renderer_data.doublespeed;
	if (frame >= timeframe || frame % 20 == 0)
		frameskip = 0;
	else
		frameskip = 1;
}

/* Update the colors on the screen if the palette changed */
void
UpdateColorsX11(void)
{
	/* Set Background color */
	static unsigned long currentbgcolor;
	unsigned long oldbgcolor = currentbgcolor;
	if (renderer_config.indexedcolor) {
		XColor color;
		color.pixel = currentbgcolor = colortableX11[24];
		color.red   = NES_palette[VRAM[0x3f00] & 63] >> 8 & 0xff00;
		color.green = NES_palette[VRAM[0x3f00] & 63]      & 0xff00;
		color.blue  = NES_palette[VRAM[0x3f00] & 63] << 8 & 0xff00;
		color.flags = DoRed | DoGreen | DoBlue;
		XStoreColor(display, colormap, &color);
		XSetForeground(display, solidbggc, currentbgcolor);
		XSetForeground(display, bgcolorgc, currentbgcolor);
		XSetForeground(display, backgroundgc, currentbgcolor);
	} else /* truecolor */ {
		currentbgcolor = paletteX11[VRAM[0x3f00] & 63];
		palette[24] = currentbgcolor;
		if (oldbgcolor != currentbgcolor) {
			XSetForeground(display, solidbggc, currentbgcolor);
			XSetForeground(display, bgcolorgc, currentbgcolor);
			XSetForeground(display, backgroundgc, currentbgcolor);
			renderer_data.redrawbackground = 1;
			renderer_data.needsredraw = 1;
		}
	}

	/* Tile colors */
	if (renderer_config.indexedcolor) {
		for (int x = 0; x < 24; x++) {
			if (VRAM[0x3f01 + x + (x / 3)] != palette_cache[0][1 + x + (x / 3)]) {
				XColor color;
				color.pixel = colortableX11[x];
				color.red   = NES_palette[VRAM[0x3f01 + x + (x / 3)] & 63] >> 8 & 0xff00;
				color.green = NES_palette[VRAM[0x3f01 + x + (x / 3)] & 63]      & 0xff00;
				color.blue  = NES_palette[VRAM[0x3f01 + x + (x / 3)] & 63] << 8 & 0xff00;
				color.flags = DoRed | DoGreen | DoBlue;
				XStoreColor(display, colormap, &color);
				/*printf("color %d (%d) = %6x\n", x, colortableX11[x], paletteX11[VRAM[0x3f01+x+(x/3)]&63]); */
			}
		}
		memcpy(palette_cache[0], VRAM + 0x3f00, 32);
	}

	/* Set palette tables */
	if (renderer_config.indexedcolor) {
		/* Already done in InitDisplayX11 */
	} else /* truecolor */ {
		for (int x = 0; x < 24; x++) {
			palette[x] = paletteX11[VRAM[0x3f01 + x + (x / 3)] & 63];
		}
	}
}

#endif /* HAVE_X */
