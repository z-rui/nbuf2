CC=gcc
CXX=g++
CFLAGS=-fPIC -I$(PWD)/include -Wall -g -O2
LDFLAGS=-L$(PWD)/lib

SUBDIRS=src/
SUBDIRS+=tests/
SUBDIRS+=benchmark/

all:
	for dir in $(SUBDIRS); do make -C $${dir} CC='$(CC)' CXX='$(CXX)' CFLAGS='$(CFLAGS)' LDFLAGS='$(LDFLAGS)'; done

clean:
	for dir in $(SUBDIRS); do make clean -C $${dir}; done
