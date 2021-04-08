AC_INIT([nbuf], [0.1], [z-rui@hotmail.com])
AM_INIT_AUTOMAKE([-Wall foreign subdir-objects])
AC_PROG_CC
AC_PROG_CXX
AM_PROG_AR
AC_CHECK_HEADER([stdint.h], , [AC_MSG_ERROR([<stdint.h> not present on this system])])
AC_CHECK_HEADERS([unistd.h])
AC_FUNC_MMAP
LT_INIT([win32-dll])
AC_CONFIG_MACRO_DIRS([m4])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([
 Makefile
 src/Makefile
 nbufc/Makefile
 tests/Makefile
 benchmark/Makefile
])
AC_OUTPUT
