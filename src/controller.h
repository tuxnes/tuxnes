/*
 * This file is part of the TuxNES project codebase.
 *
 * Please see the README and COPYING files for more information regarding
 * this project.
 */

#ifndef _CONTROLLER_H_
#define _CONTROLLER_H_

#define BUTTONA      0x01
#define BUTTONB      0x02
#define SELECTBUTTON 0x04
#define STARTBUTTON  0x08
#define UP           0x10
#define DOWN         0x20
#define LEFT         0x40
#define RIGHT        0x80

extern int sticky_keys;
extern int swap_inputs;

extern unsigned int controller[2];
extern unsigned int controllerd[2];
extern unsigned int coinslot, dipswitches;

void ctl_button(int, unsigned char, _Bool);
void ctl_keypress(int, unsigned char, _Bool);
void ctl_keypress_diag(int, unsigned char, _Bool);
void ctl_coinslot(unsigned char, _Bool);
void ctl_dipswitch(unsigned char, _Bool);

#endif
