/*
 *  $Id: xclip.c,v 1.29 2001/05/28 07:22:47 kims Exp $
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

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Intrinsic.h>
#include <X11/Xmu/Atoms.h>
#include <X11/Xmu/StdSel.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include "xcdef.h"

/* command line option table for XrmParseCommand() */
static XrmOptionDescRec opt_tab[] = {
  {"-loops",	 ".loops",	XrmoptionSepArg,	(XPointer) NULL	},
  {"-display",	 ".display",	XrmoptionSepArg,	(XPointer) NULL	},
  {"-selection", ".selection",	XrmoptionSepArg,	(XPointer) NULL	},
  {"-filter",	 ".filter",	XrmoptionNoArg,		(XPointer) ST	},
  {"-in",	 ".direction",	XrmoptionNoArg,		(XPointer) "I"	},
  {"-out",	 ".direction",	XrmoptionNoArg,		(XPointer) "O"	},
  {"-version",	 ".print",	XrmoptionNoArg,		(XPointer) "V"	},
  {"-help",	 ".print",	XrmoptionNoArg,		(XPointer) "H"	},
  {"-silent",	 ".olevel",	XrmoptionNoArg,		(XPointer) "S"	},
  {"-quiet",	 ".olevel",	XrmoptionNoArg,		(XPointer) "Q"	},
  {"-verbose",	 ".olevel",	XrmoptionNoArg,		(XPointer) "V"	},
};

