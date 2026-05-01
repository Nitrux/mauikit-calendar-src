# SPDX-FileCopyrightText: 2020 Camilo Higuita <milo.h@aol.com>
#
# SPDX-License-Identifier: GPL-3.0-or-later

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/code
    ${CMAKE_CURRENT_SOURCE_DIR}/code/models)

add_library(MauiKitCalendar${MAUI_MAJOR_VERSION})
set_source_files_properties(controls.${QT_MAJOR_VERSION}/KalendarUiUtils.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

ecm_add_qml_module(MauiKitCalendar${MAUI_MAJOR_VERSION}
    VERSION 1.0
    URI "org.mauikit.calendar"
    CLASS_NAME MauiCalendarPlugin
    NO_GENERATE_PLUGIN_SOURCE)

##header files to install
set(lib_HDRS
    code/moduleinfo.h)

target_sources(MauiKitCalendar${MAUI_MAJOR_VERSION}plugin
    PRIVATE
    code/plugin.cpp
    code/plugin.h)

target_sources(MauiKitCalendar${MAUI_MAJOR_VERSION}
    PRIVATE

    code/moduleinfo.cpp
    code/moduleinfo.h

    code/calendarmanager.cpp
    code/calendarmanager.h

    code/incidencewrapper.cpp
    code/incidencewrapper.h

    code/filter.cpp
    code/filter.h

    code/models/infinitecalendarviewmodel.cpp
    code/models/infinitecalendarviewmodel.h

    code/models/hourlyincidencemodel.cpp
    code/models/hourlyincidencemodel.h

    code/models/multidayincidencemodel.cpp
    code/models/multidayincidencemodel.h

    code/models/incidenceoccurrencemodel.cpp
    code/models/incidenceoccurrencemodel.h

    code/models/monthmodel.cpp
    code/models/monthmodel.h

    code/models/timezonelistmodel.cpp
    code/models/timezonelistmodel.h

    code/collection.cpp
    code/collection.h

    code/collectioncomboboxmodel.cpp
    code/collectioncomboboxmodel.h

    code/mimetypes.cpp
    code/mimetypes.h)

ecm_target_qml_sources(MauiKitCalendar${MAUI_MAJOR_VERSION} SOURCES
    controls.${QT_MAJOR_VERSION}/labelutils.js
    controls.${QT_MAJOR_VERSION}/dateutils.js
    controls.${QT_MAJOR_VERSION}/CollectionComboBox.qml
    controls.${QT_MAJOR_VERSION}/DateComboBox.qml
    controls.${QT_MAJOR_VERSION}/DatePicker.qml
    controls.${QT_MAJOR_VERSION}/DayGridView.qml
    controls.${QT_MAJOR_VERSION}/DayLabelsBar.qml
    controls.${QT_MAJOR_VERSION}/DaysGrid.qml
    controls.${QT_MAJOR_VERSION}/EventPage.qml
    controls.${QT_MAJOR_VERSION}/MonthsGrid.qml
    controls.${QT_MAJOR_VERSION}/MonthView.qml
    controls.${QT_MAJOR_VERSION}/TimeComboBox.qml
    controls.${QT_MAJOR_VERSION}/YearsGrid.qml
    controls.${QT_MAJOR_VERSION}/YearView.qml
    controls.${QT_MAJOR_VERSION}/KalendarUiUtils.qml
    controls.${QT_MAJOR_VERSION}/TimePicker.qml)

add_library(MauiKit${MAUI_MAJOR_VERSION}::Calendar ALIAS MauiKitCalendar${MAUI_MAJOR_VERSION})

target_compile_options(MauiKitCalendar${MAUI_MAJOR_VERSION} PRIVATE -Wno-missing-include-dirs)

generate_export_header(MauiKitCalendar${MAUI_MAJOR_VERSION} BASE_NAME Calendar)
set_target_properties(MauiKitCalendar${MAUI_MAJOR_VERSION} PROPERTIES
    VERSION ${MAUIKIT_SO_VERSION}
    SOVERSION ${MAUIKIT_SO_VERSION}
    EXPORT_NAME "Calendar")

target_include_directories(MauiKitCalendar${MAUI_MAJOR_VERSION}
    INTERFACE "$<INSTALL_INTERFACE:${KDE_INSTALL_INCLUDEDIR}/MauiKit${MAUI_MAJOR_VERSION}/Calendar>")

target_include_directories(MauiKitCalendar${MAUI_MAJOR_VERSION} PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR};${CMAKE_CURRENT_BINARY_DIR};>")

target_link_libraries(MauiKitCalendar${MAUI_MAJOR_VERSION}
    PRIVATE
    MauiKit${MAUI_MAJOR_VERSION}
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Quick
    Qt${QT_MAJOR_VERSION}::Qml
    KF${KF_MAJOR_VERSION}::I18n
    KF${KF_MAJOR_VERSION}::CoreAddons
    KF${KF_MAJOR_VERSION}::ConfigCore
    KF${KF_MAJOR_VERSION}::CalendarCore)

target_link_libraries(MauiKitCalendar${MAUI_MAJOR_VERSION}plugin
    PRIVATE
    MauiKitCalendar${MAUI_MAJOR_VERSION}
    MauiKit${MAUI_MAJOR_VERSION}
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Gui
    Qt${QT_MAJOR_VERSION}::Quick
    Qt${QT_MAJOR_VERSION}::Qml
    KF${KF_MAJOR_VERSION}::I18n
    KF${KF_MAJOR_VERSION}::CoreAddons
    KF${KF_MAJOR_VERSION}::ConfigCore
    KF${KF_MAJOR_VERSION}::CalendarCore)

ecm_finalize_qml_module(MauiKitCalendar${MAUI_MAJOR_VERSION} DESTINATION ${KDE_INSTALL_QMLDIR})

install(TARGETS MauiKitCalendar${MAUI_MAJOR_VERSION} EXPORT MauiKitCalendar${MAUI_MAJOR_VERSION}Targets ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})

install(FILES
    ${lib_HDRS}
    ${CMAKE_CURRENT_BINARY_DIR}/calendar_export.h
    DESTINATION ${KDE_INSTALL_INCLUDEDIR}/MauiKit${MAUI_MAJOR_VERSION}/Calendar
    COMPONENT Devel)
