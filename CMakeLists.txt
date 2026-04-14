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
find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Widgets Network LinguistTools)

# pkg-config for libssh2 / libvterm
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBSSH2  REQUIRED IMPORTED_TARGET libssh2)
pkg_check_modules(LIBVTERM REQUIRED IMPORTED_TARGET vterm)

# libsecret (Linux Secret Service) — optional. When present, Credentials.cpp
# stores passwords in the user's keyring instead of on disk in plaintext.
if(UNIX AND NOT APPLE)
    pkg_check_modules(LIBSECRET IMPORTED_TARGET libsecret-1)
endif()

# ---- core (C, platform-portable) -----------------------------------------
set(CORE_SOURCES
    core/profile.c
)
if(WIN32)
    list(APPEND CORE_SOURCES core/platform_win.c)
else()
    list(APPEND CORE_SOURCES core/platform_posix.c)
endif()

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
    gui/SessionLogger.h
    gui/SessionLogger.cpp
    gui/SessionManagerDialog.h
    gui/SessionManagerDialog.cpp
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
    gui/CommandLineWidget.h
    gui/CommandLineWidget.cpp
    gui/SessionHistory.h
    gui/SessionHistory.cpp
    gui/Pane.h
    gui/Pane.cpp
    gui/SessionTab.h
    gui/SessionTab.cpp
    gui/SmtpClient.h
    gui/SmtpClient.cpp
    gui/SnapshotRunner.h
    gui/SnapshotRunner.cpp
    gui/SnapshotManager.h
    gui/SnapshotManager.cpp
    gui/SnapshotsDialog.h
    gui/SnapshotsDialog.cpp
    gui/SnapshotBrowserDialog.h
    gui/SnapshotBrowserDialog.cpp
    gui/HelpDialog.h
    gui/HelpDialog.cpp
    gui/FindBar.h
    gui/FindBar.cpp
    gui/LogSettingsDialog.h
    gui/LogSettingsDialog.cpp
    gui/CronSchedule.h
    gui/CronSchedule.cpp
)

if(WIN32)
    set(PLATFORM_RESOURCES
        resources/tscrt_win.rc
        resources/tscrt_win.qrc
    )
