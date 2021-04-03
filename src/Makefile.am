EXTRA_DIST = nbuf_schema.nbuf

include_HEADERS = nbuf.h libnbuf.h nbuf_schema.nb.h nbuf.hpp nbufc/libnbufc.h
noinst_HEADERS = lex.h acutest.h nbufc/util.h

lib_LTLIBRARIES = libnbuf.la libnbufc.la
bin_PROGRAMS = nbufc/nbufc
noinst_PROGRAMS = test
TESTS = test

libnbuf_la_SOURCES = nbuf.c lex.c nbuf_schema.nb.c parse.c print.c refl.c util.c
libnbuf_la_LDFLAGS = -no-undefined

libnbufc_la_SOURCES = nbufc/compile.c nbufc/lang_c.c nbufc/lang_cpp.c nbufc/raw.c nbufc/util.c
libnbufc_la_LIBADD = libnbuf.la
libnbufc_la_LDFLAGS = -no-undefined

nbufc_nbufc_SOURCES = nbufc/nbufc.c
nbufc_nbufc_LDADD = libnbufc.la libnbuf.la

test_SOURCES = test.c
test_CPPFLAGS = -Inbufc
test_LDADD = libnbufc.la libnbuf.la
