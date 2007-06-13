#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <unistd.h>

/*
 *  xclip - puts standard in into X server selection for pasting
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

// xclip version
#define VERSION 0.02

// output level constants
#define OSILENT  0
#define OQUIET   1
#define OVERBOSE 2

// A function to print out the help message
void prhelp (){
  printf("Usage: xclip [OPTIONS] [FILES]\n");
  printf("Puts data from standard input or FILES into a X server selection for pasting.\n");
  printf("\n");
  printf("-l,  --loops     number of selection requests to wait for before exiting\n");
  printf("-d,  --display   X display to connect to (eg \"localhost:0\"\n");
  printf("-bg, --bg        wait for selection requests in the background\n");
  printf("-s,  --silent    silent mode (errors only, for scripting, etc)\n");
  printf("-q,  --quiet     normal level of output (default)\n");
  printf("-v,  --verbose   verbose mode (running commentary of what's happening)\n");
  printf("-h,  --help      usage information\n");
  printf("\n");
  printf("Report bugs to <kim.saunders@fortytwo.com.au>\n");
}

// A function to print the software version info
void prversion (){
  printf("xclip version %1.2f\n", VERSION);
  printf("Copyright (C) 2001 Kim Saunders\n");
  printf("Distributed under the terms of the GNU GPL\n");
}

int main(int argc, char *argv[])
{
  // Declare variables
  char *in = NULL;				// piped input char array
  char **files;
  int numFiles = 0;
  int currFile = 0;
  FILE* inf = NULL;
  int inelems = 0;                              // number of used elements in in
  int inalloc = 0;				// size of in
  Display *display; 		                // Display connection
  Window w;					// Window
  XSelectionRequestEvent *req;
  XEvent ev, re;				// X Event Structures

  int optc;					// option char
  int opti;					// option index
   
  int dloop = 0;				// done loops counter

  // Options that get set on the command line
  int   sloop = 1;				// number of loops
  char *sdisp = "";				// X display to connect to

  // Flags for command line options
  static int fverb = OQUIET;			// output level
  static int fhelp = 0;				// dispay help
  static int fvers = 0;				// dispay version info
  static int ffork = 0;				// fork into the background

  pid_t cpid;					// child pid if forking
   
  // options
  static struct option long_options[] = {
    { "silent" 	, 0, &fverb, OSILENT	},
    { "quiet"  	, 0, &fverb, OQUIET	},
    { "verbose"	, 0, &fverb, OVERBOSE	},
    { "help"	, 0, &fhelp, 1		},
    { "version"	, 0, &fvers, 1		},
    { "bg"	, 0, &ffork, 1		},
    { "display"	, 1,      0, 0		},
    { "loops"  	, 1,      0, 0		}
  };

  while (1) {
    // Get the option into optc
    optc = getopt_long_only(
      argc,
      argv,
      "abc",
      long_options,
      &opti
    );
     
    // Escape the loop if there are no more options
    if (optc == -1)
      break;

    if (optarg){  // the if below seems to segfault if optarg is null. Why???
      if ( long_options[opti].flag == 0 ) {
        // display option
        if ( strcmp(long_options[opti].name, "display") == 0) {
          sdisp = optarg;
	  if (fverb == OVERBOSE) // display in verbose mode only
            fprintf(stderr,"Display: %s\n", optarg);
        } 
        // loops option
        if ( strcmp(long_options[opti].name, "loops"  ) == 0) {
          sloop = atoi(optarg);
	  if (fverb == OVERBOSE) // display in verbose mode only
            fprintf(stderr,"Loops: %s\n", optarg);
        }
      }
    }
  }
   
  // --help flag set
  if (fhelp){
    prhelp();			// print help message
    exit(EXIT_SUCCESS);		// exit
  }
  
  // --version flag set
  if (fvers){
    prversion();		// print version information
    exit(EXIT_SUCCESS);		// exit
  }

  // Read remaining options (filenames)
  while (optind < argc){
    if (numFiles > 0) {
      files = realloc(files,(numFiles + 1) * sizeof(char*));
    } else {
      files = malloc(sizeof(char*));
    }
    files[numFiles++] = strdup(argv[optind]);
    optind++;
  }
   
  // Connect to the X server.
  if ( (display = XOpenDisplay(sdisp)) ){
    // successful
    if (fverb == OVERBOSE)
      printf("Connected to X server.\n");
  }
  else {
    // couldn't connect to X server. Print error.
    printf("Could not connect to X server.\n");
    perror(NULL);
    // exit
    exit(EXIT_FAILURE);
  }

  inalloc = 16;  // Reasonable ballpark figure
  in = malloc(inalloc);

  // Put chars into inc from stdin or files until we hit EOF
  do {
    if (numFiles == 0) {
      inf = stdin;
    }
    else {
      inf = fopen(files[currFile], "r");
      if (fverb == OVERBOSE)
        printf("Reading %s...\n", files[currFile]);
    }
    currFile++;

    while (!feof(inf)) {
      int n = 0;
      // If in is full (used elems = allocated elems)
      if (inelems == inalloc) {
        // Allocate another the double number of elems
	inalloc *= 2 + 1;
	in = (char *)realloc(in, inalloc * sizeof(char));
	if (in == NULL){
	  fprintf(stderr, "Error: Could not allocate memory.\n");
	  exit(EXIT_FAILURE);
	}
      }
      n = fread(in + inelems, 1, inalloc - inelems, inf);
      inelems += n;
      in[inelems] = '\0';
      /* For debugging this loop.
      fprintf(
        stderr,
        "Read: %d\nUsed elems: %d\nContents: %s\n",
	 n,
	 inelems,
	 in
      );
      */
    }
  } while (currFile < numFiles);
  
  // Create a window to own that will trap events
  w = XCreateSimpleWindow(
    display,
    DefaultRootWindow(display),
    0,
    0,
    1,
    1,
    0,
    0,
    0
  );
   
  // Take control of the selection 
  XSetSelectionOwner(display, XA_PRIMARY, w, CurrentTime);

  // fork into the background, exit parent process if required
  if (ffork){
    cpid = fork();
    if (cpid){
      exit(EXIT_SUCCESS); // exit the parent process;
    }
    fverb = OSILENT;      // no output from child
  }

  // print a message saying what we're waiting for
  if (fverb > OSILENT){
    if (sloop == 1)
      printf("Waiting for one selection request.\n");
    if (sloop  < 1)
      printf("Waiting for selection requests, Control-C to quit\n");
    if (sloop  > 1)
      printf("Waiting for %i selection requests, Control-C to quit\n", sloop);
  }
    
  while (dloop < sloop || sloop < 1){
    if (fverb > OSILENT){
      if (sloop  > 1)
        printf("  Waiting for selection request %i of %i.\n", dloop + 1, sloop);
      if (sloop == 1)
        printf("  Waiting for a selection request.\n");
      if (sloop  < 1)
        printf("  Waiting for selection request number %i\n", dloop + 1);
    }
      
    XNextEvent(display, &ev);        // Get the next event
    while (ev.type != SelectionRequest && ev.type != SelectionClear ){
      XNextEvent(display, &ev);
    }
	   
    if (ev.type == SelectionRequest){
      // printf("  Selection has been pasted.\n");
      req = &(ev.xselectionrequest);
	     
      XChangeProperty(
        display,
        req->requestor,
        req->property,
        XA_STRING,
        8,
        PropModeReplace,
        (unsigned char*) in,
        inelems
      );

      re.xselection.property  = req->property;
      re.xselection.type      = SelectionNotify;
      re.xselection.display   = req->display;
      re.xselection.requestor = req->requestor;
      re.xselection.selection = req->selection;
      re.xselection.target    = req->target;
      re.xselection.time      = req->time;
		     
      XSendEvent(display, req->requestor, 0, 0, &re);
      XFlush(display);
    }

    if (ev.type == SelectionClear){
      // Another app is telling us that it now owns the selection,
      // so we don't get selectionrequest events from the x server
      // any more, time to exit...
      printf("Error: Another application took ownership of the selection.\n");
      exit(EXIT_FAILURE);
    }
    dloop++;
  }
    
  // Disconnect from the X server
  XCloseDisplay(display);

  // Exit
  return(EXIT_SUCCESS);
}
