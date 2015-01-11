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

int sticky_keys = 0;
int swap_inputs = 0;

/* primary controller status vectors */
unsigned int controller[2] = { 0, 0 };
/* secondary (diagonal key and joystick "hat") controller status vectprs */
unsigned int controllerd[2] = { 0, 0 };
unsigned int coinslot = 0, dipswitches = 0;


void
ctl_button(int stick, unsigned char mask, _Bool value)
{
	if (swap_inputs)
		stick = !stick;
	if (value) {
		controller[stick] |= mask;
	} else {
		controller[stick] &= ~mask;
	}
}


void
ctl_keypress(int stick, unsigned char mask, _Bool value)
{
	if (swap_inputs)
		stick = !stick;
	if (sticky_keys) {
		if (value)
			controller[stick] ^= mask;
	} else if (value) {
		controller[stick] |= mask;
	} else {
		controller[stick] &= ~mask;
	}
}


void
ctl_keypress_diag(int stick, unsigned char mask, _Bool value)
{
	if (swap_inputs)
		stick = !stick;
	if (sticky_keys) {
		if (value)
			controllerd[stick] ^= mask;
	} else if (value) {
		controllerd[stick] |= mask;
	} else {
		controllerd[stick] &= ~mask;
	}
}


void
ctl_coinslot(unsigned char mask, _Bool value)
{
	if (sticky_keys) {
		if (value)
			coinslot ^= mask;
	} else if (value) {
		coinslot |= mask;
	} else {
		coinslot &= ~mask;
	}
}


void
ctl_dipswitch(unsigned char mask, _Bool value)
{
	if (value)
		dipswitches ^= mask;
}
