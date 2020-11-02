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
XrmOptionDescRec opt_tab[17];
int opt_tab_size;

/* Options that get set on the command line */
int sloop = 0;			/* number of loops */
char *sdisp = NULL;		/* X display to connect to */
Atom sseln = XA_PRIMARY;	/* X selection to work with */
Atom target = XA_STRING;
int wait = 0;              /* wait: stop xclip after wait msec
                            after last 'paste event', start counting
                            after first 'paste event' */

/* Flags for command line options */
static int fdiri = T;		/* direction is in */
static int ffilt = F;		/* filter mode */
static int frmnl = F;		/* remove (single) newline character at the very end if present */
static int fsecm = F;		/* zero out selection buffer before exiting */

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

struct requestor
{
	Window cwin;
	Atom pty;
	unsigned int context;
	unsigned long sel_pos;
	int finished;
	long chunk_size;
	struct requestor *next;
};

static struct requestor *requestors;

static struct requestor *get_requestor(Window win)
{
	struct requestor *requestor;

	if (requestors) {
	    for (requestor = requestors; requestor != NULL; requestor = requestor->next) {
	        if (requestor->cwin == win) {
		    if (xcverb >= OVERBOSE) {
			fprintf(stderr,
				"    = Reusing requestor for %s\n",
				xcnamestr(dpy, win) );
		    }

	            return requestor;
	        }
	    }
	}

	if (xcverb >= OVERBOSE) {
	    fprintf(stderr, "    + Creating new requestor for %s\n",
		    xcnamestr(dpy, win) );
	}

	requestor = (struct requestor *)calloc(1, sizeof(struct requestor));
	if (!requestor) {
	    errmalloc();
	} else {
	    requestor->context = XCLIB_XCIN_NONE;
	}

	if (!requestors) {
	    requestors = requestor;
	} else {
	    requestor->next = requestors;
	    requestors = requestor;
	}

	return requestor;
}

static void del_requestor(struct requestor *requestor)
{
	struct requestor *reqitr;

	if (!requestor) {
	    return;
	}

	if (xcverb >= OVERBOSE) {
	    fprintf(stderr,
		    "    - Deleting requestor for %s\n",
		    xcnamestr(dpy, requestor->cwin) );
	}

	if (requestors == requestor) {
	    requestors = requestors->next;
	} else {
	    for (reqitr = requestors; reqitr != NULL; reqitr = reqitr->next) {
	        if (reqitr->next == requestor) {
	            reqitr->next = reqitr->next->next;
	            break;
	        }
	    }
	}

	free(requestor);
}

int clean_requestors() {
    /* Remove any requestors for which the X window has disappeared */
    if (xcverb >= ODEBUG) {
	fprintf(stderr, "xclip: debug: checking for requestors whose window has closed\n");
    }
    struct requestor *r = requestors;
    Window win;
    XWindowAttributes dummy;
    while (r) {
	win = r->cwin;

	// check if window exists by seeing if XGetWindowAttributes works.
	// note: this triggers X's BadWindow error and runs xchandler().
	if ( !XGetWindowAttributes(dpy, win, &dummy) ) {
	    if (xcverb >= OVERBOSE) {
		fprintf(stderr, "    ! Found obsolete requestor 0x%lx\n", win);
	    }
	    del_requestor(r);
	}
	r = r -> next;
    }
    return 0;
}

