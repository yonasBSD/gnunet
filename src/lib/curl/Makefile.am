# This Makefile.am is in the public domain
AM_CPPFLAGS = -I$(top_srcdir)/src/include

if USE_COVERAGE
  AM_CFLAGS = --coverage -O0
  XLIB = -lgcov
endif

lib_LTLIBRARIES = \
  libgnunetcurl.la

libgnunetcurl_la_LDFLAGS = \
  $(GN_LIBINTL) \
  -version-info 0:1:0 \
  -no-undefined
libgnunetcurl_la_SOURCES = \
  curl.c curl_internal.h \
  curl_reschedule.c
libgnunetcurl_la_LIBADD = \
  $(top_builddir)/src/lib/util/libgnunetutil.la \
  -ljansson \
  @LIBCURL@ \
  $(XLIB)
libgnunetcurl_la_CPPFLAGS = \
 @LIBCURL_CPPFLAGS@ $(AM_CPPFLAGS) $(MHD_CFLAGS)

#check_PROGRAMS = \
#  test_curl

#TESTS = \
#  $(check_PROGRAMS)

#test_curl_SOURCES = \
#  test_curl.c
#test_curl_LDADD = \
#  libgnunetcurl.la \
#  $(top_builddir)/src/lib/util/libgnunetutil.la \
#  -ljansson -lcurl
