CC=gcc
CFLAGS=-g -Wall -I.. -fPIC
NBUFC=../nbufc/nbufc

all: nbufc libnbufc.a libnbufc.so

OBJS=raw.o lang_c.o compile.o util.o

clean:
	rm *.o libnbufc.a libnbufc.so

libnbufc.a: $(OBJS)
	ar rcu $@ $^

libnbufc.so: $(OBJS)
	$(CC) $(LDFLAGS) -shared -o $@ $^

nbufc: nbufc.o ./libnbufc.a ../libnbuf.a
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

compile.o: compile.c ../nbuf.h ../nbuf_schema.nb.h ../nbuf.h ../lex.h \
 util.h ../libnbuf.h ../nbuf_schema.nb.h libnbufc.h
lang_c.o: lang_c.c ../libnbuf.h ../nbuf.h ../nbuf_schema.nb.h libnbufc.h \
 ../nbuf.h ../nbuf_schema.nb.h util.h
nbufc.o: nbufc.c ../libnbuf.h ../nbuf.h ../nbuf_schema.nb.h libnbufc.h \
 ../nbuf.h ../nbuf_schema.nb.h util.h
raw.o: raw.c ../nbuf.h
util.o: util.c ../nbuf.h util.h libnbufc.h ../nbuf_schema.nb.h ../nbuf.h \
 ../libnbuf.h ../nbuf_schema.nb.h
