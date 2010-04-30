/*
 *  
 * 
 *  xclip.c - command line interface to X server selections 
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
#include <unistd.h>
#include <ctype.h>
#ifdef HAVE_ICONV
#include <errno.h>
#include <iconv.h>
#endif
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xmu/Atoms.h>
#include "xcdef.h"
#include "xcprint.h"
#include "xclib.h"

/* command line option table for XrmParseCommand() */
XrmOptionDescRec opt_tab[12];

/* Options that get set on the command line */
int sloop = 0;			/* number of loops */
char *sdisp = NULL;		/* X display to connect to */
Atom sseln = XA_PRIMARY;	/* X selection to work with */
Atom target = XA_STRING;

/* Flags for command line options */
static int fverb = OSILENT;	/* output level */
static int fdiri = T;		/* direction is in */
static int ffilt = F;		/* filter mode */

Display *dpy;			/* connection to X11 display */
XrmDatabase opt_db = NULL;	/* database for options */

char **fil_names;		/* names of files to read */
int fil_number = 0;		/* number of files to read */
int fil_current = 0;
FILE *fil_handle = NULL;

/* variables to hold Xrm database record and type */
XrmValue rec_val;
char *rec_typ;

int tempi = 0;

/* Use XrmParseCommand to parse command line options to option variable */
static void
doOptMain(int argc, char *argv[])
{
    /* Initialise resource manager and parse options into database */
    XrmInitialize();
    XrmParseCommand(&opt_db, opt_tab, sizeof(opt_tab) / sizeof(opt_tab[0]), PACKAGE_NAME, &argc,
		    argv);

    /* set output level */
    if (XrmGetResource(opt_db, "xclip.olevel", "Xclip.Olevel", &rec_typ, &rec_val)
	) {
	/* set verbose flag according to option */
	if (strcmp(rec_val.addr, "S") == 0)
	    fverb = OSILENT;
	if (strcmp(rec_val.addr, "Q") == 0)
	    fverb = OQUIET;
	if (strcmp(rec_val.addr, "V") == 0)
	    fverb = OVERBOSE;
    }

    /* set direction flag (in or out) */
    if (XrmGetResource(opt_db, "xclip.direction", "Xclip.Direction", &rec_typ, &rec_val)
	) {
	if (strcmp(rec_val.addr, "I") == 0)
	    fdiri = T;
	if (strcmp(rec_val.addr, "O") == 0)
	    fdiri = F;
    }

    /* set filter mode */
    if (XrmGetResource(opt_db, "xclip.filter", "Xclip.Filter", &rec_typ, &rec_val)
	) {
	/* filter mode only allowed in silent mode */
	if (fverb == OSILENT)
	    ffilt = T;
    }

    /* check for -help and -version */
    if (XrmGetResource(opt_db, "xclip.print", "Xclip.Print", &rec_typ, &rec_val)
	) {
	if (strcmp(rec_val.addr, "H") == 0)
	    prhelp(argv[0]);
	if (strcmp(rec_val.addr, "V") == 0)
	    prversion();
    }

    /* check for -display */
    if (XrmGetResource(opt_db, "xclip.display", "Xclip.Display", &rec_typ, &rec_val)
	) {
	sdisp = rec_val.addr;
	if (fverb == OVERBOSE)	/* print in verbose mode only */
	    fprintf(stderr, "Display: %s\n", sdisp);
    }

    /* check for -loops */
    if (XrmGetResource(opt_db, "xclip.loops", "Xclip.Loops", &rec_typ, &rec_val)
	) {
	sloop = atoi(rec_val.addr);
	if (fverb == OVERBOSE)	/* print in verbose mode only */
	    fprintf(stderr, "Loops: %i\n", sloop);
    }

    /* Read remaining options (filenames) */
    while ((fil_number + 1) < argc) {
	if (fil_number > 0) {
	    fil_names = xcrealloc(fil_names, (fil_number + 1) * sizeof(char *)
		);
	}
	else {
	    fil_names = xcmalloc(sizeof(char *));
	}
	fil_names[fil_number] = argv[fil_number + 1];
	fil_number++;
    }
}

/* process selection command line option */
static void
doOptSel(void)
{
    /* set selection to work with */
    if (XrmGetResource(opt_db, "xclip.selection", "Xclip.Selection", &rec_typ, &rec_val)
	) {
	switch (tolower(rec_val.addr[0])) {
	case 'p':
	    sseln = XA_PRIMARY;
	    break;
	case 's':
	    sseln = XA_SECONDARY;
	    break;
	case 'c':
	    sseln = XA_CLIPBOARD(dpy);
	    break;
	case 'b':
	    sseln = XA_STRING;
	    break;
	}

	if (fverb == OVERBOSE) {
	    fprintf(stderr, "Using selection: ");

	    if (sseln == XA_PRIMARY)
		fprintf(stderr, "XA_PRIMARY");
	    if (sseln == XA_SECONDARY)
		fprintf(stderr, "XA_SECONDARY");
	    if (sseln == XA_CLIPBOARD(dpy))
		fprintf(stderr, "XA_CLIPBOARD");
	    if (sseln == XA_STRING)
		fprintf(stderr, "XA_STRING");

	    fprintf(stderr, "\n");
	}
    }
}

