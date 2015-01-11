/*
 * This file is part of the TuxNES project codebase.
 *
 * Please see the README and COPYING files for more information regarding
 * this project.
 *
 * Description: This file contains the function prototype for the
 * ziploader.
 */

#ifndef ZIPLOADER_H
#define ZIPLOADER_H

#include "unzip.h"

int ziploader(unzFile file, char *zipname);

#endif
