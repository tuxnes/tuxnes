// SPDX-FileCopyrightText: Authors of TuxNES
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef JOYSTICK_H
#define JOYSTICK_H

extern const char *jsdevice[2];

void js_set_nesmaps(const char *);
void js_init(void);
void js_handle_input(void);

#endif
