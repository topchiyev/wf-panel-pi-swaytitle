project(
    'wf-panel-pi-swaytitle',
    'cpp',
    version: '0.1.0',
    license: 'MIT',
    meson_version: '>=0.51.0',
    default_options: [
        'cpp_std=c++17',
        'c_std=c11',
        'warning_level=2',
        'werror=false',
    ],
)

add_project_arguments(['-Wno-pedantic', '-Wno-unused-parameter', '-Wno-parentheses'], language: 'cpp')
add_project_arguments('-D_GNU_SOURCE', language : [ 'c', 'cpp' ])

share_dir = join_paths(get_option('prefix'), 'share')
resource_dir = join_paths(share_dir, 'wf-panel-pi')
metadata_dir = join_paths(resource_dir, 'metadata')
ui_dir = join_paths(resource_dir, 'ui')
sysconf_dir = join_paths(get_option('prefix'), get_option('sysconfdir'))

gettext_package = meson.project_name()

add_project_arguments('-DMETADATA_DIR="' + metadata_dir + '"', language : 'cpp')
add_project_arguments('-DPACKAGE_DATA_DIR="' + resource_dir + '"', language : [ 'c', 'cpp' ])
add_project_arguments('-DGETTEXT_PACKAGE="' + gettext_package + '"', language : [ 'c', 'cpp' ])
add_project_arguments('-DPACKAGE_LOCALE_DIR="' + share_dir + '/locale"', language : [ 'c', 'cpp' ])

gtkmm = dependency('gtkmm-3.0', version: '>=3.24')
wf_panel_pi = dependency('wf-panel-pi')

deps = [ gtkmm, wf_panel_pi ]

subdir('src')

pkgconfig = import('pkgconfig')
pkgconfig.generate(
    version:      meson.project_version(),
    filebase:     meson.project_name(),
    name:         meson.project_name(),
    description: 'Sway focused window title widget for wf-panel-pi',
    requires:     gtkmm,
    subdirs:      'wf-panel-pi-swaytitle',
    variables:    ['metadatadir=' + metadata_dir,
                   'uidir=' + ui_dir,
                   'sysconfdir=' + sysconf_dir,
                   'resourcedir=' + resource_dir,
                   'plugindir=${libdir}/wf-panel-pi']
)
