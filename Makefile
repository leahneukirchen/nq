CFLAGS=-g -Wall -O2 -D_POSIX_C_SOURCE=200809L
all: nq fq
nq: nq.c
fq: fq.c
clean:
	rm -f nq fq
check:
	prove -v ./tests
