CC=gcc
CFLAGS=-Wall -g
LDFLAGS=-lX11 -L/usr/X11R6/lib -I/usr/X11R6/include

OBJECT=xclip
INSTDIR=/usr/X11R6/bin

$(OBJECT): xclip.c
	$(CC) xclip.c $(CFLAGS) $(LDFLAGS) -o $(OBJECT)
	
clean:
	rm -f $(OBJECT)

install:
	cp $(OBJECT) $(INSTDIR)
