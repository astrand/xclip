/*
 *
 *
 *  borked.c - emulate ways X selections can break for testing.
 *  Copyright (C) 2020 Hackerb9, Peter Åstrand
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

// gcc -g -O2 borked.c xclib.o xcprint.o -o borked -lXmu -lX11

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <err.h>
#ifdef HAVE_ICONV
#include <errno.h>
#include <iconv.h>
#endif
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xmu/Atoms.h>
#include <X11/Xmu/Error.h>
#include "xcdef.h"
#include "xcprint.h"
#include "xclib.h"


/* Globals */
Display *dpy;			/* connection to X11 display */
Atom sseln = XA_PRIMARY;	/* X selection to work with */
Atom target = XA_STRING;	/* Gets changed to UTF8_STRING later. */

static int
outReadAndDie(Window win)
{
    /* Start reading (paste) but then suddenly quit in the middle.  */

    Atom sel_type = None;
    unsigned char *sel_buf;	/* buffer for selection data */
    unsigned long sel_len = 0;	/* length of sel_buf */
    XEvent evt;			/* X Event Structures */
    unsigned int context = XCLIB_XCOUT_NONE;

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
		XSetSelectionOwner(dpy, sseln, None, CurrentTime);
		xcmemzero(sel_buf,sel_len);
		errconvsel(dpy, target, sseln);
		// errconvsel does not return but exits with EXIT_FAILURE
	    }
	}

	/* normally, we'd only continue if xcout() is doing something */
	if (context == XCLIB_XCOUT_NONE)
	    break;

	/* but since we're intentionally borken, we'll always exit! */
	break;
    }

    printf("Read %ld bytes and quit in the middle\n", sel_len) ;

    return EXIT_SUCCESS;
}


static int
outReadAndSteal(Window win)
{
    /* Start reading (paste) but then suddenly set selection owner to None.  */

    Atom sel_type = None;
    unsigned char *sel_buf;	/* buffer for selection data */
    unsigned long sel_len = 0;	/* length of sel_buf */
    XEvent evt;			/* X Event Structures */
    unsigned int context = XCLIB_XCOUT_NONE;

    while (1) {
	/* only get an event if xcout() is doing something */
	if (context != XCLIB_XCOUT_NONE)
	    XNextEvent(dpy, &evt);

	/* fetch the selection, or part of it */
	xcout(dpy, win, evt, sseln, target, &sel_type, &sel_buf, &sel_len, &context);

	printf("Read %ld bytes\n", sel_len);

	if (context == XCLIB_XCOUT_BAD_TARGET) {
	    if (target == XA_UTF8_STRING(dpy)) {
		/* fallback is needed. set XA_STRING to target and restart the loop. */
		context = XCLIB_XCOUT_NONE;
		target = XA_STRING;
		continue;
	    }
	    else {
		/* no fallback available, exit with failure */
		XSetSelectionOwner(dpy, sseln, None, CurrentTime);
		xcmemzero(sel_buf,sel_len);
		errconvsel(dpy, target, sseln);
		// errconvsel does not return but exits with EXIT_FAILURE
	    }
	}

	/* normally, we'd only continue if xcout() is doing something */
	if (context == XCLIB_XCOUT_NONE)
	    break;

	/* but since we're intentionally borken, we'll steal the selection! */
	XSetSelectionOwner(dpy, sseln, None, CurrentTime);
	printf("Selection owner set to None.\n");
    }

    return EXIT_SUCCESS;
}


static int
outReadAndHang(Window win)
{
    /* Start reading (paste) but then just hang forever.  */

    Atom sel_type = None;
    unsigned char *sel_buf;	/* buffer for selection data */
    unsigned long sel_len = 0;	/* length of sel_buf */
    XEvent evt;			/* X Event Structures */
    unsigned int context = XCLIB_XCOUT_NONE;

    while (1) {
	/* only get an event if xcout() is doing something */
	if (context != XCLIB_XCOUT_NONE)
	    XNextEvent(dpy, &evt);

	/* fetch the selection, or part of it */
	xcout(dpy, win, evt, sseln, target, &sel_type, &sel_buf, &sel_len, &context);

	printf("Read %ld bytes\n", sel_len);

	if (context == XCLIB_XCOUT_BAD_TARGET) {
	    if (target == XA_UTF8_STRING(dpy)) {
		/* fallback is needed. set XA_STRING to target and restart the loop. */
		context = XCLIB_XCOUT_NONE;
		target = XA_STRING;
		continue;
	    }
	    else {
		/* no fallback available, exit with failure */
		XSetSelectionOwner(dpy, sseln, None, CurrentTime);
		xcmemzero(sel_buf,sel_len);
		errconvsel(dpy, target, sseln);
		// errconvsel does not return but exits with EXIT_FAILURE
	    }
	}

	/* normally, we'd only continue if xcout() is doing something */
	if (context == XCLIB_XCOUT_NONE)
	    break;

	/* but since we're intentionally borken, we'll hang forever! */
	printf("Selection now hanging forever. Hit ^C to cancel.\n");
	while (1)
	    sleep(1);
    }

    return EXIT_SUCCESS;
}

