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
	    "      -target      use the given target atom\n"
	    "      -rmlastnl    remove the last newline character if present\n"
	    "      -version     version information\n"
	    "      -silent      errors only, run in background (default)\n"
	    "      -quiet       run in foreground, show what's happening\n"
	    "      -verbose     running commentary\n"
	    "      -sensitive   clear sensitive data from seleciton buffer immediately upon being pasted\n"
	    "      -wait        wait milliseconds after last paste before selection is cleared\n"
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

/* a utility for finding the name of the X window that owns the selection. 
 * Sets namep to point to the string of the name (must be freed with XFree). 
 * Sets wp to point to the Window (an integer id).
 * Returns 0 if it works. Not 0, otherwise. 
 */ 
int
fetchname(Display *display, Atom selection, char **namep, Window *wp) {
    *namep = NULL;
    *wp = XGetSelectionOwner(display, selection);
    if (*wp == None)
	return 1;		/* Nobody has the selection. */

    XFetchName(display, *wp, namep);
    if (*namep)
	return 0; 		/* Hurrah! It worked on the first try. */
	
    /* Otherwise, recursively try the parent windows */
    Window p = *wp;
    Window dummy, *dummyp;
    unsigned int n;
    while (!*namep  &&  p != None) {
	if (!XQueryTree(display, p, &dummy, &p, &dummyp, &n))
	    break;
	if (p != None) {
	    XFetchName(display, p, namep);
	}
    }
    return (*namep == NULL);
}


/* failure to convert selection */
int
errconvsel(Display *display, Atom target, Atom selection)
{
    Window w = None;
    char *window_name;
    char *selection_name = XGetAtomName(display, selection); /* E.g., "PRIMARY" */

    if (!selection_name)
	return 1;		/* Invalid selection Atom  */

    /* Find the name of the window that holds the selection */
    fetchname(display, selection, &window_name, &w);

    if (w == None) {
	fprintf(stderr, "xclip: Error: There is no owner for the %s selection\n",
		selection_name);
    }
    else {
	if (window_name && window_name[0]) {
	    fprintf(stderr, "xclip: Error: '%s'", window_name);
	}
	else {
	    fprintf(stderr, "xclip: Error: window id 0x%lx", w);
	}
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

    if (window_name)
	XFree(window_name);

    exit(EXIT_FAILURE);
}