/* Use XrmParseCommand to parse command line options to option variable */
static void
doOptMain(int argc, char *argv[])
{
    /* Initialise resource manager and parse options into database */
    XrmInitialize();
    XrmParseCommand(&opt_db, opt_tab, opt_tab_size, PACKAGE_NAME, &argc,
		    argv);

    /* set output level */
    if (XrmGetResource(opt_db, "xclip.olevel", "Xclip.Olevel", &rec_typ, &rec_val)
	) {
	/* set verbose flag according to option */
	switch (rec_val.addr[0]) {
	case 'S':
	    xcverb = OSILENT;	break;
	case 'Q':
	    xcverb = OQUIET;	break;
	case 'V':
	    xcverb = OVERBOSE;	break;
	case 'D':
	    xcverb = ODEBUG;	break;
	}
    }
    if (xcverb == ODEBUG)
	fprintf(stderr, "xclip: debug: Debugging enabled.\n");

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
	if (xcverb == OSILENT)
	    ffilt = T;
    }

    /* set "remove last newline character if present" mode */
    if (XrmGetResource(opt_db, "xclip.rmlastnl", "Xclip.RmLastNl", &rec_typ, &rec_val)
	) {
	frmnl = T;
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
	if (xcverb >= OVERBOSE)	/* print in verbose or debug mode only */
	    fprintf(stderr, "Display: %s\n", sdisp);
    }

    /* check for -loops */
    if (XrmGetResource(opt_db, "xclip.loops", "Xclip.Loops", &rec_typ, &rec_val)
	) {
	sloop = atoi(rec_val.addr);
	if (xcverb >= OVERBOSE)
	    fprintf(stderr, "Loops: %i\n", sloop);
    }

    /* check for -sensitive */
    if (XrmGetResource(opt_db, "xclip.sensitive", "Xclip.Sensitive", &rec_typ, &rec_val)
	) {
	wait = 50;
	fsecm = T;
	if (xcverb >= OVERBOSE) {
	    fprintf(stderr, "Sensitive Mode Implies -wait 1\n");
	    fprintf(stderr, "Sensitive Buffers Will Be Zeroed At Exit\n");
        }
    }

    if (XrmGetResource(opt_db, "xclip.wait", "Xclip.Wait", &rec_typ, &rec_val)
        ) {
	wait = atoi(rec_val.addr);
	if (xcverb >= OVERBOSE)
	    fprintf(stderr, "wait: %i msec\n", wait);
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

    /* If filenames were given on the command line, 
     * default to reading input (unless -o was used).
     */
    if (fil_number > 0) {
      if (!XrmGetResource(opt_db, "xclip.direction", "Xclip.Direction", &rec_typ, &rec_val)) {
	  fdiri = T;		/* Direction is input */
      }
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

	if (xcverb >= OVERBOSE) {
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

/* process noutf8 and target command line options */
static void
doOptTarget(void)
{
    /* check for -noutf8 */
    if (XrmGetResource(opt_db, "xclip.noutf8", "Xclip.noutf8", &rec_typ, &rec_val)
	) {
	if (xcverb >= OVERBOSE)	/* print in verbose or debug mode only */
	    fprintf(stderr, "Using old UNICODE instead of UTF8.\n");
    }
    else if (XrmGetResource(opt_db, "xclip.target", "Xclip.Target", &rec_typ, &rec_val)
	) {
	target = XInternAtom(dpy, rec_val.addr, False);
	if (xcverb >= OVERBOSE)
	    fprintf(stderr, "Using target: %s\n", rec_val.addr);
    }
    else {
	target = XA_UTF8_STRING(dpy);
	if (xcverb >= OVERBOSE)
	    fprintf(stderr, "Using target: UTF8_STRING.\n");
    }
}

static int
doIn(Window win, const char *progname)
{
    unsigned char *sel_buf;	/* buffer for selection data */
    unsigned long sel_len = 0;	/* length of sel_buf */
    unsigned long sel_all = 0;	/* allocated size of sel_buf */
    XEvent evt;			/* X Event Structures */
    int dloop = 0;		/* done loops counter */
    int x11_fd;                 /* fd on which XEvents appear */
    fd_set in_fds;
    struct timeval tv;

    /* ConnectionNumber is a macro, it can't fail */
    x11_fd = ConnectionNumber(dpy);


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
		return EXIT_FAILURE;
	    }
	    else {
		/* file opened successfully.
		 */
		if (xcverb >= ODEBUG)
		    fprintf(stderr, "Reading %s...\n", fil_names[fil_current]);
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
		sel_buf = (unsigned char *) xcrealloc(sel_buf, sel_all * sizeof(char) );
		if (xcverb >= ODEBUG) {
		    fprintf(stderr, "xclip: debug: Increased buffersize to %ld\n", sel_all);
		}
	    }
	    sel_len += fread(sel_buf + sel_len, sizeof(char), sel_all - sel_len, fil_handle);
	}

	if (fil_handle && (fil_handle != stdin)) {
	    fclose(fil_handle);
	    fil_handle = NULL;
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

    if (fil_names) {
	free(fil_names);
	fil_names = NULL;
    }

    /* remove the last newline character if necessary */
    if (frmnl && sel_len && sel_buf[sel_len - 1] == '\n') {
	sel_len--;
    }

    /* Handle cut buffer if needed */
    if (sseln == XA_STRING) {
	XStoreBuffer(dpy, (char *) sel_buf, (int) sel_len, 0);
	XSetSelectionOwner(dpy, sseln, None, CurrentTime);
	xcmemzero(sel_buf,sel_len);
	return EXIT_SUCCESS;
    }

    /* take control of the selection so that we receive
     * SelectionRequest events from other windows
     */
    /* FIXME: Should not use CurrentTime, according to ICCCM section 2.1 */
    XSetSelectionOwner(dpy, sseln, win, CurrentTime);

    /* Double-check SetSelectionOwner did not "merely appear to succeed". */
    Window owner = XGetSelectionOwner(dpy, sseln);
    if (owner != win) {
	fprintf(stderr, "xclip: error: Failed to take ownership of selection.\n");
	return EXIT_FAILURE;
    }

    /* fork into the background, exit parent process if we
     * are in silent mode
     */
    if (xcverb == OSILENT) {
	pid_t pid;

	pid = fork();
	/* exit the parent process; */
	if (pid) {
	    XSetSelectionOwner(dpy, sseln, None, CurrentTime);
	    xcmemzero(sel_buf,sel_len);
	    exit(EXIT_SUCCESS);
	}
    }

    /* print a message saying what we're waiting for */
    if (xcverb > OSILENT) {
	if (sloop == 1)
	    fprintf(stderr, "Waiting for one selection request.\n");

	if (sloop < 1)
	    fprintf(stderr,
		    "Waiting for selection requests, Control-C to quit\n");

	if (sloop > 1)
	    fprintf(stderr,
		    "Waiting for %i selection request%s, Control-C to quit\n",
		    sloop,  (sloop==1)?"":"s");
    }

    /* Avoid making the current directory in use, in case it will need to be umounted */
    if (chdir("/") == -1) {
	errperror(3, progname, ": ", "chdir to \"/\"");
	return EXIT_FAILURE;
    }

    /* Jump into the middle of two while loops */
    goto start;

    /* loop and wait for the expected number of
     * SelectionRequest events
     */
    while (dloop < sloop || sloop < 1) {
	if (xcverb >= ODEBUG)
	    fprintf(stderr, "\n========\n");

	/* print messages about what we're waiting for
	 * if not in silent mode
	 */
	if (xcverb > OSILENT) {
	    if (sloop > 1)
		fprintf(stderr, "  Waiting for selection request %i of %i.\n", dloop + 1, sloop);

	    if (sloop == 1)
		fprintf(stderr, "  Waiting for a selection request.\n");

	    if (sloop < 1)
		fprintf(stderr, "  Waiting for selection request number %i\n", dloop + 1);
	}

	/* wait for a SelectionRequest (paste) event */
	while (1) {
	    struct requestor *requestor;
	    Window requestor_id;
	    int finished;

	    if (!XPending(dpy) && wait > 0) {
		tv.tv_sec = wait/1000;
		tv.tv_usec = (wait%1000)*1000;

		/* build fd_set */
		FD_ZERO(&in_fds);
		FD_SET(x11_fd, &in_fds);
		if (!select(x11_fd + 1, &in_fds, 0, 0, &tv)) {
		    XSetSelectionOwner(dpy, sseln, None, CurrentTime);
		    xcmemzero(sel_buf,sel_len);
		    return EXIT_SUCCESS;
		}
	    }

start:

	    XNextEvent(dpy, &evt);

	    if (xcverb >= ODEBUG)
		fprintf(stderr, "\n");

	    if (xcverb >= ODEBUG) {
		fprintf(stderr, "xclip: debug: Received %s event\n",
		    evtstr[evt.type]);
	    }

	    switch (evt.type) {
	    case SelectionRequest:
		requestor_id = evt.xselectionrequest.requestor;
		requestor = get_requestor(requestor_id);
		/* FIXME: ICCCM 2.2: check evt.time and refuse requests from
		 * outside the period of time we have owned the selection. */
		break;
	    case PropertyNotify:
		requestor_id = evt.xproperty.window;
		requestor = get_requestor(requestor_id);
		break;
	    case SelectionClear:
		if (xcverb >= OVERBOSE) {
		    fprintf(stderr, "Lost selection ownership. ");
		    requestor_id = XGetSelectionOwner(dpy, sseln);
		    if (requestor_id == None)
			fprintf(stderr, "(Some other client cleared the selection).\n");
		    else
			fprintf(stderr, "(%s did a copy).\n", xcnamestr(dpy, requestor_id) );
		}
		/* If the client loses ownership(SelectionClear event)
		 * while it has a transfer in progress, it must continue to
		 * service the ongoing transfer until it is completed.
		 * See ICCCM section 2.2.
		 */
		/* Set dloop to force exit after all transfers finish. */
		dloop = sloop;
		/* remove requestors for dead windows */
		clean_requestors();
		/* if there are no more in-progress transfers, force exit */
		if (!requestors) {
		    if (xcverb >= OVERBOSE) {
			fprintf(stderr, "Exiting.\n");
		    }
		    return EXIT_SUCCESS;
		}
		else {
		    if (xcverb >= OVERBOSE) {
			struct requestor *r = requestors;
			int i=0;
			fprintf(stderr, "Requestors: ");
			while (r) {
			    fprintf(stderr, "0x%lx\t", r->cwin);
			    r = r->next;
			    i++;
			}
			fprintf(stderr, "\n");
			fprintf(stderr,
				"Still transfering data to %d requestor%s.\n",
				i, (i==1)?"":"s");
		    }
		}
		continue;	/* Wait for INCR PropertyNotify events */
	    default:
		/* Ignore all other event types */
		if (xcverb >= ODEBUG) {
		    fprintf(stderr,
			    "xclip: debug: Ignoring X event type %d (%s)\n",
			    evt.type, evtstr[evt.type]);
		}
		continue;
	    }

	    if (xcverb >= ODEBUG) {
		fprintf(stderr, "xclip: debug: event was sent by %s\n",
			xcnamestr(dpy, requestor_id) );
		requestor_id=0;
	    }

	    finished = xcin(dpy, &(requestor->cwin), evt, &(requestor->pty),
			    target, sel_buf, sel_len, &(requestor->sel_pos),
			    &(requestor->context), &(requestor->chunk_size));

	    if (finished) {
		del_requestor(requestor);
		break;
	    }
	    if (requestor->cwin == 0) {
		del_requestor(requestor);
		break;
	    }
	}

	dloop++;		/* increment loop counter */
    }

    XSetSelectionOwner(dpy, sseln, None, CurrentTime);
    xcmemzero(sel_buf,sel_len);

    return EXIT_SUCCESS;
}

static void
printSelBuf(FILE * fout, Atom sel_type, unsigned char *sel_buf, size_t sel_len)
{
#ifdef HAVE_ICONV
    Atom html = XInternAtom(dpy, "text/html", True);
#endif

    if (xcverb >= OVERBOSE) {	/* print in verbose mode only */
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

	if (sel_charset != NULL && (cd = iconv_open("UTF-8", sel_charset)) != (iconv_t) - 1) {
	    char *out_buf_start = malloc(sel_len), *in_buf = (char *) sel_buf + 2,
		*out_buf = out_buf_start;
	    size_t in_bytesleft = sel_len - 2, out_bytesleft = sel_len;

	    while (iconv(cd, &in_buf, &in_bytesleft, &out_buf, &out_bytesleft) == -1
		   && errno == E2BIG) {
		fwrite(out_buf_start, sizeof(char), sel_len - out_bytesleft, fout);
		out_buf = out_buf_start;
		out_bytesleft = sel_len;
	    }
	    if (out_buf != out_buf_start)
		fwrite(out_buf_start, sizeof(char), sel_len - out_bytesleft, fout);

	    free(out_buf_start);
	    iconv_close(cd);
	    return;
	}
    }
#endif

    /* otherwise, print the raw buffer out */
    fwrite(sel_buf, sizeof(char), sel_len, fout);
}

static int
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

	    if (context == XCLIB_XCOUT_BAD_TARGET) {
		if (target == XA_UTF8_STRING(dpy)) {
		    /* fallback is needed. set XA_STRING to target and restart the loop. */
		    context = XCLIB_XCOUT_NONE;
		    target = XA_STRING;
		    continue;
		}
		else {
		    /* no fallback available, exit with failure */
		    if (fsecm) {
			/* If user requested -sensitive, then prevent further pastes (even though we failed to paste) */
			XSetSelectionOwner(dpy, sseln, None, CurrentTime);
			/* Clear memory buffer */
			xcmemzero(sel_buf,sel_len);
		    }
		    free(sel_buf);
		    errconvsel(dpy, target, sseln);
		    // errconvsel does not return but exits with EXIT_FAILURE
		}
	    }

	    /* only continue if xcout() is doing something */
	    if (context == XCLIB_XCOUT_NONE)
		break;
	}
    }

    /* remove the last newline character if necessary */
    if (frmnl && sel_len && sel_buf[sel_len - 1] == '\n') {
	sel_len--;
    }

    if (sel_len) {
	/* only print the buffer out, and free it, if it's not
	 * empty
	 */
	printSelBuf(stdout, sel_type, sel_buf, sel_len);

	if (fsecm) {
	    /* If user requested -sensitive, then prevent further pastes */
	    XSetSelectionOwner(dpy, sseln, None, CurrentTime);
	    /* Clear memory buffer */
	    xcmemzero(sel_buf,sel_len);
	}

	if (sseln == XA_STRING) {
	    XFree(sel_buf);
	}
	else {
	    free(sel_buf);
	}
    }

    return EXIT_SUCCESS;
}

