# This Makefile.am is in the public domain
AM_CPPFLAGS = -I$(top_srcdir)/src/include

if USE_COVERAGE
  AM_CFLAGS = --coverage -O0
endif

pkgcfgdir= $(pkgdatadir)/config.d/

libexecdir= $(pkglibdir)/libexec/

plugindir = $(libdir)/gnunet

dist_pkgcfg_DATA = \
  exit.conf

if LINUX
EXITBIN = gnunet-helper-exit
endif


libexec_PROGRAMS = \
  gnunet-daemon-exit \
  $(EXITBIN)


gnunet_helper_exit_SOURCES = \
  gnunet-helper-exit.c

gnunet_daemon_exit_SOURCES = \
 gnunet-daemon-exit.c exit.h
gnunet_daemon_exit_LDADD = \
  $(top_builddir)/src/service/dht/libgnunetdht.la \
  $(top_builddir)/src/service/statistics/libgnunetstatistics.la \
  $(top_builddir)/src/lib/util/libgnunetutil.la \
  $(top_builddir)/src/service/cadet/libgnunetcadet.la \
  $(top_builddir)/src/service/regex/libgnunetregex.la \
  $(GN_LIBINTL)
