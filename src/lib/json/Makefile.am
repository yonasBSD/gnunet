# This Makefile.am is in the public domain
AM_CPPFLAGS = -I$(top_srcdir)/src/include

if USE_COVERAGE
  AM_CFLAGS = --coverage -O0
  XLIB = -lgcov
endif

lib_LTLIBRARIES = \
  libgnunetjson.la

libgnunetjson_la_LDFLAGS = \
  $(GN_LIBINTL) \
  -version-info 3:0:0 \
  -no-undefined
libgnunetjson_la_CFLAGS = \
  $(MHD_CFLAGS) \
  $(AM_CFLAGS)
libgnunetjson_la_SOURCES = \
  json.c \
  json_generator.c \
  json_helper.c \
  json_pack.c
libgnunetjson_la_LIBADD = \
  $(top_builddir)/src/lib/util/libgnunetutil.la \
  -ljansson \
  $(MHD_LIBS) \
  $(XLIB) \
  $(Z_LIBS)

check_PROGRAMS = \
  test_json

TESTS = \
  $(check_PROGRAMS)

test_json_SOURCES = \
  test_json.c
test_json_CFLAGS = $(MHD_CFLAGS) $(AM_CFLAGS)
test_json_LDADD = \
  libgnunetjson.la \
  $(top_builddir)/src/lib/util/libgnunetutil.la \
  -ljansson
