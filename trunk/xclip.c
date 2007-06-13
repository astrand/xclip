/*
 *  $Id: xclip.c,v 1.51 2001/09/19 11:44:13 kims Exp $
 * 
 *  xclip.c - command line interface to X server selections 
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
#include <unistd.h>
#include <ctype.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xmu/Atoms.h>
#include "xcdef.h"
#include "xcprint.h"
#include "xclib.h"

/* command line option table for XrmParseCommand() */
static XrmOptionDescRec opt_tab[] = {
	{"-loops",	".loops",	XrmoptionSepArg, (XPointer) NULL },
	{"-display",	".display",	XrmoptionSepArg, (XPointer) NULL },
	{"-selection",	".selection",	XrmoptionSepArg, (XPointer) NULL },
	{"-filter",	".filter",	XrmoptionNoArg,	 (XPointer) ST   },
	{"-in",		".direction",	XrmoptionNoArg,	 (XPointer) "I"  },
	{"-out",	".direction",	XrmoptionNoArg,	 (XPointer) "O"  },
	{"-version",	".print",	XrmoptionNoArg,	 (XPointer) "V"  },
	{"-help",	".print",	XrmoptionNoArg,	 (XPointer) "H"  },
	{"-silent",	".olevel",	XrmoptionNoArg,	 (XPointer) "S"  },
	{"-quiet",	".olevel",	XrmoptionNoArg,	 (XPointer) "Q"  },
	{"-verbose",	".olevel",	XrmoptionNoArg,	 (XPointer) "V"  },
};

/* Options that get set on the command line */
int             sloop = 0;			/* number of loops */
char           *sdisp = NULL;			/* X display to connect to */
Atom            sseln = XA_PRIMARY;		/* X selection to work with */

/* Flags for command line options */
static int      fverb = OSILENT;		/* output level */
static int      fdiri = T;			/* direction is in */
static int      ffilt = F;			/* filter mode */

Display	       *dpy;				/* connection to X11 display */
XrmDatabase     opt_db = NULL;			/* database for options */

char          **fil_names;			/* names of files to read */
int             fil_number = 0;                 /* number of files to read */
int		fil_current;
FILE*           fil_handle = NULL;

/* variables to hold Xrm database record and type */
XrmValue        rec_val;
char           *rec_typ;

/* Use XrmParseCommand to parse command line options to option variable */
void doOptMain (int argc, char *argv[])
{
	/* Initialise resource manager and parse options into database */
	XrmInitialize();
	XrmParseCommand(
		&opt_db,
		opt_tab,
		sizeof(opt_tab) / sizeof(opt_tab[0]),
		XC_NAME,
		&argc,
		argv
	);
  
	/* set output level */
	if (
		XrmGetResource(
			opt_db,
			"xclip.olevel",
			"Xclip.Olevel",
			&rec_typ,
			&rec_val
		)
	)
	{
		/* set verbose flag according to option */
		if (strcmp(rec_val.addr, "S") == 0)
			fverb = OSILENT;
		if (strcmp(rec_val.addr, "Q") == 0)
			fverb = OQUIET;
		if (strcmp(rec_val.addr, "V") == 0)
			fverb = OVERBOSE;
	}
  
	/* set direction flag (in or out) */
	if (
		XrmGetResource(
			opt_db,
			"xclip.direction",
			"Xclip.Direction",
			&rec_typ,
			&rec_val
		)
	)
	{
		if (strcmp(rec_val.addr, "I") == 0)
			fdiri = T;
		if (strcmp(rec_val.addr, "O") == 0)
			fdiri = F;
	}
  
	/* set filter mode */
	if (
		XrmGetResource(
			opt_db,
			"xclip.filter",
			"Xclip.Filter",
			&rec_typ,
			&rec_val
		)
	)
	{
		/* filter mode only allowed in silent mode */
		if (fverb == OSILENT)
			ffilt = T;
	}
  
	/* check for -help and -version */
	if (
		XrmGetResource(
			opt_db,
			"xclip.print",
			"Xclip.Print",
			&rec_typ,
			&rec_val
		)
	)
	{
		if (strcmp(rec_val.addr, "H") == 0)
			prhelp(argv[0]);
		if (strcmp(rec_val.addr, "V") == 0)
			prversion();
	}
  
	/* check for -display */
	if (
		XrmGetResource(
			opt_db,
			"xclip.display",
			"Xclip.Display",
			&rec_typ,
			&rec_val
		)
	)
	{
		sdisp = rec_val.addr;
		if (fverb == OVERBOSE)	/* print in verbose mode only */
			fprintf(stderr, "Display: %s\n", sdisp);
	}
  
	/* check for -loops */
	if (
		XrmGetResource(
			opt_db,
			"xclip.loops",
			"Xclip.Loops",
			&rec_typ,
			&rec_val
		)
	)
	{
		sloop = atoi(rec_val.addr);
		if (fverb == OVERBOSE)	/* print in verbose mode only */
			fprintf(stderr, "Loops: %i\n", sloop);
	}

	/* Read remaining options (filenames) */
	while (optind < argc)
	{
		if (fil_number > 0)
		{
			fil_names = xcrealloc(
				fil_names,
				(fil_number + 1) * sizeof(char*)
			);
		} else
		{
			fil_names = xcmalloc(sizeof(char*));
		}
		fil_names[fil_number++] = strdup(argv[optind]);
		optind++;
	}
};

