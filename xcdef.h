/*
 *  $Id: xcdef.h,v 1.8 2001/12/17 06:14:40 kims Exp $
 * 
 *  xcdef.h - definitions for use throughout xclip
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

/* xclip version and name */
#define XC_VERS 0.08
#define XC_NAME "xclip"

/* output level constants */
#define OSILENT  0
#define OQUIET   1
#define OVERBOSE 2

/* generic true/false constants for stuff */
#define F 0     /* false... */
#define T 1     /* true...  */

/* true/false string constants */
#define SF "F"	/* false */
#define ST "T"	/* true  */

/* maximume size to read/write to/from a property at once in bytes */
#define XC_CHUNK 4096
