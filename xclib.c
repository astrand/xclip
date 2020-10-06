/*
 *  
 * 
 *  xclib.c - xclip library to look after xlib mechanics for xclip
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
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include "xcdef.h"
#include "xcprint.h"
#include "xclib.h"

/* global verbosity output level, defaults to OSILENT */
int xcverb = OSILENT;

/* Table of event names from event numbers */
const char *evtstr[LASTEvent] = {
    "ProtocolError", "ProtocolReply", "KeyPress", "KeyRelease",
    "ButtonPress", "ButtonRelease", "MotionNotify", "EnterNotify",
    "LeaveNotify", "FocusIn", "FocusOut", "KeymapNotify", "Expose",
    "GraphicsExpose", "NoExpose", "VisibilityNotify", "CreateNotify",
    "DestroyNotify", "UnmapNotify", "MapNotify", "MapRequest",
    "ReparentNotify", "ConfigureNotify", "ConfigureRequest",
    "GravityNotify", "ResizeRequest", "CirculateNotify",
    "CirculateRequest", "PropertyNotify", "SelectionClear",
    "SelectionRequest", "SelectionNotify", "ColormapNotify",
    "ClientMessage", "MappingNotify", "GenericEvent", };

/* a memset function that won't be optimized away by compler */
void 
xcmemzero(void *ptr, size_t len)
{
    if (xcverb >= ODEBUG) {
	fprintf(stderr, "xclip: debug: Zeroing memory buffer\n");
    }
    memset_func(ptr, 0, len);
}

/* check a pointer to allocated memory, print an error if it's null */
void
xcmemcheck(void *ptr)
{
    if (ptr == NULL)
	errmalloc();
}

/* wrapper for malloc that checks for errors */
void *
xcmalloc(size_t size)
{
    void *mem;

    mem = malloc(size);
    xcmemcheck(mem);

    return (mem);
}

/* wrapper for realloc that checks for errors */
void *
xcrealloc(void *ptr, size_t size)
{
    void *mem;

    mem = realloc(ptr, size);
    xcmemcheck(mem);

    return (mem);
}

/* a strdup() implementation since ANSI C doesn't include strdup() */
void *
xcstrdup(const char *string)
{
    void *mem;

    /* allocate a buffer big enough to hold the characters and the
     * null terminator, then copy the string into the buffer
     */
    mem = xcmalloc(strlen(string) + sizeof(char));
    strcpy(mem, string);

    return (mem);
}

/* Returns the machine-specific number of bytes per data element
 * returned by XGetWindowProperty */
static size_t
mach_itemsize(int format)
{
    if (format == 8)
	return sizeof(char);
    if (format == 16)
	return sizeof(short);
    if (format == 32)
	return sizeof(long);
    return 0;
}

/* Retrieves the contents of a selections. Arguments are:
 *
 * A display that has been opened.
 * 
 * A window
 * 
 * An event to process
 * 
 * The selection to return
 * 
 * The target(UTF8_STRING or XA_STRING) to return 
 *
 * A pointer to an atom that receives the type of the data
 *
 * A pointer to a char array to put the selection into.
 * 
 * A pointer to a long to record the length of the char array
 *
 * A pointer to an int to record the context in which to process the event
 *
 * Return value is 1 if the retrieval of the selection data is complete,
 * otherwise it's 0.
 */
