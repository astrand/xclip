/*
 *  $Id: xclib.c,v 1.9 2001/10/22 07:52:51 kims Exp $
 * 
 *  xclib.c - xclip library to look after xlib mechanics for xclip
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
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include "xcdef.h"
#include "xcprint.h"
#include "xclib.h"

/* check a pointer to allocater memory, print an error if it's null */
void xcmemcheck(void *ptr)
{
	if (ptr == NULL)
		errmalloc();
}

/* wrapper for malloc that checks for errors */
void *xcmalloc (size_t size)
{
	void *mem;
	
	mem = malloc(size);
	xcmemcheck(mem);
	
	return(mem);
}

/* wrapper for realloc that checks for errors */
void *xcrealloc (void *ptr, size_t size)
{
	void *mem;

	mem = realloc(ptr, size);
	xcmemcheck(mem);

	return(mem);
}

/* a strdup() implementation since ANSI C doesn't include strdup() */
void *xcstrdup (const char *string)
{
	void *mem;

	/* allocate a buffer big enough to hold the characters and the
	 * null terminator, then copy the string into the buffer
	 */
	mem = xcmalloc(strlen(string) + sizeof(char));
	strcpy(mem, string);

	return(mem);
}

/* return selection data. Arguments are the display and window, the selection
 * to be returned, and a pointer to a buffer and a long which get set to the
 * contents of the selection, and the size of the buffer. Returns a 0, unless
 * a SelectionNotify event is received, in which case 1 is returned. It's
 * pretty unlikely that we own the selection if we're pasting... but hey, you
 * never know....
 */
int xcout (
	Display *dpy,
	Window win,
	Atom sel,
	unsigned char **txt,
	unsigned long *len
)
{
	Atom		pty, inc, pty_type;
	int		pty_format;
	XEvent		evt;
	unsigned int	retval = 0;

	/* buffer for XGetWindowProperty to dump data into */
	unsigned char	*buffer;
	unsigned long	pty_size, pty_items;

	/* local buffer of text to return */
	unsigned char	*ltxt;

	/* Two new window properties for transferring the selection */
	pty = XInternAtom(dpy, "XCLIP_OUT",	False);
	inc = XInternAtom(dpy, "INCR",		False);

	/* initialise return length to 0 */
	*len = 0;

	/* send a selection request */
	XConvertSelection(
		dpy,
		sel,
		XA_STRING,
		pty,
		win,
		CurrentTime
	);

	/* wait for a SelectionNotify in response to our request */
	while (1)
	{
		XNextEvent(dpy, &evt);
		if (evt.type == SelectionClear)
			retval = 1;
		if (evt.type == SelectionNotify || evt.type == None )
			break;
	}

	/* quit if there is nothing in the selection */
	if (evt.type == None)
		return(0);
    
	/* find out the size and format of the data in property */
	XGetWindowProperty(
		dpy,
		win,
		pty,
		0,
		0,
		False,
		AnyPropertyType,
		&pty_type,
		&pty_format,
		&pty_items,
		&pty_size,
		&buffer
	);
	XFree(buffer);

	if (pty_format == 8)
	{
		/* not using INCR mechanism, just read and print the property */
		XGetWindowProperty(
			dpy,
			win,
			pty,
			0,
			(long)pty_size,
			False,
			AnyPropertyType,
			&pty_type,
			&pty_format,
			&pty_items,
			&pty_size,
			&buffer
		);

		/* finished with property, delete it */
		XDeleteProperty(dpy, win, pty);
		
		/* copy the buffer to the pointer for returned data */
		ltxt = (unsigned char *)xcmalloc(pty_items);
		memcpy(ltxt, buffer, pty_items);

		/* set the length of the returned data */
		*len = pty_items;

		/* free the buffer */
		XFree(buffer);
	} else if (pty_type == inc)
	{
		/* Using INCR mechanism, start by deleting the property */
		XDeleteProperty(dpy, win, pty);

		while (1)
		{
			/* To use the INCR method, we basically delete the
			 * property with the selection in it, wait for an
			 * event indicating that the property has been created,
			 * then read it, delete it, etc.
			 */

			/* flush to force any pending XDeleteProperty calls
			 * from the previous running of the loop, get the next
			 * event in the event queue
			 */
			XFlush(dpy);
			XNextEvent(dpy, &evt);

			if (evt.type == SelectionClear)
				retval = 0;

			/* skip unless is a property event */
			if (evt.type != PropertyNotify)
				continue;

			/* skip unless the property has a new value */
			if (evt.xproperty.state != PropertyNewValue)
				continue;
	
			/* check size and format of the property */
			XGetWindowProperty(
				dpy,
				win,
				pty,
				0,
				0,
				False,
				AnyPropertyType,
				&pty_type,
				&pty_format,
				&pty_items,
				&pty_size,
				(unsigned char **)&buffer
			);

			if (pty_format != 8)
			{
				/* property does not contain text, delete it
				 * to tell the other X client that we have read
				 * it and to send the next property */
				XFree(buffer);
				XDeleteProperty(dpy, win, pty);
				continue;
			}

			if (pty_size == 0)
			{
				/* no more data, exit from loop */
				XFree(buffer);
				XDeleteProperty(dpy, win, pty);
				break;
			}

			/* if we have come this far, the propery contains
			 * text, we know the size.
			 */
			XGetWindowProperty(
				dpy,
				win,
				pty,
				0,
				(long)pty_size,
				False,
				AnyPropertyType,
				&pty_type,
				&pty_format,
				&pty_items,
				&pty_size,
				(unsigned char **)&buffer
			);
			
			/* allocate memory to ammodate data in *txt */
			if (*len == 0)
			{
				*len = pty_items;
				ltxt = (unsigned char *)xcmalloc(*len);
			} else
			{
				*len += pty_items;
				ltxt = (unsigned char *)xcrealloc(ltxt, *len);
			}

			/* add data to *txt */
			memcpy(
				&ltxt[*len - pty_items],
				buffer,
				pty_items
			);
			
			/* delete property to get the next item */
			XDeleteProperty(dpy, win, pty);
		}
	}
	*txt = ltxt;
	return(retval);
}

