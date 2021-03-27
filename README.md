# Nbuf2

This is a less naïve rework of the previous [nbuf](https://github.com/z-rui/nbuf).  The main improvements are support of importing schemas and a slightly more compact wire format.

In summary, it is a lightweight serialization library.  The wire format is designed so that no (de)serialization step is needed.

The library is written in C and generates C bindings.  Encoding and decoding a binary buffer are completely implemented in header files.

The runtime library (libnbuf) provides additional features:

- Printing any message in text format.
- Parse any message from text format.
- Reflection.
- Utility functions such as loading/saving file.

The messages are statically typed by a schema file.  The schema file is compiled by the compiler (nbufc).
