/*
 *  
 * 
 *  xcprint.c - functions to print help, version, errors, etc
 *  Copyright (C) 2001 Kim Saunders
 *  Copyright (C) 2007-2020 Peter Åstrand
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
"  -i, -in          copy text into X selection from stdin or files\n"
"                   (DEFAULT if stdin is read from file or pipe)\n"
"  -o, -out         paste X selection to stdout\n"
"                   (DEFAULT if stdin is a keyboard)\n"
"      -filter      data piped in to selection will also be printed out\n"
"      -selection   primary [DEFAULT], clipboard, or secondary\n"
"  -c               shortcut for \"-selection clipboard\"\n"
"      -target      request specific data format: eg, image/jpeg\n"
"  -T               list available formats for the current selection and exit\n"
"      -sensitive   only allow copied data to be pasted once\n"
"  -l, -loops       number of paste requests to wait for before exiting\n"
"      -wait n      exit n ms after paste request, timer restarts on each paste\n"
"      -rmlastnl    remove the last newline character if present\n"
"  -d, -display     X display to connect to (eg localhost:0\")\n"
"      -version     version information\n"
"      -silent      errors only, (run in background) [DEFAULT]\n"
" -quiet/-verbose   show minimal/running commentary (foreground)\n"
"\n"
"Report bugs to https://github.com/astrand/xclip\n", name);
    exit(EXIT_SUCCESS);

/* The following lines aren't shown because we want to fit on an 80x24 screen */
//"      -noutf8      don't treat text as utf-8, use old unicode\n"
//"  -h, -help        this usage information\n"
//"      -quiet       show minimal output, running in foreground\n"
//"      -verbose     running commentary (foreground)\n"
//"      -debug       garrulous verbiage (foreground)\n"

}


/* A function to print the software version info */
void
prversion(void)
{
    fprintf(stderr, "%s version %s\n", PACKAGE_NAME, PACKAGE_VERSION);
    fprintf(stderr, "Copyright (C) 2001-2020 Kim Saunders et al.\n");
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
