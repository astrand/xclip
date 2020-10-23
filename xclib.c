/*
 *
 *
 *  xclib.c - xclip library to look after xlib mechanics for xclip
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
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xproto.h>
#include <X11/Xmu/Error.h>
#include "xcdef.h"
#include "xcprint.h"
#include "xclib.h"

/* Global verbosity output level, defaults to OSILENT */
int xcverb = OSILENT;

/* Global variables that get used/set by xchandler() */
Window xcourwin = 0;	     /* Our Window. Set by xcin, used by xchandler). */
int xcerrflag = False;	     /* Set to True in xchandler() */
XErrorEvent xcerrevt;	     /* Set to latest event in xchandler() */

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

/* Table of Requests indexed by op-code (from Xproto.h) */
const char *reqstr[X_NoOperation+1] = { "0",
    "X_CreateWindow", "X_ChangeWindowAttributes", "X_GetWindowAttributes",
    "X_DestroyWindow", "X_DestroySubwindows", "X_ChangeSaveSet",
    "X_ReparentWindow", "X_MapWindow", "X_MapSubwindows", "X_UnmapWindow",
    "X_UnmapSubwindows", "X_ConfigureWindow", "X_CirculateWindow",
    "X_GetGeometry", "X_QueryTree", "X_InternAtom", "X_GetAtomName",
    "X_ChangeProperty", "X_DeleteProperty", "X_GetProperty",
    "X_ListProperties", "X_SetSelectionOwner", "X_GetSelectionOwner",
    "X_ConvertSelection", "X_SendEvent", "X_GrabPointer", "X_UngrabPointer",
    "X_GrabButton", "X_UngrabButton", "X_ChangeActivePointerGrab",
    "X_GrabKeyboard", "X_UngrabKeyboard", "X_GrabKey", "X_UngrabKey",
    "X_AllowEvents", "X_GrabServer", "X_UngrabServer", "X_QueryPointer",
    "X_GetMotionEvents", "X_TranslateCoords", "X_WarpPointer",
    "X_SetInputFocus", "X_GetInputFocus", "X_QueryKeymap", "X_OpenFont",
    "X_CloseFont", "X_QueryFont", "X_QueryTextExtents", "X_ListFonts",
    "X_ListFontsWithInfo", "X_SetFontPath", "X_GetFontPath", "X_CreatePixmap",
    "X_FreePixmap", "X_CreateGC", "X_ChangeGC", "X_CopyGC", "X_SetDashes",
    "X_SetClipRectangles", "X_FreeGC", "X_ClearArea", "X_CopyArea",
    "X_CopyPlane", "X_PolyPoint", "X_PolyLine", "X_PolySegment",
    "X_PolyRectangle", "X_PolyArc", "X_FillPoly", "X_PolyFillRectangle",
    "X_PolyFillArc", "X_PutImage", "X_GetImage", "X_PolyText8",
    "X_PolyText16", "X_ImageText8", "X_ImageText16", "X_CreateColormap",
    "X_FreeColormap", "X_CopyColormapAndFree", "X_InstallColormap",
    "X_UninstallColormap", "X_ListInstalledColormaps", "X_AllocColor",
    "X_AllocNamedColor", "X_AllocColorCells", "X_AllocColorPlanes",
    "X_FreeColors", "X_StoreColors", "X_StoreNamedColor", "X_QueryColors",
    "X_LookupColor", "X_CreateCursor", "X_CreateGlyphCursor", "X_FreeCursor",
    "X_RecolorCursor", "X_QueryBestSize", "X_QueryExtension",
    "X_ListExtensions", "X_ChangeKeyboardMapping", "X_GetKeyboardMapping",
    "X_ChangeKeyboardControl", "X_GetKeyboardControl", "X_Bell",
    "X_ChangePointerControl", "X_GetPointerControl", "X_SetScreenSaver",
    "X_GetScreenSaver", "X_ChangeHosts", "X_ListHosts", "X_SetAccessControl",
    "X_SetCloseDownMode", "X_KillClient", "X_RotateProperties",
    "X_ForceScreenSaver", "X_SetPointerMapping", "X_GetPointerMapping",
    "X_SetModifierMapping", "X_GetModifierMapping", "120", "121", "122",
    "123", "124", "125", "126", "X_NoOperation",
};



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
	if (evt.type != SelectionNotify) {
	    if ( xcverb >= ODEBUG ) {
		fprintf(stderr,"xclib: debug: SENTCONVSEL: "
			"ignoring %s event\n", evtstr[evt.type]);
	    }
	    return (0);
	}

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
			"xclib: debug: SENTCONVSEL: "
			"Starting INCR by deleting property\n");
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

	/* return failure if selection owner signals something went wrong */
	if (evt.type == SelectionNotify && evt.xselection.property == None) {
	    *context = XCLIB_XCOUT_SELECTION_REFUSED;
	    return (0);
	}

	/* make sure that the event is relevant */
	if (evt.type != PropertyNotify) {
	    if ( xcverb >= ODEBUG ) {
		fprintf(stderr, "xclib: debug: INCR: ignoring %s event\n",
			evtstr[evt.type]);
	    }
	    return (0);
	}

	/* skip unless the property has a new value */
	if (evt.xproperty.state != PropertyNewValue) {
	    if ( xcverb >= ODEBUG ) {
		fprintf(stderr, "xclib: debug: INCR: "
			"Property does not have a new value\n");
	    }
	    return (0);
	}

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



