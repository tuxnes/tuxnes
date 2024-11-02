// SPDX-FileCopyrightText: Authors of TuxNES
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef JOYSTICK_H
#define JOYSTICK_H

extern const char *jsdevice[2];
extern int jsfd[2];

void js_set_nesmaps(const char *);
void js_init(void);
int js_handle_input(int);

#endif
