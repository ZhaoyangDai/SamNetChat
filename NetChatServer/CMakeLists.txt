cmake_minimum_required(VERSION 3.19)
project(NetChatServer LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets LinguistTools Network Sql)
find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(Qt6 REQUIRED COMPONENTS Core)

qt_standard_project_setup()

qt_add_executable(NetChatServer
    WIN32 MACOSX_BUNDLE
    main.cpp
    netchatview.cpp
    netchatview.h
    netchatview.ui
    netchatserver.h netchatserver.cpp
    heartbeatworker.cpp heartbeatworker.h
    netchatmanager.h netchatmanager.cpp
    protocol.h
)

qt_add_translations(
    TARGETS NetChatServer
    TS_FILES NetChatServer_zh_CN.ts
)

target_link_libraries(NetChatServer
    PRIVATE
        Qt::Core
        Qt::Widgets
        Qt::Network
        Qt::Sql
)

include(GNUInstallDirs)

install(TARGETS NetChatServer
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET NetChatServer
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
