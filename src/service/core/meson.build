libgnunetcore_src = ['core_api.c', 'core_api_monitor_peers.c']

gnunetservicecore_src = [
    'gnunet-service-core.c',
    'gnunet-service-core_kx.c',
    'gnunet-service-core_sessions.c',
    'gnunet-service-core_typemap.c',
]

configure_file(
    input: 'core.conf.in',
    output: 'core.conf',
    configuration: cdata,
    install: true,
    install_dir: pkgcfgdir,
)


if get_option('monolith')
    foreach p : libgnunetcore_src + gnunetservicecore_src
        gnunet_src += 'core/' + p
    endforeach
endif

libgnunetcore = library(
    'gnunetcore',
    libgnunetcore_src,
    soversion: solibversions['libgnunetcore']['soversion'],
    version: solibversions['libgnunetcore']['version'],
    install_rpath: rpath_option,
    dependencies: libgnunetutil_dep,
    include_directories: [incdir, configuration_inc],
    install: true,
    install_dir: get_option('libdir'),
)
libgnunetcore_dep = declare_dependency(link_with: libgnunetcore)
pkg.generate(
    libgnunetcore,
    url: 'https://www.gnunet.org',
    description: 'Provides API for (encrypted) P2P communication',
)

if false

    libgnunetcoretesting = library(
        'gnunetcoretesting',
        ['testing_core_cmd_connecting_peers.c'],
        soversion: solibversions['libgnunetcoretesting']['soversion'],
        version: solibversions['libgnunetcoretesting']['version'],
        install_rpath: rpath_option,
        dependencies: [
            libgnunetutil_dep,
            libgnunettestingtransport_dep,
            libgnunettesting_dep,
            libgnunetarm_dep,
            libgnunettransportapplication_dep,
            libgnunettransportcore_dep,
        ],
        include_directories: [incdir, configuration_inc],
        install: true,
        install_dir: get_option('libdir'),
    )
    libgnunetcoretesting_dep = declare_dependency(
        link_with: libgnunetcoretesting,
    )

    shared_module(
        'gnunet_test_core_plugin_cmd_just_run',
        ['test_core_plugin_cmd_just_run.c'],
        install_rpath: rpath_option,
        dependencies: [
            libgnunetutil_dep,
            libgnunettesting_dep,
            libgnunetcoretesting_dep,
            libgnunettestingtransport_dep,
            libgnunettransportcore_dep,
            libgnunettransportapplication_dep,
            libgnunettesting_dep,
            libgnunetpeerstore_dep,
            libgnunetstatistics_dep,
            libgnunethello_dep,
            libgnunetarm_dep,
        ],
        include_directories: [incdir, configuration_inc],
        install: false,
    )

endif

executable(
    'gnunet-service-core',
    gnunetservicecore_src,
    install_rpath: rpath_option,
    dependencies: [
        libgnunetcore_dep,
        libgnunetutil_dep,
        libgnunetstatistics_dep,
        libgnunettransportcore_dep,
        zlib_dep,
    ],
    include_directories: [incdir, configuration_inc],
    install: true,
    install_dir: get_option('libdir') / 'gnunet' / 'libexec',
)

configure_file(
    input: 'test_core_defaults.conf',
    output: 'test_core_defaults.conf',
    copy: true,
)

configure_file(
    input: 'test_core_api_send_to_self.conf',
    output: 'test_core_api_send_to_self.conf',
    copy: true,
)

configure_file(
    input: 'test_core_api_peer1.conf',
    output: 'test_core_api_peer1.conf',
    copy: true,
)

configure_file(
    input: 'test_core_api_peer2.conf',
    output: 'test_core_api_peer2.conf',
    copy: true,
)

configure_file(
    input: 'test_core_api_data.conf',
    output: 'test_core_api_data.conf',
    copy: true,
)


if false

    testcore_api_send_self = executable(
        'test_core_api_send_to_self',
        ['test_core_api_send_to_self.c'],
        dependencies: [
            libgnunetcore_dep,
            libgnunetutil_dep,
            libgnunettesting_dep,
        ],
        include_directories: [incdir, configuration_inc],
        install: false,
    )

    test(
        'test_core_api_send_to_self',
        testcore_api_send_self,
        suite: 'core',
        workdir: meson.current_build_dir(),
    )
endif

testcore_api_start = executable(
    'test_core_api_start_only',
    ['test_core_api_start_only.c'],
    dependencies: [libgnunetcore_dep, libgnunetutil_dep, libgnunettesting_dep],
    include_directories: [incdir, configuration_inc],
    install: false,
)

test(
    'test_core_api_start_only',
    testcore_api_start,
    suite: 'core',
    workdir: meson.current_build_dir(),
)

