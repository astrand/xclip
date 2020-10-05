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
"  -i, -in          read text into X selection from stdin or files [DEFAULT]\n"
"      -filter      text piped in to selection will also be printed out\n"
"  -o, -out         prints the selection to standard out\n"
"      -selection   primary [DEFAULT], clipboard, secondary, or buffer-cut\n"
"      -target      specify target atom: image/jpeg, UTF8_STRING [DEFAULT]\n"
"      -silent      errors only, (run in background) [DEFAULT]\n"
"      -quiet       minimal output (foreground)\n"
"      -verbose     running commentary (foreground)\n"
"      -debug       garrulous verbiage (foreground)\n"
"      -sensitive   only allow copied data to be pasted once\n"
"  -l, -loops       number of selection requests to wait for before exiting\n"
"      -wait n      exit n milliseconds pasting, timer restarts on each paste\n"
"      -noutf8      don't treat text as utf-8, use old unicode\n"
"      -rmlastnl    remove the last newline character if present\n"
"  -d, -display     X display to connect to (eg localhost:0\")\n"
"      -version     version information\n"
"  -h, -help        this usage information\n"
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
    fprintf(stderr, "xclip: Error: Could not allocate memory.\n");
    exit(EXIT_FAILURE);
}

/* failure to connect to X11 display */
void
errxdisplay(char *display)
{
    /* if the display wasn't specified, read it from the environment
     * just like XOpenDisplay would
     */
    if (display == NULL)
	display = getenv("DISPLAY");

    fprintf(stderr, "xclip: Error: Can't open display: %s\n", display ? display : "(null)");
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

    /* start looping through the variable arguments */
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

	/* append the current message to the total message */
	strcat(msg_all, msg_cur);
    }
    va_end(ap);

    perror(msg_all);

    /* free the complete string */
    free(msg_all);
}


/* failure to convert selection */
void
errconvsel(Display *display, Atom target, Atom selection)
{
    Window w = None;
    char *selection_name = XGetAtomName(display, selection); /* E.g., "PRIMARY" */

    if (!selection_name)
	exit(EXIT_FAILURE);	/* Invalid selection Atom  */

    w = XGetSelectionOwner(display, selection);
    if (w == None) {
	fprintf(stderr, "xclip: Error: There is no owner for the %s selection\n",
		selection_name);
    }
    else {
	/* Show the name of the window that holds the selection */
	fprintf(stderr, "xclip: Error: %s", xcnamestr(display, w));

	char *atom_name = XGetAtomName(display, target);
	if (atom_name) {
	    fprintf(stderr, " cannot convert %s selection to target '%s'\n",
		    selection_name, atom_name);
	    XFree(atom_name);
	}
	else {
	    /* Should never happen. */
	    fprintf(stderr, " cannot convert to NULL target.\n"); 
	}    
    }

    if (selection_name)
	XFree(selection_name);

    exit(EXIT_FAILURE);
}
