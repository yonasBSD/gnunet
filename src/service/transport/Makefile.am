# This Makefile.am is in the public domain
AM_CPPFLAGS = -I$(top_srcdir)/src/include -I$(top_builddir)/src/include

plugindir = $(libdir)/gnunet

pkgcfgdir= $(pkgdatadir)/config.d/

libexecdir= $(pkglibdir)/libexec/

pkgcfg_DATA = \
  transport.conf

if USE_COVERAGE
  AM_CFLAGS = --coverage -O0
endif

noinst_PROGRAMS = \
 gnunet-communicator-udp

TESTING_LIBS = \
  libgnunettestingtransport.la

lib_LTLIBRARIES = \
  libgnunettransportapplication.la \
  libgnunettransportcore.la \
  libgnunettransportcommunicator.la \
  libgnunettransportmonitor.la \
  $(TESTING_LIBS)

libgnunet_plugin_testing_transport_simple_send_la_SOURCES = \
  plugin_testing_transport_simple_send.c
libgnunet_plugin_testing_transport_simple_send_la_LIBADD = \
  libgnunettestingtransport.la \
  $(top_builddir)/src/service/arm/libgnunettestingarm.la \
  $(top_builddir)/src/lib/testing/libgnunettesting.la \
  $(top_builddir)/src/service/testbed/libgnunettestbed.la \
  $(top_builddir)/src/service/testbed/libgnunettestingtestbed.la \
  $(top_builddir)/src/service/arm/libgnunetarm.la \
  $(top_builddir)/src/lib/util/libgnunetutil.la \
  $(LTLIBINTL)

libgnunettestingtransport_la_SOURCES = \
  testing_transport_traits.c transport-testing-cmds.h \
  transport-testing-communicator.c transport-testing-communicator.h \
  transport-testing2.h \
  testing_cmd_transport_send_simple.c
#  testing_cmd_transport_connect_peers.c \
#  transport_api_cmd_backchannel_check.c \
#  transport_api_cmd_send_simple_performance.c \
#  transport-testing-cmds.h \
#  transport-testing-filenames2.c \
#  transport-testing-loggers2.c \
#  transport-testing-main2.c \
#  transport-testing-send2.c 
libgnunettestingtransport_la_LIBADD = \
  libgnunettransportapplication.la \
  libgnunettransportcore.la \
  $(top_builddir)/src/service/testbed/libgnunettestbed.la \
  $(top_builddir)/src/service/arm/libgnunetarm.la \
  $(top_builddir)/src/lib/testing/libgnunettesting.la \
  $(top_builddir)/src/service/testbed/libgnunettestbed.la \
  $(top_builddir)/src/service/testbed/libgnunettestingtestbed.la \
  $(top_builddir)/src/lib/hello/libgnunethello.la \
  $(top_builddir)/src/service/peerstore/libgnunetpeerstore.la \
  $(top_builddir)/src/lib/util/libgnunetutil.la
libgnunettestingtransport_la_LDFLAGS = \
  $(GN_LIBINTL) \
  $(GN_LIB_LDFLAGS) \
  -version-info 0:0:0

libgnunettransportapplication_la_SOURCES = \
  transport_api_application.c
libgnunettransportapplication_la_LIBADD = \
 $(top_builddir)/src/lib/util/libgnunetutil.la \
 $(LTLIBINTL)
libgnunettransportapplication_la_LDFLAGS = \
  $(GN_LIB_LDFLAGS)  \
  -version-info 0:0:0


libgnunettransportcore_la_SOURCES = \
  transport_api_core.c
libgnunettransportcore_la_LIBADD = \
  $(top_builddir)/src/lib/util/libgnunetutil.la \
  $(GN_LIBINTL)
libgnunettransportcore_la_LDFLAGS = \
  $(GN_LIB_LDFLAGS)  \
  -version-info 0:0:0

libgnunettransportcommunicator_la_SOURCES = \
  transport_api_communication.c
libgnunettransportcommunicator_la_LIBADD = \
  $(top_builddir)/src/lib/util/libgnunetutil.la \
  $(GN_LIBINTL)
libgnunettransportcommunicator_la_LDFLAGS = \
  $(GN_LIB_LDFLAGS)  \
  -version-info 0:0:0


libgnunettransportmonitor_la_SOURCES = \
  transport_api_monitor.c
libgnunettransportmonitor_la_LIBADD = \
  $(top_builddir)/src/lib/util/libgnunetutil.la \
  $(GN_LIBINTL)
libgnunettransportmonitor_la_LDFLAGS = \
  $(GN_LIB_LDFLAGS)  \
  -version-info 0:0:0


libexec_PROGRAMS = \
 gnunet-service-transport \
 gnunet-communicator-unix \
 gnunet-communicator-udp \
 gnunet-communicator-tcp
