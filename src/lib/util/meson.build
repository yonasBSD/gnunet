libgnunetutil_src = [
    'bandwidth.c',
    # $(BENCHMARK)',
    'bio.c',
    'buffer.c',
    'child_management.c',
    'client.c',
    'common_allocation.c',
    'common_endian.c',
    'common_logging.c',
    'compress.c',
    'configuration.c',
    'consttime_memcmp.c',
    'container_bloomfilter.c',
    'container_heap.c',
    'container_multihashmap.c',
    'container_multishortmap.c',
    'container_multiuuidmap.c',
    'container_multipeermap.c',
    'container_multihashmap32.c',
    'crypto_blind_sign.c',
    'crypto_symmetric.c',
    'crypto_crc.c',
    'crypto_cs.c',
    'crypto_ecc.c',
    'crypto_ecc_gnsrecord.c',
    'crypto_ecc_dlog.c',
    'crypto_ecc_setup.c',
    'crypto_edx25519.c',
    'crypto_elligator.c',
    'crypto_hash.c',
    'crypto_hash_file.c',
    'crypto_hkdf.c',
    'crypto_hpke.c',
    'crypto_kdf.c',
    'crypto_mpi.c',
    'crypto_paillier.c',
    'crypto_pkey.c',
    'crypto_pow.c',
    'crypto_random.c',
    'crypto_rsa.c',
    'disk.c',
    'disk.h',
    'dnsparser.c',
    'dnsstub.c',
    'getopt.c',
    'getopt_helpers.c',
    'helper.c',
    'load.c',
    'mst.c',
    'mq.c',
    'nat.c',
    'nc.c',
    'network.c',
    'nt.c',
    'os_installation.c',
    'os_network.c',
    'os_priority.c',
    'peer.c',
    'plugin.c',
    'program.c',
    'resolver_api.c',
    'resolver.h',
    'scheduler.c',
    'service.c',
    'signal.c',
    'strings.c',
    'time.c',
    'tun.c',
    'uri.c',
    'speedup.c',
    'speedup.h',
    'proc_compat.c',
    'gnunet_error_codes.c',
]

configure_file(
    input: 'util.conf',
    output: 'util.conf',
    configuration: cdata,
    install: true,
    install_dir: pkgcfgdir,
)

libgnunetutil = library(
    'gnunetutil',
    libgnunetutil_src,
    soversion: solibversions['libgnunetutil']['soversion'],
    version: solibversions['libgnunetutil']['version'],
    install_rpath: rpath_option,
    dependencies: [
        lgmp_dep,
        mhd_dep,
        sodium_dep,
        gcrypt_dep,
        curl_dep,
        json_dep,
        zlib_dep,
        sqlite_dep,
        unistr_dep,
        ltdl_dep,
        idn_dep,
        atomic_dep,
    ],
    include_directories: [incdir, configuration_inc],
    install: true,
    install_dir: get_option('libdir'),
)
libgnunetutil_dep = declare_dependency(link_with: libgnunetutil)
pkg.generate(
    libgnunetutil,
    url: 'https://www.gnunet.org',
    description: 'Provides miscellaneous utility functions and API for GNUnet',
)


executable(
    'gnunet-elligator-tvg',
    ['gnunet-elligator-tvg.c'],
    dependencies: [libgnunetutil_dep],
    include_directories: [incdir, configuration_inc],
    install: false,
)


logging_dummy = executable(
    'test_common_logging_dummy',
    ['test_common_logging_dummy.c'],
    dependencies: [libgnunetutil_dep],
    include_directories: [incdir, configuration_inc],
    install: false,
)
configure_file(
    input: 'test_client_data.conf',
    output: 'test_client_data.conf',
    copy: true,
)
configure_file(
    input: 'test_speedup_data.conf',
    output: 'test_speedup_data.conf',
    copy: true,
)
configure_file(
    input: 'test_program_data.conf',
    output: 'test_program_data.conf',
    copy: true,
)
configure_file(
    input: 'test_service_data.conf',
    output: 'test_service_data.conf',
    copy: true,
)
configure_file(
    input: 'test_configuration_data.conf',
    output: 'test_configuration_data.conf',
    copy: true,
)
configure_file(
    input: 'child_management_test.sh',
    output: 'child_management_test.sh',
    copy: true,
)