/* send selection data in response to a request. Returns 1 if a SelectionClear
 * was received, otherwise 0
 */
int xcin (Display *dpy, XEvent rev, unsigned char *txt, unsigned long len)
{
	unsigned int	retval = 0;
	unsigned int	incr = F;	/* incr mode flag */
	XEvent		res;		/* response to event */
	Atom		inc;

	if (rev.type == SelectionClear)
		return(1);
	
	/* send only continue if this is a SelectionRequest event */
	if (rev.type != SelectionRequest)
		return(0);
	
	/* test whether to use INCR or not */
	if ( len > XC_CHUNK )
	{
		incr = T;
		inc = XInternAtom(dpy, "INCR", False);
	}

	/* put the data into an property */
	if (incr)
	{
		/* send INCR response */
		XChangeProperty(
			dpy,
			rev.xselectionrequest.requestor,
			rev.xselectionrequest.property,
			inc,
			32,
			PropModeReplace,
			0,
			0
		);

		/* With the INCR mechanism, we need to know when the
		 * requestor window changes (deletes) its properties
		 */
		XSelectInput(
			dpy,
			rev.xselectionrequest.requestor,
			PropertyChangeMask
		);
	} else 
	{
		/* send data all at once (not using INCR) */
		XChangeProperty(
			dpy,
			rev.xselectionrequest.requestor,
			rev.xselectionrequest.property,
			XA_STRING,
			8,
			PropModeReplace,
			(unsigned char*) txt,
			(int)len
		);
	}
	
	/* set values for the response event */
	res.xselection.property  = rev.xselectionrequest.property;
	res.xselection.type      = SelectionNotify;
	res.xselection.display   = rev.xselectionrequest.display;
	res.xselection.requestor = rev.xselectionrequest.requestor;
	res.xselection.selection = rev.xselectionrequest.selection;
	res.xselection.target    = rev.xselectionrequest.target;
	res.xselection.time      = rev.xselectionrequest.time;

	/* send the response event */
	XSendEvent(dpy, rev.xselectionrequest.requestor, 0, 0, &res);
	XFlush(dpy);

	if (incr)
	{
		/* position in txt that the current chunk starts */
		unsigned long chunk_pos = 0;

		/* length of current chunk */
		unsigned long chunk_len = XC_CHUNK;

		while (1)
		{
			/* wait the propery we just wrote to to be deleted
			 * before we write to it again
			 */
			while (1)
			{
				XEvent	spe;

				XNextEvent(dpy, &spe);
				if (spe.type == SelectionClear)
				{
					retval = 1;
					continue;
				}
				if (spe.type != PropertyNotify)
					continue;
				if (spe.xproperty.state == PropertyDelete)
					break;
			}
	    
			/* set the chunk length to the maximum size */
			chunk_len = XC_CHUNK;

			/* if a chunk length of maximum size would extend
			 * beyond the end ot txt, set the length to be the
			 * remaining length of txt
			 */
			if ( (chunk_pos + chunk_len) > len )
				chunk_len = len - chunk_pos;

			/* if the start of the chunk is beyond the end of txt,
			 * then we've already sent all the data, so set the
			 * length to be zero
			 */
			if (chunk_pos > len)
				chunk_len = 0;

			if (chunk_len)
			{
				/* put the chunk into the property */
				XChangeProperty(
					dpy,
					rev.xselectionrequest.requestor,
					rev.xselectionrequest.property,
					XA_STRING,
					8,
					PropModeReplace,
					&txt[chunk_pos],
					(int)chunk_len
				);
			} else
			{
				/* make an empty propery to show we've finished
				 * the transfer
				 */
				XChangeProperty(
					dpy,
					rev.xselectionrequest.requestor,
					rev.xselectionrequest.property,
					XA_STRING,
					8,
					PropModeReplace,
					0,
					0	
				);
			}
			XFlush(dpy);

			/* all data has been sent, break out of the loop */
			if (!chunk_len)
				break;

			chunk_pos += XC_CHUNK;
		}
	}
	return(retval);
}
