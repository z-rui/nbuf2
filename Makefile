CC=gcc
CXX=g++
SYS_CFLAGS=-O2
SYS_LDFLAGS=

nbuf:
	mkdir -p bin/ lib/ include/
	make install -C src/ PREFIX='$(PWD)' CC='$(CC)' CXX='$(CXX)' SYS_CFLAGS='$(SYS_CFLAGS)' SYS_LDFLAGS='$(SYS_LDFLAGS)'

tests: nbuf
	make -C tests/ CC='$(CC)' SYS_CFLAGS='$(SYS_CFLAGS) -I $(PWD)/include' SYS_LDFLAGS='$(SYS_LDFLAGS) -L $(PWD)/lib'

benchmark: nbuf
	make -C benchmark/ CC='$(CC)' CXX='$(CXX)' SYS_CFLAGS='$(SYS_CFLAGS) -I $(PWD)/include' SYS_LDFLAGS='$(SYS_LDFLAGS) -L $(PWD)/lib'

clean:
	for dir in src tests benchmark; do make clean -C $${dir}; done
