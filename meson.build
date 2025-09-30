project('android-xml-converter', 'cpp',
  version: '1.0.0',
  default_options: ['cpp_std=c++17', 'warning_level=2']
)

pugixml_dep = dependency('pugixml', required: false)

if not pugixml_dep.found()
  message('pugixml not found, building from source')
  
  fs = import('fs')
  if not fs.is_dir('subprojects/pugixml')
    message('Downloading pugixml...')
    pugixml_proj = subproject('pugixml', default_options: ['tests=false'])
    pugixml_dep = pugixml_proj.get_variable('pugixml_dep')
  else
    pugixml_proj = subproject('pugixml', default_options: ['tests=false'])
    pugixml_dep = pugixml_proj.get_variable('pugixml_dep')
  endif
endif

libabx_sources = files('src/abx.cc')
libabx_headers = files('src/abx.hh')

libabx = library('abx',
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

install_headers(libabx_headers, subdir: 'libabx')


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
  description: 'Android Binary XML converter library',
  name: 'libabx',
  filebase: 'libabx'
)
