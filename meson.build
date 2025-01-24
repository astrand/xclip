project('xclip', 'c',
  version : '0.13',
  default_options : ['warning_level=3', 'c_std=c99', 'b_lto=true'])

add_project_arguments(['-DPACKAGE_NAME="xclip"',
                      '-DPACKAGE_VERSION=' + '"' + meson.project_version() + '"'],
                      language : 'c')


dep = [dependency('x11'), dependency('xmu')]
inc = [include_directories('.')]

cc = meson.get_compiler('c')
if cc.has_function('iconv_open') == true
  # #define LIBICONV_PLUG prevents a header conflict when the system
  # provides the iconv library and its iconv.h header, and libiconv is installed
  # at the same time.
  add_project_arguments('-DLIBICONV_PLUG', language : 'c')
  iconv_available = true
else
  iconv_inc = include_directories(get_option('prefix') / get_option('includedir'))
  iconv_dep = cc.find_library('iconv',
                              dirs : get_option('prefix') / get_option('libdir'),
                              has_headers : 'iconv.h',
                              header_include_directories : iconv_inc,
                              required : false)
  iconv_available = iconv_dep.found()
  if iconv_available == true
    inc += iconv_inc
    dep += iconv_dep
  endif
endif
if iconv_available == true
  add_project_arguments('-DHAVE_ICONV', language : 'c')
endif

executable('xclip',
           ['xclib.c', 'xclip.c', 'xcprint.c'],
           include_directories : inc,
           dependencies : dep,
           install : true)

borked_test = executable('borked',
                         ['borked.c', 'xclib.c', 'xcprint.c'],
                         include_directories : inc,
                         dependencies : dep)
test('borked', borked_test)

install_data(['xclip-copyfile', 'xclip-cutfile', 'xclip-pastefile'],
             install_dir : get_option('prefix') / get_option('bindir'))
install_man(['xclip-copyfile.1', 'xclip.1'])
