cmake_minimum_required(VERSION 3.16)

project(SingleApplication LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)

add_library(${PROJECT_NAME} STATIC
    singleapplication.cpp
    singleapplication.h
    singleapplication_p.cpp
    singleapplication_p.h
)
add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

find_package(Qt5 REQUIRED COMPONENTS Core Network Widgets)

target_link_libraries(${PROJECT_NAME} PUBLIC Qt5::Core Qt5::Network Qt5::Widgets)
target_compile_definitions(${PROJECT_NAME} PUBLIC QAPPLICATION_CLASS=QApplication)
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
