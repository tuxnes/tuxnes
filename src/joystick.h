/*
 * This file is part of the TuxNES project codebase.
 *
 * Please see the README and COPYING files for more information regarding
 * this project.
 */

#ifndef JOYSTICK_H
#define JOYSTICK_H

extern const char *jsdevice[2];

void js_set_nesmaps(const char *);
void js_init(void);
void js_handle_input(void);

#endif