static int
inHang(Window win)
{
    /* Own the PRIMARY selection */
    XSetSelectionOwner(dpy, XA_PRIMARY, None, CurrentTime);

    /* just hang forever.  */
    printf("Selection now hanging forever. Hit ^C to cancel.\n");
    while (1)
	sleep(1);

    return EXIT_SUCCESS;
}

static int
inPasteHang(Window win)
{
    /*                 */
    printf("Note implemented yet\n");
    // XXXX

//    printf("Selection now hanging forever. Hit ^C to cancel.\n");
//    while (1)
//	sleep(1);

    return EXIT_SUCCESS;
}

    // X11R5 spec said each request header was 4 bytes.
    // Experimentally, I'm finding that it is 32-bytes. 20 October 2020.
    // X11/SelectionI.h:MAX_SELECTION_INCR leaves room for 100 bytes.
#define MAX_SELECTION_INCR(dpy) (              \
	((65536 < XMaxRequestSize(dpy)) ?      \
	 (65536 << 2)  :		       \
	 (XMaxRequestSize(dpy) << 2))-100)

static int
inChunkTooLarge(Window win) {
    Window theirwin;
    XEvent evt;
    Atom pty;
    unsigned long sel_pos = 0;
    unsigned int context = XCLIB_XCIN_NONE;;

    long maxreq = XExtendedMaxRequestSize(dpy);
    maxreq = maxreq?maxreq:XMaxRequestSize(dpy);
    printf("maxreq is %ld words (%ld bytes)\n", maxreq, 4*maxreq);

    // Normally chunk_size = 4*maxreq - 1024.
    // But we WANT to cause an error.
    long chunk_size = 4*maxreq;
    printf("chunk_size is %ld bytes\n", chunk_size);
    unsigned char *sel_buf = malloc(chunk_size);
    unsigned long sel_len = (chunk_size);

    if (4*maxreq >= chunk_size + 32) {
	printf("OOOPS! chunk_size fits in maxreq, will not cause error.\n");
    }

    /* Own the PRIMARY selection */
    XSetSelectionOwner(dpy, XA_PRIMARY, win, CurrentTime);
    if (XGetSelectionOwner(dpy, XA_PRIMARY) == win) {
	printf("We now own PRIMARY\n");
    }
    else {
	printf("Humph, selection owned by 0x%lx\n",
	       XGetSelectionOwner(dpy, XA_PRIMARY));
	return EXIT_FAILURE;
    }

    while (1) {
	printf("\nWaiting for an event\n");
	XNextEvent(dpy, &evt); /* Wait until next request comes in */
	printf("Received %s event\n", evtstr[evt.type]);

	switch (evt.type) {
	case SelectionRequest:
	    theirwin = evt.xselectionrequest.requestor;
	    break;
	case PropertyNotify:
	    theirwin = evt.xproperty.window;
	    break;
	case SelectionClear:
	    printf("Lost selection ownership.\n");
	    return 0;
	default:
	    /* Ignore all other event types */
	    printf("Ignoring X event type %d (%s)\n",
		   evt.type, evtstr[evt.type]);
	    continue;
	}

	xcin(dpy, win, &theirwin,
	     evt, &pty,
	     target, sel_buf, sel_len,
	     &sel_pos, &context, &chunk_size);

    }

    return EXIT_SUCCESS;
}





int
main(int argc, char *argv[])
{
    XSetErrorHandler(xchandler);
    xcverb = ODEBUG;

    /* Declare variables */
    Window win;			/* Window */

    int mode = 0;		/* What sort of brokenness to emulate */
    if (argc > 1)
	mode = atoi(argv[1]);

    /* Connect to the X server. */
    if (! (dpy = XOpenDisplay(NULL)) ) {
	/* couldn't connect to X server. Print error and exit */
	errxdisplay(NULL);
    }

    /* Use UTF8_STRING since that's xclip's default */
    target = XA_UTF8_STRING(dpy);

    /* Create a window to trap events */
    win = XCreateSimpleWindow(dpy, DefaultRootWindow(dpy),
			      0, 0, 1, 1, 0, 0, 0);

    /* get events about property changes */
    XSelectInput(dpy, win, PropertyChangeMask);

    switch (mode) {
    case 0:
	printf("Mode 0: start reading X selection for paste but then exit suddenly.\n");
	outReadAndDie(win);
	break;
    case 1:
	printf("Mode 1: start reading X selection (paste), but then steal selection (copy).\n");
	outReadAndSteal(win);
	break;
    case 2:
	printf("Mode 2: start reading X selection (paste), but then just hang forever.\n");
	outReadAndHang(win);
	break;
    case 10:
	printf("Mode 10: own selection, but never reply to paste requests.\n");
	inHang(win);
	break;
    case 11:
	printf("Mode 11: own selection, start paste response, but then hang.\n");
	inPasteHang(win);
	break;
    case 12:
	printf("Mode 12: own selection, but call xcin with too large a chunk.\n");
	inChunkTooLarge(win);
	break;
    default:
	errx(1, "Unknown mode number '%s'", argv[1]);
    }

    /* Disconnect from the X server */
    printf("Closing the X Display\n");
    XCloseDisplay(dpy);

    /* exit */
    return 0;
}
