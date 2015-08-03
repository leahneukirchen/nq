CFLAGS=-g -Wall -O2
all: nq
nq: nq.c
clean:
	rm -f nq
check:
	prove -v ./tests