/* process noutf8 command line option */
static void
doOptNoUtf8(void)
{
    /* check for -noutf8 */
    if (XrmGetResource(opt_db, "xclip.noutf8", "Xclip.noutf8", &rec_typ, &rec_val)
	) {
	if (fverb == OVERBOSE)	/* print in verbose mode only */
	    fprintf(stderr, "Using old UNICODE instead of UTF8.\n");
    }
    else {
	target = XA_UTF8_STRING(dpy);
	if (fverb == OVERBOSE)	/* print in verbose mode only */
	    fprintf(stderr, "Using UTF8_STRING.\n");
    }
}

static void
doIn(Window win, const char *progname)
{
    unsigned char *sel_buf;	/* buffer for selection data */
    unsigned long sel_len = 0;	/* length of sel_buf */
    unsigned long sel_all = 0;	/* allocated size of sel_buf */
    XEvent evt;			/* X Event Structures */
    int dloop = 0;		/* done loops counter */

    /* in mode */
    sel_all = 16;		/* Reasonable ballpark figure */
    sel_buf = xcmalloc(sel_all * sizeof(char));

    /* Put chars into inc from stdin or files until we hit EOF */
    do {
	if (fil_number == 0) {
	    /* read from stdin if no files specified */
	    fil_handle = stdin;
	}
	else {
	    if ((fil_handle = fopen(fil_names[fil_current], "r")) == NULL) {
		errperror(3, progname, ": ", fil_names[fil_current]
		    );
		exit(EXIT_FAILURE);
	    }
	    else {
		/* file opened successfully. Print
		 * message (verbose mode only).
		 */
		if (fverb == OVERBOSE)
		    fprintf(stderr, "Reading %s...\n", fil_names[fil_current]
			);
	    }
	}

	fil_current++;
	while (!feof(fil_handle)) {
	    /* If sel_buf is full (used elems =
	     * allocated elems)
	     */
	    if (sel_len == sel_all) {
		/* double the number of
		 * allocated elements
		 */
		sel_all *= 2;
		sel_buf = (unsigned char *) xcrealloc(sel_buf, sel_all * sizeof(char)
		    );
	    }
	    sel_len += fread(sel_buf + sel_len, sizeof(char), sel_all - sel_len, fil_handle);
	}
    } while (fil_current < fil_number);

    /* if there are no files being read from (i.e., input
     * is from stdin not files, and we are in filter mode,
     * spit all the input back out to stdout
     */
    if ((fil_number == 0) && ffilt) {
	fwrite(sel_buf, sizeof(char), sel_len, stdout);
	fclose(stdout);
    }

    /* Handle cut buffer if needed */
    if (sseln == XA_STRING) {
	XStoreBuffer(dpy, (char *) sel_buf, (int) sel_len, 0);
	return;
    }

    /* take control of the selection so that we receive
     * SelectionRequest events from other windows
     */
    /* FIXME: Should not use CurrentTime, according to ICCCM section 2.1 */
    XSetSelectionOwner(dpy, sseln, win, CurrentTime);

    /* fork into the background, exit parent process if we
     * are in silent mode
     */
    if (fverb == OSILENT) {
	pid_t pid;

	pid = fork();
	/* exit the parent process; */
	if (pid)
	    exit(EXIT_SUCCESS);
    }

    /* print a message saying what we're waiting for */
    if (fverb > OSILENT) {
	if (sloop == 1)
	    fprintf(stderr, "Waiting for one selection request.\n");

	if (sloop < 1)
	    fprintf(stderr, "Waiting for selection requests, Control-C to quit\n");

	if (sloop > 1)
	    fprintf(stderr, "Waiting for %i selection requests, Control-C to quit\n", sloop);
    }

    /* Avoid making the current directory in use, in case it will need to be umounted */
    chdir("/");

    /* loop and wait for the expected number of
     * SelectionRequest events
     */
    while (dloop < sloop || sloop < 1) {
	/* print messages about what we're waiting for
	 * if not in silent mode
	 */
	if (fverb > OSILENT) {
	    if (sloop > 1)
		fprintf(stderr, "  Waiting for selection request %i of %i.\n", dloop + 1, sloop);

	    if (sloop == 1)
		fprintf(stderr, "  Waiting for a selection request.\n");

	    if (sloop < 1)
		fprintf(stderr, "  Waiting for selection request number %i\n", dloop + 1);
	}

	/* wait for a SelectionRequest event */
	while (1) {
	    static unsigned int clear = 0;
	    static unsigned int context = XCLIB_XCIN_NONE;
	    static unsigned long sel_pos = 0;
	    static Window cwin;
	    static Atom pty;
	    int finished;

	    XNextEvent(dpy, &evt);

	    finished = xcin(dpy, &cwin, evt, &pty, target, sel_buf, sel_len, &sel_pos, &context);

	    if (evt.type == SelectionClear)
		clear = 1;

	    if ((context == XCLIB_XCIN_NONE) && clear)
		exit(EXIT_SUCCESS);

	    if (finished)
		break;
	}

	dloop++;		/* increment loop counter */
    }
}

