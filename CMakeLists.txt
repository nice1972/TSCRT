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

# ---- Tests --------------------------------------------------------------
option(TSCRT_WIN_BUILD_TESTS "Build unit tests" ON)
if(TSCRT_WIN_BUILD_TESTS)
    enable_testing()
    find_package(Qt6 REQUIRED COMPONENTS Test)

    add_executable(test_action_parser
        tests/test_action_parser.cpp
        gui/ActionParser.cpp
        gui/ActionParser.h)
    target_include_directories(test_action_parser PRIVATE gui include)
    target_link_libraries(test_action_parser PRIVATE Qt6::Core Qt6::Test)
    add_test(NAME action_parser COMMAND test_action_parser)

    add_executable(test_profile
        tests/test_profile.cpp
        core/profile.c
        core/platform_win.c
        include/tscrt.h
        include/profile.h)
    target_include_directories(test_profile PRIVATE include core)
    target_link_libraries(test_profile PRIVATE Qt6::Core Qt6::Test)
    add_test(NAME profile COMMAND test_profile)
endif()

# ---- Install + CPack (NSIS) ---------------------------------------------
include(GNUInstallDirs)

install(TARGETS tscrt_win
        RUNTIME DESTINATION .)

# Locate Qt's deployment helper.
get_target_property(_qmake_loc Qt6::qmake IMPORTED_LOCATION)
get_filename_component(_qt_bin_dir "${_qmake_loc}" DIRECTORY)
find_program(WINDEPLOYQT_EXECUTABLE
    NAMES windeployqt6 windeployqt
    HINTS "${_qt_bin_dir}" "${CMAKE_PREFIX_PATH}/share/qt6/bin"
    REQUIRED)

# Stage Qt + plugins next to the exe at install time.
install(CODE "
    message(STATUS \"Running windeployqt: ${WINDEPLOYQT_EXECUTABLE}\")
    execute_process(
        COMMAND \"${WINDEPLOYQT_EXECUTABLE}\"
                --release
                --no-translations
                --no-system-d3d-compiler
                --no-opengl-sw
                --no-compiler-runtime
                --dir \"\${CMAKE_INSTALL_PREFIX}\"
                \"\${CMAKE_INSTALL_PREFIX}/tscrt_win.exe\")
")

# Bundle the third-party DLLs that windeployqt does not handle.
set(_TP_DLLS
    libssh2-1.dll
    libvterm-0.dll
    libcrypto-3-x64.dll
    libssl-3-x64.dll
    zlib1.dll
    libwinpthread-1.dll
    libgcc_s_seh-1.dll
    libstdc++-6.dll
)
foreach(_dll ${_TP_DLLS})
    if(EXISTS "${CMAKE_PREFIX_PATH}/bin/${_dll}")
        install(FILES "${CMAKE_PREFIX_PATH}/bin/${_dll}" DESTINATION .)
    endif()
endforeach()

# Pack with CPack / NSIS.
set(CPACK_PACKAGE_NAME              "tscrt_win")
set(CPACK_PACKAGE_VENDOR            "tscrt project")
set(CPACK_PACKAGE_VERSION           "1.0.0")
set(CPACK_PACKAGE_FILE_NAME         "tscrt_win-${CPACK_PACKAGE_VERSION}-win64")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "tscrt for Windows - SSH and serial console terminal emulator")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "tscrt_win")
set(CPACK_PACKAGE_EXECUTABLES       "tscrt_win" "tscrt for Windows")
set(CPACK_CREATE_DESKTOP_LINKS      "tscrt_win")

set(CPACK_GENERATOR "NSIS")
set(CPACK_NSIS_DISPLAY_NAME           "tscrt for Windows")
set(CPACK_NSIS_PACKAGE_NAME           "tscrt for Windows")
set(CPACK_NSIS_HELP_LINK              "https://github.com/")
set(CPACK_NSIS_URL_INFO_ABOUT         "https://github.com/")
set(CPACK_NSIS_CONTACT                "tscrt-project@example.com")
set(CPACK_NSIS_MODIFY_PATH            OFF)
set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
set(CPACK_NSIS_INSTALLED_ICON_NAME    "tscrt_win.exe")
set(CPACK_NSIS_MUI_ICON
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/tscrt_win.ico")
set(CPACK_NSIS_MUI_UNIICON
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/tscrt_win.ico")

include(CPack)

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
