CC=gcc
CFLAGS=-g -Wall -fPIC
NBUFC=nbufc/nbufc

all: libnbuf.a libnbuf.so
	make -C nbufc/ CFLAGS='-I.. $(CFLAGS)'

OBJS=refl.o parse.o print.o lex.o nbuf.o util.o nbuf_schema.nb.o

clean:
	rm -f $(OBJS) libnbuf.a libnbuf.so
	make clean -C nbufc/

libnbuf.a: $(OBJS)
	ar rcu $@ $^

libnbuf.so: $(OBJS)
	$(CC) $(LDFLAGS) -shared -o $@ $^

%.nb.h %.nb.c: %.nbuf
	$(NBUFC) -c_out $<

lex.o: lex.c lex.h nbuf.h
nbuf.o: nbuf.c nbuf.h
parse.o: parse.c libnbuf.h nbuf.h nbuf_schema.nb.h lex.h
print.o: print.c libnbuf.h nbuf.h nbuf_schema.nb.h
refl.o: refl.c libnbuf.h nbuf.h nbuf_schema.nb.h
util.o: util.c libnbuf.h nbuf.h nbuf_schema.nb.h
