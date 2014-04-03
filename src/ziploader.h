/*
 * This file is part of the TuxNES project codebase.
 *
 * Please see the README and COPYING files for more information regarding
 * this project.
 *
 * $Id: ziploader.h,v 1.2 2001/04/11 21:45:48 tmmm Exp $
 *
 * Description: This file contains the function prototype for the
 * ziploader.
 */

#ifndef _ZIPLOADER_H_
#define _ZIPLOADER_H_

#include "unzip.h"

int ziploader(unzFile file, char *zipname);

#endif
