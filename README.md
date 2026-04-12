# TSCRT

**TSCRT** is a cross-platform SSH and serial console terminal emulator built with Qt 6, libssh2, and libvterm. It targets engineers who need a reliable, scriptable, multi-session console for working with embedded devices, network gear, and remote servers.

[한국어 README](README.ko.md)

---

## Features

### Sessions & Terminal
- **SSH** sessions powered by libssh2 with keepalive and auto-reconnect
- **Serial / RS-232** sessions for embedded targets
- Multi-tab session management with persistent profiles
- **Split panes** (horizontal/vertical) per tab
- **Input broadcast** to all panes in a tab (`Ctrl+Shift+B`)
- **Find bar** with regex and case-sensitive search across the full scrollback (`Ctrl+F`)
- Persistent terminal-wide highlight / **Mark** feature
- Full-screen mode (`F11`) and quick tab navigation (`Ctrl+Alt+←/→`)

### Automation
- **Automation Engine** — startup actions, pattern-triggered actions on received bytes, periodic tasks
- **Snapshots** — run shell commands on-connect, on-pattern, or on a schedule, and capture the output
- **Cron scheduler** — 5-field cron expressions for snapshot timing
- **Button bar** — per-session customizable quick-action buttons
- **Command-line widget** for inline action / script execution

### Logging & Reliability
- Per-pane session logger with a dedicated log settings dialog
- **SMTP client** for email notifications
- **Crash handler** with debug-symbol support (Windows: dbghelp)
- Auto-reconnect with suppression control

### Security
- Credentials encrypted at rest via **DPAPI** on Windows
- macOS uses Security.framework; Linux falls back to plaintext (re-entry required after migration)

### Internationalization
- Built-in UI translations: **English**, **한국어**, **日本語**, **中文**
- In-app HTML help guides per language

---

## Requirements

| Component | Version |
|-----------|---------|
| CMake     | ≥ 3.20  |
| C / C++   | C11 / C++17 |
| Qt        | ≥ 6.5 (Core, Gui, Widgets, Network, LinguistTools, Test) |
| libssh2   | latest  |
| libvterm  | latest  |
| pkg-config | required |

**Windows**: MSYS2 UCRT64 toolchain (gcc 15+).
**macOS**: 12+ with Homebrew.

---

## Build — Windows (MSYS2 UCRT64)

Install dependencies:

```bash
pacman -S --needed \
  mingw-w64-ucrt-x86_64-gcc \
  mingw-w64-ucrt-x86_64-cmake \
  mingw-w64-ucrt-x86_64-ninja \
  mingw-w64-ucrt-x86_64-pkgconf \
  mingw-w64-ucrt-x86_64-qt6-base \
  mingw-w64-ucrt-x86_64-qt6-tools \
  mingw-w64-ucrt-x86_64-libssh2 \
  mingw-w64-ucrt-x86_64-libvterm
```

Build and install:

```bash
source env.sh           # set up MSYS2 UCRT64 PATH/PKG_CONFIG
./build.sh              # incremental Ninja build (use `clean` to rebuild from scratch)
./install.sh            # stage tscrt_win.exe + Qt DLLs into build/install/
./install.sh package    # also generate the NSIS installer (build/tscrt_win-*-win64.exe)
```

The build script automatically terminates any running `tscrt_win.exe` before linking to avoid `Permission denied`.

---

## Build — macOS

See [`mac_build.txt`](mac_build.txt). Quick start:

```bash
brew install qt@6 libssh2 libvterm pkg-config cmake ninja
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
cd build && cpack -G DragNDrop   # produces a .dmg
```

---

## Tests

Qt Test–based unit tests are built by default:

```bash
ctest --test-dir build --output-on-failure
```

Suites: `test_action_parser`, `test_cron`, `test_profile`.

---

## Project Layout

```
core/         portable C — profile I/O, platform abstraction
gui/          Qt/C++ — main window, sessions, dialogs, automation, snapshots
include/      public headers shared between core and gui
resources/    icon, qrc, NSIS .rc, HTML help, walk_deps.py
translations/ Qt .ts files (en/ko/ja/zh)
tests/        Qt Test unit tests
```

---

## License

Copyright © 2026 TePSEG Co., Ltd. — see [LICENSE.txt](LICENSE.txt).

Third-party: Qt 6 (LGPL/GPL), libssh2 (BSD-3), libvterm (MIT), OpenSSL (Apache 2.0), zlib, MinGW runtime, ICU, HarfBuzz, FreeType, libpng, libjpeg.

---

## Contact

- Developer: ygjeon@tepseg.com