test_bio = executable(
    'test_bio',
    ['test_bio.c'],
    dependencies: [libgnunetutil_dep],
    include_directories: [incdir, configuration_inc],
    install: false,
)
test(
    'test_bio',
    test_bio,
    workdir: meson.current_build_dir(),
    suite: ['util', 'util-bio'],
)
test_child_management = executable(
    'test_child_management',
    ['test_child_management.c'],
    dependencies: [libgnunetutil_dep],
    include_directories: [incdir, configuration_inc],
    install: false,
)
test(
    'test_child_management',
    test_child_management,
    workdir: meson.current_build_dir(),
    suite: ['util', 'util-child'],
)

# test_client.nc

testcommonalloc = executable(
    'test_common_allocation',
    ['test_common_allocation.c'],
    dependencies: [libgnunetutil_dep],
    build_by_default: false,
    include_directories: [incdir, configuration_inc],
    install: false,
)
test(
    'test_common_allocation',
    testcommonalloc,
    workdir: meson.current_build_dir(),
    suite: ['util', 'util-common'],
)

testcommonlog = executable(
    'test_common_logging',
    ['test_common_logging.c'],
    dependencies: [libgnunetutil_dep],
    build_by_default: false,
    include_directories: [incdir, configuration_inc],
    install: false,
)
test(
    'test_common_logging',
    testcommonlog,
    workdir: meson.current_build_dir(),
    suite: ['util', 'util-common'],
)
testcommonendian = executable(
    'test_common_endian',
    ['test_common_endian.c'],
    dependencies: [libgnunetutil_dep],
    build_by_default: false,
    include_directories: [incdir, configuration_inc],
    install: false,
)
test(
    'test_common_endian',
    testcommonendian,
    workdir: meson.current_build_dir(),
    suite: ['util', 'util-common'],
)
testconf = executable(
    'test_configuration',
    ['test_configuration.c'],
    dependencies: [libgnunetutil_dep],
    build_by_default: false,
    include_directories: [incdir, configuration_inc],
    install: false,
)
test(
    'test_configuration',
    testconf,
    workdir: meson.current_build_dir(),
    suite: ['util', 'util-configuration'],
)
testcontainerbloom = executable(
    'test_container_bloomfilter',
    ['test_container_bloomfilter.c'],
    dependencies: [libgnunetutil_dep],
    include_directories: [incdir, configuration_inc],
    build_by_default: false,
    install: false,
)
test(
    'test_container_bloomfilter',
    testcontainerbloom,
    workdir: meson.current_build_dir(),
    suite: ['util', 'util-container'],
)

testcontainerdll = executable(
    'test_container_dll',
    ['test_container_dll.c'],
    dependencies: [libgnunetutil_dep],
    include_directories: [incdir, configuration_inc],
    build_by_default: false,
    install: false,
)
test(
    'test_container_dll',
    testcontainerdll,
    workdir: meson.current_build_dir(),
    suite: ['util', 'util-container'],
)

testcontainermhm = executable(
    'test_container_multihashmap',
    ['test_container_multihashmap.c'],
    dependencies: [libgnunetutil_dep],
    include_directories: [incdir, configuration_inc],
    build_by_default: false,
    install: false,
)
test(
    'test_container_multihashmap',
    testcontainermhm,
    workdir: meson.current_build_dir(),
    suite: ['util', 'util-container'],
)

testcontainermhm32 = executable(
    'test_container_multihashmap32',
    ['test_container_multihashmap32.c'],
    dependencies: [libgnunetutil_dep],
    include_directories: [incdir, configuration_inc],
    build_by_default: false,
    install: false,
)
test(
    'test_container_multihashmap32',
    testcontainermhm32,
    workdir: meson.current_build_dir(),
    suite: ['util', 'util-container'],
)

testcontainermpm = executable(
    'test_container_multipeermap',
    ['test_container_multipeermap.c'],
    dependencies: [libgnunetutil_dep],
    include_directories: [incdir, configuration_inc],
    build_by_default: false,
    install: false,
)
test(
    'test_container_multipeermap',
    testcontainermpm,
    workdir: meson.current_build_dir(),
    suite: ['util', 'util-container'],
)

testcontainerheap = executable(
    'test_container_heap',
    ['test_container_heap.c'],
    dependencies: [libgnunetutil_dep],
    include_directories: [incdir, configuration_inc],
    build_by_default: false,
    install: false,
)
test(
    'test_container_heap',
    testcontainerheap,
    workdir: meson.current_build_dir(),
    suite: ['util', 'util-container'],
)