static void
printSelBuf(FILE *fout, Atom sel_type, unsigned char *sel_buf, size_t sel_len)
{
    Atom html = XInternAtom(dpy, "text/html", True);

    if (fverb == OVERBOSE) {	/* print in verbose mode only */
	char *atom_name = XGetAtomName(dpy, sel_type);
	fprintf(stderr, "Type is %s.\n", atom_name);
	XFree(atom_name);
    }

    if (sel_type == XA_INTEGER) {
	/* if the buffer contains integers, print them */
	long *long_buf = (long *) sel_buf;
	size_t long_len = sel_len / sizeof(long);
	while (long_len--)
	    fprintf(fout, "%ld\n", *long_buf++);
	return;
    }

    if (sel_type == XA_ATOM) {
	/* if the buffer contains atoms, print their names */
	Atom *atom_buf = (Atom *) sel_buf;
	size_t atom_len = sel_len / sizeof(Atom);
	while (atom_len--) {
	    char *atom_name = XGetAtomName(dpy, *atom_buf++);
	    fprintf(fout, "%s\n", atom_name);
	    XFree(atom_name);
	}
	return;
    }

#ifdef HAVE_ICONV
    if (html != None && sel_type == html) {
	/* if the buffer contains UCS-2 (UTF-16), convert to
	 * UTF-8.  Mozilla-based browsers do this for the
	 * text/html target.
	 */
	iconv_t cd;
	char *sel_charset = NULL;
	if (sel_buf[0] == 0xFF && sel_buf[1] == 0xFE)
	    sel_charset = "UTF-16LE";
	else if (sel_buf[0] == 0xFE && sel_buf[1] == 0xFF)
	    sel_charset = "UTF-16BE";
     
	if (sel_charset != NULL &&
	        (cd = iconv_open("UTF-8", sel_charset)) != (iconv_t) -1) {
	    char *out_buf_start = malloc(sel_len), *in_buf = (char *) sel_buf+2,
	         *out_buf = out_buf_start;
	    size_t in_bytesleft = sel_len-2, out_bytesleft = sel_len;
     
	    while (iconv(cd, &in_buf, &in_bytesleft, &out_buf, &out_bytesleft) == -1 && errno == E2BIG) {
		fwrite(out_buf_start, sizeof(char), sel_len-out_bytesleft, fout);
		out_buf = out_buf_start;
		out_bytesleft = sel_len;
	    }
	    if (out_buf != out_buf_start)
		fwrite(out_buf_start, sizeof(char), sel_len-out_bytesleft, fout);
     
	    free(out_buf_start);
	    iconv_close(cd);
	    return;
	}
    }
#endif

    /* otherwise, print the raw buffer out */
    fwrite(sel_buf, sizeof(char), sel_len, fout);
}

static void
doOut(Window win)
{
    Atom sel_type = None;
    unsigned char *sel_buf;	/* buffer for selection data */
    unsigned long sel_len = 0;	/* length of sel_buf */
    XEvent evt;			/* X Event Structures */
    unsigned int context = XCLIB_XCOUT_NONE;

    if (sseln == XA_STRING)
	sel_buf = (unsigned char *) XFetchBuffer(dpy, (int *) &sel_len, 0);
    else {
	while (1) {
	    /* only get an event if xcout() is doing something */
	    if (context != XCLIB_XCOUT_NONE)
		XNextEvent(dpy, &evt);

	    /* fetch the selection, or part of it */
	    xcout(dpy, win, evt, sseln, target, &sel_type, &sel_buf, &sel_len, &context);

	    /* fallback is needed. set XA_STRING to target and restart the loop. */
	    if (context == XCLIB_XCOUT_FALLBACK) {
		context = XCLIB_XCOUT_NONE;
		target = XA_STRING;
		continue;
	    }

	    /* only continue if xcout() is doing something */
	    if (context == XCLIB_XCOUT_NONE)
		break;
	}
    }

    if (sel_len) {
	/* only print the buffer out, and free it, if it's not
	 * empty
	 */
	printSelBuf(stdout, sel_type, sel_buf, sel_len);
	if (sseln == XA_STRING)
	    XFree(sel_buf);
	else
	    free(sel_buf);
    }
}