/* xcin:
 * put data into a selection, in response to a SelectionRequest event from
 * another window (and any subsequent events relating to an INCR transfer).
 *
 * ARGUMENTS are:
 *
 * * A display
 *
 * * Our window: just used to stash in a global variable, xcourwin, so our X
 * 	         error handler (xchandler) can send a message to our window to
 * 	         break the main loop out of blocking on XNextEvent() .
 *
 * * Their window: The window that sent us an event.
 *
 * * The event to respond to
 *
 * * A pointer to an Atom. This gets set to the property nominated by the other
 *   app in it's SelectionRequest. Things are likely to break if you change the
 *   value of this yourself.
 *
 * * The target(UTF8_STRING or XA_STRING) to respond to
 *
 * * A pointer to an array of chars to read selection data from.
 *
 * * The length of the array of chars.
 *
 * * In the case of an INCR transfer, the position within the array of chars
 *   that is being processed.
 *
 * * The context that event is the be processed within.
 *
 * RETURN VALUE
 * *  1 if we are finished with a multipart request.
 * * -1 if XChangeProperty returned an error.
 * *  0 otherwise.
 */
int
xcin(Display * dpy,
     Window ourwin,
     Window * theirwin,
     XEvent evt,
     Atom * pty, Atom target, unsigned char *txt, unsigned long len,
     unsigned long *pos, unsigned int *context, long *chunk_size)
{
    unsigned long chunk_len;   /* length of current chunk (for incr xfr only)*/
    XEvent res;		       /* response to event */
    static Atom inc;
    static Atom targets;

    xcourwin = ourwin;		/* For sending error events to our caller */

    if (!targets) {
	targets = XInternAtom(dpy, "TARGETS", False);
    }

    if (!inc) {
	inc = XInternAtom(dpy, "INCR", False);
    }

    /* Selections larger than the maximum request size must be sent
       incrementally. See ICCCM section 2.5. Note that we must subtract some
       bytes to account for the protocol header of a request. As of 2020, the
       header is 32 bytes, but we'll leave aside 1024, just in case. */
    if (!(*chunk_size)) {
	*chunk_size = XExtendedMaxRequestSize(dpy) << 2; /* Words to bytes */
	if (!(*chunk_size)) {
	    *chunk_size = XMaxRequestSize(dpy) << 2;
	}		                                 /* Leave room for */
	*chunk_size -= 1024;				 /* request header */
    }

    /* xsel(1) hangs if given more than 4,000,000 bytes at a time. */
    /* FIXME: report bug to xsel and then allow this number to vary */
    *chunk_size = 4*1000*1000;

    int xcchangeproperr = False; /* Flag if XChangeProperty() failed */
    switch (*context) {
    case XCLIB_XCIN_NONE:
	if ( xcverb >= ODEBUG  &&  evt.xselectionrequest.target) {
	    fprintf(stderr, "xclib: debug: XCIN_NONE: target: %s\n",
		    xcatomstr(dpy, evt.xselectionrequest.target));
	    
	}

	if (evt.type != SelectionRequest) {
	    if ( xcverb >= ODEBUG ) {
		fprintf(stderr,
			"xclib: debug: XCIN_NONE: ignoring %s event\n",
			evtstr[evt.type]);
	    }
	    return (0);
	}
	/* set the window and property that is being used */
	*theirwin = evt.xselectionrequest.requestor;
	*pty = evt.xselectionrequest.property;

	/* reset position to 0 */
	*pos = 0;

	/* put the data into a property */
	if (evt.xselectionrequest.target == targets) {
	    Atom types[2] = { targets, target };

	    if ( xcverb >= ODEBUG ) {
		fprintf(stderr,"xclib: debug: XCIN_NONE:"
			"sending list of TARGETS\n");
	    }

	    /* send data all at once (not using INCR) */
	    xcchangeproperr = xcchangeprop(dpy, *theirwin, *pty,
			 XA_ATOM, 32, PropModeReplace, (unsigned char *) types,
			 (int) (sizeof(types) / sizeof(Atom)) );
	    if (xcverb >= OVERBOSE && xcchangeproperr) {
		fprintf(stderr, "Detected XChangeProperty failure\n");
	    }
	}
	else if (len > *chunk_size) {
	    /* send INCR response */
	    if ( xcverb >= ODEBUG ) {
		fprintf (stderr, "xclib: debug: XCIN_NONE: "
			 "Starting INCR response\n");
	    }
	    xcchangeproperr = xcchangeprop( dpy, *theirwin, *pty, inc,
					    32, PropModeReplace, 0, 0);
	    if (xcverb >= OVERBOSE && xcchangeproperr) {
		fprintf(stderr, "Detected XChangeProperty failure\n");
	    }
	    else {		/* No error */
		/* With the INCR mechanism, we need to know
		 * when the requestor window changes (deletes)
		 * its properties
		 */
		XSelectInput(dpy, *theirwin, PropertyChangeMask);
		*context = XCLIB_XCIN_INCR;
	    }
	}
	else {
	    /* send data all at once (not using INCR) */
	    if ( xcverb >= ODEBUG ) {
		fprintf(stderr, "xclib: debug: XCIN_NONE: "
			"Sending data all at once (%d bytes)\n", (int) len);
	    }

	    xcchangeproperr = xcchangeprop( dpy, *theirwin, *pty, target,
					    8, PropModeReplace,
					    (unsigned char *) txt,  (int) len);
	    if (xcverb >= OVERBOSE && xcchangeproperr) {
		fprintf(stderr, "Detected XChangeProperty failure\n");
	    }
	}

	/* set values for the response event */
	res.xselection.property = *pty;
	res.xselection.type = SelectionNotify;
	res.xselection.display = evt.xselectionrequest.display;
	res.xselection.requestor = *theirwin;
	res.xselection.selection = evt.xselectionrequest.selection;
	res.xselection.target = evt.xselectionrequest.target;
	res.xselection.time = evt.xselectionrequest.time;

	if (xcchangeproperr) {
	    /* if XChangeProp failed, refuse XSelectionRequestion */
	    res.xselection.property = None;
	    fprintf(stderr, "xclib: error: XCIN_NONE: "
		    "XChangeProp failed (%d), "
		    "refusing request.\n", xcchangeproperr);
	}

	/* send the response event */
	XSendEvent(dpy, evt.xselectionrequest.requestor, 0, 0, &res);
	XFlush(dpy);

	/* don't treat TARGETS request as contents request */
	if (evt.xselectionrequest.target == targets)
	    return (1);		/* Finished with request */

	/* if XChangeProp failed, requestor is done no matter what */
	if (xcchangeproperr)
	    return (-1);		/* Error in request */

	/* if len <= chunk_size, then the data was sent all at
	 * once and the transfer is now complete, return 1
	 */
	if (len > *chunk_size)
	    return (0);
	else
	    return (1);

	break;

    case XCLIB_XCIN_INCR:
	/* ignore non-property events */
	if (evt.type != PropertyNotify) {
	    if ( xcverb >= ODEBUG ) {
		fprintf(stderr,
			"xclib: debug: INCR: ignoring %s event\n",
			evtstr[evt.type]);
	    }
	    return (0);
	}

	/* ignore the event unless it's to report that the
	 * property has been deleted
	 */
	if (evt.xproperty.state != PropertyDelete) {
	    if ( xcverb >= ODEBUG ) {
		if ( evt.xproperty.state == 0 )
		    fprintf(stderr,
			    "xclib: debug: INCR: ignoring PropertyNewValue\n");
		else
		    fprintf(stderr,
			    "xclib: debug: INCR: ignoring state %d\n",
			    evt.xproperty.state);
	    }
	    return (0);
	}

	/* set the chunk length to the maximum size */
	chunk_len = *chunk_size;

	/* if a chunk length of maximum size would extend
	 * beyond the end of txt, set the length to be the
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
	    xcchangeproperr = xcchangeprop( dpy, *theirwin, *pty, target,
					    8, PropModeReplace, &txt[*pos],
					    (int) chunk_len);
	    if (xcverb >= OVERBOSE && xcchangeproperr) {
		fprintf(stderr, "Detected XChangeProperty failure\n");
	    }
	}
	else {
	    /* make an empty property to show we've
	     * finished the transfer
	     */
	    if ( xcverb >= ODEBUG ) {
		fprintf(stderr, "xclib: debug: Signalling end of INCR\n");
	    }
	    xcchangeproperr = xcchangeprop( dpy, *theirwin, *pty, target,
					    8, PropModeReplace, 0, 0);
	    if (xcverb >= OVERBOSE && xcchangeproperr) {
		fprintf(stderr, "Detected XChangeProperty failure\n");
	    }
	}
	XFlush(dpy);

	/* Did XChangeProp fail? Refuse xfr and return -1*/
	/* Fixme: This doesn't seem to work to tell xsel to quit XXX */ 
	if (xcchangeproperr) {
	    if (xcverb >= ODEBUG) {
		fprintf(stderr, "xclib: debug: Refusing INCR transfer.\n");
	    }
	    *context = XCLIB_XCIN_NONE;

	    /* set values for the response event */
	    res.xselection.type = SelectionNotify;
	    res.xselection.display = evt.xselectionrequest.display;
	    res.xselection.requestor = *theirwin;
	    res.xselection.selection = evt.xselectionrequest.selection;
	    res.xselection.target = evt.xselectionrequest.target;
	    res.xselection.time = evt.xselectionrequest.time;
	    
	    /* Refuse selection request */
	    res.xselection.property = None; 
	    if (! XSendEvent(dpy, evt.xselectionrequest.requestor,0,0, &res)) {
		fprintf(stderr, "xclib: error: Failed to send event "
			"to requestor to signal our refusal.\n");
	    }

	    XFlush(dpy);
	    return (-1);
	}

	/* all data has been sent, break out of the loop */
	if (!chunk_len) {
	    if (xcverb >= ODEBUG) {
		fprintf(stderr, "xclib: debug: Finished INCR transfer.\n");
	    }
	    *context = XCLIB_XCIN_NONE;
	}

	*pos += *chunk_size;

	/* if chunk_len == 0, we just finished the transfer, return 1
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
 *
 * ARGUMENTS are: 
 *
 * * The Display.
 *
 * * A Window to look up.
 *
 * * A pointer to a string to return the result.
 *   (Note that the caller must XFree() the string when finished.)
 *
 * RETURNS 0 if it works. Not 0, otherwise.
 *
 * [See also, xcnamestr() wrapper below for statically allocated string.]
 */
int
xcfetchname(Display *display, Window w, char **namep) {
    *namep = NULL;
    if (w == None)
	return 1;		/* No window, no name. */

    void *fn = XSetErrorHandler(xcnull);

    xcerrflag = False;
    XFetchName(display, w, namep);
    if (xcerrflag == True) {	/* Give up if the window disappeared */
	XSetErrorHandler(fn);
	return 1;
    }
    if (*namep) {
	XSetErrorHandler(fn);
	return 0; 		/* Hurrah! It worked on the first try. */
    }

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

    XSetErrorHandler(fn);
    return (*namep == NULL);
}


/* A convenience wrapper for xcfetchname() that returns a string so it can be
 * used within printf calls with no need to later XFree anything.
 * It also smoothes over problems by returning the ID number if no name exists.
 *
 * ARGUMENTS are:
 *
 * The Display
 *
 * A Window ID number, e.g., 0xfa1afe1
 *
 * RETURNS a pointer to a statically allocated string containing
 * either the window name followed by the ID in parens, if it can be found,
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

/* xcatomstr: A convenience wrapper for XGetAtomName() that returns a
 * string so it can be used within printf calls with no need to later XFree
 * anything. It also smoothes over problems by returning "(null)" if no name
 * exists and ignoring errors.
 *
 * Given an Atom, return
 * either the Atom name, IFF it can be found,
 * otherwise, the string "(null)".
 *
 * String is statically allocated and is updated at each call.
 */
char xcatom[4096];
char *
xcatomstr(Display *display, Atom a) {
    char *atom_name;
    void *fn = XSetErrorHandler(xcnull); /* Suppress error messages */
    atom_name = XGetAtomName(display, a);
    XSetErrorHandler(fn);    
    if (atom_name && atom_name[0]) {
	snprintf( xcatom, sizeof(xcatom)-1, "%s", atom_name);
    }
    else {
	snprintf( xcatom, sizeof(xcatom)-1, "(null)");
    }
    if (atom_name)
	XFree(atom_name);

    xcatom[sizeof(xcatom) - 1] = '\0'; /* Ensure NULL termination */
    return xcatom;
}


/* xcnull()
 * An X error handler that saves errors but does not print them.
 */
int xcnull(Display *dpy, XErrorEvent *evt) {
    xcerrflag = True;
    xcerrevt = *evt;
    return 0;
}

/* xchandler(): Xlib Error handler that saves last error event.
 *
 * Also, wakes up the main loop from blocking on XNextEvent when a BadWindow
 * error is detected. Requires global xcourwin set to our window ID (currently
 * that's done when xcin() is called).
 *
 * Usage: XSetErrorHandler(xchandler);
 */
int xchandler(Display *dpy, XErrorEvent *evt) {
    void *fn = XSetErrorHandler(XmuSimpleErrorHandler);
    xcerrflag = True;
    xcerrevt = *evt;

    char buf[256];
    XGetErrorText(dpy, evt->error_code, buf, sizeof(buf)-1);
    if (xcverb >= OVERBOSE) {
	fprintf(stderr, "\tXErrorHandler: XError (code %d): %s\n",
		evt->error_code, buf);
    }
    if (xcverb >= ODEBUG) {
	fprintf(stderr,
		"\t\tResource ID: 0x%lx\n"
		"\t\tSerial Num: %lu\n"
		"\t\tError code: %u\n"
		"\t\tRequest op-code: %u major, %u minor\n"
		"\t\tFailed request: %s\n"
		,
		evt->resourceid,
		evt->serial,
		evt->error_code,
		evt->request_code,
		evt->minor_code,
		reqstr[evt->request_code] );
    }

    XSetErrorHandler(fn);

    if (evt->error_code == BadWindow) {
	if (xcourwin) {
	    /* We need to break the mainloop out of XNextEvent so it can delete
	     * the window from the list of requestors right away before another
	     * window with the same ID is created.
	     * Solution: Send ourselves a ClientMessage event.
	     */
	    if (xcverb >= ODEBUG) {
		fprintf(stderr, "\tSending event to self (0x%lx): "
			"Window 0x%lx is bad\n", xcourwin, evt->resourceid);
	    }
	    XClientMessageEvent e;
	    e.type = ClientMessage;
	    e.window = xcourwin;
	    e.format = 8;	/* Message is 8-bit bytes, twenty of 'em */
	    strncpy(e.data.b, "Break event loop", 20);
	    e.data.b[19]='\0';

	    if (! XSendEvent(dpy, xcourwin, 0, 0, (XEvent *)&e) ) {
		fprintf(stderr, "xclib: error: Failed to send "
			"event to main loop to handle BadWindow.\n");
	    }

	    XFlush(dpy);
	}
	else {			/* else, xcourwin is 0 */
	    if (xcverb >= ODEBUG && evt->error_code == BadWindow) {
		fprintf(stderr, "xclib: debug: cannot send event "
			"because xcourwin not initialized yet.\n");
	    }
	}
    }

    return 0;
}


/*
 * xcchangeprop: wrapper for XChangeProperty that gets errors from xchandler.
 * 		 Returns zero if there was no error. Not zero, otherwise.
 *
 * According to ICCCM section 2.5, we should confirm that
 * XChangeProperty succeeded without any Alloc errors before
 * replying with SelectionNotify.
 *
 * Doing so is a bit complicated. We use an error handler, xchandler,
 * which modifies a global variable, xcerrflag, which we examine after
 * each XChangeProperty() + XSync(). If xcerrflag is set, we return
 * non-zero, which sets the xcchangeproperr flag, and, in turn, signals
 * that we should refuse the selection request using XSend. (Perhaps
 * this would be easier with libxcb instead of libX11).
 */

int
xcchangeprop(Display *display, Window w, Atom property, Atom type,
	     int format, int mode, unsigned char *data, int nelements) {
    xcerrflag = False;		/* global set by xchandler() */
    XChangeProperty(display, w, property, type, format, mode, data, nelements);
    XSync(display, False);
    if (xcverb >= OVERBOSE && xcerrflag) {
	char buf[256];
	XGetErrorText(display, xcerrevt.error_code, buf, sizeof(buf)-1);
	fprintf(stderr, "xclib: error: XChangeProp failed with %s\n", buf);
    }

    if (xcerrflag)
	return xcerrevt.error_code;
    else
	return 0;
}