elseif(APPLE)
    set(PLATFORM_RESOURCES
        resources/tscrt_win.qrc
        resources/tscrt_mac.icns
    )
    set_source_files_properties(resources/tscrt_mac.icns PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources")
else()
    set(PLATFORM_RESOURCES
        resources/tscrt_win.qrc
    )
endif()

# ---- Translations --------------------------------------------------------
set(TS_FILES
    translations/tscrt_win_en.ts
    translations/tscrt_win_ko.ts
    translations/tscrt_win_ja.ts
    translations/tscrt_win_zh.ts
    translations/tscrt_win_es.ts
    translations/tscrt_win_fr.ts
    translations/tscrt_win_de.ts
    translations/tscrt_win_pt.ts
    translations/tscrt_win_ru.ts
    translations/tscrt_win_it.ts
)

if(WIN32)
    set(_TSCRT_EXE_TYPE WIN32)
elseif(APPLE)
    set(_TSCRT_EXE_TYPE MACOSX_BUNDLE)
else()
    set(_TSCRT_EXE_TYPE "")
endif()

add_executable(tscrt_win ${_TSCRT_EXE_TYPE}
    ${GUI_SOURCES}
    ${CORE_SOURCES}
    ${PLATFORM_RESOURCES}
)

if(APPLE)
    set_target_properties(tscrt_win PROPERTIES
        OUTPUT_NAME                       tscrt_mac
        MACOSX_BUNDLE_GUI_IDENTIFIER      "com.tepseg.tscrt"
        MACOSX_BUNDLE_BUNDLE_NAME         "TSCRT"
        MACOSX_BUNDLE_BUNDLE_VERSION      "1.0.2"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0.2"
        MACOSX_BUNDLE_ICON_FILE           "tscrt_mac.icns"
        MACOSX_BUNDLE_COPYRIGHT           "© 2026 TePSEG Co., Ltd."
    )
elseif(UNIX)
    set_target_properties(tscrt_win PROPERTIES
        OUTPUT_NAME tscrt_linux)
endif()

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

    add_executable(test_cron
        tests/test_cron.cpp
        gui/CronSchedule.cpp
        gui/CronSchedule.h)
    target_include_directories(test_cron PRIVATE gui include)
    target_link_libraries(test_cron PRIVATE Qt6::Core Qt6::Test)
    add_test(NAME cron COMMAND test_cron)

    if(WIN32)
        set(_PLATFORM_SRC core/platform_win.c)
    else()
        set(_PLATFORM_SRC core/platform_posix.c)
    endif()
    add_executable(test_profile
        tests/test_profile.cpp
        core/profile.c
        ${_PLATFORM_SRC}
        include/tscrt.h
        include/profile.h)
    target_include_directories(test_profile PRIVATE include core)
    target_link_libraries(test_profile PRIVATE Qt6::Core Qt6::Test)
    add_test(NAME profile COMMAND test_profile)
endif()

# ---- Install + CPack (NSIS) ---------------------------------------------
include(GNUInstallDirs)

if(WIN32)
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
# We resolve them relative to the Qt bin dir (which is the MSYS2 ucrt64
# bin dir in our setup).
#
# This list was generated by resources/walk_deps.py running an objdump
# BFS over tscrt_win.exe + every Qt6 DLL/plugin we ship. Re-run that
# script after upgrading Qt or adding new dependencies and paste the
# output here.
set(_TP_DLLS
    # MinGW C/C++ runtime
    libgcc_s_seh-1.dll
    libstdc++-6.dll
    libwinpthread-1.dll
    # SSH / terminal
    libssh2-1.dll
    libvterm-0.dll
    # OpenSSL + compression
    libcrypto-3-x64.dll
    libssl-3-x64.dll
    zlib1.dll
    libzstd.dll
    libbz2-1.dll
    # Qt6 transitive: text/i18n
    libb2-1.dll
    libdouble-conversion.dll
    libpcre2-8-0.dll
    libpcre2-16-0.dll
    libicudt78.dll
    libicuin78.dll
    libicuuc78.dll
    libintl-8.dll
    libiconv-2.dll
    libglib-2.0-0.dll
    # Qt6 transitive: text shaping / fonts
    libharfbuzz-0.dll
    libfreetype-6.dll
    libgraphite2.dll
    libpng16-16.dll
    libjpeg-8.dll
    libmd4c.dll
    # Qt Network: brotli
    libbrotlicommon.dll
    libbrotlidec.dll
)
foreach(_dll ${_TP_DLLS})
    set(_path "${_qt_bin_dir}/${_dll}")
    if(EXISTS "${_path}")
        install(FILES "${_path}" DESTINATION .)
        message(STATUS "Bundling DLL: ${_dll}")
    else()
        message(FATAL_ERROR
            "Required runtime DLL not found: ${_path}\n"
            "Install it with: pacman -S mingw-w64-ucrt-x86_64-<package>")
    endif()
endforeach()

# Pack with CPack / NSIS.
set(CPACK_PACKAGE_NAME              "tscrt_win")
set(CPACK_PACKAGE_VENDOR            "TePSEG Co., Ltd.")
set(CPACK_PACKAGE_VERSION           "1.0.2")
set(CPACK_PACKAGE_FILE_NAME         "tscrt_win-${CPACK_PACKAGE_VERSION}-win64")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "TSCRT - SSH and serial console terminal emulator")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "tscrt_win")
set(CPACK_RESOURCE_FILE_LICENSE
    "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.txt")
# NOTE: CPACK_PACKAGE_EXECUTABLES hardcodes the shortcut target to
# "$INSTDIR/bin/<exe>" which is wrong because we install the binary to
# the install root (.) — see install(TARGETS ... DESTINATION .) above.
# Use CPACK_NSIS_MENU_LINKS to create the correct shortcut directly.
set(CPACK_NSIS_MENU_LINKS
    "tscrt_win.exe"  "TSCRT"
    "Uninstall.exe"  "Uninstall TSCRT")
set(CPACK_NSIS_CREATE_ICONS_EXTRA
    "CreateShortCut '$DESKTOP\\\\TSCRT.lnk' '$INSTDIR\\\\tscrt_win.exe'")
