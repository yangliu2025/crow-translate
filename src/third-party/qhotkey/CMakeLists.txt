cmake_minimum_required(VERSION 3.16)

project(qhotkey VERSION 1.2.2 LANGUAGES CXX)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(Qt5 REQUIRED COMPONENTS Core Widgets X11Extras)
find_package(X11 REQUIRED)

qt5_wrap_cpp(MOC_HEADERS
    QHotkey/qhotkey.h
    QHotkey/qhotkey_p.h
)

add_library(qhotkey STATIC
    QHotkey/qhotkey.cpp
    QHotkey/qhotkey_x11.cpp
    QHotkey/qhotkey.h
    QHotkey/qhotkey_p.h
    ${MOC_HEADERS}
)
add_library(QHotkey::QHotkey ALIAS qhotkey)

target_include_directories(qhotkey PRIVATE ${X11_INCLUDE_DIR})
target_include_directories(qhotkey PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/QHotkey)

target_link_libraries(qhotkey
    PRIVATE ${X11_LIBRARIES}
    PUBLIC Qt5::Core Qt5::Widgets Qt5::X11Extras
)
