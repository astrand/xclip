/*
 *  $Id: xclib.h,v 1.4 2001/09/19 08:38:01 kims Exp $
 * 
 *  xclib.h - header file for functions in xclib.c
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

#include <X11/Xlib.h>

/* functions in xclib.c */
extern char *xcout(Display*, Window, Atom);
extern int xcin(Display*, Window, XEvent, char*);
extern void *xcmalloc(size_t);
extern void *xcrealloc(void*, size_t);
