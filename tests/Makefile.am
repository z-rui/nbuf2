AM_CPPFLAGS = -I$(srcdir)/../src
NBUFC=../src/nbufc/nbufc

BUILT_SOURCES =
EXTRA_DIST =
noinst_PROGRAMS =

noinst_PROGRAMS += test
EXTRA_DIST += test.nbuf logging.nbuf datetime.nbuf
BUILT_SOURCES += test.nb.h logging.nb.h datetime.nb.h
test_SOURCES = test.c
nodist_test_SOURCES = test.nb.c logging.nb.c datetime.nb.c
test_LDADD = ../src/libnbuf.la

noinst_PROGRAMS += testpp
BUILT_SOURCES += test.nb.hpp logging.nb.hpp datetime.nb.hpp
testpp_CXXFLAGS = $(AM_CXXFLAGS)  # to avoid name clash on test.o
testpp_SOURCES = test.cpp
testpp_LDADD = ../src/libnbuf.la

noinst_PROGRAMS += potato_test
EXTRA_DIST += potato.nbuf 
BUILT_SOURCES += potato.nb.h
potato_test_SOURCES = potato_test.c
potato_test_LDADD = ../src/libnbuf.la

# FIXME: this won't work in a VPATH build, since nbufc only searches
# for imports in CWD.
%.nb.c %.nb.h: %.nbuf
	$(NBUFC) -c_out $<
%.nb.hpp: %.nbuf
	$(NBUFC) -cpp_out $<

