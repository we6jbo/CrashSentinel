# #ju56Us
cmake_minimum_required(VERSION 3.21)

project(CrashSentinel VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network)

qt_add_executable(CrashSentinel
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui

    context_provenance.cpp
    context_provenance.h
    tg_context_snapshot.json

    crash_monitor.cpp
    crash_monitor.h

    repair_attestation.cpp
    repair_attestation.h

    crash_settings.cpp
    crash_settings.h

    resource_guard.cpp
    resource_guard.h

    version_info.h

    self_protection.cpp
    self_protection.h

    status_snapshot.cpp
    status_snapshot.h

    update_checker.cpp
    update_checker.h
)

target_link_libraries(CrashSentinel PRIVATE
    Qt6::Widgets
    Qt6::Network
)

install(TARGETS CrashSentinel
    RUNTIME DESTINATION bin
)

include("${CMAKE_CURRENT_SOURCE_DIR}/audit_diffs.cmake")
