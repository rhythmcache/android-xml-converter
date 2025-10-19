project('abx-tools', 'cpp',
  version: '0.2.1',
  default_options: [
    'cpp_std=c++20',
    'warning_level=2',
    'werror=false',
    'default_library=static',
    'buildtype=release',
    'optimization=3',
    'b_ndebug=true',
  ],
  license: 'MIT',
  meson_version: '>=0.55.0'
)

pkg_version = meson.project_version()
pkg_name = meson.project_name()

includedir = get_option('includedir')
libdir = get_option('libdir')

is_windows = host_machine.system() == 'windows'

abx2xml_sources = files('src/abx2xml/abx2xml.cc')
abx2xml_headers = files('src/abx2xml/abx2xml.hpp')


if not is_windows
  libabx2xml_shared = shared_library('abx2xml',
    abx2xml_sources,
    install: true,
    version: pkg_version,
    soversion: '1',
    include_directories: include_directories('src/abx2xml')
  )
endif


libabx2xml_static = static_library('abx2xml',
  abx2xml_sources,
  install: true,
  include_directories: include_directories('src/abx2xml')
)

install_headers(abx2xml_headers, subdir: 'abx2xml')


if not is_windows
  pkg = import('pkgconfig')
  pkg.generate(
    libabx2xml_shared,
    name: 'abx2xml',
    description: 'Android Binary XML to XML converter library',
    version: pkg_version,
    filebase: 'abx2xml',
    subdirs: 'abx2xml'
  )
endif


abx2xml_dep = declare_dependency(
  link_with: libabx2xml_static,
  include_directories: include_directories('src/abx2xml')
)

abx2xml_cli = executable('abx2xml',
  'src/abx2xml/cli_abx2xml.cc',
  dependencies: abx2xml_dep,
  install: true
)

pugixml_dep = dependency('pugixml', 
  fallback: ['pugixml', 'pugixml_dep'],
  required: true,
  default_options: ['tests=false']
)

xml2abx_sources = files('src/xml2abx/xml2abx.cc')
xml2abx_headers = files('src/xml2abx/xml2abx.hpp')


if not is_windows
  libxml2abx_shared = shared_library('xml2abx',
    xml2abx_sources,
    dependencies: pugixml_dep,
    install: true,
    version: pkg_version,
    soversion: '1',
    include_directories: include_directories('src/xml2abx')
  )
endif


libxml2abx_static = static_library('xml2abx',
  xml2abx_sources,
  dependencies: pugixml_dep,
  install: true,
  include_directories: include_directories('src/xml2abx')
)

install_headers(xml2abx_headers, subdir: 'xml2abx')


if not is_windows
  pkg.generate(
    libxml2abx_shared,
    name: 'xml2abx',
    description: 'XML to Android Binary XML converter library',
    version: pkg_version,
    filebase: 'xml2abx',
    subdirs: 'xml2abx',
    requires: 'pugixml'
  )
endif


xml2abx_dep = declare_dependency(
  link_with: libxml2abx_static,
  dependencies: pugixml_dep,
  include_directories: include_directories('src/xml2abx')
)

xml2abx_cli = executable('xml2abx',
  'src/xml2abx/cli_xml2abx.cc',
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
  'Build type': is_windows ? 'static only' : 'static + shared',
}, section: 'Libraries')

summary({
  'abx2xml': true,
  'xml2abx': true,
}, section: 'CLI Tools', bool_yn: true)