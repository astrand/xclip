/*
 *  
 * 
 *  xclib.h - header file for functions in xclib.c
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

#include <X11/Xlib.h>

/* global verbosity output level */
extern int xcverb;

/* global error flags from xchandler() */
extern int xcerrflag;
extern XErrorEvent xcerrevt;

/* output level constants for xcverb */
#define OSILENT  0
#define OQUIET   1
#define OVERBOSE 2
#define ODEBUG   9

/* xcout() contexts */
#define XCLIB_XCOUT_NONE		0	/* no context */
#define XCLIB_XCOUT_SENTCONVSEL		1	/* sent a request */
#define XCLIB_XCOUT_INCR		2	/* in an incr loop */
#define XCLIB_XCOUT_BAD_TARGET		3	/* given target failed */
#define XCLIB_XCOUT_SELECTION_REFUSED	4	/* owner signaled an error */

/* xcin() contexts */
#define XCLIB_XCIN_NONE		0
#define XCLIB_XCIN_SELREQ	1
#define XCLIB_XCIN_INCR		2

/* functions in xclib.c */
extern int xcout(
	Display*,
	Window,
	XEvent,
	Atom,
	Atom,
	Atom*,
	unsigned char**,
	unsigned long*,
	unsigned int*
);
extern int xcin(
	Display*,
	Window,
	Window*,
	XEvent,
	Atom*,
	Atom,
	unsigned char*,
	unsigned long,
	unsigned long*,
	unsigned int*,
	long*
);
extern void *xcmalloc(size_t);
extern void *xcrealloc(void*, size_t);
extern void *xcstrdup(const char *);
extern void xcmemcheck(void*);
extern int xcfetchname(Display *, Window, char **);
extern char *xcnamestr(Display *, Window);
extern char *xcatomstr(Display *, Atom);
extern void xcmemzero(void *ptr, size_t len);
extern int xchandler(Display *, XErrorEvent *);
extern int xcnull(Display *dpy, XErrorEvent *evt);
extern int xcchangeprop(Display *, Window, Atom, Atom, int, int, unsigned char *, int);


/* volatile prevents compiler from causing dead-store elimination with optimization enabled */
typedef void *(*memset_t)(void *, int, size_t);
static volatile memset_t memset_func = memset;

/* Table of event names from event numbers */
extern const char *evtstr[LASTEvent];

struct requestor
{
    Window cwin;
    Atom pty;
    Time stamp;
    unsigned int context;
    unsigned long sel_pos;
    int finished;
    long chunk_size;
    struct requestor *next;
};