int
xcout(Display * dpy,
      Window win,
      XEvent evt, Atom sel, Atom target, Atom * type, unsigned char **txt, unsigned long *len,
      unsigned int *context)
{
    /* a property for other windows to put their selection into */
    static Atom pty;
    static Atom inc;
    int pty_format;

    /* buffer for XGetWindowProperty to dump data into */
    unsigned char *buffer;
    unsigned long pty_size, pty_items, pty_machsize;

    /* local buffer of text to return */
    unsigned char *ltxt = *txt;

    if (!pty) {
	pty = XInternAtom(dpy, "XCLIP_OUT", False);
    }

    if (!inc) {
	inc = XInternAtom(dpy, "INCR", False);
    }

    switch (*context) {
	/* there is no context, do an XConvertSelection() */
    case XCLIB_XCOUT_NONE:
	/* initialise return length to 0 */
	if (*len > 0) {
	    free(*txt);
	    *len = 0;
	}

	/* send a selection request */
	XConvertSelection(dpy, sel, target, pty, win, CurrentTime);
	*context = XCLIB_XCOUT_SENTCONVSEL;
	return (0);

    case XCLIB_XCOUT_SENTCONVSEL:
	if (evt.type != SelectionNotify)
	    return (0);

	/* return failure when the current target failed */
	if (evt.xselection.property == None) {
	    *context = XCLIB_XCOUT_BAD_TARGET;
	    return (0);
	}

	/* find the size and format of the data in property */
	XGetWindowProperty(dpy,
			   win,
			   pty,
			   0,
			   0,
			   False,
			   AnyPropertyType, type, &pty_format, &pty_items, &pty_size, &buffer);
	XFree(buffer);

	if (*type == inc) {
	    /* start INCR mechanism by deleting property */
	    if (xcverb >= OVERBOSE) {
		fprintf(stderr,
			"xclib: debug: Starting INCR by deleting property\n");
	    }
	    XDeleteProperty(dpy, win, pty);
	    XFlush(dpy);
	    *context = XCLIB_XCOUT_INCR;
	    return (0);
	}

	/* not using INCR mechanism, just read the property */
	XGetWindowProperty(dpy,
			   win,
			   pty,
			   0,
			   (long) pty_size,
			   False,
			   AnyPropertyType, type, &pty_format, &pty_items, &pty_size, &buffer);

	/* finished with property, delete it */
	XDeleteProperty(dpy, win, pty);

	/* compute the size of the data buffer we received */
	pty_machsize = pty_items * mach_itemsize(pty_format);

	/* copy the buffer to the pointer for returned data */
	ltxt = (unsigned char *) xcmalloc(pty_machsize);
	memcpy(ltxt, buffer, pty_machsize);

	/* set the length of the returned data */
	*len = pty_machsize;
	*txt = ltxt;

	/* free the buffer */
	XFree(buffer);

	*context = XCLIB_XCOUT_NONE;

	/* complete contents of selection fetched, return 1 */
	return (1);

    case XCLIB_XCOUT_INCR:
	/* To use the INCR method, we basically delete the
	 * property with the selection in it, wait for an
	 * event indicating that the property has been created,
	 * then read it, delete it, etc.
	 */

	/* make sure that the event is relevant */
	if (evt.type != PropertyNotify)
	    return (0);

	/* skip unless the property has a new value */
	if (evt.xproperty.state != PropertyNewValue)
	    return (0);

	/* check size and format of the property */
	XGetWindowProperty(dpy,
			   win,
			   pty,
			   0,
			   0,
			   False,
			   AnyPropertyType,
			   type, &pty_format, &pty_items, &pty_size, (unsigned char **) &buffer);

	if (pty_size == 0) {
	    /* no more data, exit from loop */
	    if (xcverb >= ODEBUG) {
		fprintf(stderr, "INCR transfer complete\n");
	    }
	    XFree(buffer);
	    XDeleteProperty(dpy, win, pty);
	    *context = XCLIB_XCOUT_NONE;

	    /* this means that an INCR transfer is now
	     * complete, return 1
	     */
	    return (1);
	}

	XFree(buffer);

	/* if we have come this far, the property contains
	 * text, we know the size.
	 */
	XGetWindowProperty(dpy,
			   win,
			   pty,
			   0,
			   (long) pty_size,
			   False,
			   AnyPropertyType,
			   type, &pty_format, &pty_items, &pty_size, (unsigned char **) &buffer);

	/* compute the size of the data buffer we received */
	pty_machsize = pty_items * mach_itemsize(pty_format);

	/* allocate memory to accommodate data in *txt */
	if (*len == 0) {
	    *len = pty_machsize;
	    ltxt = (unsigned char *) xcmalloc(*len);
	}
	else {
	    *len += pty_machsize;
	    ltxt = (unsigned char *) xcrealloc(ltxt, *len);
	}

	/* add data to ltxt */
	memcpy(&ltxt[*len - pty_machsize], buffer, pty_machsize);

	*txt = ltxt;
	XFree(buffer);

	/* delete property to get the next item */
	XDeleteProperty(dpy, win, pty);
	XFlush(dpy);
	return (0);
    }

    return (0);
}

/* put data into a selection, in response to a SelectionRequest event from
 * another window (and any subsequent events relating to an INCR transfer).
 *
 * Arguments are:
 *
 * A display
 * 
 * A window
 * 
 * The event to respond to
 * 
 * A pointer to an Atom. This gets set to the property nominated by the other
 * app in it's SelectionRequest. Things are likely to break if you change the
 * value of this yourself.
 * 
 * The target(UTF8_STRING or XA_STRING) to respond to
 *
 * A pointer to an array of chars to read selection data from.
 * 
 * The length of the array of chars.
 *
 * In the case of an INCR transfer, the position within the array of chars
 * that is being processed.
 *
 * The context that event is the be processed within.
 */
