# This Makefile.am is in the public domain
AM_CPPFLAGS = -I$(top_srcdir)/src/include

if USE_COVERAGE
  AM_CFLAGS = --coverage
endif

if HAVE_SQLITE
lib_LTLIBRARIES = libgnunetsq.la
endif

libgnunetsq_la_SOURCES = \
  sq.c \
  sq_exec.c \
  sq_prepare.c \
  sq_query_helper.c \
  sq_result_helper.c
libgnunetsq_la_LIBADD = -lsqlite3 \
 $(top_builddir)/src/lib/util/libgnunetutil.la
libgnunetsq_la_LDFLAGS = \
 $(GN_LIBINTL) \
 $(GN_LIB_LDFLAGS) \
  -version-info 0:0:0

if ENABLE_TEST_RUN
TESTS = \
 test_sq
endif

check_PROGRAMS= \
 test_sq

test_sq_SOURCES = \
  test_sq.c
test_sq_LDADD = \
  libgnunetsq.la \
  $(top_builddir)/src/lib/util/libgnunetutil.la  \
  -lsqlite3 $(XLIB)