/* process selection command line option */
void doOptSel (int argc, char *argv[])
{
	/* set selection to work with */
	if (
		XrmGetResource(
			opt_db,
			"xclip.selection",
			"Xclip.Selection",
			&rec_typ,
			&rec_val
		)
	)
	{
		switch (tolower(rec_val.addr[0]))
		{
			case 'p':
				sseln = XA_PRIMARY;
				break;
			case 's':
				sseln = XA_SECONDARY;
				break;
			case 'c':
				sseln = XA_CLIPBOARD(dpy);
				break;
		}
    
		if (fverb == OVERBOSE)
		{
			fprintf(stderr, "Using selection: ");
	   
			if (sseln == XA_PRIMARY)
				fprintf(stderr, "XA_PRIMARY");
			if (sseln == XA_SECONDARY)
				fprintf(stderr, "XA_SECONDARY");
			if (sseln == XA_CLIPBOARD(dpy))
				fprintf(stderr, "XA_CLIPBOARD");

			fprintf(stderr, "\n");
		}
	}
}

int main (int argc, char *argv[])
{
	/* Declare variables */
	char *seltxt;			/* selection text string */
	unsigned long stelems = 0;	/* number of used elements in seltxt */
	unsigned long stalloc = 0;	/* size of seltxt */
	Window win;			/* Window */
	XEvent evt;			/* X Event Structures */
	int dloop = 0;			/* done loops counter */

	pid_t pid;			/* child pid if forking */

	/* parse command line options */
	doOptMain(argc, argv);
   
	/* Connect to the X server. */
	if ( (dpy = XOpenDisplay(sdisp)) )
	{
		/* successful */
		if (fverb == OVERBOSE)
			fprintf(stderr, "Connected to X server.\n");
	} else
	{
		/* couldn't connect to X server. Print error and exit */
		errxdisplay(sdisp);
	}

	/* parse selection command line option */
	doOptSel(argc, argv);
  
	/* Create a window to trap events */
	win = XCreateSimpleWindow(
		dpy,
		DefaultRootWindow(dpy),
		0,
		0,
		1,
		1,
		0,
		0,
		0
	);

	/* get events about property changes */
	XSelectInput(dpy, win, PropertyChangeMask);
  
	if (fdiri)
	{
		/* in mode */
		stalloc = 16;  /* Reasonable ballpark figure */
		seltxt = xcmalloc(stalloc);

		/* Put chars into inc from stdin or files until we hit EOF */
		do {
			
			if (fil_number == 0)
			{
				/* read from stdin if no files specified */
				fil_handle = stdin;
			} else
			{
				/* open the current file for reading */
				if (
					(fil_handle = fopen(
						fil_names[fil_current],
						"r"
					)) == NULL
				)
				{
					errperror(
						argv[0],
						": ",
						fil_names[fil_current]
					);
					exit(EXIT_FAILURE);
				} else
				{
					/* file opened successfully. Print
					 * message (verbose mode only).
					 */
					if (fverb == OVERBOSE)
						fprintf(
							stderr,
							"Reading %s...\n",
							fil_names[fil_current]
						);
				}
			}
			fil_current++;
			while (!feof(fil_handle))
			{
				int n = 0;

				/* If in is full (used elems =
				 * allocated elems)
				 */
				if (stelems == stalloc)
				{
					/* double the number of
					 * allocated elements
					 */
					stalloc *= 2 + 1;
					seltxt = (char *)xcrealloc(
						seltxt,
						stalloc * sizeof(char)
					);
				}
				n = fread(
					seltxt + stelems,
					1,
					stalloc - stelems,
					fil_handle
				);
	
				stelems += n;
				seltxt[stelems] = '\0';
			}
		} while (fil_current < fil_number);

		/* if there are no files being read from (i.e., input
		 * is from stdin not files, and we are in filter mode,
		 * spit all the input back out to stdout
		 */
		if ((fil_number == 0) && ffilt)
			printf(seltxt);
    
		/* take control of the selection so that we receive
		 * SelectionRequest events from other windows
		 */
		XSetSelectionOwner(dpy, sseln, win, CurrentTime);

		/* fork into the background, exit parent process if we
		 * are in silent mode
		 */
		if (fverb == OSILENT)
		{
			pid = fork();
			/* exit the parent process; */
			if (pid)
				exit(EXIT_SUCCESS);
		}

		/* print a message saying what we're waiting for */
		if (fverb > OSILENT)
		{
			if (sloop == 1)
				fprintf(
					stderr,
					"Waiting for one selection request.\n"
				);
				
			if (sloop  < 1)
				fprintf(
					stderr,
					"Waiting for selection requests, Control-C to quit\n"
				);
				
			if (sloop  > 1)
				fprintf(
					stderr,
					"Waiting for %i selection requests, Control-C to quit\n",
					sloop
				);
		}
  
		/* loop and wait for the expected number of
		 * SelectionRequest events
		 */
		while (dloop < sloop || sloop < 1)
		{
			/* print messages about what we're waiting for
			 * if not in silent mode
			 */
			if (fverb > OSILENT)
			{
				if (sloop  > 1)
					fprintf(
						stderr,
						"  Waiting for selection request %i of %i.\n",
						dloop + 1,
						sloop
					);
					
				if (sloop == 1)
					fprintf(
						stderr,
						"  Waiting for a selection request.\n"
					);
					
				if (sloop  < 1)
					fprintf(
						stderr,
						"  Waiting for selection request number %i\n",
						dloop + 1
					);
			}
     
			/* wait for a SelectionRequest event */
			while (1)
			{
				XNextEvent(dpy, &evt);

				/* request for selection, process
				 * with xcin()
				 */
				if (evt.type == SelectionRequest)
					break;

				/* lost ownership of selection, exit */
				if (evt.type == SelectionClear)
					exit(EXIT_SUCCESS);
			}

			/* recieved request, send response with xcin().
			 * xcin() will return a true value if it
			 * received a SelectionClear, in which case
			 * xclip should exit
			 */
			if (xcin(dpy, win, evt, seltxt))
				exit(EXIT_SUCCESS);

			dloop++;	/* increment loop counter */
		}
	} else
	{
		/* out mode - get selection, print it, free the memory */
		seltxt = xcout(dpy, win, sseln);
		printf(seltxt);
		free(seltxt);
	}

	/* Disconnect from the X server */
	XCloseDisplay(dpy);

	/* exit */
	return(EXIT_SUCCESS);
}