if HAVE_EXPERIMENTAL
if HAVE_QUICHE
libexec_PROGRAMS += \
 gnunet-communicator-quic
endif
endif
if HAVE_NGHTTP
libexec_PROGRAMS += \
 gnunet-communicator-http3
endif


#bin_PROGRAMS = \
# gnunet-transport

bin_SCRIPTS = \
 gnunet-transport-certificate-creation

# See: https://www.gnu.org/software/automake/manual/html_node/Scripts.html#Scripts
do_subst = sed -e 's,[@]pkgdatadir[@],$(pkgdatadir),g'


gnunet-transport-certificate-creation: gnunet-transport-certificate-creation.in Makefile
	$(AWK) -v bdir="$(bindir)" -v py="$(PYTHON)" -v awkay="$(AWK_BINARY)" -v pfx="$(prefix)" -v prl="$(PERL)" -v sysconfdirectory="$(sysconfdir)" -v pkgdatadirectory="$(pkgdatadir)" -f $(top_srcdir)/scripts/dosubst.awk < $(srcdir)/gnunet-transport-certificate-creation.in > gnunet-transport-certificate-creation
	@chmod +x gnunet-transport-certificate-creation




gnunet_communicator_unix_SOURCES = \
 gnunet-communicator-unix.c
gnunet_communicator_unix_LDADD = \
  libgnunettransportcommunicator.la \
  $(top_builddir)/src/service/statistics/libgnunetstatistics.la \
  $(top_builddir)/src/lib/util/libgnunetutil.la

gnunet_communicator_tcp_SOURCES = \
 gnunet-communicator-tcp.c
gnunet_communicator_tcp_LDADD = \
  libgnunettransportcommunicator.la \
  $(top_builddir)/src/service/peerstore/libgnunetpeerstore.la \
  $(top_builddir)/src/service/nat/libgnunetnatnew.la \
  $(top_builddir)/src/service/statistics/libgnunetstatistics.la \
  $(top_builddir)/src/lib/hello/libgnunethello.la \
  $(top_builddir)/src/lib/util/libgnunetutil.la \
  $(LIBGCRYPT_LIBS)

gnunet_communicator_udp_SOURCES = \
 gnunet-communicator-udp.c
gnunet_communicator_udp_LDADD = \
  libgnunettransportapplication.la \
  libgnunettransportcommunicator.la \
  $(top_builddir)/src/service/nat/libgnunetnatnew.la \
  $(top_builddir)/src/service/statistics/libgnunetstatistics.la \
  $(top_builddir)/src/lib/util/libgnunetutil.la \
  $(LIBGCRYPT_LIBS)

if HAVE_EXPERIMENTAL
if HAVE_QUICHE
gnunet_communicator_quic_SOURCES = \
  gnunet-communicator-quic.c
gnunet_communicator_quic_LDADD = \
  libgnunettransportapplication.la \
  libgnunettransportcommunicator.la \
  $(top_builddir)/src/service/peerstore/libgnunetpeerstore.la \
  $(top_builddir)/src/service/nat/libgnunetnatnew.la \
  $(top_builddir)/src/service/statistics/libgnunetstatistics.la \
  $(top_builddir)/src/lib/util/libgnunetutil.la \
  -lquiche \
  $(LIBGCRYPT_LIBS)
endif
endif

if HAVE_NGHTTP
gnunet_communicator_http3_SOURCES = \
 gnunet-communicator-http3.c
gnunet_communicator_http3_CFLAGS = \
  $(NGHTTP_CFLAGS) $(NGTCP_CFLAGS) $(NGTCP_CRYPTO_CFLAGS)
gnunet_communicator_http3_LDADD = \
  libgnunettransportapplication.la \
  libgnunettransportcommunicator.la \
  $(top_builddir)/src/service/peerstore/libgnunetpeerstore.la \
  $(top_builddir)/src/service/nat/libgnunetnatnew.la \
  $(top_builddir)/src/service/statistics/libgnunetstatistics.la \
  $(top_builddir)/src/lib/util/libgnunetutil.la \
  $(NGTCP_LIBS) \
  $(NGTCP_CRYPTO_LIBS) \
  $(NGHTTP_LIBS) \
  $(LIBGCRYPT_LIBS) \
  -lgnutls
endif

#gnunet_transport_SOURCES = \
# gnunet-transport.c
#gnunet_transport_LDADD = \
#  libgnunettransport.la \
#  $(top_builddir)/src/lib/hello/libgnunethello.la \
#  $(top_builddir)/src/lib/util/libgnunetutil.la \
#  $(GN_LIBINTL)

gnunet_service_transport_SOURCES = \
 gnunet-service-transport.c transport.h
