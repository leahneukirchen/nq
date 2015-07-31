CFLAGS=-g -std=c89 -Wall -O2
all: nq
nq: nq.c
clean:
	rm -f nq
