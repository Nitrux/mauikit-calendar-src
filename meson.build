project('mauikit-calendar', 'cpp', version: '4.0.5', meson_version: '>=1.0.0', default_options: ['cpp_std=c++17', 'buildtype=release'])

cpp_compiler = meson.get_compiler('cpp')
if cpp_compiler.get_id() == 'gcc' and cpp_compiler.has_argument('-Wno-sfinae-incomplete')
    add_project_arguments('-Wno-sfinae-incomplete', language: 'cpp')
endif
qt6 = import('qt6')
qt = dependency('qt6', version: '>=6.4', modules: ['Core', 'Quick', 'Qml', 'Sql'])
kf6_include_prefix = get_option('prefix') / 'include' / 'KF6'
kf6_system_include = get_option('prefix') / 'include'
kf6_core = declare_dependency(
    include_directories: include_directories(kf6_include_prefix / 'KCoreAddons'),
    dependencies: [cpp_compiler.find_library('KF6CoreAddons')],
)
kf6_i18n = declare_dependency(
    include_directories: include_directories(kf6_include_prefix / 'KI18n'),
    dependencies: [cpp_compiler.find_library('KF6I18n')],
)
kf6_config = declare_dependency(
    include_directories: include_directories(kf6_include_prefix / 'KConfig', kf6_include_prefix / 'KConfigCore'),
    dependencies: [cpp_compiler.find_library('KF6ConfigCore')],
)
kf6_calendar = declare_dependency(
    include_directories: include_directories(kf6_system_include, kf6_include_prefix / 'KCalendarCore'),
    dependencies: [cpp_compiler.find_library('KF6CalendarCore')],
)
mauikit = dependency('MauiKit4', version: '>=4.0.5')
cpp_args = host_machine.cpu_family() == 'x86_64' ? ['-march=x86-64-v3'] : []
export_header = configure_file(
    input: 'meson-content.in',
    output: 'calendar_export.h',
    configuration: {'CONTENT': ('#pragma once\n#define CALENDAR_EXPORT __attribute__((visibility("default")))\n')}
)
version_header = configure_file(
    input: 'meson-content.in',
    output: 'calendar_version.h',
    configuration: {'CONTENT': ('#pragma once\n#define Calendar_VERSION_STRING "' + meson.project_version() + '"\n#define GIT_BRANCH "Stable"\n#define GIT_COMMIT_HASH "' + meson.project_version() + '"\n')}
)
lib_sources = files('src/code/moduleinfo.cpp', 'src/code/calendarmanager.cpp', 'src/code/incidencewrapper.cpp', 'src/code/filter.cpp', 'src/code/models/infinitecalendarviewmodel.cpp', 'src/code/models/hourlyincidencemodel.cpp', 'src/code/models/multidayincidencemodel.cpp', 'src/code/models/incidenceoccurrencemodel.cpp', 'src/code/models/monthmodel.cpp', 'src/code/models/timezonelistmodel.cpp', 'src/code/collection.cpp', 'src/code/collectioncomboboxmodel.cpp', 'src/code/mimetypes.cpp')
calendar_moc_headers = ['src/code/calendarmanager.h', 'src/code/incidencewrapper.h', 'src/code/filter.h', 'src/code/models/infinitecalendarviewmodel.h', 'src/code/models/hourlyincidencemodel.h', 'src/code/models/multidayincidencemodel.h', 'src/code/models/incidenceoccurrencemodel.h', 'src/code/models/monthmodel.h', 'src/code/models/timezonelistmodel.h', 'src/code/collection.h', 'src/code/collectioncomboboxmodel.h', 'src/code/mimetypes.h']
calendar_moc = qt6.preprocess(moc_headers: calendar_moc_headers, dependencies: [qt])
lib = shared_library('MauiKitCalendar4', lib_sources + calendar_moc + [export_header, version_header], dependencies: [qt, kf6_core, kf6_i18n, kf6_config, kf6_calendar, mauikit], include_directories: include_directories('src/code', 'src/code/models'), cpp_args: cpp_args, version: '4.0.5', soversion: '4', install: true)
plugin_moc = qt6.preprocess(moc_headers: ['src/code/plugin.h'], dependencies: [qt])
plugin = shared_module('MauiKitCalendar4plugin', ['src/code/plugin.cpp'] + plugin_moc, link_with: lib, dependencies: [qt, kf6_core, kf6_i18n, kf6_config, kf6_calendar, mauikit], install: true, install_dir: get_option('libdir') / 'qt6/qml/org/mauikit/calendar')
qml_dir = get_option('libdir') / 'qt6/qml/org/mauikit/calendar'
install_data(configure_file(
    input: 'meson-content.in',
    output: 'qmldir',
    configuration: {'CONTENT': ('module org.mauikit.calendar\nplugin MauiKitCalendar4plugin\n')}
), install_dir: qml_dir)
install_subdir('src/controls.6', install_dir: qml_dir, strip_directory: true)
install_headers(files('src/code/moduleinfo.h') + [export_header, version_header], subdir: 'MauiKit4/Calendar')
pkg = import('pkgconfig')
pkg.generate(lib, name: 'MauiKit Calendar 4', filebase: 'MauiKitCalendar4', version: meson.project_version(), description: 'MauiKit calendar library', requires: 'MauiKit4')

cmake_targets = configure_file(
    input: 'meson-content.in',
    output: 'MauiKitCalendar4Targets.cmake',
    configuration: {'CONTENT': (('''get_filename_component(_MAUIKIT_PREFIX "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
if(NOT TARGET MauiKitCalendar4)
  add_library(MauiKitCalendar4 SHARED IMPORTED)
  set_target_properties(MauiKitCalendar4 PROPERTIES
    IMPORTED_LOCATION "${_MAUIKIT_PREFIX}/@LIBDIR@/libMauiKitCalendar4.so.4.0.5"
    INTERFACE_INCLUDE_DIRECTORIES "${_MAUIKIT_PREFIX}/include/MauiKit4/Calendar")
endif()
if(NOT TARGET MauiKit4::Calendar)
  add_library(MauiKit4::Calendar ALIAS MauiKitCalendar4)
endif()
if(NOT TARGET MauiKit::Calendar)
  add_library(MauiKit::Calendar ALIAS MauiKitCalendar4)
endif()
''')).replace('@LIBDIR@', (get_option('libdir')))}
)
cmake_config = configure_file(
    input: 'meson-content.in',
    output: 'MauiKitCalendar4Config.cmake',
    configuration: {'CONTENT': ('''include(CMakeFindDependencyMacro)
find_dependency(Qt6 6.4 COMPONENTS Core)
include("${CMAKE_CURRENT_LIST_DIR}/MauiKitCalendar4Targets.cmake")
set(MauiKitCalendar4_FOUND TRUE)
''')}
)
cmake_version = configure_file(
    input: 'meson-content.in',
    output: 'MauiKitCalendar4ConfigVersion.cmake',
    configuration: {'CONTENT': (('''set(PACKAGE_VERSION "@VERSION@")
if(PACKAGE_FIND_VERSION VERSION_LESS_EQUAL PACKAGE_VERSION)
  set(PACKAGE_VERSION_COMPATIBLE TRUE)
endif()
if(PACKAGE_FIND_VERSION VERSION_EQUAL PACKAGE_VERSION)
  set(PACKAGE_VERSION_EXACT TRUE)
endif()
''')).replace('@VERSION@', (meson.project_version()))}
)
install_data([cmake_targets, cmake_config, cmake_version], install_dir: get_option('libdir') / 'cmake' / 'MauiKitCalendar4')
