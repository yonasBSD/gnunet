libgnunetcurl_src = ['curl.c', 'curl_reschedule.c']

libgnunetcurl = library(
    'gnunetcurl',
    libgnunetcurl_src,
    soversion: solibversions['libgnunetcurl']['soversion'],
    version: solibversions['libgnunetcurl']['version'],
    install_rpath: rpath_option,
    dependencies: [libgnunetutil_dep, curl_dep, json_dep],
    include_directories: [incdir, configuration_inc],
    install: true,
    install_dir: get_option('libdir'),
)
libgnunetcurl_dep = declare_dependency(link_with: libgnunetcurl)

