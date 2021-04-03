AC_INIT([nbuf], [0.1], [z-rui@hotmail.com])
AM_INIT_AUTOMAKE([-Wall foreign subdir-objects])
AC_PROG_CC
AC_PROG_CXX
AM_PROG_AR
LT_INIT([win32-dll])
AC_CONFIG_MACRO_DIRS([m4])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([
 Makefile
 src/Makefile
 tests/Makefile
 benchmark/Makefile
])
PKG_CHECK_MODULES([PROTOBUF], [protobuf],
		  AM_CONDITIONAL([HAVE_PROTOBUF], [true]),
		  AM_CONDITIONAL([HAVE_PROTOBUF], [false]))
PKG_CHECK_MODULES([CAPNP], [capnp],
		  AM_CONDITIONAL([HAVE_CAPNP], [true]),
		  AM_CONDITIONAL([HAVE_CAPNP], [false]))
PKG_CHECK_MODULES([JSON_C], [json-c],
		  AM_CONDITIONAL([HAVE_JSON_C], [true]),
		  AM_CONDITIONAL([HAVE_JSON_C], [false]))
AC_LANG_PUSH([C++])
AC_CHECK_HEADER("flatbuffers/flatbuffers.h",
 AM_CONDITIONAL([HAVE_FLATBUFFERS], [true]),
 AM_CONDITIONAL([HAVE_FLATBUFFERS], [false]))
AC_LANG_POP
AC_OUTPUT
