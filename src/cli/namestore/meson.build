executable ('gnunet-namestore',
          ['gnunet-namestore.c'],
          dependencies: [libgnunetnamestore_dep,
                         libgnunetutil_dep,
                         libgnunetgnsrecord_dep,
                         libgnunetidentity_dep],
          include_directories: [incdir, configuration_inc],
        install: true,
        install_dir: get_option('bindir'))
executable ('gnunet-namestore-dbtool',
            ['gnunet-namestore-dbtool.c'],
            dependencies: [libgnunetnamestore_dep,
                           libgnunetutil_dep,
                           libgnunetgnsrecord_dep,
                           libgnunetidentity_dep],
            include_directories: [incdir, configuration_inc],
            install: true,
            install_dir: get_option('bindir'))
executable ('gnunet-namestore-zonefile',
            ['gnunet-namestore-zonefile.c'],
            dependencies: [libgnunetnamestore_dep,
                           libgnunetutil_dep,
                           libgnunetgnsrecord_dep,
                           libgnunetidentity_dep],
            include_directories: [incdir, configuration_inc],
            install: true,
            install_dir: get_option('bindir'))
executable ('gnunet-zoneimport',
            ['gnunet-zoneimport.c'],
            dependencies: [libgnunetnamestore_dep,
                           libgnunetutil_dep,
                           libgnunetstatistics_dep,
                           libgnunetgnsrecord_dep,
                           libgnunetidentity_dep],
            include_directories: [incdir, configuration_inc],
            install: true,
            install_dir: get_option('bindir'))

