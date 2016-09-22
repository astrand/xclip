-include config.mk

PREFIX		?= /usr
BINDIR		?= $(PREFIX)/bin
MANDIR		?= $(PREFIX)/share/man

VERSION		:= 0.13

CC		?= gcc
CFLAGS		?= -Wall
CFLAGS		+= -DPACKAGE_NAME=\"xclip\" -DPACKAGE_VERSION=\"$(VERSION)\"
LDFLAGS		?= 
LIBS		+= -lX11 -lXmu

STRIP			?= -s
INSTALL			?= install
INSTALL_MAN_DIR		?= $(INSTALL) -d -m 755
INSTALL_PROGRAM_DIR	?= $(INSTALL) -d -m 755
INSTALL_MAN		?= $(INSTALL) -m 644
INSTALL_PROGRAM		?= $(INSTALL) -m 755 $(STRIP)
INSTALL_SCRIPT		?= $(INSTALL) -m 755

OBJS	:= xclib.o xcprint.o xclip.o
BINS	:= xclip
SCRIPTS	:= xclip-copyfile xclip-pastefile xclip-cutfile
MANS	:= xclip.1 xclip-copyfile.1

CHECK_HEADERS := X11/Xmu/Atoms.h X11/Intrinsic.h

.SUFFIXES: .c .o

.c.o:
	$(CC) $(CFLAGS) -o $@ -c $<

all: xclip

configure:
	@(for h in $(CHECK_HEADERS); do echo "#include <$$h>"; done; \
		echo "int main(void) {};") | $(CC) $(CFLAGS) -x c -o /dev/null -

${OBJS}: configure

xclip: $(X11OBJ) $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(X11OBJ) $(OBJS) $(LIBS)

install: install-bin install-man xclip

install-bin: $(BINS) $(SCRIPTS)
	$(INSTALL_PROGRAM_DIR) $(DESTDIR)$(BINDIR)
	$(INSTALL_PROGRAM) $(BINS) $(DESTDIR)$(BINDIR)
	$(INSTALL_SCRIPT) $(SCRIPTS) $(DESTDIR)$(BINDIR)

install-man: $(MANS)
	$(INSTALL_MAN_DIR) $(DESTDIR)$(MANDIR)/man1
	$(INSTALL_MAN) $(MANS) $(DESTDIR)$(MANDIR)/man1

clean:
	rm -f *.o *~ xclip

.PHONY: all configure install install-bin install-man clean
