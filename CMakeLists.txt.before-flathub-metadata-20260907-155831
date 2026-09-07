# #ju56Us
# READ ME: CMake tells Qt Creator how CrashSentinel is built.
# The qt_add_executable() list is the project source list. Qt's AUTOMOC/AUTOUIC/
# AUTORCC support is enabled above it. The executable links Qt Widgets for the
# GUI and Qt Network for the update checker. START-HERE.md is documentation only.

cmake_minimum_required(VERSION 3.21)

project(CrashSentinel VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network)

qt_add_executable(CrashSentinel
    # Documentation-only project map shown in Qt Creator.
    START-HERE.md
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

    publishing_status.cpp
    publishing_status.h

    recovery_diagnostics.cpp
    recovery_diagnostics.h

    deep_diagnostics.cpp
    deep_diagnostics.h
    state_paths.cpp
    state_paths.h
)

target_link_libraries(CrashSentinel PRIVATE
    Qt6::Widgets
    Qt6::Network
)

install(TARGETS CrashSentinel
    RUNTIME DESTINATION bin
)

include("${CMAKE_CURRENT_SOURCE_DIR}/audit_diffs.cmake")
