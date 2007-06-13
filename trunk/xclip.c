#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

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

#define VERSION 0.01

int main(int argc, char *argv[])
 {
   // Declare variables
   char *in = NULL;				// piped input char array
   char inc;					// current char of input
   int inelems = 0;                             // number of used elements in in
   int inalloc = 0;				// size of in
   Display *display; 		                // Display connection
   Window w;					// Window
   XSelectionRequestEvent *req;
   XEvent ev, re;				// X Event Structures

   int optc;                                    // option char
   int opti;                                    // option index
   
   int dloop = 0;				// done loops counter

   // Stuff that gets set
   int   sloop = 1;
   char *sdisp = "";

   // Flags for command line options
   static int fverb = 1;		// verbose mode (default)
   static int fhelp = 0;		// dispay help
   static int fvers = 0;		// dispay version info
   
   // options
   static struct option long_options[] = {
     { "quiet"  	, 0, &fverb, 0 },
     { "help"		, 0, &fhelp, 1 },
     { "version"	, 0, &fvers, 1 },
     { "display"	, 1,      0, 0 },
     { "loops"  	, 1,      0, 0 }
   };

   while (1){
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

       if (optarg){  // the if below segfaults if optarg is null. Why???
         if ( long_options[opti].flag == 0 ) {
	       
	     // display option
	     if ( strncmp(long_options[opti].name, "display") == 0) {
		     sdisp = optarg;
		     printf("Display: %s\n", optarg);
             }
	     // loops option
	     if ( strncmp(long_options[opti].name, "loops"  ) == 0) {
		     sloop = atoi(optarg);
		     printf("Loops: %s\n", optarg);
	     }
          }
       }
   }
   
   if (fhelp){
     printf("Usage: xclip [OPTIONS]\n");
     printf("Puts data from standard input into a X server selection for pasting.\n");
     printf("\n");
     printf("-l, --loops     number of selection requests to wait for before exiting\n");
     printf("-d, --display   X display to connect to (eg \"localhost:0\"\n");
     printf("-v, --verbose   verbose mode (default)\n");
     printf("-q, --quiet     terse mode (for use in scripts, etc)\n");
     printf("-h, --help      usage information\n");
     printf("\n");
     printf("Report bugs to <kim.saunders@fortytwo.com.au>\n");
     exit(EXIT_SUCCESS);
   }
   if (fvers){
     printf("xclip version %1.2f\n", VERSION);
     printf("Copyright (C) 2001 Kim Saunders\n");
     printf("Distributed under the terms of the GNU GPL\n");
     exit(EXIT_SUCCESS);
   }
   while (optind < argc){
     printf("Arg: %s\n", argv[optind++]);
   }

   if ( display = XOpenDisplay(sdisp) )          // Display connection
   {
     printf("Connected to X server.\n");
   }
   else {
     printf("Could not connect to X server.\n");
     perror(NULL);
     exit(EXIT_FAILURE);
   }

   // Put chars into inc from stdin until we hit EOF
   while ( (inc = getchar()) != EOF){
     // If in is full (used elems = allocated elems)
     if (inelems == inalloc){
       // Allocate another 10 elems
       inalloc += 10;
       in = (char *)realloc((char *)in, inalloc * sizeof(char));
       
       if(in == NULL){
         printf("Error: Could not allocate memory\n");
         exit(EXIT_FAILURE);
       }
     }
     in[inelems] = inc;
     inelems++;
   }
	 
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

   if (sloop == 1){
     printf("Waiting for one selection request.\n");
   }
   if (sloop < 1){
     printf("Waiting for selection requests, Control-C to quit\n");
   }
   if (sloop > 1){
     printf("Waiting for %i selection requests, Control-C to quit\n", sloop);
   }
   while (dloop < sloop || sloop < 1){
     if (sloop > 1){
       printf("  Waiting for selection request %i ", dloop + 1);
       printf("of %i.\n"       , sloop);
     }
     if (sloop == 1){
       printf("  Waiting for a selection request.\n");
     }
     if (sloop < 1){
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
