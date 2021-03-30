NBUFC=../bin/nbufc
CFLAGS=-fPIC -Wall -g -I../include -O2 
CXXFLAGS=$(CFLAGS)
LDFLAGS=$(SYS_LDFLAGS)

TARGETS=benchmark
TARGETS+=benchmarkpp
TARGETS+=benchmark_pb  # Protocol Buffers
TARGETS+=benchmark_fb  # Flatbuffers
TARGETS+=benchmark_cp  # Cap'n Proto

all: $(TARGETS)

clean:
	rm -f *.o *.nb.c *.nb.h *.pb.cc *.pb.h *.capnp.c++ *.capnp.h *_generated.h

benchmark: benchmark.o benchmark.nb.o
	$(CC) $(LDFLAGS) -o $@ $^ -lnbuf

benchmarkpp: benchmarkpp.o
	$(CXX) $(LDFLAGS) -o $@ $^ -lnbuf

benchmark.o: benchmark.c benchmark.nb.h

benchmarkpp.o: benchmark.cc benchmark.nb.hpp
	$(CXX) $(CFLAGS) -c -o $@ $<

%.nb.h %.nb.c: %.nbuf
	$(NBUFC) -c_out $<

%.nb.hpp: %.nbuf
	$(NBUFC) -cpp_out $<

benchmark_pb: benchmark_pb.o benchmark.pb.o
	$(CXX) $(LDFLAGS) -o $@ $^ -lprotobuf

benchmark_pb.o: benchmark_pb.cc benchmark.pb.h

%.pb.h %.pb.cc: %.proto
	protoc --cpp_out=. $<

benchmark_fb: benchmark_fb.o
	$(CXX) $(LDFLAGS) -o $@ $^ -lflatbuffers

benchmark_fb.o: benchmark_fb.cc benchmark_generated.h

%_generated.h: %.fbs
	flatc --cpp $<

benchmark_cp: benchmark_cp.o benchmark.capnp.o
	$(CXX) $(LDFLAGS) -o $@ $^ -lcapnp -lcapnpc

benchmark_cp.o: benchmark_cp.cc benchmark.capnp.h

benchmark.capnp:
	#

benchmark.capnp.o: benchmark.capnp.c++ benchmark.capnp.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<

%.capnp.h %.capnp.c++: %.capnp
	capnp compile -oc++ $<

