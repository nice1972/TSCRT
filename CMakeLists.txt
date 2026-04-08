cmake_minimum_required(VERSION 3.20)
project(tscrt_win LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Qt6 (MSYS2 UCRT64)
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets LinguistTools)

# pkg-config for libssh2 / libvterm
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBSSH2  REQUIRED IMPORTED_TARGET libssh2)
pkg_check_modules(LIBVTERM REQUIRED IMPORTED_TARGET vterm)

# ---- core (C, platform-portable) -----------------------------------------
set(CORE_SOURCES
    core/profile.c
    core/platform_win.c
)

# ---- gui (C++/Qt) --------------------------------------------------------
set(GUI_SOURCES
    gui/main.cpp
    gui/MainWindow.cpp
    gui/MainWindow.h
    gui/ISession.h
    gui/SshSession.h
    gui/SshSession.cpp
    gui/SerialSession.h
    gui/SerialSession.cpp
    gui/TerminalWidget.h
    gui/TerminalWidget.cpp
    gui/SessionEditDialog.h
    gui/SessionEditDialog.cpp
    gui/SettingsDialog.h
    gui/SettingsDialog.cpp
    gui/Logging.h
    gui/Logging.cpp
    gui/CrashHandler.h
    gui/CrashHandler.cpp
    gui/Credentials.h
    gui/Credentials.cpp
    gui/ActionParser.h
    gui/ActionParser.cpp
    gui/AutomationEngine.h
    gui/AutomationEngine.cpp
    gui/ButtonBar.h
    gui/ButtonBar.cpp
    gui/SessionTab.h
    gui/SessionTab.cpp
)

set(WIN_RESOURCES
    resources/tscrt_win.rc
    resources/tscrt_win.qrc
)

# ---- Translations --------------------------------------------------------
set(TS_FILES
    translations/tscrt_win_ko.ts
    translations/tscrt_win_en.ts
)

add_executable(tscrt_win WIN32
    ${GUI_SOURCES}
    ${CORE_SOURCES}
    ${WIN_RESOURCES}
)

qt_add_translations(tscrt_win
    TS_FILES        ${TS_FILES}
    RESOURCE_PREFIX "/i18n")

# Make sure CMake hands the .rc file to windres correctly.
set_source_files_properties(resources/tscrt_win.rc
    PROPERTIES LANGUAGE RC)

target_include_directories(tscrt_win PRIVATE
    include
    core
    gui
)

target_link_libraries(tscrt_win PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    PkgConfig::LIBSSH2
    PkgConfig::LIBVTERM
    ws2_32
    dbghelp
    crypt32
)

target_compile_definitions(tscrt_win PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
)

# Quality gate: warnings are errors. Tightened progressively.
if(MSVC)
    target_compile_options(tscrt_win PRIVATE /W4 /WX)
else()
    target_compile_options(tscrt_win PRIVATE
        -Wall -Wextra -Werror
        -Wno-unused-parameter
        -Wno-format-truncation
        -Wno-missing-field-initializers)
endif()

# Target Windows 10 (1809+) APIs
target_compile_definitions(tscrt_win PRIVATE
    _WIN32_WINNT=0x0A00
    WINVER=0x0A00
    NTDDI_VERSION=0x0A000006)
