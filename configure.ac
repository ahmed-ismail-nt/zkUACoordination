#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([zkUA], [0.1], [aismail@auto.tuwien.ac.at])
AM_INIT_AUTOMAKE(zkUA,0.1)
AC_OUTPUT(Makefile)
AC_CONFIG_SRCDIR([src/zk_jsonDecode.c])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([1.9])

# Checks for programs.
AC_PROG_CC
AC_PROG_LIBTOOL

# Checks for libraries.

# Checks for header files.
AC_FUNC_ALLOCA
AC_CHECK_HEADERS([arpa/inet.h fcntl.h limits.h mach/mach.h malloc.h netdb.h netinet/in.h stddef.h stdint.h stdlib.h string.h sys/ioctl.h sys/param.h sys/socket.h sys/time.h unistd.h wchar.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_TYPE_INT16_T
AC_TYPE_INT32_T
AC_TYPE_INT64_T
AC_TYPE_INT8_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([clock_gettime gethostname gettimeofday inet_ntoa memmove memset select socket strchr strdup strerror strstr strtol strtoul])

#AC_CONFIG_FILES([Makefile])
AC_OUTPUT
