# TSCRT

**Funziona su Windows, Linux e macOS.**

**TSCRT** è un emulatore di terminale SSH e console seriale multipiattaforma, sviluppato con Qt 6, libssh2 e libvterm. È rivolto agli ingegneri che hanno bisogno di una console multi-sessione affidabile e scriptabile per lavorare con dispositivi embedded, apparati di rete e server remoti.

[English](README.md) | [한국어](README.ko.md) | [日本語](README.ja.md) | [中文](README.zh.md) | [Español](README.es.md) | [Français](README.fr.md) | [Deutsch](README.de.md) | [Português](README.pt.md) | [Русский](README.ru.md) | **Italiano**

![Sessione SSH con pannelli divisi in TSCRT](docs/images/tscrt-split-panes.png)

---

## Download

I binari precompilati sono pubblicati su [GitHub Releases](https://github.com/nice1972/TSCRT/releases/latest).

| Piattaforma | File | Note |
|-------------|------|------|
| **Windows x64** | [tscrt_win-1.0.10-win64.exe](https://github.com/nice1972/TSCRT/releases/download/v1.0.10/tscrt_win-1.0.10-win64.exe) | Programma di installazione NSIS |
| **Ubuntu / Debian (amd64)** | [tscrt_1.0.10_amd64.deb](https://github.com/nice1972/TSCRT/releases/download/v1.0.10/tscrt_1.0.10_amd64.deb) | `sudo apt install ./tscrt_1.0.10_amd64.deb` |
| **macOS (Universal)** | [tscrt_mac-1.0.10-universal.dmg](https://github.com/nice1972/TSCRT/releases/download/v1.0.10/tscrt_mac-1.0.10-universal.dmg) | Trascinare `tscrt_mac.app` in `/Applications` |

---

## Funzionalità

### Sessioni e terminale
- Sessioni **SSH** basate su libssh2 con keepalive, riconnessione automatica e **verifica della chiave host (TOFU)**
- Sessioni **Seriale / RS-232** per target embedded
- Gestione multi-scheda con profili persistenti
- **Tipo di terminale per sessione** (xterm-256color, vt100, ecc.) con fallback globale predefinito
- **Pannelli divisi** (orizzontale/verticale) per scheda
- **Broadcast di input** a tutti i pannelli di una scheda (`Ctrl+Shift+B`)
- **Barra di ricerca** con regex e sensibilità al maiuscolo/minuscolo su tutto lo scrollback (`Ctrl+F`)
- Evidenziazione persistente a livello di terminale / funzione **Mark**
- Modalità schermo intero (`F11`) — tutte le scorciatoie restano attive

### Multi-finestra
- **Stacca scheda in una nuova finestra** — clic destro su una scheda e scegliere "Stacca in nuova finestra" per spostarla in un'istanza TSCRT separata mantenendo attiva la connessione
- **Trascinare schede tra finestre** — trascinare una scheda dalla barra di una finestra e rilasciarla su un'altra
- Menu contestuale della scheda: Rinomina, Duplica, Appunta, Esegui snapshot, Stacca, Chiudi

### Automazione
- **Motore di automazione** — azioni all'avvio, azioni innescate da pattern (con limite di frequenza), attività periodiche
- **Snapshot** — eseguire comandi shell alla connessione, su pattern o secondo una pianificazione, catturando l'output
- **Schedulatore Cron** — espressioni cron a 5 campi per la tempistica degli snapshot
- **Barra pulsanti** — pulsanti di azione rapida personalizzabili per sessione
- **Widget riga di comando** per esecuzione inline di azioni / script

### Sicurezza
- **Verifica chiave host SSH** — modello TOFU con file `known_hosts`; le chiavi non corrispondenti vengono rifiutate rigidamente
- Credenziali cifrate a riposo: **DPAPI** (Windows), **Keychain** (macOS), fallback in chiaro (Linux)
- Profilo Windows protetto da **ACL solo proprietario**
- SMTP: **validazione del certificato TLS obbligatoria**, AUTH in chiaro rifiutato, injection di header bloccata
- Dump di crash: permessi solo per il proprietario, eliminazione automatica dopo 7 giorni
- Trigger di automazione: limitazione di frequenza per pattern e globale
- Riconnessione: ritardo base minimo di 250 ms, limite assoluto di 100 tentativi

### Logging e affidabilità
- Logger di sessione per pannello con dialogo dedicato alle impostazioni di log
- **Client SMTP** per notifiche email (STARTTLS/SMTPS richiesti per l'autenticazione)
- **Gestore di crash** con supporto ai simboli di debug
- Riconnessione automatica con backoff esponenziale e jitter

### Internazionalizzazione
- Traduzioni UI integrate: **English**, **한국어**, **日本語**, **中文**, **Deutsch**, **Español**, **Français**, **Italiano**, **Português**, **Русский**
- Guide HTML in-app per ogni lingua

---

## Requisiti

| Componente | Versione |
|-----------|---------|
| CMake     | >= 3.20  |
| C / C++   | C11 / C++17 |
| Qt        | >= 6.5 (Core, Gui, Widgets, Network, LinguistTools, Test) |
| libssh2   | ultima  |
| libvterm  | ultima  |
| pkg-config | obbligatorio |

**Windows**: toolchain MSYS2 UCRT64 (gcc 15+).
**macOS**: 12+ con Homebrew.

---

## Build — Windows (MSYS2 UCRT64)

```bash
pacman -S --needed \
  mingw-w64-ucrt-x86_64-{gcc,cmake,ninja,pkgconf,qt6-base,qt6-tools,libssh2,libvterm}

source env.sh
./build.sh              # incrementale (usare `clean` per rebuild completo)
./install.sh            # prepara exe + DLL di Qt
./install.sh package    # installer NSIS
```

## Build — macOS

Vedere [`mac_build.txt`](mac_build.txt).

```bash
brew install qt@6 libssh2 libvterm pkg-config cmake ninja
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
cd build && cpack -G DragNDrop
```

---

## Test

```bash
ctest --test-dir build --output-on-failure
```

Suite: `test_action_parser`, `test_cron`, `test_profile`.

---

## Struttura del progetto

```
core/         C portabile — I/O profili, astrazione piattaforma
gui/          Qt/C++ — finestra principale, sessioni, dialoghi, automazione, snapshot
include/      header pubblici condivisi tra core e gui
resources/    icona, qrc, NSIS .rc, aiuto HTML
translations/ file Qt .ts (en/ko/ja/zh/de/es/fr/it/pt/ru)
tests/        test unitari Qt Test
```

---

## Licenza

Copyright (c) 2026 TePSEG Co., Ltd. — vedere [LICENSE.txt](LICENSE.txt).

Terze parti: Qt 6 (LGPL/GPL), libssh2 (BSD-3), libvterm (MIT), OpenSSL (Apache 2.0), zlib, MinGW runtime, ICU, HarfBuzz, FreeType, libpng, libjpeg.

---

## Contatti

- Sviluppatore: ygjeon@tepseg.com
