// SPDX-FileCopyrightText: Authors of TuxNES
// SPDX-License-Identifier: GPL-2.0-or-later

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "globals.h"
#include "screenshot.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>

char *screenshotfile;

static const char *screenshotext = "";
static int screenshotnumber = 0;


void
screenshot_init(const char *ext)
{
	DIR *dir;
	struct dirent *dirp;

	/* Allocate space for (tuxnesdir) + (basefilename-snap-xxxx) + (ext) + ('\0') */
	screenshotfile = (char *)malloc(strlen(tuxnesdir) + strlen(basefilename) + 1 + 4 + 1 + 4 + strlen(ext) + 1);
	if (screenshotfile == NULL) {
		perror("malloc");
		exit(EXIT_FAILURE);
	}
	sprintf(screenshotfile, "%s-snap-", basefilename);

	/* open the screenshot directory */
	dir = opendir(tuxnesdir);
	if (dir == NULL) {
		return;
	}

	/* iterate through the files and establish the starting screenshot number */
	while ((dirp = readdir(dir)) != NULL) {
		if ((strlen(dirp->d_name) >= 8)
		 && (strncmp(dirp->d_name, screenshotfile, strlen(screenshotfile)) == 0)
		 && (strncmp(dirp->d_name + strlen(dirp->d_name) - strlen(ext), ext, strlen(ext)) == 0)) {
			int currentnumber;
			dirp->d_name[strlen(dirp->d_name) - strlen(ext)] = '\0';
			currentnumber = atoi(dirp->d_name + strlen(screenshotfile));
			if (screenshotnumber < currentnumber) {
				screenshotnumber = currentnumber;
			}
		}
	}

	closedir(dir);

	screenshotext = ext;

	if (++screenshotnumber > 9999) {
		screenshotnumber = 0;
	}
}


void
screenshot_new(void)
{
	struct stat buf[1];

	/* make sure we don't over-write screenshots written by a concurrent TuxNES process */
	do {
		sprintf(screenshotfile, "%s%s-snap-%04u%s", tuxnesdir, basefilename, screenshotnumber++, screenshotext);
	} while (!stat(screenshotfile, buf) && screenshotnumber <= 9999);

	if (screenshotnumber > 9999) {
		screenshotnumber = 0;
	}
}
