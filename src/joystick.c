/*
 * This file is part of the TuxNES project codebase.
 *
 * Please see the README and COPYING files for more information regarding
 * this project.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "controller.h"
#include "globals.h"
#include "joystick.h"
#include "renderer.h"

#include <ctype.h>
#include <stdio.h>
#include <stdbool.h>

#ifdef HAVE_LINUX_JOYSTICK_H
#include <linux/joystick.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#endif /* HAVE_LINUX_JOYSTICK_H */


/* joystick dead-zone */
#define JS_IGNORE 8192

/* joystick mapping */
#define JS_MAX_BUTTONS     32
#define JS_MAX_AXES        8
#define JS_MAX_NES_BUTTONS 9
#define JS_MAX_NES_STICKS  2

#define PAUSEDISPLAY 0xff

const char *jsdevice[JS_MAX_NES_STICKS] = { NULL, NULL };
static int jsfd[JS_MAX_NES_STICKS] = { -1, -1 };


struct js_nesmap {
	unsigned char button[JS_MAX_BUTTONS];
	struct {
		unsigned char neg;
		unsigned char pos;
	} axis[JS_MAX_AXES];
};

#define JS_DEFAULT_MAP { \
	{ \
		BUTTONB, SELECTBUTTON, BUTTONA, STARTBUTTON, \
		BUTTONA, BUTTONB, SELECTBUTTON, STARTBUTTON, \
		PAUSEDISPLAY, 0, 0, 0, \
		0, 0, 0, 0, \
		0, 0, 0, 0, \
		0, 0, 0, 0, \
		0, 0, 0, 0, \
		0, 0, 0, 0, \
	}, { \
		{ LEFT, RIGHT }, { UP, DOWN }, { 0, 0 }, { 0, 0 }, \
		{ LEFT, RIGHT }, { UP, DOWN }, { 0, 0 }, { 0, 0 }, \
	} \
}

static struct js_nesmap js_nesmaps[JS_MAX_NES_STICKS] = { JS_DEFAULT_MAP, JS_DEFAULT_MAP };
static bool js_usermapped2button[JS_MAX_NES_STICKS][2] = { { false, false }, { false, false } };


static void
assign_button(int stick, int js_button, unsigned char nes_button)
{
	if (js_button < 2)
		js_usermapped2button[stick][js_button] = true;
	if (js_button < JS_MAX_BUTTONS)
		js_nesmaps[stick].button[js_button] = nes_button;
}


static void
assign_axis(int stick, int js_axis, unsigned char nes_button_neg, unsigned char nes_button_pos)
{
	if (js_axis < JS_MAX_AXES) {
		js_nesmaps[stick].axis[js_axis].neg = nes_button_neg;
		js_nesmaps[stick].axis[js_axis].pos = nes_button_pos;
	}
}


static const char *
parse_mapspec(const char *s, int stick)
{
	static const unsigned char sequence[JS_MAX_NES_BUTTONS] = {
		BUTTONA,
		BUTTONB,
		STARTBUTTON,
		SELECTBUTTON,
		LEFT,
		RIGHT,
		UP,
		DOWN,
		PAUSEDISPLAY,
	};

	int sequence_i = 0;
	int sequence_inc = 1;

	while (*s) {
		if (*s == '+') {
			return s + 1;

		} else if (*s == ',') {
			sequence_i += sequence_inc;
			if (sequence_i >= JS_MAX_NES_BUTTONS)
				return s + 1;
			sequence_inc = 1;

		} else if ((*s == 'b' || *s == 'B') && isdigit(*(s + 1))) {
			int index = 0;
			do {
				++s;
				index *= 10;
				index += *s - '0';
			} while (isdigit(*(s + 1)));
			assign_button(stick, index, sequence[sequence_i]);

		} else if ((*s == 'a' || *s == 'A') && isdigit(*(s + 1))) {
			int index = 0;
			do {
				++s;
				index *= 10;
				index += *s - '0';
			} while (isdigit(*(s + 1)));
			sequence_inc = 2;
			if ((sequence_i + 1) < JS_MAX_NES_BUTTONS) {
				assign_axis(stick, index, sequence[sequence_i], sequence[sequence_i + 1]);
			}
		}

		++s;
	}

	return s;
}


