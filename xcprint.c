/*
 *  $Id: xcprint.c,v 1.2 2001/05/05 15:22:46 kims Exp $
 * 
 *  xcprint.c - functions to print help, version, etc
 *  Copyright (C) 2001 Kim Saunders
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "xcdef.h"

void prhelp (){
  fprintf(stderr, "Usage: %s [OPTIONS] [FILES]\n", XC_NAME);
  fprintf(stderr, "Puts data from standard input or FILES into a X server selection for pasting.\n");
  fprintf(stderr, "\n");
  fprintf(stderr, "-i,  -in        in mode\n");
  fprintf(stderr, "-o,  -out       out mode\n");
  fprintf(stderr, "-l,  -loops     number of selection requests to wait for before exiting\n");
  fprintf(stderr, "-d,  -display   X display to connect to (eg \"localhost:0\")\n");
  fprintf(stderr, "-h,  -help      usage information\n");
  fprintf(stderr, "     -version   version information\n");
  fprintf(stderr, "     -silent    silent mode (errors only, default)\n");
  fprintf(stderr, "     -quiet     show what's happing, don't fork into background\n");
  fprintf(stderr, "     -verbose   running commentary\n");
  fprintf(stderr, "\n");
  fprintf(stderr, "Report bugs to <kim.saunders@fortytwo.com.au>\n");
  exit(EXIT_SUCCESS);
}

/* A function to print the software version info */
void prversion (){
  fprintf(stderr, "%s version %1.2f\n", XC_NAME, XC_VERS);
  fprintf(stderr, "Copyright (C) 2001 Kim Saunders\n");
  fprintf(stderr, "Distributed under the terms of the GNU GPL\n");
  exit(EXIT_SUCCESS);
}

/* failure message for malloc() problems */
void errmalloc (){
  fprintf(stderr, "Error: Could not allocate memory.\n");
  exit(EXIT_FAILURE);
}

/* error message where XGetWindowProperty format > 8 */
void errformat (){
  fprintf(stderr, "Error: Support for format not yet implemented.\n");
  exit(EXIT_FAILURE);
}
