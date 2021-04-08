AM_CPPFLAGS = -I$(srcdir)/../src
NBUFC=../nbufc/nbufc

noinst_PROGRAMS = benchmark
EXTRA_DIST = benchmark.nbuf
CLEANFILES = benchmark.nb.h benchmark.nb.c
BUILT_SOURCES = benchmark.nb.h
benchmark_SOURCES = benchmark.c
nodist_benchmark_SOURCES = benchmark.nb.c
benchmark_LDADD = ../src/libnbuf.la

noinst_PROGRAMS += benchmarkpp
CLEANFILES += benchmark.nb.hpp
BUILT_SOURCES += benchmark.nb.hpp
benchmarkpp_CXXFLAGS = $(AM_CXXFLAGS) # to avoid name clash on benchmark.o
benchmarkpp_SOURCES = benchmark.cc
benchmarkpp_LDADD = ../src/libnbuf.la

%.nb.h %.nb.c: %.nbuf
	$(NBUFC) -I $(srcdir) -c_out $(<F)

%.nb.hpp: %.nbuf
	$(NBUFC) -I $(srcdir) -cpp_out $(<F)

noinst_PROGRAMS += benchmark_pb
EXTRA_DIST += benchmark.proto
CLEANFILES += benchmark.pb.h benchmark.pb.cc
BUILT_SOURCES += benchmark.pb.h
benchmark_pb_SOURCES = benchmark_pb.cc
nodist_benchmark_pb_SOURCES = benchmark.pb.cc
benchmark_pb_LDADD = -lprotobuf

%.pb.h %.pb.cc: %.proto
	protoc --cpp_out=. -I $(srcdir) $(<F)

noinst_PROGRAMS += benchmark_fb
EXTRA_DIST += benchmark.fbs
CLEANFILES += benchmark_generated.h
BUILT_SOURCES += benchmark_generated.h
benchmark_fb_SOURCES = benchmark_fb.cc
benchmark_fb_LDADD = -lflatbuffers

%_generated.h: %.fbs
	flatc --cpp $<

noinst_PROGRAMS += benchmark_cp
EXTRA_DIST += benchmark.capnp
CLEANFILES += benchmark.capnp.h benchmark.capnp.c++
BUILT_SOURCES += benchmark.capnp.h
benchmark_cp_SOURCES = benchmark_cp.cc
nodist_benchmark_cp_SOURCES = benchmark.capnp.c++
benchmark_cp_LDADD = -lcapnp -lcapnpc

%.capnp: # disable the implicit rule % <- %.c++

%.capnp.h %.capnp.c++: %.capnp
	capnp compile -oc++ --src-prefix $(srcdir) $<

noinst_PROGRAMS += benchmark_json
benchmark_json_SOURCES = benchmark_json.c
benchmark_json_CPPFLAGS = `pkg-config --cflags-only-I json-c`
benchmark_json_LDADD = -ljson-c