int
main(int argc, char *argv[])
{
    /* Declare variables */
    Window win;			/* Window */
    int exit_code;

     /* As a convenience to command-line users, default to -o if stdin
     * is a tty. Will be overriden by -i or if user specifies a
     * filename as input.
     */
    if (isatty(0)) {
      fdiri = F;		/* direction is out */
    }

    /* set up option table. I can't figure out a better way than this to
     * do it while sticking to pure ANSI C. The option and specifier
     * members have a type of volatile char *, so they need to be allocated
     * by strdup or malloc, you can't set them to a string constant at
     * declare time, this is not pure ANSI C apparently, although it does
     * work with gcc
     */

    int i=0;
    /* loop option entry */
    opt_tab[i].option = xcstrdup("-loops");
    opt_tab[i].specifier = xcstrdup(".loops");
    opt_tab[i].argKind = XrmoptionSepArg;
    opt_tab[i].value = (XPointer) NULL;
    i++;

    /* display option entry */
    opt_tab[i].option = xcstrdup("-display");
    opt_tab[i].specifier = xcstrdup(".display");
    opt_tab[i].argKind = XrmoptionSepArg;
    opt_tab[i].value = (XPointer) NULL;
    i++;

    /* selection option entry */
    opt_tab[i].option = xcstrdup("-selection");
    opt_tab[i].specifier = xcstrdup(".selection");
    opt_tab[i].argKind = XrmoptionSepArg;
    opt_tab[i].value = (XPointer) NULL;
    i++;

    /* filter option entry */
    opt_tab[i].option = xcstrdup("-filter");
    opt_tab[i].specifier = xcstrdup(".filter");
    opt_tab[i].argKind = XrmoptionNoArg;
    opt_tab[i].value = (XPointer) xcstrdup(ST);
    i++;

    /* in option entry */
    opt_tab[i].option = xcstrdup("-in");
    opt_tab[i].specifier = xcstrdup(".direction");
    opt_tab[i].argKind = XrmoptionNoArg;
    opt_tab[i].value = (XPointer) xcstrdup("I");
    i++;

    /* out option entry */
    opt_tab[i].option = xcstrdup("-out");
    opt_tab[i].specifier = xcstrdup(".direction");
    opt_tab[i].argKind = XrmoptionNoArg;
    opt_tab[i].value = (XPointer) xcstrdup("O");
    i++;

    /* version option entry */
    opt_tab[i].option = xcstrdup("-version");
    opt_tab[i].specifier = xcstrdup(".print");
    opt_tab[i].argKind = XrmoptionNoArg;
    opt_tab[i].value = (XPointer) xcstrdup("V");
    i++;

    /* help option entry */
    opt_tab[i].option = xcstrdup("-help");
    opt_tab[i].specifier = xcstrdup(".print");
    opt_tab[i].argKind = XrmoptionNoArg;
    opt_tab[i].value = (XPointer) xcstrdup("H");
    i++;

    /* silent option entry */
    opt_tab[i].option = xcstrdup("-silent");
    opt_tab[i].specifier = xcstrdup(".olevel");
    opt_tab[i].argKind = XrmoptionNoArg;
    opt_tab[i].value = (XPointer) xcstrdup("S");
    i++;

    /* quiet option entry */
    opt_tab[i].option = xcstrdup("-quiet");
    opt_tab[i].specifier = xcstrdup(".olevel");
    opt_tab[i].argKind = XrmoptionNoArg;
    opt_tab[i].value = (XPointer) xcstrdup("Q");
    i++;

    /* verbose option entry */
    opt_tab[i].option = xcstrdup("-verbose");
    opt_tab[i].specifier = xcstrdup(".olevel");
    opt_tab[i].argKind = XrmoptionNoArg;
    opt_tab[i].value = (XPointer) xcstrdup("V");
    i++;

    /* debug option entry */
    opt_tab[i].option = xcstrdup("-debug");
    opt_tab[i].specifier = xcstrdup(".olevel");
    opt_tab[i].argKind = XrmoptionNoArg;
    opt_tab[i].value = (XPointer) xcstrdup("D");
    i++;

    /* utf8 option entry */
    opt_tab[i].option = xcstrdup("-noutf8");
    opt_tab[i].specifier = xcstrdup(".noutf8");
    opt_tab[i].argKind = XrmoptionNoArg;
    opt_tab[i].value = (XPointer) xcstrdup("N");
    i++;

    /* target option entry */
    opt_tab[i].option = xcstrdup("-target");
    opt_tab[i].specifier = xcstrdup(".target");
    opt_tab[i].argKind = XrmoptionSepArg;
    opt_tab[i].value = (XPointer) NULL;
    i++;

    /* "remove newline if it is the last character" entry */
    opt_tab[i].option = xcstrdup("-rmlastnl");
    opt_tab[i].specifier = xcstrdup(".rmlastnl");
    opt_tab[i].argKind = XrmoptionNoArg;
    opt_tab[i].value = (XPointer) xcstrdup(ST);
    i++;

    /* sensitive mode for pasting passwords */
    opt_tab[i].option = xcstrdup("-sensitive");
    opt_tab[i].specifier = xcstrdup(".sensitive");
    opt_tab[i].argKind = XrmoptionNoArg;
    opt_tab[i].value = (XPointer) xcstrdup("s");
    i++;

    /* wait option entry */
    opt_tab[i].option = xcstrdup("-wait");
    opt_tab[i].specifier = xcstrdup(".wait");
    opt_tab[i].argKind = XrmoptionSepArg;
    opt_tab[i].value = (XPointer) NULL;
    i++;

    /* save size of opt_tab for doOptMain to use */
    opt_tab_size = i;
    if ( ( sizeof(opt_tab) / sizeof(opt_tab[0]) ) < opt_tab_size ) {
	fprintf(stderr,
		"xclip: programming error: opt_tab declared to hold %ld options, but %d defined\n",
		sizeof(opt_tab) / sizeof(opt_tab[0]), opt_tab_size);
	return EXIT_FAILURE;
    }
		

    /* parse command line options */
    doOptMain(argc, argv);

    /* Connect to the X server. */
    if ((dpy = XOpenDisplay(sdisp))) {
	/* successful */
	if (xcverb >= ODEBUG)
	    fprintf(stderr, "Connected to X server.\n");
    }
    else {
	/* couldn't connect to X server. Print error and exit */
	errxdisplay(sdisp);
    }

    /* parse selection command line option */
    doOptSel();

    /* parse noutf8 and target command line options */
    doOptTarget();

    /* Create a window to trap events */
    win = XCreateSimpleWindow(dpy, DefaultRootWindow(dpy), 0, 0, 1, 1, 0, 0, 0);

    /* get events about property changes */
    XSelectInput(dpy, win, PropertyChangeMask);

    /* If we get an X error, catch it instead of barfing */
    XSetErrorHandler(xchandler);

    if (fdiri)
	exit_code = doIn(win, argv[0]);
    else
	exit_code = doOut(win);

    /* Disconnect from the X server */
    XCloseDisplay(dpy);

    /* exit */
    return exit_code;
}
