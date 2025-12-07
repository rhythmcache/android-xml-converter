project('android-xml-converter', 'cpp',
    version: '0.2.2',
    default_options: [
        'cpp_std=c++20',
        'warning_level=2',
        'werror=false',
        'default_library=static',
        'buildtype=release',
        'optimization=3',
        'b_ndebug=true',
    ],
    license: 'Apache-2',
    meson_version: '>=0.55.0'
)

pkg_version = meson.project_version()
pkg_name = meson.project_name()

includedir = get_option('includedir')
libdir = get_option('libdir')

is_windows = host_machine.system() == 'windows'
build_shared = get_option('default_library') in ['shared', 'both'] and not is_windows


abx2xml_sources = files('src/cpp/abx2xml/abx2xml.cc')
abx2xml_headers = files('src/cpp/abx2xml/abx2xml.hpp')

libabx2xml = library('abx2xml',
    abx2xml_sources,
    install: true,
    version: pkg_version,
    soversion: '1',
    include_directories: include_directories('src/cpp/abx2xml')
)

install_headers(abx2xml_headers, subdir: 'abx2xml')

if build_shared
    pkg = import('pkgconfig')
    pkg.generate(
        libabx2xml,
        name: 'abx2xml',
        description: 'Android Binary XML to XML converter library',
        version: pkg_version,
        filebase: 'abx2xml',
        subdirs: 'abx2xml'
    )
endif

abx2xml_dep = declare_dependency(
    link_with: libabx2xml,
    include_directories: include_directories('src/cpp/abx2xml')
)

abx2xml_cli = executable('abx2xml',
    'src/cpp/abx2xml/cli_abx2xml.cc',
    dependencies: abx2xml_dep,
    install: true
)


pugixml_dep = dependency('pugixml',
    fallback: ['pugixml', 'pugixml_dep'],
    required: true,
    default_options: ['tests=false']
)

xml2abx_sources = files('src/cpp/xml2abx/xml2abx.cc')
xml2abx_headers = files('src/cpp/xml2abx/xml2abx.hpp')

libxml2abx = library('xml2abx',
    xml2abx_sources,
    dependencies: pugixml_dep,
    install: true,
    version: pkg_version,
    soversion: '1',
    include_directories: include_directories('src/cpp/xml2abx')
)

install_headers(xml2abx_headers, subdir: 'xml2abx')

if build_shared
    pkg.generate(
        libxml2abx,
        name: 'xml2abx',
        description: 'XML to Android Binary XML converter library',
        version: pkg_version,
        filebase: 'xml2abx',
        subdirs: 'xml2abx',
        requires: 'pugixml'
    )
endif

xml2abx_dep = declare_dependency(
    link_with: libxml2abx,
    dependencies: pugixml_dep,
    include_directories: include_directories('src/cpp/xml2abx')
)

xml2abx_cli = executable('xml2abx',
    'src/cpp/xml2abx/cli_xml2abx.cc',
    dependencies: xml2abx_dep,
    install: true
)

summary({
    'prefix': get_option('prefix'),
    'libdir': get_option('libdir'),
    'includedir': get_option('includedir'),
}, section: 'Directories')

summary({
    'abx2xml library': 'standalone (no dependencies)',
    'xml2abx library': 'requires pugixml',
    'Library type': get_option('default_library'),
    'Build shared': build_shared,
}, section: 'Libraries')

summary({
    'abx2xml': true,
    'xml2abx': true,
}, section: 'CLI Tools', bool_yn: true)
