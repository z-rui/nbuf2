CC=gcc
CXX=g++
CFLAGS=-O2
LDFLAGS=

nbuf:
	mkdir -p bin/ lib/ include/
	make install -C src/ PREFIX='$(PWD)' CC='$(CC)' CXX='$(CXX)' SYS_CFLAGS='$(CFLAGS)' SYS_LDFLAGS='$(LDFLAGS)'

tests: nbuf
	make -C tests/ CC='$(CC)' SYS_CFLAGS='$(CFLAGS) -I $(PWD)/include' SYS_LDFLAGS='$(LDFLAGS) -L $(PWD)/lib'

benchmark: nbuf
	make -C benchmark/ CC='$(CC)' CXX='$(CXX)' SYS_CFLAGS='$(CFLAGS) -I $(PWD)/include' SYS_LDFLAGS='$(LDFLAGS) -L $(PWD)/lib'

clean:
	for dir in src tests benchmark; do make clean -C $${dir}; done
