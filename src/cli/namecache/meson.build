executable(
    'gnunet-namecache',
    gnunetnamecache_src,
    install_rpath: rpath_option,
    dependencies: [
        libgnunetnamecache_dep,
        libgnunetutil_dep,
        libgnunetgnsrecord_dep,
        libgnunetidentity_dep,
    ],
    include_directories: [incdir, configuration_inc],
    install: true,
    install_dir: get_option('bindir'),
)