int main(int argc, char *argv[])
{
  /* Declare variables */
  char *seltxt;				/* selection text */
  char **files;
  int numFiles = 0;
  int currFile = 0;
  FILE* inf = NULL;
  unsigned long stelems = 0;		/* of used elements in seltxt */
  unsigned long stalloc = 0;		/* size of seltxt */
  Display *dpy;				/* Display connection */
  Window win;				/* Window */
  XSelectionRequestEvent *req;
  XEvent ev, re;			/* X Event Structures */
  int dloop = 0;			/* done loops counter */

  /* Options that get set on the command line */
  int   sloop = 0;			/* number of loops */
  char *sdisp = "";			/* X display to connect to */
  Atom  sseln = XA_PRIMARY;		/* X selection to work with */

  /* Flags for command line options */
  static int fverb = OSILENT;		/* output level */
  static int fdiri = T;			/* direction is in */
  static int ffilt = F;			/* filter mode */

  pid_t cpid;				/* child pid if forking */
   
  XrmDatabase	opt_db = NULL;
  XrmValue	val;
  char		*res_type;
	  
  XrmInitialize();			/* initialise resource manager */
  XrmParseCommand(			/* read options into xrm database */
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
      &res_type,
      &val
    )
  ){
    if (strcmp(val.addr, "S") == 0)
      fverb = OSILENT;
    if (strcmp(val.addr, "Q") == 0)
      fverb = OQUIET;
    if (strcmp(val.addr, "V") == 0)
      fverb = OVERBOSE;
  }
  
  /* set direction flag (in or out) */
  if (
    XrmGetResource(
      opt_db,
      "xclip.direction",
      "Xclip.Direction",
      &res_type,
      &val
    )
  ){
    if (strcmp(val.addr, "I") == 0)
      fdiri = T;
    if (strcmp(val.addr, "O") == 0)
      fdiri = F;
  }
  
  /* set filter mode */
  if (
    XrmGetResource(
      opt_db,
      "xclip.filter",
      "Xclip.Filter",
      &res_type,
      &val
    )
  ){
      if (fverb == OSILENT)
        ffilt = T;		/* filter mode only allowed in silent mode */
  }
  
  /* check for -help and -version */
  if (
    XrmGetResource(
      opt_db,
      "xclip.print",
      "Xclip.Print",
      &res_type,
      &val
    )
  ){
    if (strcmp(val.addr, "H") == 0)
      prhelp();
    if (strcmp(val.addr, "V") == 0)
      prversion();
  }
  
  /* check for -display */
  if (
    XrmGetResource(
      opt_db,
      "xclip.display",
      "Xclip.Display",
      &res_type,
      &val
    )
  ){
    sdisp = val.addr;
    if (fverb == OVERBOSE)	/* display in verbose mode only */
      fprintf(stderr, "Display: %s\n", sdisp);
  }
  
  /* check for -loops */
  if (
    XrmGetResource(
      opt_db,
      "xclip.loops",
      "Xclip.Loops",
      &res_type,
      &val
    )
  ){
    sloop = atoi(val.addr);
    if (fverb == OVERBOSE)	/* display in verbose mode only */
      fprintf(stderr, "Loops: %i\n", sloop);
  }

  /* Read remaining options (filenames) */
  while (optind < argc){
    if (numFiles > 0) {
      files = realloc(files,(numFiles + 1) * sizeof(char*));
    } else {
      files = malloc(sizeof(char*));
    }
    files[numFiles++] = strdup(argv[optind]);
    optind++;
  }
   
  /* Connect to the X server. */
  if ( (dpy = XOpenDisplay(sdisp)) ){
    /* successful */
    if (fverb == OVERBOSE)
      fprintf(stderr, "Connected to X server.\n");
  }
  else {
    /* couldn't connect to X server. Print error. */
    fprintf(stderr, "Error: Could not connect to X server.\n");
    perror(NULL);
    exit(EXIT_FAILURE); /* exit */
  }
  
  /* set selection to work with */
  if (
    XrmGetResource(
      opt_db,
      "xclip.selection",
      "Xclip.Selection",
      &res_type,
      &val
    )
  ){
	 switch (tolower(val.addr[0])){
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
	 if (fverb == OVERBOSE){
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
  
  /* Create a window that will trap events */
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
  
  if (fdiri){
    stalloc = 16;  /* Reasonable ballpark figure */
    seltxt = malloc(stalloc);
    if (seltxt == NULL)
      errmalloc();

    /* Put chars into inc from stdin or files until we hit EOF */
    do {
      /* read from standard in if no files specified */
      if (numFiles == 0) {
        inf = stdin;
      }
      else {
        if ( (inf = fopen(files[currFile], "r")) == NULL ){ /* open the file */
  	  /* error, exit if there is a problem opening the file */
	  fprintf(
	    stderr,
	    "Error: can't open %s for reading\n",
	    files[currFile]
	  );
          perror(NULL);
          exit(EXIT_FAILURE);
        }
        else {
	  /* file opened successfully. Print message (verbose mode only). */
          if (fverb == OVERBOSE)
            fprintf(stderr, "Reading %s...\n", files[currFile]);
        }
      }
      currFile++;

      while (!feof(inf)) {
        int n = 0;

        /* If in is full (used elems = allocated elems) */
        if (stelems == stalloc) {
          /* Allocate another the double number of elems */
	  stalloc *= 2 + 1;
	  seltxt = (char *)realloc(seltxt, stalloc * sizeof(char));
	  if (seltxt == NULL)
	    errmalloc();
        }
        n = fread(seltxt + stelems, 1, stalloc - stelems, inf);
	
        stelems += n;
        seltxt[stelems] = '\0';
      }
    } while (currFile < numFiles);

    /* if there are no files being read from (i.e., input is from stdin not
     * files, and we are in filter mode, spit all the input back out to stdout
     */
    if ((numFiles == 0) && ffilt)
      printf(seltxt);
    
    /* Take control of the selection */
    XSetSelectionOwner(
      dpy,
      sseln,
      win,
      CurrentTime
    );

    /* fork into the background, exit parent process if we are in silent mode */
    if (fverb == OSILENT){
      cpid = fork();
      if (cpid){
        exit(EXIT_SUCCESS); /* exit the parent process; */
      }
    }

    /* print a message saying what we're waiting for */
    if (fverb > OSILENT){
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
    
    /* loop to wait for the required number of SelectionRequest events */
    while (dloop < sloop || sloop < 1){
      /* print messages about what we're waiting for if not in silent mode */
      if (fverb > OSILENT){
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
      
      XNextEvent(dpy, &ev);        /* Get the next event */
      while (ev.type != SelectionRequest && ev.type != SelectionClear ){
	/*
	 * ignore event, get next event if event isn't a SelectionRequest or
	 * a SelectionClear
	 */
        XNextEvent(dpy, &ev);
      }
	   
      if (ev.type == SelectionRequest){
	/* event is a SelectionRequest generate and sent a response */
        req = &(ev.xselectionrequest);
	     
        XChangeProperty(
          dpy,
          req->requestor,
          req->property,
          XA_STRING,
          8,
          PropModeReplace,
          (unsigned char*) seltxt,
          stelems
        );

        re.xselection.property  = req->property;
        re.xselection.type      = SelectionNotify;
        re.xselection.display   = req->display;
        re.xselection.requestor = req->requestor;
        re.xselection.selection = req->selection;
        re.xselection.target    = req->target;
        re.xselection.time      = req->time;
		     
        XSendEvent(dpy, req->requestor, 0, 0, &re);
        XFlush(dpy);
      }

      if (ev.type == SelectionClear){
        /*
         * Another app is telling us that it now owns the selection,
         * so we don't get SelectionRequest events from the X server
         * any more, time to exit...
         */
        if (fverb == OVERBOSE){
  	  /* print message in verbose mode only. */
          fprintf(
	    stderr,
	    "Another application took ownership of the selection.\n"
	  );
        }
        exit(EXIT_SUCCESS);
      }
      dloop++;		/* increment loop counter */
    }

  }
  else {
    static Atom	pty;
    int		format;
    Atom	type_return;

    unsigned char	*data;
    unsigned long	nitems, pty_size;
    
    pty = XInternAtom(dpy, "XCLIP_OUT", False); /* a property of our window
						   for apps to put their
						   selection into */
    XConvertSelection(
      dpy,
      sseln,
      XA_STRING,
      pty,
      win,
      CurrentTime
    );
    XNextEvent(dpy, &ev);
    while (ev.type != SelectionNotify && ev.type != None ){
        XNextEvent(dpy, &ev);
    }
    
    /* only get and print data if the selection is not empty */
    if (ev.type != None){
      /* Just see how much data is in our property */
      XGetWindowProperty(
        dpy,
        win,
        pty,
        0,
        0,
        False,
        AnyPropertyType,
        &type_return,
        &format,
        &nitems,
        &pty_size,
        &data
      );

      /* I read somewhere that requesting a pty_size that is too big
         can crash some X servers. Doesn't happen to me...
	 
	 actually get property and put into seltxt
       */
      XGetWindowProperty(
        dpy,
        win,
        pty,
        0,
        pty_size,
        False,
        AnyPropertyType,
        &type_return,
        &format,
        &nitems,
        &pty_size,
        (unsigned char **)&seltxt
      );

      /*
       * format seems to be 16 or 32 when a large amount of text is in the
       * selection. The XGetWindowProperty(3) man page explains the what the
       * format field is all about. It explains what to do where it is 16 or
       * 32, but I do not understand what needs to be done in these cases, and
       * after many hours of experimenting, research, etc, but unfortunately, I
       * haven't been able to implement _any_ functionality where format is 16
       * or 32.
       *
       * If you have much C experience, _please_ take a look at the man page,
       * and if you are able to modify this code to print the output for formats
       * of 16 and 32, please do and submit the patch!
       */
      if ( format == 8 ){
        printf(seltxt);
      }
      else {
	/* format == 0 means the selection is empty, no error */
	if (format)
          errformat();
      }
    }
  }

  /* Disconnect from the X server */
  XCloseDisplay(dpy);

  /* exit */
  return(EXIT_SUCCESS);
}
