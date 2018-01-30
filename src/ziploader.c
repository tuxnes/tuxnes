/*
 * This file is part of the TuxNES project codebase.
 *
 * Please see the README and COPYING files for more information regarding
 * this project.
 *
 * Description: Loads a .nes rom from a .zip file.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ziploader.h"
#include "mapper.h"

#include <stdio.h>
#include <string.h>


int
ziploader(unzFile file, char *zipname)
{
	int res;
	int fsize = 0;

	/* Search .zip file and file the ROM file with extension .nes */
	for (res = unzGoToFirstFile(file);
	     res == UNZ_OK;
	     res = unzGoToNextFile(file)) {
		unz_file_info fileinfo;
		char filename[256 + 1];
		const char *extension;

		unzGetCurrentFileInfo(file, &fileinfo, filename, sizeof filename, NULL, 0, NULL, 0);
		filename[sizeof filename - 1] = '\0';
		extension = strrchr(filename, '.');
		if (extension && !strcasecmp(extension, ".nes")) {
			fsize = fileinfo.uncompressed_size;
			break;
		}
	}
	if (res != UNZ_OK || !fsize)
		return 0;

	if (unzOpenCurrentFile(file) != UNZ_OK) {
		fprintf(stderr, "Error in zip file\n");
		return 0;
	}

	res = unzReadCurrentFile(file, ROM, fsize);
	if (unzCloseCurrentFile(file) == UNZ_CRCERROR) {
		fprintf(stderr, "ZIP file has a CRC error.\n");
		return 0;
	}

	if (res <= 0 || res != fsize) {
		fprintf(stderr, "Error reading ZIP file.\n");

		if (res == UNZ_ERRNO)
			fprintf(stderr, "Unknown error\n");
		else if (res == UNZ_EOF)
			fprintf(stderr, "Unexpected End of File\n");
		else if (res == UNZ_PARAMERROR)
			fprintf(stderr, "Parameter error\n");
		else if (res == UNZ_BADZIPFILE)
			fprintf(stderr, "Corrupt ZIP file\n");
		else if (res == UNZ_INTERNALERROR)
			fprintf(stderr, "Internal error\n");
		else if (res == UNZ_CRCERROR)
			fprintf(stderr, "CRC error\n");
		return 0;
	}

	return 1;
}
