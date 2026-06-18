name = 'swaytitle'

sources = files(
  'swaytitle.cpp',
)

metadata = files(
  name + '.xml'
)

shared_module(name, sources,
        dependencies: deps,
        install: true,
        install_dir: get_option('libdir') / 'wf-panel-pi',
        cpp_args : '-DPLUGIN_NAME="' + name + '"',
)

install_data(metadata, install_dir: metadata_dir)