gnunet_service_transport_LDADD = \
  $(top_builddir)/src/service/peerstore/libgnunetpeerstore.la \
  $(top_builddir)/src/lib/hello/libgnunethello.la \
  $(top_builddir)/src/service/statistics/libgnunetstatistics.la \
  $(top_builddir)/src/service/nat/libgnunetnatnew.la \
  $(top_builddir)/src/lib/util/libgnunetutil.la \
  $(LIBGCRYPT_LIBS) \
  $(GN_LIBINTL)

plugin_LTLIBRARIES = \
	libgnunet_plugin_testing_transport_simple_send.la

#libgnunet_test_transport_plugin_cmd_nat_mapping_la_SOURCES = \
# test_transport_plugin_cmd_nat_mapping.c
#libgnunet_test_transport_plugin_cmd_nat_mapping_la_LIBADD = \
#  libgnunettransporttesting2.la \
#  libgnunettransportapplication.la \
#  libgnunettransportcore.la \
#  $(top_builddir)/src/service/testing/libgnunettesting.la \
#  $(top_builddir)/src/service/peerstore/libgnunetpeerstore.la \
#  $(top_builddir)/src/service/statistics/libgnunetstatistics.la \
#  $(top_builddir)/src/lib/hello/libgnunethello.la \
#  $(top_builddir)/src/service/arm/libgnunetarm.la \
#  $(top_builddir)/src/lib/util/libgnunetutil.la \
#  $(LTLIBINTL)
#libgnunet_test_transport_plugin_cmd_nat_mapping_la_LDFLAGS = \
#  $(GN_PLUGIN_LDFLAGS)

check_PROGRAMS = \
 test_communicator_basic-tcp \
 test_communicator_basic-udp \
 test_communicator_rekey-tcp \
 test_communicator_bidirect-tcp \
 test_communicator_rekey-udp \
 test_communicator_backchannel-udp
if HAVE_NGHTTP
 check_PROGRAMS += test_communicator_basic-http3
 check_PROGRAMS += test_communicator_bidirect-http3
endif

if HAVE_EXPERIMENTAL
check_PROGRAMS += test_communicator_basic-quic \
 test_communicator_basic-unix
# check_SCRIPTS= \
  # test_transport_start_testcase.sh \
  # test_transport_simple_send_performance.sh \
  # test_transport_nat_icmp_tcp.sh \
  # test_transport_nat_upnp.sh \
  # test_transport_simple_send_string.sh \
  # test_transport_simple_send.sh \
  # test_transport_simple_send_broadcast.sh \
  # test_transport_udp_backchannel.sh \
  # test_transport_simple_send_dv_circle.sh \
  # test_transport_simple_send_dv_inverse.sh
endif

if ENABLE_TEST_RUN
AM_TESTS_ENVIRONMENT=export GNUNET_PREFIX=$${GNUNET_PREFIX:-@libdir@};export PATH=$${GNUNET_PREFIX:-@prefix@}/bin:$$PATH;unset XDG_DATA_HOME;unset XDG_CONFIG_HOME;
TESTS = \
  $(check_SCRIPTS) \
  $(check_PROGRAMS)
endif



test_communicator_basic_unix_SOURCES = \
 test_communicator_basic.c
test_communicator_basic_unix_LDADD = \
 libgnunettestingtransport.la \
 $(top_builddir)/src/lib/testing/libgnunettesting.la \
 $(top_builddir)/src/lib/util/libgnunetutil.la \
 $(top_builddir)/src/service/statistics/libgnunetstatistics.la

test_communicator_basic_tcp_SOURCES = \
 test_communicator_basic.c
test_communicator_basic_tcp_LDADD = \
 libgnunettestingtransport.la \
 $(top_builddir)/src/lib/testing/libgnunettesting.la \
 $(top_builddir)/src/lib/util/libgnunetutil.la \
 $(top_builddir)/src/service/statistics/libgnunetstatistics.la

test_communicator_basic_udp_SOURCES = \
 test_communicator_basic.c
test_communicator_basic_udp_LDADD = \
 libgnunettestingtransport.la \
 $(top_builddir)/src/lib/testing/libgnunettesting.la \
 $(top_builddir)/src/lib/util/libgnunetutil.la \
 $(top_builddir)/src/service/statistics/libgnunetstatistics.la

test_communicator_basic_quic_SOURCES = \
 test_communicator_basic.c
test_communicator_basic_quic_LDADD = \
 libgnunettestingtransport.la \
 $(top_builddir)/src/lib/testing/libgnunettesting.la \
 $(top_builddir)/src/lib/util/libgnunetutil.la \
 $(top_builddir)/src/service/statistics/libgnunetstatistics.la

if HAVE_NGHTTP
test_communicator_basic_http3_SOURCES = \
 test_communicator_basic.c
test_communicator_basic_http3_LDADD = \
 libgnunettestingtransport.la \
 $(top_builddir)/src/lib/testing/libgnunettesting.la \
 $(top_builddir)/src/lib/util/libgnunetutil.la \
 $(top_builddir)/src/service/statistics/libgnunetstatistics.la

