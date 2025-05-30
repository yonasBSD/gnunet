libgnunetregex_src = ['regex_api_announce.c', 'regex.c', 'regex_api_search.c']
gnunetserviceregex_src = [
    'gnunet-service-regex.c',
    'regex_internal_dht.c',
    'regex_internal.c',
]

configure_file(
    input: 'regex.conf.in',
    output: 'regex.conf',
    configuration: cdata,
    install: true,
    install_dir: pkgcfgdir,
)


if get_option('monolith')
    foreach p : libgnunetregex_src + gnunetserviceregex_src
        gnunet_src += 'regex/' + p
    endforeach
endif

libgnunetregex = library(
    'gnunetregex',
    libgnunetregex_src,
    soversion: solibversions['libgnunetregex']['soversion'],
    version: solibversions['libgnunetregex']['version'],
    install_rpath: rpath_option,
    dependencies: libgnunetutil_dep,
    include_directories: [incdir, configuration_inc],
    install: true,
    install_dir: get_option('libdir'),
)
pkg.generate(
    libgnunetregex,
    url: 'https://www.gnunet.org',
    description: 'Provides API for accessing the regex service',
)
libgnunetregex_dep = declare_dependency(link_with: libgnunetregex)

executable(
    'gnunet-service-regex',
    gnunetserviceregex_src,
    install_rpath: rpath_option,
    dependencies: [
        libgnunetregex_dep,
        libgnunetutil_dep,
        libgnunetdht_dep,
        libgnunetstatistics_dep,
        libgnunetregex_dep,
        libgnunetregexblock_dep,
    ],
    include_directories: [incdir, configuration_inc],
    install: true,
    install_dir: get_option('libdir') / 'gnunet' / 'libexec',
)
executable(
    'gnunet-daemon-regexprofiler',
    [
        'gnunet-daemon-regexprofiler.c',
        'regex_internal.c',
        'regex_internal_dht.c',
        'regex_test_lib.c',
        'regex_test_graph.c',
        'regex_test_random.c',
    ],
    install_rpath: rpath_option,
    dependencies: [
        libgnunetregex_dep,
        libgnunetutil_dep,
        libgnunetdht_dep,
        libgnunetstatistics_dep,
        libgnunetregex_dep,
        libgnunetregexblock_dep,
    ],
    include_directories: [incdir, configuration_inc],
    install: true,
    install_dir: get_option('libdir') / 'gnunet' / 'libexec',
)

testregex = executable(
    'test_regex',
    ['test_regex.c'],
    install_rpath: rpath_option,
    dependencies: [libgnunetregex_dep, libgnunetutil_dep],
    include_directories: [incdir, configuration_inc],
    build_by_default: false,
    install: false,
)
test(
    'test_regex',
    testregex,
    workdir: meson.current_build_dir(),
    suite: ['regex'],
)

