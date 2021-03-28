NBUFC=../bin/nbufc
CC=gcc
CFLAGS=-fPIC -I../include -Wall -g -O2
LIBS=../lib/libnbuf.a

all: test potato_test

clean:
	rm -f *.o *.nb.c *.nb.h

potato_test: potato_test.c potato.nb.h
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)

test: test.o test.nb.o logging.nb.o datetime.nb.o
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

test.o: test.c test.nb.h logging.nb.h datetime.nb.h

%.nb.h %.nb.c: %.nbuf
	$(NBUFC) -c_out $<
