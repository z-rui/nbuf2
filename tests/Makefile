NBUFC=../bin/nbufc
CFLAGS=-fPIC -Wall -g $(SYS_CFLAGS)
LDFLAGS=$(SYS_LDFLAGS)
LIBS=-lnbuf

all: test potato_test

clean:
	rm -f *.o *.nb.c *.nb.h

potato_test: potato_test.c potato.nb.h
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS)

test: test.o test.nb.o logging.nb.o datetime.nb.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

testpp: testpp.o
	$(CXX) $(LDFLAGS) -o $@ $^ $(LIBS)

testpp.o: test.cpp test.nb.hpp logging.nb.hpp datetime.nb.hpp
	$(CXX) -c -o $@ -g -Wall -I../src $<

test.o: test.c test.nb.h logging.nb.h datetime.nb.h

%.nb.h %.nb.c: %.nbuf
	$(NBUFC) -c_out $<

%.nb.hpp: %.nbuf
	$(NBUFC) -cpp_out $<
