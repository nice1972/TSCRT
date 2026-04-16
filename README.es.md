# TSCRT

**Funciona en Windows, Linux y macOS.**

**TSCRT** es un emulador de terminal SSH y consola serie multiplataforma, desarrollado con Qt 6, libssh2 y libvterm. Está dirigido a ingenieros que necesitan una consola multisesión fiable y programable para trabajar con dispositivos embebidos, equipos de red y servidores remotos.

[English](README.md) | [한국어](README.ko.md) | [日本語](README.ja.md) | [中文](README.zh.md) | **Español** | [Français](README.fr.md) | [Deutsch](README.de.md) | [Português](README.pt.md) | [Русский](README.ru.md) | [Italiano](README.it.md)

![Sesión SSH con paneles divididos en TSCRT](docs/images/tscrt-split-panes.png)

---

## Descargar

Los binarios precompilados están publicados en [GitHub Releases](https://github.com/nice1972/TSCRT/releases/latest).

| Plataforma | Archivo | Notas |
|------------|---------|-------|
| **Windows x64** | [tscrt_win-1.0.8-win64.exe](https://github.com/nice1972/TSCRT/releases/download/v1.0.8/tscrt_win-1.0.8-win64.exe) | Instalador NSIS |
| **Ubuntu / Debian (amd64)** | [tscrt_1.0.8_amd64.deb](https://github.com/nice1972/TSCRT/releases/download/v1.0.8/tscrt_1.0.8_amd64.deb) | `sudo apt install ./tscrt_1.0.8_amd64.deb` |
| **macOS (Universal)** | [tscrt_mac-1.0.8-universal.dmg](https://github.com/nice1972/TSCRT/releases/download/v1.0.8/tscrt_mac-1.0.8-universal.dmg) | Arrastrar `tscrt_mac.app` a `/Applications` |

---

## Características

### Sesiones y terminal
- Sesiones **SSH** basadas en libssh2 con keepalive, reconexión automática y **verificación de clave de host (TOFU)**
- Sesiones **Serie / RS-232** para dispositivos embebidos
- Gestión multipestaña con perfiles persistentes
- **Tipo de terminal por sesión** (xterm-256color, vt100, etc.) con valor global por defecto
- **Paneles divididos** (horizontal/vertical) por pestaña
- **Difusión de entrada** a todos los paneles de una pestaña (`Ctrl+Shift+B`)
- **Barra de búsqueda** con regex y distinción de mayúsculas sobre todo el scrollback (`Ctrl+F`)
- Resaltado persistente de todo el terminal / función **Mark**
- Modo pantalla completa (`F11`) — todos los atajos siguen activos

### Multi-ventana
- **Separar pestaña a nueva ventana** — haga clic derecho en una pestaña y elija "Separar a nueva ventana" para moverla a una instancia TSCRT separada manteniendo viva la conexión
- **Arrastrar pestañas entre ventanas** — arrastre una pestaña desde la barra de una ventana y suéltela en otra
- Menú contextual de pestaña: Renombrar, Duplicar, Fijar, Ejecutar snapshot, Separar, Cerrar

### Automatización
- **Motor de automatización** — acciones al iniciar, acciones por patrón (con límite de frecuencia), tareas periódicas
- **Snapshots** — ejecute comandos de shell al conectar, por patrón o según calendario, y capture la salida
- **Programador Cron** — expresiones cron de 5 campos para la planificación de snapshots
- **Barra de botones** — botones de acción rápida personalizables por sesión
- **Widget de línea de comandos** para ejecución inline de acciones / scripts

### Seguridad
- **Verificación de clave SSH de host** — modelo TOFU con archivo `known_hosts`; las claves no coincidentes se rechazan con firmeza
- Credenciales cifradas en reposo: **DPAPI** (Windows), **Keychain** (macOS), texto plano de respaldo (Linux)
- Perfil de Windows protegido por **ACL de solo propietario**
- SMTP: **validación de certificado TLS obligatoria**, AUTH en texto plano rechazado, inyección de cabeceras bloqueada
- Volcados de fallo: permisos de solo propietario, eliminación automática tras 7 días
- Disparadores de automatización: limitación de frecuencia por patrón y global
- Reconexión: retardo base mínimo de 250 ms, tope duro de 100 intentos

### Registro y fiabilidad
- Registrador de sesión por panel con diálogo dedicado de configuración de registro
- **Cliente SMTP** para notificaciones por correo (STARTTLS/SMTPS obligatorio para autenticación)
- **Gestor de fallos** con soporte para símbolos de depuración
- Reconexión automática con retroceso exponencial y jitter

### Internacionalización
- Traducciones de UI integradas: **English**, **한국어**, **日本語**, **中文**, **Deutsch**, **Español**, **Français**, **Italiano**, **Português**, **Русский**
- Guías de ayuda HTML en la aplicación por idioma

---

## Requisitos

| Componente | Versión |
|-----------|---------|
| CMake     | >= 3.20  |
| C / C++   | C11 / C++17 |
| Qt        | >= 6.5 (Core, Gui, Widgets, Network, LinguistTools, Test) |
| libssh2   | última  |
| libvterm  | última  |
| pkg-config | obligatorio |

**Windows**: toolchain MSYS2 UCRT64 (gcc 15+).
**macOS**: 12+ con Homebrew.

---

## Build — Windows (MSYS2 UCRT64)

```bash
pacman -S --needed \
  mingw-w64-ucrt-x86_64-{gcc,cmake,ninja,pkgconf,qt6-base,qt6-tools,libssh2,libvterm}

source env.sh
./build.sh              # incremental (use `clean` para recompilación completa)
./install.sh            # prepara exe + DLLs de Qt
./install.sh package    # instalador NSIS
```

## Build — macOS

Consulte [`mac_build.txt`](mac_build.txt).

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

## Estructura del proyecto

```
core/         C portable — E/S de perfil, abstracción de plataforma
gui/          Qt/C++ — ventana principal, sesiones, diálogos, automatización, snapshots
include/      cabeceras públicas compartidas entre core y gui
resources/    icono, qrc, NSIS .rc, ayuda HTML
translations/ archivos Qt .ts (en/ko/ja/zh/de/es/fr/it/pt/ru)
tests/        tests unitarios de Qt Test
```

---

## Licencia

Copyright (c) 2026 TePSEG Co., Ltd. — véase [LICENSE.txt](LICENSE.txt).

Terceros: Qt 6 (LGPL/GPL), libssh2 (BSD-3), libvterm (MIT), OpenSSL (Apache 2.0), zlib, MinGW runtime, ICU, HarfBuzz, FreeType, libpng, libjpeg.

---

## Contacto

- Desarrollador: ygjeon@tepseg.com
