# Benchmarks

A message with several repeated objects are created for each benchmark.  The speed of following operations are tested:

  - Serialize
  - Deserialize
  - Print in text format
  - Parse from text format

The last two operations are for reference only; they are not performance-critical.

The following data are from a single run on the following machine.  Note, performance heavily depends on the workload.  The workload here may not be representative.

```
$ uname -a
Linux carbon 5.4.94 #1 SMP Fri Feb 19 11:54:16 EST 2021 x86_64 Intel(R) Core(TM) i7-6500U CPU @ 2.50GHz GenuineIntel GNU/Linux
$ gcc --version
gcc (Gentoo 10.2.0-r5 p6) 10.2.0
```

## Nbuf

```
create_serialize(&buf): 738ns/op
deserialize_use(&buf): 661ns/op
print_text_format(f, root): 138221ns/op
parse_text_format(&buf1, &buf): 64229ns/op
```

## Protocol Buffers

Protocol Buffers requires a separate serialize and deserialize step to convert between in-memory and on-wire representations.

```
create(&root): 2270ns/op
buf = serialize(root): 3217ns/op
deserialize(&root, buf): 5107ns/op
use(root): 504ns/op
print_text_format(&s, root): 119212ns/op
parse_text_format(s, &root): 131406ns/op
```

## Flatbuffers

Flatbuffers requires a separate verify step before reading the message, unless the user wants to live dangerously.
Without the verify step, Flatbuffers is the fastest library to perfrom reads.

```
create_serialize(builder): 8670ns/op
verify(builder): 1161ns/op
deserialize_use(root): 505ns/op
print_text_format(parser, builder, &buf): 131103ns/op
parse_text_format(&parser, buf): 191678ns/op
```

## Cap'n Proto

```
{capnp::MallocMessageBuilder builder; create_serialize(&builder);}: 1367ns/op
deserialize_use(builder): 1125ns/op
print_text_format(root, &buf): 332032ns/op
{capnp::MallocMessageBuilder builder; parse_text_format(&builder, buf);}: 950106ns/op
```

# Size comparison

All programs are dynamically linked and striped.  Here're the benchmark binaries:

```
-rwxr-xr-x 1 rui rui 18632 Mar 28 16:38 benchmark
-rwxr-xr-x 1 rui rui 23048 Mar 28 16:38 benchmark_cp
-rwxr-xr-x 1 rui rui 67960 Mar 28 16:38 benchmark_fb
-rwxr-xr-x 1 rui rui 48032 Mar 28 16:38 benchmark_pb
```

The runtime libraries:  (Cap'n Proto needs `libcapnpc` for the text format.)

```
-rwxr-xr-x 1 root root 682K Mar 25 20:57 /usr/lib64/libcapnpc.so
-rwxr-xr-x 1 root root 646K Mar 25 20:57 /usr/lib64/libcapnp.so
-rwxr-xr-x 1 root root 543K Mar 25 20:58 /usr/lib64/libflatbuffers.so
-rwxr-xr-x 1 root root  43K Mar 28 12:10 /usr/lib64/libnbuf.so
-rwxr-xr-x 1 root root 3.2M Mar 22 11:56 /usr/lib64/libprotobuf.so
```

The serialized binaries are as follows.  Protocol buffers are the most compact since it does not allocate for non-existent fields, and use variable-sized integers.

```
-rw-r--r-- 1 rui rui 5472 Mar 28 16:26 benchmark.cp.bin
-rw-r--r-- 1 rui rui 3976 Mar 28 16:36 benchmark_fb.bin
-rw-r--r-- 1 rui rui 4516 Mar 28 16:25 benchmark.nb.bin
-rw-r--r-- 1 rui rui 2266 Mar 28 16:25 benchmark.pb.bin
```
