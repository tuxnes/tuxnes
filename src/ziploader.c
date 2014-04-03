/*
 * This file is part of the TuxNES project codebase.
 *
 * Please see the README and COPYING files for more information regarding
 * this project.
 *
 * Description: Loads a .nes rom from a .zip file.
 */

#include <string.h>
#include <ctype.h>


#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "unzip.h"
#include "mapper.h"


int
ziploader (unzFile file, char *zipname)
{
  char filename[1024];
  int fsize = 0;
  int file_marker;
  unz_file_info fileinfo;
  char *extension;
  char *ptr;
  int res;
  char romname[1024];
  
  file_marker = unzGoToFirstFile (file);
  
  /* Search .zip file and file the ROM file with extension .nes */
  while(file_marker == UNZ_OK)
    {
      unzGetCurrentFileInfo (file, &fileinfo, filename,128, NULL,0, NULL,0);

      extension = strrchr(filename,'.');

      if  (extension && !strcasecmp (extension,".nes"))
        {
          strcpy(romname,filename);
          fsize = fileinfo.uncompressed_size;
          break;
        }
      file_marker = unzGoToNextFile(file);
    }

  if (!(file_marker == UNZ_END_OF_LIST_OF_FILE 
    || file_marker == UNZ_OK) || fsize == 0)
    return (0);
    
  ptr = ROM;
  unzLocateFile (file,romname,1);
  unzGetCurrentFileInfo (file, &fileinfo, romname,128, NULL,0, NULL,0);
  
  if (unzOpenCurrentFile(file) != UNZ_OK)
    {
      printf("Error in zip file\n");
      return (0);
    }

  res = unzReadCurrentFile(file,ptr,fsize);
  if (unzCloseCurrentFile(file) == UNZ_CRCERROR)
    {
      fprintf (stderr,"ZIP file has a CRC error.\n");
      return (0);
    }
  
  if (res <= 0 || res != fsize)
    {
      fprintf (stderr,"Error reading ZIP file.\n");

      if (res == UNZ_ERRNO)
        fprintf (stderr,"Unkown error\n");
      if (res == UNZ_EOF)
        fprintf (stderr,"Unexpected End of File\n");
      if (res == UNZ_PARAMERROR)
        fprintf (stderr,"Parameter error\n");
      if (res == UNZ_BADZIPFILE)
        fprintf (stderr,"Corrupt ZIP file\n");
      if (res == UNZ_INTERNALERROR)
        fprintf (stderr,"Internal error\n");
      if (res == UNZ_CRCERROR)
        fprintf (stderr,"CRC error\n");
      return (0);
    }

  return (1);
}
