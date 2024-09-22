// SPDX-FileCopyrightText: Authors of TuxNES
// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Description: Loads a .nes rom from a file.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "loader.h"
#include "mapper.h"

#ifdef HAVE_LIBZ
#include <zlib.h>
#include <unzip.h>
#else
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#ifdef HAVE_LIBZ
static int
load_rom_zip(const char *filename)
{
	int size = 0;
	int res;

	unzFile romfd = unzOpen(filename);
	if (!romfd) {
		perror(filename);
		exit(EXIT_FAILURE);
	}

	/* Search .zip file for first file with extension .nes */
	for (res = unzGoToFirstFile(romfd);
	     res == UNZ_OK;
	     res = unzGoToNextFile(romfd)) {
		unz_file_info fileinfo;
		char filename[256 + 1];

		unzGetCurrentFileInfo(romfd, &fileinfo, filename, sizeof filename, NULL, 0, NULL, 0);
		filename[sizeof filename - 1] = '\0';
		const char *extension = strrchr(filename, '.');
		if (extension && !strcasecmp(extension, ".nes")) {
			size = fileinfo.uncompressed_size;
			break;
		}
	}

	if (res != UNZ_OK
	 || !size
	 || unzOpenCurrentFile(romfd) != UNZ_OK) {
		fprintf(stderr, "Error in zip file\n");
		exit(EXIT_FAILURE);
	}

	res = unzReadCurrentFile(romfd, ROM, size);
	if (unzCloseCurrentFile(romfd) == UNZ_CRCERROR) {
		fprintf(stderr, "ZIP file has a CRC error.\n");
		exit(EXIT_FAILURE);
	}

	if (res <= 0 || res != size) {
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
		exit(EXIT_FAILURE);
	}

	unzClose(romfd);
	return size;
}

static int
load_rom_gz(const char *filename)
{
	int size = 0;
	int c;

	gzFile romfd = gzopen(filename, "rb");
	if (!romfd) {
		perror(filename);
		exit(EXIT_FAILURE);
	}

	while ((c = gzgetc(romfd)) != -1) {
		ROM[size++] = c;
	}

	gzclose(romfd);
	return size;
}
#endif

int
load_rom(const char *filename)
{
#ifdef HAVE_LIBZ
	const char *extension = strrchr(filename, '.');
	if (extension && !strcasecmp(extension, ".zip")) {
		return load_rom_zip(filename);
	}
	return load_rom_gz(filename);
#else
	/* Open ROM file */
	int romfd = open(filename, O_RDONLY);
	if (romfd < 0) {
		perror(filename);
		exit(EXIT_FAILURE);
	}

	/* Get file size */
	int size = lseek(romfd, 0, SEEK_END);
	lseek(romfd, 0, SEEK_SET);

	if (size < 0) {
		perror(filename);
		exit(EXIT_FAILURE);
	} else if (!size) {
		fprintf(stderr, "Unable to read %s (empty file)\n", filename);
		exit(EXIT_FAILURE);
	}

	/* load the ROM */
	if (read(romfd, ROM, size) != size) {
		perror(filename);
		exit(EXIT_FAILURE);
	}

	close(romfd);
	return size;
#endif
}