int
main(int argc, char *argv[])
{
    /* Declare variables */
    Window win;			/* Window */

    /* set up option table. I can't figure out a better way than this to
     * do it while sticking to pure ANSI C. The option and specifier
     * members have a type of volatile char *, so they need to be allocated
     * by strdup or malloc, you can't set them to a string constant at
     * declare time, this is note pure ANSI C apparently, although it does
     * work with gcc
     */

    /* loop option entry */
    opt_tab[0].option = xcstrdup("-loops");
    opt_tab[0].specifier = xcstrdup(".loops");
    opt_tab[0].argKind = XrmoptionSepArg;
    opt_tab[0].value = (XPointer) NULL;

    /* display option entry */
    opt_tab[1].option = xcstrdup("-display");
    opt_tab[1].specifier = xcstrdup(".display");
    opt_tab[1].argKind = XrmoptionSepArg;
    opt_tab[1].value = (XPointer) NULL;

    /* selection option entry */
    opt_tab[2].option = xcstrdup("-selection");
    opt_tab[2].specifier = xcstrdup(".selection");
    opt_tab[2].argKind = XrmoptionSepArg;
    opt_tab[2].value = (XPointer) NULL;

    /* filter option entry */
    opt_tab[3].option = xcstrdup("-filter");
    opt_tab[3].specifier = xcstrdup(".filter");
    opt_tab[3].argKind = XrmoptionNoArg;
    opt_tab[3].value = (XPointer) xcstrdup(ST);

    /* in option entry */
    opt_tab[4].option = xcstrdup("-in");
    opt_tab[4].specifier = xcstrdup(".direction");
    opt_tab[4].argKind = XrmoptionNoArg;
    opt_tab[4].value = (XPointer) xcstrdup("I");

    /* out option entry */
    opt_tab[5].option = xcstrdup("-out");
    opt_tab[5].specifier = xcstrdup(".direction");
    opt_tab[5].argKind = XrmoptionNoArg;
    opt_tab[5].value = (XPointer) xcstrdup("O");

    /* version option entry */
    opt_tab[6].option = xcstrdup("-version");
    opt_tab[6].specifier = xcstrdup(".print");
    opt_tab[6].argKind = XrmoptionNoArg;
    opt_tab[6].value = (XPointer) xcstrdup("V");

    /* help option entry */
    opt_tab[7].option = xcstrdup("-help");
    opt_tab[7].specifier = xcstrdup(".print");
    opt_tab[7].argKind = XrmoptionNoArg;
    opt_tab[7].value = (XPointer) xcstrdup("H");

    /* silent option entry */
    opt_tab[8].option = xcstrdup("-silent");
    opt_tab[8].specifier = xcstrdup(".olevel");
    opt_tab[8].argKind = XrmoptionNoArg;
    opt_tab[8].value = (XPointer) xcstrdup("S");

    /* quiet option entry */
    opt_tab[9].option = xcstrdup("-quiet");
    opt_tab[9].specifier = xcstrdup(".olevel");
    opt_tab[9].argKind = XrmoptionNoArg;
    opt_tab[9].value = (XPointer) xcstrdup("Q");

    /* verbose option entry */
    opt_tab[10].option = xcstrdup("-verbose");
    opt_tab[10].specifier = xcstrdup(".olevel");
    opt_tab[10].argKind = XrmoptionNoArg;
    opt_tab[10].value = (XPointer) xcstrdup("V");

    /* utf8 option entry */
    opt_tab[11].option = xcstrdup("-noutf8");
    opt_tab[11].specifier = xcstrdup(".noutf8");
    opt_tab[11].argKind = XrmoptionNoArg;
    opt_tab[11].value = (XPointer) xcstrdup("N");

    /* parse command line options */
    doOptMain(argc, argv);

    /* Connect to the X server. */
    if ((dpy = XOpenDisplay(sdisp))) {
	/* successful */
	if (fverb == OVERBOSE)
	    fprintf(stderr, "Connected to X server.\n");
    }
    else {
	/* couldn't connect to X server. Print error and exit */
	errxdisplay(sdisp);
    }

    /* parse selection command line option */
    doOptSel();

    /* parse noutf8 command line option */
    doOptNoUtf8();

    /* Create a window to trap events */
    win = XCreateSimpleWindow(dpy, DefaultRootWindow(dpy), 0, 0, 1, 1, 0, 0, 0);

    /* get events about property changes */
    XSelectInput(dpy, win, PropertyChangeMask);

    if (fdiri)
	doIn(win, argv[0]);
    else
	doOut(win);

    /* Disconnect from the X server */
    XCloseDisplay(dpy);

    /* exit */
    return (EXIT_SUCCESS);
}
