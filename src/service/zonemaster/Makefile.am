# This Makefile.am is in the public domain
AM_CPPFLAGS = -I$(top_srcdir)/src/include

plugindir = $(libdir)/gnunet

pkgcfgdir= $(pkgdatadir)/config.d/

libexecdir= $(pkglibdir)/libexec/

pkgcfg_DATA = \
   zonemaster.conf

if USE_COVERAGE
  AM_CFLAGS = --coverage -O0
  XLIBS = -lgcov
endif

libexec_PROGRAMS = \
 gnunet-service-zonemaster

gnunet_service_zonemaster_SOURCES = \
 gnunet-service-zonemaster.c
gnunet_service_zonemaster_LDADD = \
  $(top_builddir)/src/service/dht/libgnunetdht.la \
  $(top_builddir)/src/lib/gnsrecord/libgnunetgnsrecord.la \
  $(top_builddir)/src/service/identity/libgnunetidentity.la  \
  $(top_builddir)/src/service/statistics/libgnunetstatistics.la \
  $(top_builddir)/src/lib/util/libgnunetutil.la \
  $(top_builddir)/src/service/namestore/libgnunetnamestore.la \
  $(top_builddir)/src/service/namecache/libgnunetnamecache.la \
  $(GN_LIBINTL) \
  -lpthread
