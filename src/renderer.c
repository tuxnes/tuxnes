// SPDX-FileCopyrightText: Authors of TuxNES
// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Description: This file provides the "auto" and "none"
 * pseudo-renderers.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <poll.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "consts.h"
#include "globals.h"
#include "joystick.h"
#include "renderer.h"

#ifdef HAVE_X
extern int      InitDisplayX11(int argc, char **argv);
extern void     UpdateColorsX11(void);
extern void     UpdateDisplayX11(void);
#endif

/* exports */
int     InitDisplayAuto(int argc, char **argv);
int     InitDisplayNone(int argc, char **argv);
void    UpdateColorsNone(void);
void    UpdateDisplayNone(void);

/* imports */
extern void     fbinit(void);

/* globals */
struct Renderer renderers[] = {
#ifdef HAVE_X
	{ "x11", "X11 renderer",
	  InitDisplayX11, UpdateDisplayX11, UpdateColorsX11 },
#endif /* HAVE_X */
	{ "auto", "Choose one automatically",
	  InitDisplayAuto, 0, 0 },
	{ "none", "Don't draw anything",
	  InitDisplayNone, UpdateDisplayNone, UpdateColorsNone },
	{ 0, 0, 0, 0, 0 }     /* terminator */
}, *renderer = 0;

struct RendererConfig renderer_config = {
	.geometry = NULL,
	.display_id = NULL,
	.inroot = 0,
	.indexedcolor = 1,
	.scaler_magstep = 1,
	.magstep = 1,
};

/* global renderer data */
struct RendererData renderer_data = {
	.basetime = 0,
	.pause_display = 0,  /* Initially we start with the emulation running.  If you want to start paused, change this. */
	.halfspeed = 0,
	.doublespeed = 0,
	.desync = 1,
	.needsredraw = 1,        /* Refresh screen display */
	.redrawbackground = 1,   /* Redraw tile background */
	.redrawall = 1,          /* Redraw all scanlines */
};

int
InitDisplayAuto(int argc, char **argv)
{
	const char *rendname = 0;

	if (getenv("DISPLAY")) {
#ifdef HAVE_X
		rendname = "x11";
#endif
	}
	if (!rendname) {
		rendname = "none";
		fprintf(stderr,
		        "======================================================\n"
		        "Warning: No Suitable Renderer Detected\n"
		        "\n"
		        "The `%s' fall-back renderer will be used.\n"
		        "\n"
		        "To avoid this in the future, install additional\n"
		        "libraries and recompile, or use the --renderer=...\n"
		        "option to specify a renderer explicitly.\n"
		        "\n"
		        "The option --help=options lists available renderers.\n"
		        "======================================================\n",
		        rendname);
	}
	for (renderer = renderers; renderer->name; renderer++)
		if (!strcmp(renderer->name, rendname))
			break;
	if (!renderer) {
		fprintf(stderr, "%s: unrecognized renderer name `%s'\n",
		        *argv, rendname);
		return 1;
	}
	return renderer->InitDisplay(argc, argv);
}

int
InitDisplayNone(int argc, char **argv)
{
	struct timeval time;

	fbinit();
	gettimeofday(&time, NULL);
	renderer_data.basetime = time.tv_sec;
	return 0;
}

void
UpdateDisplayNone(void)
{
	struct timeval time;
	static unsigned int frame;
	unsigned int timeframe;

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

	/* Slow down if we're getting ahead */
	if (frame > timeframe + 1 && frameskip == 0) {
		usleep(16666 * (frame - timeframe - 1));
	}

	/* Input loop */
	struct pollfd fds[] = {
		{ .fd = jsfd[0], .events = POLLIN, },
		{ .fd = jsfd[1], .events = POLLIN, },
	};
	int nready;
	do {
		nready = poll(fds, 2, renderer_data.pause_display ? -1 : 0);
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
		}
	} while (nready);

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
UpdateColorsNone(void)
{
	/* no-op */
}
