# This Makefile.am is in the public domain
AM_CPPFLAGS = -I$(top_srcdir)/src/include -I$(top_builddir)/src/include

if USE_COVERAGE
  AM_CFLAGS = --coverage -O0
endif

pkgcfgdir= $(pkgdatadir)/config.d/

libexecdir= $(pkglibdir)/libexec/

plugindir = $(libdir)/gnunet

pkgcfg_DATA = \
  vpn.conf

if LINUX
VPNBIN = gnunet-helper-vpn
endif


lib_LTLIBRARIES = \
  libgnunetvpn.la


libexec_PROGRAMS = \
  $(VPNBIN) \
  gnunet-service-vpn

gnunet_helper_vpn_SOURCES = \
  gnunet-helper-vpn.c
gnunet_helper_vpn_LDADD = \
  $(top_builddir)/src/lib/util/libgnunetutil.la \
  $(GN_LIBINTL)

gnunet_service_vpn_SOURCES = \
 gnunet-service-vpn.c
gnunet_service_vpn_LDADD = \
  $(top_builddir)/src/service/statistics/libgnunetstatistics.la \
  $(top_builddir)/src/lib/util/libgnunetutil.la \
  $(top_builddir)/src/service/cadet/libgnunetcadet.la \
  $(top_builddir)/src/service/regex/libgnunetregex.la \
  $(GN_LIBINTL)
gnunet_service_vpn_CFLAGS = \
  -I$(top_srcdir)/src/service/exit $(CFLAGS)

libgnunetvpn_la_SOURCES = \
 vpn_api.c vpn.h
libgnunetvpn_la_LIBADD = \
 $(top_builddir)/src/lib/util/libgnunetutil.la $(XLIB)
libgnunetvpn_la_LDFLAGS = \
  $(GN_LIBINTL) \
  $(GN_LIB_LDFLAGS)
