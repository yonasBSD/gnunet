# This Makefile.am is in the public domain
AM_CPPFLAGS = -I$(top_srcdir)/src/include

pkgcfgdir= $(pkgdatadir)/config.d/

libexecdir= $(pkglibdir)/libexec/

dist_pkgcfg_DATA = \
  template.conf

if USE_COVERAGE
  AM_CFLAGS = -fprofile-arcs -ftest-coverage
endif

# Note: In a real installation,
# bin_PROGRAMS should be used for gnunet-template
# libexec_PROGRAMS should be used for gnunet-service-template

noinst_PROGRAMS = \
 gnunet-service-template


gnunet_service_template_SOURCES = \
 gnunet-service-template.c
gnunet_service_template_LDADD = \
  $(top_builddir)/src/lib/util/libgnunetutil.la \
  $(GN_LIBINTL)


check_PROGRAMS = \
 test_template_api

if ENABLE_TEST_RUN
AM_TESTS_ENVIRONMENT=export GNUNET_PREFIX=$${GNUNET_PREFIX:-@libdir@};export PATH=$${GNUNET_PREFIX:-@prefix@}/bin:$$PATH;unset XDG_DATA_HOME;unset XDG_CONFIG_HOME;
TESTS = $(check_PROGRAMS)
endif

test_template_api_SOURCES = \
 test_template_api.c
test_template_api_LDADD = \
  $(top_builddir)/src/lib/util/libgnunetutil.la