set(CPACK_NSIS_DELETE_ICONS_EXTRA
    "Delete '$DESKTOP\\\\TSCRT.lnk'")

set(CPACK_GENERATOR "NSIS")
set(CPACK_NSIS_DISPLAY_NAME           "TSCRT")
set(CPACK_NSIS_PACKAGE_NAME           "TSCRT")
set(CPACK_NSIS_HELP_LINK              "mailto:ygjeon@tepseg.com")
set(CPACK_NSIS_URL_INFO_ABOUT         "mailto:ygjeon@tepseg.com")
set(CPACK_NSIS_CONTACT                "ygjeon@tepseg.com")
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
elseif(APPLE)
    install(TARGETS tscrt_win BUNDLE DESTINATION .)

    # Locate macdeployqt to bundle Qt plugins/frameworks into the .app.
    get_target_property(_qmake_loc Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(_qt_bin_dir "${_qmake_loc}" DIRECTORY)
    find_program(MACDEPLOYQT_EXECUTABLE
        NAMES macdeployqt
        HINTS "${_qt_bin_dir}"
        REQUIRED)

    install(CODE "
        message(STATUS \"Running macdeployqt: ${MACDEPLOYQT_EXECUTABLE}\")
        execute_process(
            COMMAND \"${MACDEPLOYQT_EXECUTABLE}\"
                    \"\${CMAKE_INSTALL_PREFIX}/tscrt_mac.app\"
                    -always-overwrite)
    ")

    # Pack with CPack / DragNDrop (.dmg).
    set(CPACK_PACKAGE_NAME              "tscrt_mac")
    set(CPACK_PACKAGE_VENDOR            "TePSEG Co., Ltd.")
    set(CPACK_PACKAGE_VERSION           "1.0.2")
    set(CPACK_PACKAGE_FILE_NAME         "tscrt_mac-${CPACK_PACKAGE_VERSION}-macos")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
        "TSCRT - SSH and serial console terminal emulator")
    set(CPACK_RESOURCE_FILE_LICENSE
        "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.txt")

    set(CPACK_GENERATOR        "DragNDrop")
    set(CPACK_DMG_VOLUME_NAME  "TSCRT")
    set(CPACK_DMG_FORMAT       "UDZO")

    include(CPack)
else()
    # ---- Linux (Ubuntu/Debian) install + CPack DEB --------------------
    install(TARGETS tscrt_win
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

    install(FILES resources/tscrt.desktop
            DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)

    install(FILES resources/tscrt_win.png
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps
            RENAME tscrt.png)

    install(FILES LICENSE.txt
            DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/tscrt)

    # Pack with CPack / DEB.
    set(CPACK_PACKAGE_NAME              "tscrt")
    set(CPACK_PACKAGE_VENDOR            "TePSEG Co., Ltd.")
    set(CPACK_PACKAGE_VERSION           "1.0.2")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
        "TSCRT - SSH and serial console terminal emulator")
    set(CPACK_RESOURCE_FILE_LICENSE
        "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.txt")

    set(CPACK_GENERATOR "DEB")

    # Detect host architecture for the package name and control field.
    execute_process(COMMAND dpkg --print-architecture
                    OUTPUT_VARIABLE _DEB_ARCH
                    OUTPUT_STRIP_TRAILING_WHITESPACE
                    RESULT_VARIABLE _DEB_ARCH_RC)
    if(NOT _DEB_ARCH_RC EQUAL 0 OR _DEB_ARCH STREQUAL "")
        set(_DEB_ARCH "amd64")
    endif()

    set(CPACK_PACKAGE_FILE_NAME
        "tscrt_${CPACK_PACKAGE_VERSION}_${_DEB_ARCH}")
    set(CPACK_DEBIAN_PACKAGE_NAME         "tscrt")
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "${_DEB_ARCH}")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER
        "TePSEG <ygjeon@tepseg.com>")
    set(CPACK_DEBIAN_PACKAGE_SECTION      "net")
    set(CPACK_DEBIAN_PACKAGE_PRIORITY     "optional")
    set(CPACK_DEBIAN_PACKAGE_HOMEPAGE
        "https://github.com/nice1972/TSCRT")
    set(CPACK_DEBIAN_PACKAGE_DESCRIPTION
        "TSCRT - SSH and serial console terminal emulator\n Qt-based terminal for SSH and serial sessions with automation,\n session snapshots, and encrypted credential storage.")
    # Auto-resolve shared-library deps; fall back to an explicit list for
    # the subset dpkg-shlibdeps cannot see (pure runtime plugins etc.).
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS    ON)
    set(CPACK_DEBIAN_PACKAGE_DEPENDS
        "libc6, libssh2-1, libvterm0, libsecret-1-0, libqt6core6 | libqt6core6t64, libqt6gui6 | libqt6gui6t64, libqt6widgets6 | libqt6widgets6t64, libqt6network6 | libqt6network6t64")

    # postinst / postrm: refresh icon + desktop caches so the launcher
    # icon shows up right after install and disappears on remove.
    set(_DEB_CONTROL_DIR "${CMAKE_CURRENT_BINARY_DIR}/debian-control")
    file(MAKE_DIRECTORY "${_DEB_CONTROL_DIR}")
    file(WRITE "${_DEB_CONTROL_DIR}/postinst"
"#!/bin/sh
set -e
if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database -q /usr/share/applications || true
fi
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    gtk-update-icon-cache -q -t /usr/share/icons/hicolor || true
fi
exit 0
")
    file(WRITE "${_DEB_CONTROL_DIR}/postrm"
"#!/bin/sh
set -e
if [ \"$1\" = remove ] || [ \"$1\" = purge ]; then
    if command -v update-desktop-database >/dev/null 2>&1; then
        update-desktop-database -q /usr/share/applications || true
    fi
    if command -v gtk-update-icon-cache >/dev/null 2>&1; then
        gtk-update-icon-cache -q -t /usr/share/icons/hicolor || true
    fi
fi
exit 0
")
    file(CHMOD "${_DEB_CONTROL_DIR}/postinst"
         PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                     GROUP_READ GROUP_EXECUTE
                     WORLD_READ WORLD_EXECUTE)
    file(CHMOD "${_DEB_CONTROL_DIR}/postrm"
         PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                     GROUP_READ GROUP_EXECUTE
                     WORLD_READ WORLD_EXECUTE)
    set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
        "${_DEB_CONTROL_DIR}/postinst;${_DEB_CONTROL_DIR}/postrm")

    include(CPack)
endif()

target_include_directories(tscrt_win PRIVATE
    include
    core
    gui
)

target_link_libraries(tscrt_win PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    PkgConfig::LIBSSH2
    PkgConfig::LIBVTERM
)

if(WIN32)
    target_link_libraries(tscrt_win PRIVATE ws2_32 dbghelp crypt32 advapi32)
    target_compile_definitions(tscrt_win PRIVATE
        UNICODE _UNICODE WIN32_LEAN_AND_MEAN)
elseif(APPLE)
    find_library(SECURITY_FW Security)
    if(SECURITY_FW)
        target_link_libraries(tscrt_win PRIVATE ${SECURITY_FW})
    endif()
elseif(UNIX)
    if(LIBSECRET_FOUND)
        target_link_libraries(tscrt_win PRIVATE PkgConfig::LIBSECRET)
        target_compile_definitions(tscrt_win PRIVATE TSCRT_HAS_LIBSECRET=1)
    else()
        target_compile_definitions(tscrt_win PRIVATE TSCRT_HAS_LIBSECRET=0)
        message(WARNING
            "libsecret-1 not found via pkg-config. Credentials will be "
            "stored in plaintext. Install libsecret-1-dev to enable "
            "keyring-backed encryption.")
    endif()
endif()

# Quality gate: warnings are errors. Tightened progressively.
if(MSVC)
    target_compile_options(tscrt_win PRIVATE /W4 /WX)
else()
    target_compile_options(tscrt_win PRIVATE
        -Wall -Wextra -Werror
        -Wno-unknown-warning-option
        -Wno-unused-parameter
        -Wno-format-truncation
        -Wno-missing-field-initializers)
endif()

if(WIN32)
    # Target Windows 10 (1809+) APIs
    target_compile_definitions(tscrt_win PRIVATE
        _WIN32_WINNT=0x0A00
        WINVER=0x0A00
        NTDDI_VERSION=0x0A000006)
endif()