testcrypto_symmetric = executable(
    'test_crypto_symmetric',
    ['test_crypto_symmetric.c'],
    dependencies: [libgnunetutil_dep],
    include_directories: [incdir, configuration_inc],
    build_by_default: false,
    install: false,
)
test(
    'test_crypto_symmetric',
    testcrypto_symmetric,
    workdir: meson.current_build_dir(),
    suite: ['util', 'util-crypto'],
)

testcrypto_crc = executable(
    'test_crypto_crc',
    ['test_crypto_crc.c'],
    dependencies: [libgnunetutil_dep],
    include_directories: [incdir, configuration_inc],
    build_by_default: false,
    install: false,
)
test(
    'test_crypto_crc',
    testcrypto_crc,
    workdir: meson.current_build_dir(),
    suite: ['util', 'util-crypto'],
)

testcrypto_cs = executable(
    'test_crypto_cs',
    ['test_crypto_cs.c'],
    dependencies: [sodium_dep, libgnunetutil_dep],
    include_directories: [incdir, configuration_inc],
    build_by_default: false,
    install: false,
)
test(
    'test_crypto_cs',
    testcrypto_cs,
    workdir: meson.current_build_dir(),
    suite: ['util', 'util-crypto'],
)

testcrypto_ecdsa = executable(
    'test_crypto_ecdsa',
    ['test_crypto_ecdsa.c'],
    dependencies: [gcrypt_dep, libgnunetutil_dep],
    include_directories: [incdir, configuration_inc],
    build_by_default: false,
    install: false,
)
test(
    'test_crypto_ecdsa',
    testcrypto_ecdsa,
    workdir: meson.current_build_dir(),
    suite: ['util', 'util-crypto'],
)

testcrypto_eddsa = executable(
    'test_crypto_eddsa',
    ['test_crypto_eddsa.c'],
    dependencies: [gcrypt_dep, libgnunetutil_dep],
    include_directories: [incdir, configuration_inc],
    build_by_default: false,
    install: false,
)
test(
    'test_crypto_eddsa',
    testcrypto_eddsa,
    workdir: meson.current_build_dir(),
    suite: ['util', 'util-crypto'],
)

testcrypto_ecdh_eddsa = executable(
    'test_crypto_ecdh_eddsa',
    ['test_crypto_ecdh_eddsa.c'],
    dependencies: [gcrypt_dep, libgnunetutil_dep],
    include_directories: [incdir, configuration_inc],
    build_by_default: false,
    install: false,
)
test(
    'test_crypto_ecdh_eddsa',
    testcrypto_ecdh_eddsa,
    workdir: meson.current_build_dir(),
    suite: ['util', 'util-crypto'],
)

testcrypto_ecdh_ecdsa = executable(
    'test_crypto_ecdh_ecdsa',
    ['test_crypto_ecdh_ecdsa.c'],
    dependencies: [gcrypt_dep, libgnunetutil_dep],
    include_directories: [incdir, configuration_inc],
    build_by_default: false,
    install: false,
)
test(
    'test_crypto_ecdh_ecdsa',
    testcrypto_ecdh_ecdsa,
    workdir: meson.current_build_dir(),
    suite: ['util', 'util-crypto'],
)

testcrypto_hpke = executable(
    'test_crypto_hpke',
    ['test_crypto_hpke.c'],
    dependencies: [libgnunetutil_dep],
    include_directories: [incdir, configuration_inc],
    build_by_default: false,
    install: false,
)
test(
    'test_crypto_hpke',
    testcrypto_hpke,
    workdir: meson.current_build_dir(),
    suite: ['util', 'util-crypto'],
)


testcrypto_edx25519 = executable(
    'test_crypto_edx25519',
    ['test_crypto_edx25519.c'],
    dependencies: [gcrypt_dep, libgnunetutil_dep],
    include_directories: [incdir, configuration_inc],
    build_by_default: false,
    install: false,
)
test(
    'test_crypto_edx25519',
    testcrypto_edx25519,
    workdir: meson.current_build_dir(),
    suite: ['util', 'util-crypto'],
)

