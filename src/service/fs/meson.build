libgnunetfs_src = [
    'fs_api.c',
    'fs_directory.c',
    'fs_dirmetascan.c',
    'fs_download.c',
    'fs_file_information.c',
    'fs_getopt.c',
    'fs_list_indexed.c',
    'fs_publish.c',
    'fs_publish_ksk.c',
    'fs_publish_ublock.c',
    'fs_misc.c',
    'fs_namespace.c',
    'fs_search.c',
    'fs_sharetree.c',
    'fs_tree.c',
    'fs_unindex.c',
    'fs_uri.c',
    'meta_data.c',
]

gnunetservicefs_src = [
    'gnunet-service-fs.c',
    'gnunet-service-fs_cp.c',
    'gnunet-service-fs_indexing.c',
    'gnunet-service-fs_pe.c',
    'gnunet-service-fs_pr.c',
    'gnunet-service-fs_push.c',
    'gnunet-service-fs_put.c',
    'gnunet-service-fs_cadet_client.c',
    'gnunet-service-fs_cadet_server.c',
]

configure_file(
    input: 'fs.conf.in',
    output: 'fs.conf',
    configuration: cdata,
    install: true,
    install_dir: pkgcfgdir,
)


if get_option('monolith')
    foreach p : libgnunetfs_src + gnunetservicefs_src
        gnunet_src += 'fs/' + p
    endforeach
endif

libgnunetfs = library(
    'gnunetfs',
    libgnunetfs_src,
    soversion: solibversions['libgnunetfs']['soversion'],
    version: solibversions['libgnunetfs']['version'],
    install_rpath: rpath_option,
    dependencies: [
        libgnunetutil_dep,
        extractor_dep,
        libgnunetdatastore_dep,
        libgnunetstatistics_dep,
        unistr_dep,
    ],
    include_directories: [incdir, configuration_inc],
    install: true,
    install_dir: get_option('libdir'),
)
libgnunetfs_dep = declare_dependency(link_with: libgnunetfs)
pkg.generate(
    libgnunetfs,
    url: 'https://www.gnunet.org',
    description: 'Provides API for GNUnet File-Sharing service',
)

executable(
    'gnunet-service-fs',
    gnunetservicefs_src,
    install_rpath: rpath_option,
    dependencies: [
        libgnunetfs_dep,
        libgnunetutil_dep,
        libgnunetstatistics_dep,
        libgnunetcore_dep,
        libgnunetdht_dep,
        libgnunetidentity_dep,
        m_dep,
        libgnunetcadet_dep,
        libgnunetpeerstore_dep,
        libgnunetdatastore_dep,
        libgnunetblock_dep,
    ],
    include_directories: [incdir, configuration_inc],
    install: true,
    install_dir: get_option('libdir') / 'gnunet' / 'libexec',
)
executable(
    'gnunet-helper-fs-publish',
    ['gnunet-helper-fs-publish.c'],
    install_rpath: rpath_option,
    dependencies: [
        libgnunetfs_dep,
        extractor_dep,
        libgnunetutil_dep,
        libgnunetblock_dep,
    ],
    include_directories: [incdir, configuration_inc],
    install: true,
    install_dir: get_option('libdir') / 'gnunet' / 'libexec',
)

