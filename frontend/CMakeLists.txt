# SPDX-License-Identifier: MIT
cmake_minimum_required(VERSION 3.22)
# 121gw-qt: a Qt6 companion for the EEVblog 121GW multimeter (live LCD, remote buttons, MCP for AI
# assistants), ported from Readout (https://github.com/amd989/readout, MIT).
project(121gw-qt VERSION 0.1.0 LANGUAGES CXX)
# Every spelling of the version (release, or nightly with GW121_NIGHTLY set): GW121_VERSION_<KEY>.
include(cmake/GW121Version.cmake)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
# Static QML plugins aren't used; skipping their CMake configs also avoids Ubuntu noble's
# qt6-lottie-dev, which references a plugin only qml6-module-qt-labs-lottieqt ships.
set(QT_SKIP_AUTO_QML_PLUGIN_INCLUSION ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Widgets Qml Quick QuickControls2 Svg Bluetooth Network Test)

# GCC and Clang: every warning is an error. MSVC: level 3, not yet fatal; cl's conversion warnings
# (C4244, C4267) in code written against GCC's -Wall -Wextra are not errors anywhere else. Make /WX
# part of it once the Windows build is clean.
if(MSVC)
    set(GW121_WARNINGS /W3 /permissive- /Zc:__cplusplus /utf-8)
else()
    set(GW121_WARNINGS -Wall -Wextra -Werror)
endif()

# The meter itself: packets, decoding, the log. No Bluetooth and no UI, so tests and every
# front end share it.
add_library(readout_core STATIC
    src/core/metermode.cpp src/core/metermode.h
    src/core/meterpacket.cpp src/core/meterpacket.h
    src/core/packetbuilder.cpp src/core/packetbuilder.h
    src/core/reading.cpp src/core/reading.h
    src/core/measurementlog.cpp src/core/measurementlog.h
    src/core/dial.cpp src/core/dial.h
    src/core/keyinference.cpp src/core/keyinference.h
    src/core/simulatedmeter.cpp src/core/simulatedmeter.h
    src/core/sessionstore.cpp src/core/sessionstore.h
    src/core/ymodem.cpp src/core/ymodem.h
    src/core/cardprotocol.cpp src/core/cardprotocol.h
)
target_include_directories(readout_core PUBLIC src/core)
target_link_libraries(readout_core PUBLIC Qt6::Core)
target_compile_options(readout_core PRIVATE ${GW121_WARNINGS})

# The app: Bluetooth, the MCP server and the QML front panel.
# Connections, the session and the MCP server: everything but the UI, so the tests use it too.
add_library(gw121_app STATIC
    src/app/meterconnection.h
    src/app/simulatedconnection.cpp src/app/simulatedconnection.h
    src/app/simulatedcard.cpp src/app/simulatedcard.h
    src/app/cardtransfer.cpp src/app/cardtransfer.h
    src/app/bleconnection.cpp src/app/bleconnection.h
    src/app/metersession.cpp src/app/metersession.h
    src/app/metertools.cpp src/app/metertools.h
    src/app/mcpserver.cpp src/app/mcpserver.h
)
target_include_directories(gw121_app PUBLIC src/app)
target_link_libraries(gw121_app PUBLIC readout_core Qt6::Bluetooth Qt6::Network)
target_compile_options(gw121_app PRIVATE ${GW121_WARNINGS})

# WIN32: no console window on Windows. MACOSX_BUNDLE: macOS grants Bluetooth only to a bundle whose
# Info.plist says why it wants it (NSBluetoothAlwaysUsageDescription); without one it ends the app.
add_executable(121gw-qt WIN32 MACOSX_BUNDLE
    src/app/main.cpp
    src/app/metercontroller.cpp src/app/metercontroller.h
    src/app/lcditem.cpp src/app/lcditem.h
    src/app/facelayout.cpp src/app/facelayout.h
)
file(GLOB GW121_QML RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} CONFIGURE_DEPENDS qml/*.qml qml/qmldir qml/qtquickcontrols2.conf)
qt_add_resources(121gw-qt ui PREFIX / FILES ${GW121_QML} icons/121gw-qt.svg)
# Qt Quick Controls reads its style from :/qtquickcontrols2.conf.
qt_add_resources(121gw-qt style PREFIX / BASE qml FILES qml/qtquickcontrols2.conf)
target_include_directories(121gw-qt PRIVATE src/app)
target_compile_definitions(121gw-qt PRIVATE GW121_VERSION="${GW121_VERSION_FULL}")
target_link_libraries(121gw-qt PRIVATE gw121_app Qt6::Gui Qt6::Widgets Qt6::Qml Qt6::Quick Qt6::QuickControls2 Qt6::Svg Qt6::Bluetooth Qt6::Network)
target_compile_options(121gw-qt PRIVATE ${GW121_WARNINGS})
set_target_properties(121gw-qt PROPERTIES
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/packaging/macos/Info.plist.in"
    MACOSX_BUNDLE_BUNDLE_NAME "121gw-qt"
    MACOSX_BUNDLE_GUI_IDENTIFIER "io.github.ayourk.GW121Qt"
    MACOSX_BUNDLE_BUNDLE_VERSION "${GW121_VERSION_BUNDLE}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${GW121_VERSION_BASE}")
install(TARGETS 121gw-qt RUNTIME DESTINATION bin BUNDLE DESTINATION .)
# Desktop integration and the manual page, for every Linux and BSD package (deb, rpm, Arch, AppImage).
# The Debian menu entry and its XPM icon are debian/'s own.
if(UNIX AND NOT APPLE)
    include(GNUInstallDirs)
    install(FILES packaging/linux/121gw-qt.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES icons/121gw-qt.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
    install(FILES docs/man/121gw-qt.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
endif()

enable_testing()
add_executable(tst_core tests/tst_core.cpp)
target_link_libraries(tst_core PRIVATE readout_core Qt6::Test)
add_test(NAME core COMMAND tst_core)
add_executable(tst_mcp tests/tst_mcp.cpp)
target_link_libraries(tst_mcp PRIVATE gw121_app Qt6::Test)
add_test(NAME mcp COMMAND tst_mcp)
add_executable(tst_ymodem tests/tst_ymodem.cpp)
target_link_libraries(tst_ymodem PRIVATE readout_core Qt6::Test)
add_test(NAME ymodem COMMAND tst_ymodem)
