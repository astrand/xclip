project('xclip', 'c',
  version : '0.13',
  default_options : ['warning_level=3', 'c_std=c99', 'b_lto=true'])

add_project_arguments(['-DPACKAGE_NAME="xclip"',
                      '-DPACKAGE_VERSION=' + '"' + meson.project_version() + '"'],
                      language : 'c')


dep = [dependency('x11'), dependency('xmu')]
inc = [include_directories('.')]
cc = meson.get_compiler('c')
iconv_inc = include_directories(get_option('prefix') / get_option('includedir'))
iconv_dep = cc.find_library('iconv',
                            dirs : get_option('prefix') / get_option('libdir'),
                            has_headers : 'iconv.h',
                            header_include_directories : iconv_inc,
                            required : false)
if iconv_dep.found() == true
  dep += iconv_dep
  inc += iconv_inc
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

