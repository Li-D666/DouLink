cmake_minimum_required(VERSION 3.16)

# Qt 5.14 ships CMake config files that declare very old policy minimums.
# CMake 4.x refuses those, so relax the policy floor when configuring with 4.x.
if(CMAKE_VERSION VERSION_GREATER_EQUAL "4.0" AND NOT DEFINED CMAKE_POLICY_VERSION_MINIMUM)
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "Minimum policy version for legacy Qt config files" FORCE)
endif()

project(DouLink VERSION 1.0.0 LANGUAGES CXX)

if(WIN32)
    enable_language(RC)
endif()

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

if(MSVC)
    add_compile_options(/W3 /utf-8)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(Qt5 5.14 REQUIRED COMPONENTS Core Gui Widgets Network SerialPort)

# ---------------------------------------------------------------------------
# doulink_core: domain + application + infrastructure (QtCore/QtNetwork/
# QtSerialPort). The UI layer is deliberately not part of this library.
# ---------------------------------------------------------------------------
add_library(doulink_core STATIC
    src/core/Types.cpp
    src/core/Types.h
    src/core/ByteCodec.cpp
    src/core/ByteCodec.h
    src/core/FrameCodec.cpp
    src/core/FrameCodec.h
    src/core/ITransport.h
    src/app/Session.cpp
    src/app/Session.h
    src/app/SessionManager.cpp
    src/app/SessionManager.h
    src/infra/SerialTransport.cpp
    src/infra/SerialTransport.h
    src/infra/TcpClientTransport.cpp
    src/infra/TcpClientTransport.h
    src/infra/TcpServerTransport.cpp
    src/infra/TcpServerTransport.h
    src/infra/UdpTransport.cpp
    src/infra/UdpTransport.h
    src/infra/TransportFactory.cpp
    src/infra/TransportFactory.h
)

target_include_directories(doulink_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(doulink_core PUBLIC
    Qt5::Core
    Qt5::Network
    Qt5::SerialPort
)

# ---------------------------------------------------------------------------
# DouLink: Qt Widgets desktop application.
# ---------------------------------------------------------------------------
add_executable(DouLink WIN32
    src/main.cpp
    src/ui/BrandMark.cpp
    src/ui/BrandMark.h
    src/ui/LogView.cpp
    src/ui/LogView.h
    src/ui/SendPanel.cpp
    src/ui/SendPanel.h
    src/ui/SessionDialog.cpp
    src/ui/SessionDialog.h
    src/ui/SessionListDelegate.cpp
    src/ui/SessionListDelegate.h
    src/ui/Theme.cpp
    src/ui/Theme.h
    src/ui/MainWindow.cpp
    src/ui/MainWindow.h
    src/ui/mainwindow.ui
    src/ui/sessiondialog.ui
    src/ui/sendpanel.ui
    resources/resources.qrc
)

target_link_libraries(DouLink PRIVATE
    doulink_core
    Qt5::Widgets
    Qt5::Gui
)

if(WIN32)
    set_target_properties(DouLink PROPERTIES WIN32_EXECUTABLE TRUE)

    # Embed the app icon into the PE file. QApplication::setWindowIcon() only
    # affects the running window; Explorer/taskbar use the executable resource.
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/app.rc.in"
        "${CMAKE_CURRENT_BINARY_DIR}/app.rc"
        @ONLY
    )
    target_sources(DouLink PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/app.rc")
    set_property(SOURCE "${CMAKE_CURRENT_BINARY_DIR}/app.rc" APPEND PROPERTY
        OBJECT_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/resources/app.ico")
endif()