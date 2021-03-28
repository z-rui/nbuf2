CFLAGS=-g -Wall -I.. -fPIC $(SYS_CFLAGS)
LDFLAGS=-L. -L.. $(SYS_LDFLAGS)
SHLIB_EXT=so

LIBS=libnbufc.a
BINS=nbufc

ifeq ($(OS), Windows_NT)
	SHLIB_EXT=dll
	LDFLAGS+=-Wl,--out-implib,$@.a
	BINS+=libnbufc.$(SHLIB_EXT)
	LIBS+=libnbufc.$(SHLIB_EXT).a
else
	LIBS+=libnbufc.$(SHLIB_EXT)
endif

all: nbufc $(LIBS)

install: all
	cp $(LIBS) $(PREFIX)/lib
	cp $(BINS) $(PREFIX)/bin
	cp libnbufc.h $(PREFIX)/include

OBJS=raw.o lang_c.o compile.o util.o

clean:
	rm -f *.o

libnbufc.a: $(OBJS)
	ar rcu $@ $^

libnbufc.$(SHLIB_EXT): $(OBJS)
	$(CC) $(LDFLAGS) -shared -o $@ $^ -lnbuf

nbufc: nbufc.o $(LIBS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< -lnbufc -lnbuf

compile.o: compile.c ../nbuf.h ../nbuf_schema.nb.h ../nbuf.h ../lex.h \
 util.h ../libnbuf.h ../nbuf_schema.nb.h libnbufc.h
lang_c.o: lang_c.c ../libnbuf.h ../nbuf.h ../nbuf_schema.nb.h libnbufc.h \
 ../nbuf.h ../nbuf_schema.nb.h util.h
nbufc.o: nbufc.c ../libnbuf.h ../nbuf.h ../nbuf_schema.nb.h libnbufc.h \
 ../nbuf.h ../nbuf_schema.nb.h util.h
raw.o: raw.c ../nbuf.h
util.o: util.c ../nbuf.h util.h libnbufc.h ../nbuf_schema.nb.h ../nbuf.h \
 ../libnbuf.h ../nbuf_schema.nb.h
