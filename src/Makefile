CFLAGS=-g -Wall -fPIC $(SYS_CFLAGS)
LDFLAGS=$(SYS_LDFLAGS)
SHLIB_EXT=so

LIBS=libnbuf.a
BINS=
ifeq ($(OS), Windows_NT)
	SHLIB_EXT=dll
	LDFLAGS+=-Wl,--out-implib,$@.a
	BINS+=libnbuf.$(SHLIB_EXT)
	LIBS+=libnbuf.$(SHLIB_EXT).a
else
	LIBS+=libnbuf.$(SHLIB_EXT)
endif
HDRS=nbuf.h nbuf.hpp libnbuf.h nbuf_schema.nb.h


all: $(LIBS)
	make -C nbufc/

install: $(LIBS) $(BINS)
	cp $(LIBS) $(PREFIX)/lib
	for i in $(BINS); do cp $$i $(PREFIX)/bin; done
	cp $(HDRS) $(PREFIX)/include
	make install -C nbufc/

OBJS=refl.o parse.o print.o lex.o nbuf.o util.o nbuf_schema.nb.o

clean:
	rm -f $(OBJS)
	make clean -C nbufc/

libnbuf.a: $(OBJS)
	ar rcu $@ $^

libnbuf.$(SHLIB_EXT): $(OBJS)
	$(CC) $(LDFLAGS) -shared -o $@ $^

test: test.o
	$(CC) $(LDFLAGS) -o $@ $^ -lnbuf -lnbufc

test.o:  test.c acutest.h nbuf_schema.nb.h nbuf.h libnbuf.h \
 nbufc/libnbufc.h
lex.o: lex.c lex.h
nbuf.o: nbuf.c nbuf.h
parse.o: parse.c libnbuf.h nbuf.h nbuf_schema.nb.h lex.h
print.o: print.c libnbuf.h nbuf.h nbuf_schema.nb.h
refl.o: refl.c libnbuf.h nbuf.h nbuf_schema.nb.h
util.o: util.c libnbuf.h nbuf.h nbuf_schema.nb.h
