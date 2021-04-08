AM_CPPFLAGS = -I$(srcdir)/../src
NBUFC=../nbufc/nbufc

noinst_PROGRAMS =
EXTRA_DIST =
CLEANFILES =
BUILT_SOURCES =

noinst_PROGRAMS += test
EXTRA_DIST += test.nbuf logging.nbuf datetime.nbuf
CLEANFILES += test.nb.h logging.nb.h datetime.nb.h
CLEANFILES += test.nb.c logging.nb.c datetime.nb.c
BUILT_SOURCES += test.nb.h logging.nb.h datetime.nb.h
test_SOURCES = test.c
nodist_test_SOURCES = test.nb.c logging.nb.c datetime.nb.c
test_LDADD = ../src/libnbuf.la

test.nb.c test.nb.h: test.nbuf
	$(NBUFC) -I $(srcdir) -c_out test.nbuf
logging.nb.c logging.nb.h: logging.nbuf
	$(NBUFC) -I $(srcdir) -c_out logging.nbuf
datetime.nb.c datetime.nb.h: datetime.nbuf
	$(NBUFC) -I $(srcdir) -c_out datetime.nbuf

noinst_PROGRAMS += testpp
CLEANFILES += test.nb.hpp logging.nb.hpp datetime.nb.hpp
BUILT_SOURCES += test.nb.hpp logging.nb.hpp datetime.nb.hpp
testpp_CXXFLAGS = $(AM_CXXFLAGS)  # to avoid name clash on test.o
testpp_SOURCES = test.cpp
testpp_LDADD = ../src/libnbuf.la

test.nb.hpp: test.nbuf
	$(NBUFC) -I $(srcdir) -cpp_out test.nbuf
logging.nb.hpp: logging.nbuf
	$(NBUFC) -I $(srcdir) -cpp_out logging.nbuf
datetime.nb.hpp: datetime.nbuf
	$(NBUFC) -I $(srcdir) -cpp_out datetime.nbuf

noinst_PROGRAMS += potato_test
EXTRA_DIST += potato.nbuf 
CLEANFILES += potato.nb.h potato.nb.c
BUILT_SOURCES += potato.nb.h
potato_test_SOURCES = potato_test.c
potato_test_LDADD = ../src/libnbuf.la

potato.nb.h: potato.nbuf
	$(NBUFC) -I $(srcdir) -c_out potato.nbuf
