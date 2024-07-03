ALL=nq nqtail nqterm

CFLAGS=-g -Wall -O2

DESTDIR=
PREFIX=/usr/local
BINDIR=$(PREFIX)/bin
MANDIR=$(PREFIX)/share/man

INSTALL=install

all: $(ALL)

clean: FRC
	rm -f nq nqtail

check: FRC all
	prove -v ./tests

install: FRC all
	mkdir -p $(DESTDIR)$(BINDIR) $(DESTDIR)$(MANDIR)/man1
	$(INSTALL) -m0755 $(ALL) $(DESTDIR)$(BINDIR)
	$(INSTALL) -m0644 $(ALL:=.1) $(DESTDIR)$(MANDIR)/man1

FRC:
