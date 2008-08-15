/*
 *  
 * 
 *  xcprint.c - functions to print help, version, errors, etc
 *  Copyright (C) 2001 Kim Saunders
 *  Copyright (C) 2007-2008 Peter Åstrand
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
#include "xcprint.h"

/* print the help screen. argument is argv[0] from main() */
void
prhelp(char *name)
{
    fprintf(stderr,
	    "Usage: %s [OPTION] [FILE]...\n"
	    "Access an X server selection for reading or writing.\n"
	    "\n"
	    "  -i, -in          read text into X selection from standard input or files\n"
	    "                   (default)\n"
	    "  -o, -out         prints the selection to standard out (generally for\n"
	    "                   piping to a file or program)\n"
	    "  -l, -loops       number of selection requests to "
	    "wait for before exiting\n"
	    "  -d, -display     X display to connect to (eg "
	    "localhost:0\")\n"
	    "  -h, -help        usage information\n"
	    "      -selection   selection to access (\"primary\", "
	    "\"secondary\", \"clipboard\" or \"buffer-cut\")\n"
	    "      -noutf8      don't treat text as utf-8, use old unicode\n"
	    "      -version     version information\n"
	    "      -silent      errors only, run in background (default)\n"
	    "      -quiet       run in foreground, show what's happening\n"
	    "      -verbose     running commentary\n"
	    "\n" "Report bugs to <astrand@lysator.liu.se>\n", name);
    exit(EXIT_SUCCESS);
}


/* A function to print the software version info */
void
prversion(void)
{
    fprintf(stderr, "%s version %s\n", PACKAGE_NAME, PACKAGE_VERSION);
    fprintf(stderr, "Copyright (C) 2001-2008 Kim Saunders et al.\n");
    fprintf(stderr, "Distributed under the terms of the GNU GPL\n");
    exit(EXIT_SUCCESS);
}

/* failure message for malloc() problems */
void
errmalloc(void)
{
    fprintf(stderr, "Error: Could not allocate memory.\n");
    exit(EXIT_FAILURE);
}

/* failure to connect to X11 display */
void
errxdisplay(char *display)
{
    /* if the display wasn't specified, read it from the enviroment
     * just like XOpenDisplay would
     */
    if (display == NULL)
	display = getenv("DISPLAY");

    fprintf(stderr, "Error: Can't open display: %s\n", display);
    exit(EXIT_FAILURE);
}

/* a wrapper for perror that joins multiple prefixes together. Arguments
 * are an integer, and any number of strings. The integer needs to be set to
 * the number of strings that follow.
 */
void
errperror(int prf_tot, ...)
{
    va_list ap;			/* argument pointer */
    char *msg_all;		/* all messages so far */
    char *msg_cur;		/* current message string */
    int prf_cur;		/* current prefix number */

    /* start off with an empty string */
    msg_all = xcstrdup("");

    /* start looping through the viariable arguments */
    va_start(ap, prf_tot);

    /* loop through each of the arguments */
    for (prf_cur = 0; prf_cur < prf_tot; prf_cur++) {
	/* get the current argument */
	msg_cur = va_arg(ap, char *);

	/* realloc msg_all so it's big enough for itself, the current
	 * argument, and a null terminator
	 */
	msg_all = (char *) xcrealloc(msg_all, strlen(msg_all) + strlen(msg_cur) + sizeof(char)
	    );

	/* append the current message the the total message */
	strcat(msg_all, msg_cur);
    }
    va_end(ap);

    perror(msg_all);

    /* free the complete string */
    free(msg_all);
}
