# TSCRT

**Läuft unter Windows, Linux und macOS.**

**TSCRT** ist ein plattformübergreifender SSH- und Serial-Konsolen-Terminalemulator, der mit Qt 6, libssh2 und libvterm entwickelt wurde. Er richtet sich an Ingenieure, die eine zuverlässige, skriptfähige Multi-Session-Konsole für eingebettete Geräte, Netzwerktechnik und Remote-Server benötigen.

[English](README.md) | [한국어](README.ko.md) | [日本語](README.ja.md) | [中文](README.zh.md) | [Español](README.es.md) | [Français](README.fr.md) | **Deutsch** | [Português](README.pt.md) | [Русский](README.ru.md) | [Italiano](README.it.md)

![TSCRT Split-Pane-SSH-Sitzung](docs/images/tscrt-split-panes.png)

---

## Herunterladen

Vorkompilierte Binärdateien werden auf [GitHub Releases](https://github.com/nice1972/TSCRT/releases/latest) veröffentlicht.

| Plattform | Datei | Hinweise |
|-----------|-------|----------|
| **Windows x64** | [tscrt_win-1.0.10-win64.exe](https://github.com/nice1972/TSCRT/releases/download/v1.0.10/tscrt_win-1.0.10-win64.exe) | NSIS-Installationsprogramm |
| **Ubuntu / Debian (amd64)** | [tscrt_1.0.10_amd64.deb](https://github.com/nice1972/TSCRT/releases/download/v1.0.10/tscrt_1.0.10_amd64.deb) | `sudo apt install ./tscrt_1.0.10_amd64.deb` |
| **macOS (Universal)** | [tscrt_mac-1.0.10-universal.dmg](https://github.com/nice1972/TSCRT/releases/download/v1.0.10/tscrt_mac-1.0.10-universal.dmg) | `tscrt_mac.app` in `/Applications` ziehen |

---

## Funktionen

### Sitzungen & Terminal
- **SSH**-Sitzungen auf Basis von libssh2 mit Keepalive, automatischer Wiederverbindung und **Host-Key-Verifizierung (TOFU)**
- **Serielle / RS-232**-Sitzungen für eingebettete Ziele
- Multi-Tab-Sitzungsverwaltung mit dauerhaften Profilen
- **Terminaltyp pro Sitzung** (xterm-256color, vt100 usw.) mit globalem Standard-Fallback
- **Geteilte Fenster** (horizontal/vertikal) pro Tab
- **Eingabe-Broadcast** an alle Panes eines Tabs (`Ctrl+Shift+B`)
- **Suchleiste** mit Regex und Groß-/Kleinschreibung über den gesamten Scrollback (`Ctrl+F`)
- Dauerhafte terminalweite Hervorhebung / **Mark**-Funktion
- Vollbildmodus (`F11`) — alle Shortcuts bleiben aktiv

### Mehrfenster
- **Tab in neues Fenster ablösen** — Rechtsklick auf einen Tab und „In neues Fenster ablösen" wählen, um ihn in eine separate TSCRT-Instanz zu verschieben, während die Verbindung aktiv bleibt
- **Tabs zwischen Fenstern ziehen** — einen Tab aus der Tab-Leiste eines Fensters ziehen und auf einem anderen ablegen
- Tab-Kontextmenü: Umbenennen, Duplizieren, Anheften, Snapshot ausführen, Ablösen, Schließen

### Automatisierung
- **Automatisierungs-Engine** — Startaktionen, musterbasierte Aktionen (ratenlimitiert), periodische Aufgaben
- **Snapshots** — Shell-Befehle beim Verbinden, bei Muster-Treffern oder nach Zeitplan ausführen und die Ausgabe erfassen
- **Cron-Scheduler** — 5-Feld-Cron-Ausdrücke für Snapshot-Timing
- **Button-Leiste** — pro Sitzung anpassbare Schnellaktionsschaltflächen
- **Befehlszeilen-Widget** für Inline-Aktionen / Skript-Ausführung

### Sicherheit
- **SSH-Host-Key-Verifizierung** — TOFU-Modell mit `known_hosts`-Datei; abweichende Schlüssel werden strikt abgelehnt
- Zugangsdaten ruhend verschlüsselt: **DPAPI** (Windows), **Keychain** (macOS), Klartext-Fallback (Linux)
- Windows-Profil durch **ACL mit exklusivem Besitzerzugriff** geschützt
- SMTP: **TLS-Zertifikatsprüfung erzwungen**, Klartext-AUTH abgelehnt, Header-Injection blockiert
- Crash-Dumps: Nur-Besitzer-Berechtigungen, automatische Löschung nach 7 Tagen
- Automatisierungs-Trigger: pro Muster und global ratenlimitiert
- Wiederverbindung: mindestens 250 ms Basisverzögerung, harte Obergrenze von 100 Versuchen

### Logging & Zuverlässigkeit
- Sitzungslogger pro Pane mit eigenem Log-Einstellungsdialog
- **SMTP-Client** für E-Mail-Benachrichtigungen (STARTTLS/SMTPS für Authentifizierung erforderlich)
- **Crash-Handler** mit Unterstützung für Debug-Symbole
- Automatische Wiederverbindung mit exponentiellem Backoff und Jitter

### Internationalisierung
- Eingebaute UI-Übersetzungen: **English**, **한국어**, **日本語**, **中文**, **Deutsch**, **Español**, **Français**, **Italiano**, **Português**, **Русский**
- In-App-HTML-Hilfe pro Sprache

---

## Anforderungen

| Komponente | Version |
|-----------|---------|
| CMake     | >= 3.20  |
| C / C++   | C11 / C++17 |
| Qt        | >= 6.5 (Core, Gui, Widgets, Network, LinguistTools, Test) |
| libssh2   | aktuell  |
| libvterm  | aktuell  |
| pkg-config | erforderlich |

**Windows**: MSYS2 UCRT64 Toolchain (gcc 15+).
**macOS**: 12+ mit Homebrew.

---

## Build — Windows (MSYS2 UCRT64)

```bash
pacman -S --needed \
  mingw-w64-ucrt-x86_64-{gcc,cmake,ninja,pkgconf,qt6-base,qt6-tools,libssh2,libvterm}

source env.sh
./build.sh              # inkrementell (für vollständigen Rebuild `clean` verwenden)
./install.sh            # Exe + Qt-DLLs bereitstellen
./install.sh package    # NSIS-Installer
```

## Build — macOS

Siehe [`mac_build.txt`](mac_build.txt).

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

Suiten: `test_action_parser`, `test_cron`, `test_profile`.

---

## Projektstruktur

```
core/         portables C — Profil-E/A, Plattformabstraktion
gui/          Qt/C++ — Hauptfenster, Sitzungen, Dialoge, Automatisierung, Snapshots
include/      öffentliche Header, gemeinsam genutzt von core und gui
resources/    Icon, qrc, NSIS .rc, HTML-Hilfe
translations/ Qt .ts-Dateien (en/ko/ja/zh/de/es/fr/it/pt/ru)
tests/        Qt-Test-Unit-Tests
```

---

## Lizenz

Copyright (c) 2026 TePSEG Co., Ltd. — siehe [LICENSE.txt](LICENSE.txt).

Drittanbieter: Qt 6 (LGPL/GPL), libssh2 (BSD-3), libvterm (MIT), OpenSSL (Apache 2.0), zlib, MinGW-Runtime, ICU, HarfBuzz, FreeType, libpng, libjpeg.

---

## Kontakt

- Entwickler: ygjeon@tepseg.com
