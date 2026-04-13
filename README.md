# TSCRT

**TSCRT** is a cross-platform SSH and serial console terminal emulator built with Qt 6, libssh2, and libvterm. It targets engineers who need a reliable, scriptable, multi-session console for working with embedded devices, network gear, and remote servers.

[한국어](README.ko.md) | [日本語](README.ja.md) | [中文](README.zh.md) | [Español](README.es.md) | [Français](README.fr.md) | [Deutsch](README.de.md) | [Português](README.pt.md) | [Русский](README.ru.md) | [Italiano](README.it.md)

![TSCRT split-pane SSH session](docs/images/tscrt-split-panes.png)

---

## Features

### Sessions & Terminal
- **SSH** sessions powered by libssh2 with keepalive, auto-reconnect, and **host key verification (TOFU)**
- **Serial / RS-232** sessions for embedded targets
- Multi-tab session management with persistent profiles
- **Per-session terminal type** (xterm-256color, vt100, etc.) with global default fallback
- **Split panes** (horizontal/vertical) per tab
- **Input broadcast** to all panes in a tab (`Ctrl+Shift+B`)
- **Find bar** with regex and case-sensitive search across the full scrollback (`Ctrl+F`)
- Persistent terminal-wide highlight / **Mark** feature
- Full-screen mode (`F11`) — all shortcuts remain active

### Multi-window
- **Detach tab to new window** — right-click a tab and choose "Detach to New Window" to move it into a separate TSCRT instance while keeping the connection alive
- **Drag tabs between windows** — drag a tab from one window's tab bar and drop it on another
- Tab context menu: Rename, Duplicate, Pin, Run Snapshot, Detach, Close

### Automation
- **Automation Engine** — startup actions, pattern-triggered actions (rate-limited), periodic tasks
- **Snapshots** — run shell commands on-connect, on-pattern, or on a schedule, and capture the output
- **Cron scheduler** — 5-field cron expressions for snapshot timing
- **Button bar** — per-session customizable quick-action buttons
- **Command-line widget** for inline action / script execution

### Security
- **SSH host key verification** — TOFU model with `known_hosts` file; mismatched keys are hard-rejected
- Credentials encrypted at rest: **DPAPI** (Windows), **Keychain** (macOS), plaintext fallback (Linux)
- Windows profile protected by **owner-only ACL**
- SMTP: **TLS certificate validation enforced**, plaintext AUTH rejected, header injection blocked
- Crash dumps: owner-only permissions, auto-deleted after 7 days
- Automation triggers: per-pattern and global rate limiting
- Reconnect: minimum 250 ms base delay, hard cap at 100 attempts

### Logging & Reliability
- Per-pane session logger with a dedicated log settings dialog
- **SMTP client** for email notifications (STARTTLS/SMTPS required for auth)
- **Crash handler** with debug-symbol support
- Auto-reconnect with exponential backoff and jitter

### Internationalization
- Built-in UI translations: **English**, **한국어**, **日本語**, **中文**
- In-app HTML help guides per language

---

## Requirements

| Component | Version |
|-----------|---------|
| CMake     | >= 3.20  |
| C / C++   | C11 / C++17 |
| Qt        | >= 6.5 (Core, Gui, Widgets, Network, LinguistTools, Test) |
| libssh2   | latest  |
| libvterm  | latest  |
| pkg-config | required |

**Windows**: MSYS2 UCRT64 toolchain (gcc 15+).
**macOS**: 12+ with Homebrew.

---

## Build — Windows (MSYS2 UCRT64)

```bash
pacman -S --needed \
  mingw-w64-ucrt-x86_64-{gcc,cmake,ninja,pkgconf,qt6-base,qt6-tools,libssh2,libvterm}

source env.sh
./build.sh              # incremental (use `clean` for full rebuild)
./install.sh            # stage exe + Qt DLLs
./install.sh package    # NSIS installer
```

## Build — macOS

See [`mac_build.txt`](mac_build.txt).

```bash
brew install qt@6 libssh2 libvterm pkg-config cmake ninja
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
cd build && cpack -G DragNDrop
```

---

## Tests

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
resources/    icon, qrc, NSIS .rc, HTML help
translations/ Qt .ts files (en/ko/ja/zh)
tests/        Qt Test unit tests
```

---

## License

Copyright (c) 2026 TePSEG Co., Ltd. — see [LICENSE.txt](LICENSE.txt).

Third-party: Qt 6 (LGPL/GPL), libssh2 (BSD-3), libvterm (MIT), OpenSSL (Apache 2.0), zlib, MinGW runtime, ICU, HarfBuzz, FreeType, libpng, libjpeg.

---

## Contact

- Developer: ygjeon@tepseg.com
