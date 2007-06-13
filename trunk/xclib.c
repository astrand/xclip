/*
 *  $Id: xclib.c,v 1.6 2001/09/15 05:50:59 kims Exp $
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

/* wrapper for malloc that checks for errors */
void *xcmalloc (size_t size)
{
	void *mem;
	
	mem = malloc(size);
	if (mem == NULL)
		errmalloc();
	return(mem);
}

/* wrapper for realloc that checks for errors */
void *xcrealloc (void *ptr, size_t size)
{
	void *mem;

	mem = realloc(ptr, size);
	if (mem == NULL)
		errmalloc();
	return(mem);
}

/* return selection data */
char *xcout (Display *dpy, Window win, Atom sel)
{
	Atom		pty, inc, pty_type;
	int		pty_format;
	XEvent		evt;
	char		*txt;
	char		*rtn_str;
	unsigned int	rtn_siz = 0;

	unsigned char	*data;
	unsigned long	pty_items, pty_size;

	/* Two new window properties for transferring the selection */
	pty = XInternAtom(dpy, "XCLIP_OUT",	False);
	inc = XInternAtom(dpy, "INCR",		False);

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
		if (evt.type == SelectionNotify || evt.type == None )
			break;
	}

	/* quit if there is nothing in the selection */
	if (evt.type == None)
		return(rtn_str);
    
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
		&data
	);

	if (pty_format == 8)
	{
		/* not using INCR mechanism, just read and print the property */
		XGetWindowProperty(
			dpy,
			win,
			pty,
			0,
			pty_size,
			False,
			AnyPropertyType,
			&pty_type,
			&pty_format,
			&pty_items,
			&pty_size,
			(unsigned char **)&txt
		);

		/* finished with property, delete it */
		XDeleteProperty(dpy, win, pty);

		/* make a copy of the selection string, then free it */
		rtn_str = strdup(txt);
		XFree(txt);

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
				(unsigned char **)&txt
			);

			if (pty_format != 8){
				/* property does not contain text, delete it
				 * to tell the other X client that we have read
				 * it and to send the next property */
				XDeleteProperty(dpy, win, pty);
				continue;
			}

			if (pty_size == 0){
				/* no more data, exit from loop */
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
				pty_size,
				False,
				AnyPropertyType,
				&pty_type,
				&pty_format,
				&pty_items,
				&pty_size,
				(unsigned char **)&txt
			);
			
			/* allocate memory to ammodate date in rtn_str */
			if (rtn_siz == 0)
			{
				rtn_siz = pty_items;
				rtn_str = (char *)xcmalloc(rtn_siz);
			} else
			{
				rtn_siz += pty_items;
				rtn_str = (char *)xcrealloc(rtn_str, rtn_siz);
			}
			
			/* add data to return_str */
			strncat(rtn_str, txt, pty_items);
			
			/* delete property to get the next item */
			XDeleteProperty(dpy, win, pty);
		}
	}
	return(rtn_str);
}

/* send selection data in response to a request returns 0 */
int xcin (Display *dpy, Window win, XEvent rev, char *txt)
{
	unsigned int			incr = F;	/* incr mode flag */
	XEvent				res;		/* response to event */
	Atom				inc;
	unsigned int			selc = F;	/* SelClear event */

	if (rev.type == SelectionClear)
		return(1);
	
	/* send only continue if this is a SelectionRequest event */
	if (rev.type != SelectionRequest)
		return(0);
	
	/* test whether to use INCR or not */
	if ( strlen(txt) > XC_CHUNK )
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
			strlen(txt)
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
		unsigned int sel_pos = 0; /* position in sel_str */
		unsigned int sel_end = 0;
		char *chk_str;

		chk_str = (char *)xcmalloc(XC_CHUNK);

		while (1)
		{
			unsigned int chk_pos = 0;

			while (1)
			{
				XEvent	spe;

				XNextEvent(dpy, &spe);
				if (spe.type == SelectionClear)
				{
					selc = T;
					continue;
				}
				if (spe.type != PropertyNotify)
					continue;
				if (spe.xproperty.state == PropertyDelete)
					break;
			}
	    

			if (!sel_end)
			{
				for (chk_pos=0; chk_pos<=XC_CHUNK; chk_pos++)
				{
					if (txt[sel_pos] == (char)NULL)
					{
						sel_end = 1;
						break;
					}
					chk_str[chk_pos] = txt[sel_pos];
					sel_pos++;
				}
			}

			if (chk_pos)
			{
				XChangeProperty(
					dpy,
					rev.xselectionrequest.requestor,
					rev.xselectionrequest.property,
					XA_STRING,
					8,
					PropModeReplace,
					chk_str,
					chk_pos
				);
			} else
			{
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

			/* no more chars to send, break out of the loop */
			if (!chk_pos)
				break;
		}
	}
	return(selc);
}