int
xcin(Display * dpy,
     Window * win,
     XEvent evt,
     Atom * pty, Atom target, unsigned char *txt, unsigned long len, unsigned long *pos,
     unsigned int *context, long *chunk_size)
{
    unsigned long chunk_len;	/* length of current chunk (for incr
				 * transfers only)
				 */
    XEvent res;			/* response to event */
    static Atom inc;
    static Atom targets;

    if (!targets) {
	targets = XInternAtom(dpy, "TARGETS", False);
    }

    if (!inc) {
	inc = XInternAtom(dpy, "INCR", False);
    }

    /* We consider selections larger than a quarter of the maximum
       request size to be "large". See ICCCM section 2.5 */
    if (!(*chunk_size)) {
	*chunk_size = XExtendedMaxRequestSize(dpy) / 4;
	if (!(*chunk_size)) {
	    *chunk_size = XMaxRequestSize(dpy) / 4;
	}
	if ( xcverb >= ODEBUG ) {
	    fprintf(stderr,
		    "xclib: debug: INCR chunk size is %ld\n", (*chunk_size));
	}
    }

    switch (*context) {
    case XCLIB_XCIN_NONE:
	if ( xcverb >= ODEBUG )
	    fprintf(stderr, "xclib: debug: context: XCLIB_XCIN_NONE\n");

	if ( xcverb >= ODEBUG  &&  evt.xselectionrequest.target) {
	    char *tempstr = XGetAtomName(dpy, evt.xselectionrequest.target);
	    fprintf(stderr, "xclib: debug: target: %s\n", tempstr);
	    XFree(tempstr);
	}

	if (evt.type != SelectionRequest) {
	    if ( xcverb >= ODEBUG ) {
		fprintf(stderr,
			"xclib: debug: ignoring %s event (context is NONE)\n",
			evtstr[evt.type]);
	    }
	    return (0);
	}
	/* set the window and property that is being used */
	*win = evt.xselectionrequest.requestor;
	*pty = evt.xselectionrequest.property;

	/* reset position to 0 */
	*pos = 0;

	/* put the data into a property */
	if (evt.xselectionrequest.target == targets) {
	    Atom types[2] = { targets, target };

	    if ( xcverb >= ODEBUG ) {
		fprintf(stderr, "xclib: debug: sending list of TARGETS\n");
	    }

	    /* send data all at once (not using INCR) */
	    XChangeProperty(dpy,
			    *win,
			    *pty,
			    XA_ATOM,
			    32, PropModeReplace, (unsigned char *) types,
			    (int) (sizeof(types) / sizeof(Atom))
		);
	}
	else if (len > *chunk_size) {
	    /* send INCR response */
	    if ( xcverb >= ODEBUG ) {
		fprintf (stderr, "xclib: debug: Starting INCR response\n");
	    }
	    XChangeProperty(dpy, *win, *pty, inc, 32, PropModeReplace, 0, 0);

	    /* With the INCR mechanism, we need to know
	     * when the requestor window changes (deletes)
	     * its properties
	     */
	    XSelectInput(dpy, *win, PropertyChangeMask);

	    *context = XCLIB_XCIN_INCR;
	}
	else {
	    /* send data all at once (not using INCR) */
	    if ( xcverb >= ODEBUG ) {
		fprintf(stderr, "xclib: debug: Sending data all at once"
			" (%d bytes)\n", (int) len);
	    }

	    XChangeProperty(dpy,
			    *win,
			    *pty,
			    target,
			    8, PropModeReplace, (unsigned char *) txt,
			    (int) len);
	}

	/* Perhaps FIXME: According to ICCCM section 2.5, we should
	   confirm that XChangeProperty succeeded without any Alloc
	   errors before replying with SelectionNotify. However, doing
	   so would require an error handler which modifies a global
	   variable, plus doing XSync after each XChangeProperty. */

	/* set values for the response event */
	res.xselection.property = *pty;
	res.xselection.type = SelectionNotify;
	res.xselection.display = evt.xselectionrequest.display;
	res.xselection.requestor = *win;
	res.xselection.selection = evt.xselectionrequest.selection;
	res.xselection.target = evt.xselectionrequest.target;
	res.xselection.time = evt.xselectionrequest.time;

	/* send the response event */
	XSendEvent(dpy, evt.xselectionrequest.requestor, 0, 0, &res);
	XFlush(dpy);

	/* don't treat TARGETS request as contents request */
	if (evt.xselectionrequest.target == targets)
	    return (1);		/* Finished with request */

	/* if len <= chunk_size, then the data was sent all at
	 * once and the transfer is now complete, return 1
	 */
	if (len > *chunk_size)
	    return (0);
	else
	    return (1);

	break;

    case XCLIB_XCIN_INCR:
	/* length of current chunk */

	/* ignore non-property events */
	if (evt.type != PropertyNotify)
	    return (0);

	/* ignore the event unless it's to report that the
	 * property has been deleted
	 */
	if (evt.xproperty.state != PropertyDelete) {
	    if ( xcverb >= ODEBUG ) {
		if ( evt.xproperty.state == 0 ) 
		    fprintf(stderr,
			    "xclib: debug: ignoring PropertyNewValue\n");
		else
		    fprintf(stderr,
			    "xclib: debug: ignoring state %d\n",
			    evt.xproperty.state);
	    }
	    return (0);
	}

	/* set the chunk length to the maximum size */
	chunk_len = *chunk_size;

	/* if a chunk length of maximum size would extend
	 * beyond the end ot txt, set the length to be the
	 * remaining length of txt
	 */
	if ((*pos + chunk_len) > len)
	    chunk_len = len - *pos;

	/* if the start of the chunk is beyond the end of txt,
	 * then we've already sent all the data, so set the
	 * length to be zero
	 */
	if (*pos > len)
	    chunk_len = 0;

	if (chunk_len) {
	    /* put the chunk into the property */
	    if ( xcverb >= ODEBUG ) {
		fprintf(stderr, "xclib: debug: Sending chunk of "
			" %d bytes\n", (int) chunk_len);
	    }
	    XChangeProperty(dpy,
			    *win,
			    *pty,
			    target,
			    8, PropModeReplace, &txt[*pos],
			    (int) chunk_len);
	}
	else {
	    /* make an empty property to show we've
	     * finished the transfer
	     */
	    if ( xcverb >= ODEBUG ) {
		fprintf(stderr, "xclib: debug: Signalling end of INCR\n");
	    }
	    XChangeProperty(dpy, *win, *pty, target, 8, PropModeReplace, 0, 0);
	}
	XFlush(dpy);

	/* all data has been sent, break out of the loop */
	if (!chunk_len) {
	    if (xcverb >= ODEBUG) {
		fprintf(stderr, "xclib: debug: Finished INCR transfer.\n");
	    }
	    *context = XCLIB_XCIN_NONE;
	}

	*pos += *chunk_size;

	/* if chunk_len == 0, we just finished the transfer,
	 * return 1
	 */
	if (chunk_len > 0)
	    return (0);
	else
	    return (1);
	break;
    }
    return (0);
}


