EXTRA_DIST = nbuf_schema.nbuf

include_HEADERS = nbuf.h libnbuf.h nbuf_schema.nb.h nbuf.hpp
noinst_HEADERS = lex.h acutest.h

lib_LTLIBRARIES = libnbuf.la
noinst_PROGRAMS = test
TESTS = test
CLEANFILES = test.nb.h test.nb.hpp test.nb.c test.nbuf test.out

libnbuf_la_SOURCES = nbuf.c lex.c nbuf_schema.nb.c parse.c print.c refl.c util.c compile.c
libnbuf_la_LDFLAGS = -no-undefined

test_SOURCES = test.c
test_LDADD = libnbuf.la
