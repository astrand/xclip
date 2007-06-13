/*
 *  $Id: xcprint.c,v 1.9 2001/09/19 08:38:01 kims Exp $
 * 
 *  xcprint.c - functions to print help, version, errors, etc
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
#include <stdarg.h>
#include <string.h>
#include "xcdef.h"
#include "xclib.h"

/* print the help screen. argument is argv[0] from main() */
void prhelp (char *name)
{
	fprintf(stderr, "Usage: %s [OPTIONS] [FILES]\n", name);
	fprintf(stderr, "Puts data from standard input or FILES into an X ");
	fprintf(stderr, "server selection for pasting.\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "-i,  -in        in mode\n");
	fprintf(stderr, "-o,  -out       out mode\n");
	fprintf(stderr, "-l,  -loops     number of selection requests to ");
	fprintf(stderr, "wait for before exiting\n");
	fprintf(stderr, "-s,  -selection selection to access (\"primary\", ");
	fprintf(stderr, "\"secondary\" or \"clipboard\")\n");
	fprintf(stderr, "-d,  -display   X display to connect to (eg ");
	fprintf(stderr, "\"localhost:0\")\n");
	fprintf(stderr, "-h,  -help      usage information\n");
	fprintf(stderr, "     -version   version information\n");
	fprintf(stderr, "     -silent    silent mode (errors only, default)\n");
	fprintf(stderr, "     -quiet     show what's happing, don't fork ");
	fprintf(stderr, "into background\n");
	fprintf(stderr, "     -verbose   running commentary\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Report bugs to <kim.saunders@mercuryit.com.au>\n");
	exit(EXIT_SUCCESS);
}

/* A function to print the software version info */
void prversion ()
{
	fprintf(stderr, "%s version %1.2f\n", XC_NAME, XC_VERS);
	fprintf(stderr, "Copyright (C) 2001 Kim Saunders\n");
	fprintf(stderr, "Distributed under the terms of the GNU GPL\n");
	exit(EXIT_SUCCESS);
}

/* failure message for malloc() problems */
void errmalloc ()
{
	fprintf(stderr, "Error: Could not allocate memory.\n");
	exit(EXIT_FAILURE);
}

/* failure to connect to X11 display */
void errxdisplay (char *display)
{
	/* if the display wasn't specified, read it from the enviroment
	 * just like XOpenDisplay would
	 */
	if (display == NULL)
		display = getenv("DISPLAY");
	
	fprintf(stderr, "Error: Can't open display: %s\n", display);
	exit(EXIT_FAILURE);
}

/* a wrapper for perror that joins multiple prefixes together */
void errperror(char *msg, ...)
{
	va_list ap;		/* argument pointer */
	char *msg_all = NULL;	/* all messages so far */
	char *msg_cur;		/* current message string */

	if (msg != NULL)
	{
		msg_all = strdup(msg);
	
		va_start(ap, msg);
		while ((msg_cur = va_arg(ap,char *)))
		{
			msg_all = (char *)xcrealloc(
				msg_all,
				strlen(msg_all) + strlen(msg_cur) + 1
			);
			strcat(msg_all, msg_cur);
		}
		va_end(ap);
	}
	
	perror(msg_all);
}