testcrypto_ecc_dlog = executable(
    'test_crypto_ecc_dlog',
    ['test_crypto_ecc_dlog.c'],
    dependencies: [gcrypt_dep, sodium_dep, libgnunetutil_dep],
    include_directories: [incdir, configuration_inc],
    build_by_default: false,
    install: false,
)
test(
    'test_crypto_ecc_dlog',
    testcrypto_ecc_dlog,
    workdir: meson.current_build_dir(),
    suite: ['util', 'util-crypto'],
)

testcrypto_elligator = executable(
    'test_crypto_elligator',
    ['test_crypto_elligator.c'],
    dependencies: [gcrypt_dep, libgnunetutil_dep, lgmp_dep, sodium_dep],
    include_directories: [incdir, configuration_inc],
    build_by_default: false,
    install: false,
)
test(
    'test_crypto_elligator',
    testcrypto_elligator,
    workdir: meson.current_build_dir(),
    suite: ['util', 'util-crypto'],
)

testcrypto_hash = executable(
    'test_crypto_hash',
    ['test_crypto_hash.c'],
    dependencies: [gcrypt_dep, libgnunetutil_dep],
    include_directories: [incdir, configuration_inc],
    build_by_default: false,
    install: false,
)
test(
    'test_crypto_hash',
    testcrypto_hash,
    workdir: meson.current_build_dir(),
    suite: ['util', 'util-crypto'],
)

testcrypto_hash_context = executable(
    'test_crypto_hash_context',
    ['test_crypto_hash_context.c'],
    dependencies: [gcrypt_dep, libgnunetutil_dep],
    include_directories: [incdir, configuration_inc],
    build_by_default: false,
    install: false,
)
test(
    'test_crypto_hash_context',
    testcrypto_hash_context,
    workdir: meson.current_build_dir(),
    suite: ['util', 'util-crypto'],
)

testcrypto_hkdf = executable(
    'test_crypto_hkdf',
    ['test_crypto_hkdf.c'],
    dependencies: [gcrypt_dep, libgnunetutil_dep],
    include_directories: [incdir, configuration_inc],
    build_by_default: false,
    install: false,
)
test(
    'test_crypto_hkdf',
    testcrypto_hkdf,
    workdir: meson.current_build_dir(),
    suite: ['util', 'util-crypto'],
)

testcrypto_kdf = executable(
    'test_crypto_kdf',
    ['test_crypto_kdf.c'],
    dependencies: [gcrypt_dep, libgnunetutil_dep],
    include_directories: [incdir, configuration_inc],
    build_by_default: false,
    install: false,
)
test(
    'test_crypto_kdf',
    testcrypto_kdf,
    workdir: meson.current_build_dir(),
    suite: ['util', 'util-crypto'],
)

testcrypto_paillier = executable(
    'test_crypto_paillier',
    ['test_crypto_paillier.c'],
    dependencies: [gcrypt_dep, libgnunetutil_dep],
    include_directories: [incdir, configuration_inc],
    build_by_default: false,
    install: false,
)
test(
    'test_crypto_paillier',
    testcrypto_paillier,
    workdir: meson.current_build_dir(),
    suite: ['util', 'util-crypto'],
)

testcrypto_random = executable(
    'test_crypto_random',
    ['test_crypto_random.c'],
    dependencies: [gcrypt_dep, libgnunetutil_dep],
    include_directories: [incdir, configuration_inc],
    build_by_default: false,
    install: false,
)
test(
    'test_crypto_random',
    testcrypto_random,
    workdir: meson.current_build_dir(),
    suite: ['util', 'util-crypto'],
)

testcrypto_rsa = executable(
    'test_crypto_rsa',
    ['test_crypto_rsa.c'],
    dependencies: [gcrypt_dep, libgnunetutil_dep],
    include_directories: [incdir, configuration_inc],
    build_by_default: false,
    install: false,
)
test(
    'test_crypto_rsa',
    testcrypto_rsa,
    workdir: meson.current_build_dir(),
    suite: ['util', 'util-crypto'],
)

testdisk = executable(
    'test_disk',
    ['test_disk.c'],
    dependencies: [libgnunetutil_dep],
    include_directories: [incdir, configuration_inc],
    build_by_default: false,
    install: false,
)
test(
    'test_disk',
    testdisk,
    workdir: meson.current_build_dir(),
    suite: ['util', 'util-misc'],
)

