# This Makefile.am is in the public domain
AM_CPPFLAGS = -I$(top_srcdir)/src/include

libexecdir= $(pkglibdir)/libexec/

pkgcfgdir= $(pkgdatadir)/config.d/

pkgcfg_DATA = \
  nat.conf

if LINUX
  NATBIN = gnunet-helper-nat-server gnunet-helper-nat-client
  NATSERVER = gnunet-helper-nat-server.c
  NATCLIENT = gnunet-helper-nat-client.c
else
if XFREEBSD
  NATBIN = gnunet-helper-nat-server gnunet-helper-nat-client
  NATSERVER = gnunet-helper-nat-server.c
  NATCLIENT = gnunet-helper-nat-client.c
endif
else
install-exec-hook:
endif

libexec_PROGRAMS = \
 $(NATBIN) \
 gnunet-service-nat


gnunet_helper_nat_server_SOURCES = \
 $(NATSERVER)

gnunet_helper_nat_client_SOURCES = \
 $(NATCLIENT)


if USE_COVERAGE
  AM_CFLAGS = -fprofile-arcs -ftest-coverage
endif

lib_LTLIBRARIES = \
  libgnunetnatnew.la

libgnunetnatnew_la_SOURCES = \
  nat_api.c \
  nat_api_stun.c nat_stun.h \
  nat.h
libgnunetnatnew_la_LIBADD = \
  $(top_builddir)/src/lib/util/libgnunetutil.la \
  $(GN_LIBINTL) @EXT_LIBS@
libgnunetnatnew_la_LDFLAGS = \
  $(GN_LIB_LDFLAGS)  \
  -version-info 2:0:0

gnunet_service_nat_SOURCES = \
 gnunet-service-nat.c gnunet-service-nat.h \
 gnunet-service-nat_externalip.c gnunet-service-nat_externalip.h \
 gnunet-service-nat_stun.c gnunet-service-nat_stun.h \
 gnunet-service-nat_mini.c gnunet-service-nat_mini.h \
 gnunet-service-nat_helper.c gnunet-service-nat_helper.h
gnunet_service_nat_LDADD = \
  $(top_builddir)/src/lib/util/libgnunetutil.la \
  $(top_builddir)/src/service/statistics/libgnunetstatistics.la \
  $(LIBGCRYPT_LIBS) \
  -lgcrypt \
  $(GN_LIBINTL)

#check_PROGRAMS = \
#  test_nat \
#  test_nat_mini \
#  test_nat_test \
#  test_stun

if ENABLE_TEST_RUN
 AM_TESTS_ENVIRONMENT=export GNUNET_PREFIX=$${GNUNET_PREFIX:-@libdir@};export PATH=$${GNUNET_PREFIX:-@prefix@}/bin:$$PATH;unset XDG_DATA_HOME;unset XDG_CONFIG_HOME;
 TESTS = $(check_PROGRAMS)
endif

#test_nat_SOURCES = \
#  test_nat.c
#test_nat_LDADD = \
# libgnunetnat.la \
# $(top_builddir)/src/lib/util/libgnunetutil.la

#test_nat_mini_SOURCES = \
#  test_nat_mini.c
#test_nat_mini_LDADD = \
# libgnunetnat.la \
# $(top_builddir)/src/lib/util/libgnunetutil.la

#test_nat_test_SOURCES = \
#  test_nat_test.c
#test_nat_test_LDADD = \
# libgnunetnat.la \
# $(top_builddir)/src/lib/util/libgnunetutil.la

#test_stun_SOURCES = \
#  test_stun.c
#test_stun_LDADD = \
# libgnunetnat.la \
# $(top_builddir)/src/lib/util/libgnunetutil.la

EXTRA_DIST = \
 test_nat_data.conf \
 test_nat_test_data.conf \
 test_stun.conf
