project('android-xml-converter', 'cpp',
  version: '0.1.0',
  default_options: [
    'cpp_std=c++17',
    'warning_level=2',
    'buildtype=release',
    'optimization=3',
    'b_ndebug=true',
    'strip=true',
    'b_lto=true',
    'b_pie=true',
    'b_staticpic=true',
    'cpp_args=-Wno-unused-function'
  ]
)


pugixml_dep = dependency('pugixml', required: false)

if not pugixml_dep.found()
  message('pugixml not found, building from source')
  
  fs = import('fs')
  if not fs.is_dir('subprojects/pugixml')
    message('Downloading pugixml...')
    pugixml_proj = subproject('pugixml',
      default_options: ['tests=false', 'default_library=static']
    )
    pugixml_dep = pugixml_proj.get_variable('pugixml_dep')
  else
    pugixml_proj = subproject('pugixml',
      default_options: ['tests=false', 'default_library=static']
    )
    pugixml_dep = pugixml_proj.get_variable('pugixml_dep')
  endif
endif

# Library sources - includes both C++ and C wrapper
libabx_sources = files(
  'src/abx.cc',
  'src/abx_c.cc'
)

libabx_headers = files('src/abx.hpp')
libabx_c_headers = files('src/abx.h')

# Build library with both C++ and C API
libabx = static_library('abx',
  libabx_sources,
  dependencies: pugixml_dep,
  include_directories: include_directories('src'),
  install: true
)

libabx_dep = declare_dependency(
  link_with: libabx,
  include_directories: include_directories('src'),
  dependencies: pugixml_dep
)

# Install both C++ and C headers
install_headers(libabx_headers, subdir: 'libabx')
install_headers(libabx_c_headers, subdir: 'libabx')

# CLI tools
abx2xml = executable('abx2xml',
  'src/cli/abx2xml.cc',
  dependencies: libabx_dep,
  install: true
)

xml2abx = executable('xml2abx',
  'src/cli/xml2abx.cc',
  dependencies: libabx_dep,
  install: true
)

pkg = import('pkgconfig')
pkg.generate(libabx,
  description: 'Android Binary XML converter library with C and C++ API',
  name: 'libabx',
  filebase: 'libabx'
)