void
js_set_nesmaps(const char *s)
{
	while (*s) {
		if (isdigit(*s) && *(s + 1) == ':') {
			int stick = *s - '1';
			s += 2;
			if (stick < 0 || stick > JS_MAX_NES_STICKS) {
				fprintf(stderr, "ignoring joystick map (limited to joystick 1 and 2)\n");
				continue;
			}
			s = parse_mapspec(s, stick);
		} else {
			++s;
		}
	}
}


static void
stick_open(int stick)
{
#ifdef HAVE_LINUX_JOYSTICK_H
	unsigned char axes = 2;
	unsigned char buttons = 2;
	int version = 0x000800;

	if ((jsfd[stick] = open(jsdevice[stick], O_RDONLY)) < 0) {
		perror(jsdevice[stick]);
		return;
	}

	ioctl(jsfd[stick], JSIOCGVERSION, &version);
	ioctl(jsfd[stick], JSIOCGAXES,    &axes);
	ioctl(jsfd[stick], JSIOCGBUTTONS, &buttons);

	if (verbose) {
		fprintf(stderr,
		        "Joystick %d (%s) has %d axes and %d buttons. Driver version is %d.%d.%d.\n",
		        stick + 1, jsdevice[stick], axes, buttons,
		        (version >> 16),
		        (version >>  8) & 0xff,
		        (version      ) & 0xff);
	}

	/* set joystick to non-blocking */
	fcntl(jsfd[stick], F_SETFL, O_NONBLOCK);

	/* modify button map for a 2-button joystick */
	if (buttons == 2) {
		if (!js_usermapped2button[stick][0])
			js_nesmaps[stick].button[0] = BUTTONB;
		if (!js_usermapped2button[stick][1])
			js_nesmaps[stick].button[1] = BUTTONA;
	}
#else
	fprintf(stderr,
	        "Joystick support was disabled at compile-time. To enable it, make sure\n"
	        "you are running a recent version of the Linux kernel with the joystick\n"
	        "driver enabled, and install the header file <linux/joystick.h>.\n");
#endif /* HAVE_LINUX_JOYSTICK_H */
}


void
js_init(void)
{
	for (int stick = 0; stick < JS_MAX_NES_STICKS; ++stick) {
		if (jsdevice[stick]) {
			stick_open(stick);
		}
	}
}


#ifdef HAVE_LINUX_JOYSTICK_H
static void
stick_read(int stick)
{
	struct js_event js;

	while (read(jsfd[stick], &js, sizeof (struct js_event)) == sizeof (struct js_event)) {
#if 0
		/* verbose joystick reporting */
		if (verbose) {
			fprintf(stderr, "%s: type %d number %d value %d\n",
			        jsdevice[stick], js.type, js.number, js.value);
		}
#endif

		switch (js.type) {
		case 1:  /* button report */
			{
				unsigned char nes_button = js_nesmaps[stick].button[js.number];
				if (nes_button == PAUSEDISPLAY) {
					if (js.value) {
						renderer_data.pause_display = !renderer_data.pause_display;
					}
				} else {
					ctl_button(stick, nes_button, js.value);
				}
			}
			break;

		case 2:  /* axis report */
			{
				unsigned char nes_button_neg = js_nesmaps[stick].axis[js.number].neg;
				unsigned char nes_button_pos = js_nesmaps[stick].axis[js.number].pos;
				ctl_button(stick, nes_button_neg, js.value < -JS_IGNORE);
				ctl_button(stick, nes_button_pos, js.value >  JS_IGNORE);
			}
			break;
		}
	}

	if (errno != EAGAIN) {
		if (errno) {
			perror(jsdevice[stick]);
		} else {
			fprintf(stderr, "%s: device violates joystick protocol, disabling\n",
			        jsdevice[stick]);
			close(jsfd[stick]);
			jsfd[stick] = -1;
		}
	}
}
#endif /* HAVE_LINUX_JOYSTICK_H */


void
js_handle_input(void)
{
#ifdef HAVE_LINUX_JOYSTICK_H
	for (int stick = 0; stick < JS_MAX_NES_STICKS; ++stick) {
		if (jsfd[stick] >= 0) {
			stick_read(stick);
		}
	}
#endif /* HAVE_LINUX_JOYSTICK_H */
}