testgetopt = executable(
    'test_getopt',
    ['test_getopt.c'],
    dependencies: [libgnunetutil_dep],
    include_directories: [incdir, configuration_inc],
    build_by_default: false,
    install: false,
)
test(
    'test_getopt',
    testgetopt,
    workdir: meson.current_build_dir(),
    suite: ['util', 'util-misc'],
)

testhexcoder = executable(
    'test_hexcoder',
    ['test_hexcoder.c'],
    dependencies: [libgnunetutil_dep],
    include_directories: [incdir, configuration_inc],
    build_by_default: false,
    install: false,
)
test(
    'test_hexcoder',
    testhexcoder,
    workdir: meson.current_build_dir(),
    suite: ['util', 'util-misc'],
)

testmq = executable(
    'test_mq',
    ['test_mq.c'],
    dependencies: [libgnunetutil_dep],
    include_directories: [incdir, configuration_inc],
    build_by_default: false,
    install: false,
)
test(
    'test_mq',
    testmq,
    workdir: meson.current_build_dir(),
    suite: ['util', 'util-misc'],
)

testos_network = executable(
    'test_os_network',
    ['test_os_network.c'],
    dependencies: [libgnunetutil_dep],
    include_directories: [incdir, configuration_inc],
    build_by_default: false,
    install: false,
)
test(
    'test_os_network',
    testos_network,
    workdir: meson.current_build_dir(),
    suite: ['util', 'util-os'],
)

testpeer = executable(
    'test_peer',
    ['test_peer.c'],
    dependencies: [gcrypt_dep, libgnunetutil_dep],
    include_directories: [incdir, configuration_inc],
    build_by_default: false,
    install: false,
)
test(
    'test_peer',
    testpeer,
    workdir: meson.current_build_dir(),
    suite: ['util', 'util-misc'],
)

testplugin_plug = shared_module(
    'gnunet_plugin_utiltest',
    ['test_plugin_plug.c'],
    install_rpath: rpath_option,
    dependencies: [libgnunetutil_dep],
    include_directories: [incdir, configuration_inc],
    build_by_default: false,
    install: false,
)
testplugin = executable(
    'test_plugin',
    ['test_plugin.c'],
    dependencies: [libgnunetutil_dep],
    include_directories: [incdir, configuration_inc],
    build_by_default: false,
    install: false,
)
test(
    'test_plugin',
    testplugin,
    depends: [testplugin_plug],
    workdir: meson.current_build_dir(),
    suite: ['util', 'util-misc'],
)

testprogram = executable(
    'test_program',
    ['test_program.c'],
    dependencies: [libgnunetutil_dep],
    include_directories: [incdir, configuration_inc],
    build_by_default: false,
    install: false,
)
test(
    'test_program',
    testprogram,
    workdir: meson.current_build_dir(),
    suite: ['util', 'util-misc'],
)

# test_resolver_api.nc

testscheduler = executable(
    'test_scheduler',
    ['test_scheduler.c'],
    dependencies: [libgnunetutil_dep],
    include_directories: [incdir, configuration_inc],
    build_by_default: false,
    install: false,
)
test(
    'test_scheduler',
    testscheduler,
    workdir: meson.current_build_dir(),
    suite: ['util', 'util-scheduler'],
)

testscheduler_delay = executable(
    'test_scheduler_delay',
    ['test_scheduler_delay.c'],
    dependencies: [libgnunetutil_dep],
    include_directories: [incdir, configuration_inc],
    build_by_default: false,
    install: false,
)
test(
    'test_scheduler_delay',
    testscheduler_delay,
    workdir: meson.current_build_dir(),
    suite: ['util', 'util-scheduler'],
)

testscheduler_hogging_cancel = executable(
    'test_scheduler_hogging_cancel',
    ['test_scheduler_hogging_cancel.c'],
    dependencies: [libgnunetutil_dep],
    include_directories: [incdir, configuration_inc],
    build_by_default: false,
    install: false,
)
test(
    'test_scheduler_hogging_cancel',
    testscheduler_hogging_cancel,
    workdir: meson.current_build_dir(),
    suite: ['util', 'util-scheduler'],
)

testscheduler_hogging_prio = executable(
    'test_scheduler_hogging_priority',
    ['test_scheduler_hogging_priority.c'],
    dependencies: [libgnunetutil_dep],
    include_directories: [incdir, configuration_inc],
    build_by_default: false,
    install: false,
)
test(
    'test_scheduler_hogging_priority',
    testscheduler_hogging_prio,
    workdir: meson.current_build_dir(),
    suite: ['util', 'util-scheduler'],
)

