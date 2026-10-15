cmake_minimum_required(VERSION 3.16)

project(EACV2CompatibilityTest VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Network Widgets)

qt_add_executable(eac-v2-compatibility-test
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow.h
    src/systeminspector.cpp
    src/systeminspector.h
)

target_link_libraries(eac-v2-compatibility-test PRIVATE Qt6::Network Qt6::Widgets)

install(TARGETS eac-v2-compatibility-test
    RUNTIME DESTINATION bin
)
