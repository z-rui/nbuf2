AM_CPPFLAGS = -I$(srcdir)/../src
NBUFC=../src/nbufc/nbufc

noinst_PROGRAMS = benchmark
BUILT_SOURCES = benchmark.nb.h
benchmark_SOURCES = benchmark.c
nodist_benchmark_SOURCES = benchmark.nb.c
benchmark_LDADD = ../src/libnbuf.la

noinst_PROGRAMS += benchmarkpp
BUILT_SOURCES += benchmark.nb.hpp
benchmarkpp_CXXFLAGS = $(AM_CXXFLAGS) # to avoid name clash on benchmark.o
benchmarkpp_SOURCES = benchmark.cc
benchmarkpp_LDADD = ../src/libnbuf.la

%.nb.h %.nb.c: %.nbuf
	$(NBUFC) -c_out $<

%.nb.hpp: %.nbuf
	$(NBUFC) -cpp_out $<

if HAVE_PROTOBUF
noinst_PROGRAMS += benchmark_pb
BUILT_SOURCES += benchmark.pb.h
benchmark_pb_SOURCES = benchmark_pb.cc
nodist_benchmark_pb_SOURCES = benchmark.pb.cc
benchmark_pb_CPPFLAGS = $(PROTOBUF_CFLAGS)
benchmark_pb_LDADD = $(PROTOBUF_LIBS)

%.pb.h %.pb.cc: %.proto
	protoc --cpp_out=. $<
endif

if HAVE_FLATBUFFERS
noinst_PROGRAMS += benchmark_fb
BUILT_SOURCES += benchmark_generated.h
benchmark_fb_SOURCES = benchmark_fb.cc
benchmark_fb_LDADD = -lflatbuffers

%_generated.h: %.fbs
	flatc --cpp $<
endif

if HAVE_CAPNP
noinst_PROGRAMS += benchmark_cp
BUILT_SOURCES += benchmark.capnp.h
benchmark_cp_SOURCES = benchmark_cp.cc
nodist_benchmark_cp_SOURCES = benchmark.capnp.c++
benchmark_cp_CPPFLAGS = $(CAPNP_CFLAGS)
benchmark_cp_LDADD = $(CAPNP_LIBS) -lcapnpc

%.capnp: # disable the implicit rule % <- %.c++

%.capnp.h %.capnp.c++: %.capnp
	capnp compile -oc++ $<
endif

if HAVE_JSON_C
noinst_PROGRAMS += benchmark_json
benchmark_json_SOURCES = benchmark_json.c
benchmark_json_CPPFLAGS = $(JSON_C_CFLAGS)
benchmark_json_LDADD = $(JSON_C_LIBS)
endif

