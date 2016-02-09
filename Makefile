ALL=nq fq tq

CFLAGS=-g -Wall -O2

DESTDIR=
PREFIX=/usr/local
BINDIR=$(PREFIX)/bin
MANDIR=$(PREFIX)/share/man

all: $(ALL)

clean: FRC
	rm -f nq fq

check: FRC all
	prove -v ./tests

install: FRC all
	mkdir -p $(DESTDIR)$(BINDIR) $(DESTDIR)$(MANDIR)/man1
	install -m0755 $(ALL) $(DESTDIR)$(BINDIR)
	install -m0644 $(ALL:=.1) $(DESTDIR)$(MANDIR)/man1

uninstall:
	for util in $(ALL); do\
		rm -f $(DESTDIR)$(BINDIR)/$$util $(DESTDIR)$(MANDIR)/man1/$$util.1; \
	done

FRC:
