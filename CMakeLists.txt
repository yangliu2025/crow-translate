cmake_minimum_required(VERSION 3.16.0)

project(crow-translate
    VERSION 2.12.0
    DESCRIPTION "A lightweight Linux translator with Google, Bing, Mozhi and Edge TTS support"
    HOMEPAGE_URL https://invent.kde.org/office/crow-translate
    LANGUAGES CXX
)

set(APPLICATION_NAME "Crow Translate")
set(APPLICATION_ID "io.crow_translate.CrowTranslate")
set(EXECUTABLE_NAME crow)
set(DESKTOP_FILE ${APPLICATION_ID}.desktop)
set(METAINFO_FILE ${APPLICATION_ID}.metainfo.xml)

if(NOT CMAKE_SYSTEM_NAME STREQUAL Linux)
    message(FATAL_ERROR "This fork supports Linux only")
endif()

set(CPACK_PACKAGE_VENDOR ${APPLICATION_NAME})
set(CPACK_PACKAGE_EXECUTABLES ${EXECUTABLE_NAME} ${APPLICATION_NAME})
set(CPACK_PACKAGE_INSTALL_DIRECTORY ${APPLICATION_NAME})
set(CPACK_PACKAGE_CONTACT genaloner@gmail.com)
set(CPACK_RESOURCE_FILE_LICENSE ${CMAKE_CURRENT_SOURCE_DIR}/COPYING)
set(CPACK_PROJECT_CONFIG_FILE ${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPackProjectConfig.cmake)

set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_GENERATE_SHLIBS_POLICY >=)
set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "qtgstreamer-plugins-qt5, gstreamer1.0-plugins-good, gstreamer1.0-alsa, gstreamer1.0-pulseaudio, libqt5multimedia5-plugins")
set(CPACK_RPM_FILE_NAME RPM-DEFAULT)
set(CPACK_RPM_PACKAGE_LICENSE GPLv3)
set(CPACK_RPM_PACKAGE_GROUP Applications/Text)
set(CPACK_RPM_COMPRESSION_TYPE lzma)
set(CPACK_EXTERNAL_PACKAGE_SCRIPT ${CMAKE_SOURCE_DIR}/cmake/AppImage.cmake)
set(CPACK_EXTERNAL_ENABLE_STAGING YES)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

if(CMAKE_CXX_CLANG_TIDY)
    # Exclude generated files
    # We generate a dummy .clang-tidy file in the binary directory that disables
    # all clang-tidy checks except one that will never match. This one check is
    # necessary; clang-tidy reports an error when no checks are enabled.
    # https://gitlab.kitware.com/cmake/cmake/-/issues/19772
    file(WRITE ${CMAKE_BINARY_DIR}/.clang-tidy "Checks: '-*,llvm-twine-local'")
endif()

option(WITH_PORTABLE_MODE "Enable portable functionality" OFF)
option(WITH_KWAYLAND "Use KWayland for better Wayland integration" ON)

find_package(ECM REQUIRED NO_MODULE)
list(APPEND CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

add_subdirectory(src/third-party/singleapplication)
add_subdirectory(src/third-party/qhotkey)
add_subdirectory(src/qonlinetranslator)

find_package(Qt5 REQUIRED COMPONENTS Widgets LinguistTools Concurrent DBus WebSockets X11Extras)
find_package(Tesseract REQUIRED)
find_package(XCB REQUIRED COMPONENTS XCB)
if(WITH_KWAYLAND)
    find_package(KF5Wayland REQUIRED COMPONENTS Client)
endif()

include(GNUInstallDirs)
include(CPack)
include(ECMInstallIcons)

set(APP_ICONS
    data/icons/app/128-apps-crow-translate.png
    data/icons/app/16-apps-crow-translate.png
    data/icons/app/22-apps-crow-translate.png
    data/icons/app/256-apps-crow-translate.png
    data/icons/app/32-apps-crow-translate.png
    data/icons/app/48-apps-crow-translate.png
    data/icons/app/512-apps-crow-translate.png
    data/icons/app/64-apps-crow-translate.png
)

qt5_add_translation(QM_FILES
    data/translations/${PROJECT_NAME}_ar_SA.ts
    data/translations/${PROJECT_NAME}_az_AZ.ts
    data/translations/${PROJECT_NAME}_de_DE.ts
    data/translations/${PROJECT_NAME}_el_GR.ts
    data/translations/${PROJECT_NAME}_es_ES.ts
    data/translations/${PROJECT_NAME}_eu_ES.ts
    data/translations/${PROJECT_NAME}_fi_FI.ts
    data/translations/${PROJECT_NAME}_fr_FR.ts
    data/translations/${PROJECT_NAME}_hi_IN.ts
    data/translations/${PROJECT_NAME}_hu_HU.ts
    data/translations/${PROJECT_NAME}_id_ID.ts
    data/translations/${PROJECT_NAME}_it_IT.ts
    data/translations/${PROJECT_NAME}_ko_KR.ts
    data/translations/${PROJECT_NAME}_ms_MY.ts
    data/translations/${PROJECT_NAME}_nl_NL.ts
    data/translations/${PROJECT_NAME}_pl_PL.ts
    data/translations/${PROJECT_NAME}_pt_BR.ts
    data/translations/${PROJECT_NAME}_pt_PT.ts
    data/translations/${PROJECT_NAME}_ru_RU.ts
    data/translations/${PROJECT_NAME}_sq_AL.ts
    data/translations/${PROJECT_NAME}_tr_TR.ts
    data/translations/${PROJECT_NAME}_ug_CN.ts
    data/translations/${PROJECT_NAME}_uk_UA.ts
    data/translations/${PROJECT_NAME}_zh_CN.ts
    data/translations/${PROJECT_NAME}_zh_TW.ts
)

configure_file(src/cmake.h.in cmake.h)

add_executable(${PROJECT_NAME}
    data/icons/flags.qrc
    ${QM_FILES}
    data/icons/icon-theme.qrc
    data/icons/engines/engines.qrc
    src/languagesdialog.cpp
    src/languagesdialog.ui
    src/cli.cpp
    src/contextmenu.cpp
    src/edgetts.cpp
    src/languagebuttonswidget.cpp
    src/languagebuttonswidget.ui
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow.ui
    src/ocr/ocr.cpp
    src/ocr/screengrabbers/abstractscreengrabber.cpp
    src/ocr/screengrabbers/genericscreengrabber.cpp
    src/ocr/snippingarea.cpp
    src/popupwindow.cpp
    src/popupwindow.ui
    src/screenwatcher.cpp
    src/selection.cpp
    src/settings/appsettings.cpp
    src/settings/ocrlanguageslistwidget.cpp
    src/settings/settingsdialog.cpp
    src/settings/settingsdialog.ui
    src/settings/shortcutsmodel/shortcutitem.cpp
    src/settings/shortcutsmodel/shortcutsmodel.cpp
    src/settings/shortcutsmodel/shortcutsview.cpp
    src/settings/tesseractparameterstablewidget.cpp
    src/settings/autostartmanager/abstractautostartmanager.cpp
    src/sourcetextedit.cpp
    src/speakbuttons.cpp
    src/speakbuttons.ui
    src/transitions/languagedetectedtransition.cpp
    src/transitions/ocruninitializedtransition.cpp
    src/transitions/playerstoppedtransition.cpp
    src/transitions/retranslationtransition.cpp
    src/transitions/snippingvisibletransition.cpp
    src/transitions/textemptytransition.cpp
    src/transitions/translatorabortedtransition.cpp
    src/transitions/translatorerrortransition.cpp
    src/translationedit.cpp
    src/trayicon.cpp
)

target_sources(${PROJECT_NAME} PRIVATE
    src/xdgdesktopportal.cpp
    src/ocr/screengrabbers/dbusscreengrabber.cpp
    src/ocr/screengrabbers/waylandgnomescreengrabber.cpp
    src/ocr/screengrabbers/waylandplasmascreengrabber.cpp
    src/ocr/screengrabbers/waylandportalscreengrabber.cpp
    src/settings/autostartmanager/unixautostartmanager.cpp
    src/settings/autostartmanager/portalautostartmanager.cpp
)
if(WITH_KWAYLAND)
    target_sources(${PROJECT_NAME} PRIVATE
        src/waylandhelper.cpp
    )
endif()

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME ${EXECUTABLE_NAME})

target_link_libraries(${PROJECT_NAME} PRIVATE
    SingleApplication::SingleApplication
    QHotkey::QHotkey
    QOnlineTranslator::QOnlineTranslator
    Qt5::Concurrent
    Qt5::DBus
    Qt5::WebSockets
    Qt5::X11Extras
    Tesseract::Tesseract
    XCB::XCB
)

if(WITH_KWAYLAND)
    target_link_libraries(${PROJECT_NAME} PRIVATE KF5::WaylandClient)
    target_compile_definitions(${PROJECT_NAME} PRIVATE WITH_KWAYLAND)
endif()

if(WITH_PORTABLE_MODE)
    target_compile_definitions(${PROJECT_NAME} PRIVATE WITH_PORTABLE_MODE)
endif()

if(NOT DEFINED QT_BIN_DIR)
    # -DQT_BIN_DIR=/path/to/qt/executables can be passed to CMake directly
    if(ECM_VERSION VERSION_LESS 5.93)
        include(ECMQueryQmake)
        query_qmake(QT_BIN_DIR QT_INSTALL_BINS)
    else()
        include(ECMQueryQt)
        ecm_query_qt(QT_BIN_DIR QT_INSTALL_BINS)
    endif()
endif()

configure_file(data/${DESKTOP_FILE}.in ${CMAKE_CURRENT_BINARY_DIR}/data/${DESKTOP_FILE} @ONLY)

install(TARGETS ${PROJECT_NAME})
install(FILES ${QM_FILES} DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/${PROJECT_NAME}/${PROJECT_NAME}/translations)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/data/${DESKTOP_FILE} DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications)
install(FILES data/${METAINFO_FILE} DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/metainfo)

ecm_install_icons(ICONS
    ${APP_ICONS}
    data/icons/app/sc-apps-crow-translate.svg
    data/icons/app/16-status-crow-translate-tray-dark.png
    data/icons/app/16-status-crow-translate-tray-light.png
    data/icons/app/22-status-crow-translate-tray-dark.png
    data/icons/app/22-status-crow-translate-tray-light.png
    data/icons/app/24-status-crow-translate-tray-dark.png
    data/icons/app/24-status-crow-translate-tray-light.png
    data/icons/app/sc-status-crow-translate-tray-dark.svg
    data/icons/app/sc-status-crow-translate-tray-light.svg
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons
)
