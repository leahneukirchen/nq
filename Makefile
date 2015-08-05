CFLAGS=-g -Wall -O2
all: nq fq
nq: nq.c
fq: fq.c
clean:
	rm -f nq
check:
	prove -v ./tests