test_communicator_bidirect_http3_SOURCES = \
 test_communicator_basic.c
test_communicator_bidirect_http3_LDADD = \
 libgnunettestingtransport.la \
 $(top_builddir)/src/lib/testing/libgnunettesting.la \
 $(top_builddir)/src/lib/util/libgnunetutil.la \
 $(top_builddir)/src/service/statistics/libgnunetstatistics.la
endif

test_communicator_rekey_tcp_SOURCES = \
 test_communicator_basic.c
test_communicator_rekey_tcp_LDADD = \
 libgnunettestingtransport.la \
 $(top_builddir)/src/lib/testing/libgnunettesting.la \
 $(top_builddir)/src/lib/util/libgnunetutil.la \
 $(top_builddir)/src/service/statistics/libgnunetstatistics.la

test_communicator_rekey_udp_SOURCES = \
 test_communicator_basic.c
test_communicator_rekey_udp_LDADD = \
 libgnunettestingtransport.la \
 $(top_builddir)/src/lib/testing/libgnunettesting.la \
 $(top_builddir)/src/lib/util/libgnunetutil.la \
 $(top_builddir)/src/service/statistics/libgnunetstatistics.la

test_communicator_backchannel_udp_SOURCES = \
 test_communicator_basic.c
test_communicator_backchannel_udp_LDADD = \
 libgnunettestingtransport.la \
 $(top_builddir)/src/lib/testing/libgnunettesting.la \
 $(top_builddir)/src/lib/util/libgnunetutil.la \
 $(top_builddir)/src/service/statistics/libgnunetstatistics.la

test_communicator_bidirect_tcp_SOURCES = \
 test_communicator_basic.c
test_communicator_bidirect_tcp_LDADD = \
 libgnunettestingtransport.la \
 $(top_builddir)/src/lib/testing/libgnunettesting.la \
 $(top_builddir)/src/lib/util/libgnunetutil.la \
 $(top_builddir)/src/service/statistics/libgnunetstatistics.la

#test_transport_api_tcp_SOURCES = \
# test_transport_api.c
#test_transport_api_tcp_LDADD = \
# $(top_builddir)/src/lib/hello/libgnunethello.la \
# $(top_builddir)/src/lib/util/libgnunetutil.la  \
# libgnunettestingtransport.la

EXTRA_DIST = \
test_transport_start_testcase.sh \
test_transport_simple_send_performance.sh \
test_transport_nat_icmp_tcp.sh \
test_transport_nat_upnp.sh \
test_transport_simple_send_string.sh \
test_transport_simple_send.sh \
test_transport_simple_send_broadcast.sh \
test_transport_udp_backchannel.sh \
test_transport_simple_send_dv_circle.sh \
test_transport_simple_send_dv_inverse.sh \
gnunet-transport-certificate-creation.in \
test_plugin_hostkey \
test_plugin_hostkey.ecc \
test_delay \
template_cfg_peer1.conf\
template_cfg_peer2.conf\
test_transport_api_data.conf\
test_transport_api_multi_peer1.conf\
test_transport_api_multi_peer2.conf\
test_transport_api_tcp_nat_peer1.conf\
test_transport_api_tcp_nat_peer2.conf\
test_transport_api_tcp_peer1.conf\
test_transport_api_tcp_peer2.conf\
test_transport_api_udp_nat_peer1.conf\
test_transport_api_udp_nat_peer2.conf\
test_transport_api_udp_peer1.conf\
test_transport_api_udp_peer2.conf\
test_transport_api_unix_peer1.conf\
test_transport_api_unix_peer2.conf\
test_transport_api_monitor_peers_peer1.conf\
test_transport_api_monitor_peers_peer2.conf\
test_transport_api_monitor_validation_peer1.conf\
test_transport_api_monitor_validation_peer2.conf\
test_transport_defaults.conf\
test_communicator_unix_basic_peer1.conf \
test_communicator_unix_basic_peer2.conf \
test_communicator_tcp_basic_peer1.conf \
test_communicator_tcp_basic_peer2.conf \
test_communicator_udp_basic_peer1.conf \
test_communicator_http3_basic_peer1.conf \
test_communicator_http3_basic_peer2.conf \
test_communicator_udp_basic_peer2.conf \
test_communicator_tcp_rekey_peer1.conf \
test_communicator_tcp_rekey_peer2.conf \
test_communicator_udp_rekey_peer1.conf \
test_communicator_udp_rekey_peer2.conf \
test_communicator_udp_backchannel_peer1.conf \
test_communicator_udp_backchannel_peer2.conf \
test_communicator_tcp_bidirect_peer1.conf \
test_communicator_tcp_bidirect_peer2.conf