testservice = executable(
    'test_service',
    ['test_service.c'],
    dependencies: [libgnunetutil_dep],
    include_directories: [incdir, configuration_inc],
    build_by_default: false,
    install: false,
)
test(
    'test_service',
    testservice,
    workdir: meson.current_build_dir(),
    suite: ['util', 'util-misc'],
)

teststrings = executable(
    'test_strings',
    ['test_strings.c'],
    dependencies: [libgnunetutil_dep],
    include_directories: [incdir, configuration_inc],
    build_by_default: false,
    install: false,
)
test(
    'test_strings',
    teststrings,
    workdir: meson.current_build_dir(),
    suite: ['util', 'util-strings'],
)

teststrings_todata = executable(
    'test_strings_to_data',
    ['test_strings_to_data.c'],
    dependencies: [libgnunetutil_dep],
    include_directories: [incdir, configuration_inc],
    build_by_default: false,
    install: false,
)
test(
    'test_strings_to_data',
    teststrings_todata,
    workdir: meson.current_build_dir(),
    suite: ['util', 'util-strings'],
)

testspeedup = executable(
    'test_speedup',
    ['test_speedup.c'],
    dependencies: [libgnunetutil_dep],
    include_directories: [incdir, configuration_inc],
    build_by_default: false,
    install: false,
)
test(
    'test_speedup',
    testspeedup,
    workdir: meson.current_build_dir(),
    suite: ['util', 'util-misc'],
)

testtime = executable(
    'test_time',
    ['test_time.c'],
    dependencies: [libgnunetutil_dep],
    include_directories: [incdir, configuration_inc],
    build_by_default: false,
    install: false,
)
test(
    'test_time',
    testtime,
    workdir: meson.current_build_dir(),
    suite: ['util', 'util-misc'],
)

testtun = executable(
    'test_tun',
    ['test_tun.c'],
    dependencies: [libgnunetutil_dep],
    include_directories: [incdir, configuration_inc],
    build_by_default: false,
    install: false,
)
test(
    'test_tun',
    testtun,
    workdir: meson.current_build_dir(),
    suite: ['util', 'util-misc'],
)

testuri = executable(
    'test_uri',
    ['test_uri.c'],
    dependencies: [libgnunetutil_dep],
    include_directories: [incdir, configuration_inc],
    build_by_default: false,
    install: false,
)
test(
    'test_uri',
    testuri,
    workdir: meson.current_build_dir(),
    suite: ['util', 'util-misc'],
)

testos_start_process = executable(
    'test_os_start_process',
    ['test_os_start_process.c'],
    dependencies: [libgnunetutil_dep],
    include_directories: [incdir, configuration_inc],
    build_by_default: false,
    install: false,
)
test(
    'test_os_start_process',
    testos_start_process,
    workdir: meson.current_build_dir(),
    suite: ['util', 'util-os'],
)

testcommon_logging_runtime = executable(
    'test_common_logging_runtime_loglevels',
    ['test_common_logging_runtime_loglevels.c'],
    dependencies: [libgnunetutil_dep],
    include_directories: [incdir, configuration_inc],
    build_by_default: false,
    install: false,
)
test(
    'test_common_logging_runtime_loglevels',
    testcommon_logging_runtime,
    depends: [logging_dummy],
    workdir: meson.current_build_dir(),
    suite: ['util', 'util-common'],
)

testutil_perf = [
    'perf_crypto_asymmetric',
    # 'perf_crypto_cs', FIXME FTBFS
    'perf_crypto_ecc_dlog',
    'perf_crypto_hash',
    'perf_crypto_paillier',
    'perf_crypto_rsa',
    'perf_crypto_symmetric',
    'perf_malloc',
    'perf_mq',
    'perf_scheduler',
]

foreach t : testutil_perf

    test_filename = t + '.sh'
    testbin = executable(
        t,
        [t + '.c'],
        dependencies: [libgnunetutil_dep, gcrypt_dep, sodium_dep],
        include_directories: [incdir, configuration_inc],
        build_by_default: false,
        install: false,
    )
    test(t, testbin, suite: ['perf'], workdir: meson.current_build_dir())
endforeach