/* xcfetchname(): a utility for finding the name of a given X window.
 * (Like XFetchName but recursively walks up tree of parent windows.)
 * Sets namep to point to the string of the name (must be freed with XFree).
 * Returns 0 if it works. Not 0, otherwise.
 * [See also, xcnamestr() wrapper below.]
 */
int
xcfetchname(Display *display, Window w, char **namep) {
    *namep = NULL;
    if (w == None)
	return 1;		/* No window, no name. */

    XFetchName(display, w, namep);
    if (*namep)
	return 0; 		/* Hurrah! It worked on the first try. */

    /* Otherwise, recursively try the parent windows */
    Window p = w;
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


/* A convenience wrapper for xcfetchname() that returns a string so it can be
 * used within printf calls with no need to later XFree anything.
 * It also smoothes over problems by returning the ID number if no name exists.
 *
 * Given a Window ID number, e.g., 0xfa1afe1, return
 * either the window name followed by the ID in parens, IFF it can be found,
 * otherwise, the string "window id 0xfa1afe1". 
 *
 * Example output:  "'Falafel' (0xfa1afe1)"
 * Example output:  "window id 0xfa1afe1"
 *
 * String is statically allocated and is updated at each call.
 */
char xcname[4096];
char *
xcnamestr(Display *display, Window w) {
    char *window_name;
    xcfetchname(display, w, &window_name);
    if (window_name && window_name[0]) {
	snprintf( xcname, sizeof(xcname)-1, "'%s' (0x%lx)", window_name, w);
    }
    else {
	snprintf( xcname, sizeof(xcname)-1, "window id 0x%lx", w );
    }
    if (window_name)
	XFree(window_name);

    xcname[sizeof(xcname) - 1] = '\0'; /* Ensure NULL termination */
    return xcname;
}


/* Xlib Error handler that saves last error event */
/* Usage: XSetErrorHandler(xchandler); */
int xcerrflag = False;
XErrorEvent xcerrevt;
int xchandler(Display *dpy, XErrorEvent *evt) {
    xcerrflag = True;
    xcerrevt = *evt;

    int len=255;
    char buf[len+1];
    XGetErrorText(dpy, evt->error_code, buf, len);
    if (xcverb >= OVERBOSE) {
	fprintf(stderr, "\tXErrorHandler: XError (type %d): %s\n",
		evt->type, buf);
    }
    if (xcverb >= ODEBUG) {
	fprintf(stderr, 
		"\t\tEvent Type: %d\n"
		"\t\tResource ID: 0x%lx\n"
		"\t\tSerial Num: %lu\n"
		"\t\tError code: %u\n"
		"\t\tRequest op code: %u major, %u minor\n",
		evt->type, 
		evt->resourceid, 
		evt->serial, 
		evt->error_code, 
		evt->request_code, 
		evt->minor_code);
    }

    return 0;
}


