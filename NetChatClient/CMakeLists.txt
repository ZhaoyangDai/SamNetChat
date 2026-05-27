cmake_minimum_required(VERSION 3.19)
project(NetChatClient LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Network)
find_package(Qt6 REQUIRED COMPONENTS Core)

qt_standard_project_setup()

qt_add_executable(NetChatClient
    WIN32 MACOSX_BUNDLE
    main.cpp
    netchatview.cpp
    netchatview.h
    netchatview.ui
    protocol.h
    logindialog.h logindialog.cpp logindialog.ui
    registerview.h registerview.cpp registerview.ui
    netchatclient.h netchatclient.cpp
)

target_link_libraries(NetChatClient
    PRIVATE
        Qt::Core
        Qt::Widgets
        Qt::Network
)
target_link_libraries(NetChatClient PRIVATE Qt6::Core)

include(GNUInstallDirs)

install(TARGETS NetChatClient
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET NetChatClient
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
