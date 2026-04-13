# TSCRT

**TSCRT** es un emulador de terminal SSH y consola serie multiplataforma, desarrollado con Qt 6, libssh2 y libvterm. Dirigido a ingenieros que trabajan con dispositivos embebidos, equipos de red y servidores remotos.

[English](README.md) | [한국어](README.ko.md) | [日本語](README.ja.md) | [中文](README.zh.md) | [Français](README.fr.md) | [Deutsch](README.de.md) | [Português](README.pt.md) | [Русский](README.ru.md) | [Italiano](README.it.md)

![Sesion SSH con paneles divididos en TSCRT](docs/images/tscrt-split-panes.png)

---

## Funciones principales

- Sesiones **SSH** — keepalive, reconexion automatica, **verificacion de clave de host (TOFU)**
- Sesiones **Serie / RS-232**
- Gestion multitab con perfiles persistentes, **tipo de terminal por sesion**
- **Paneles divididos** (horizontal/vertical), **difusion de entrada**
- **Separar pestana en nueva ventana** (conexion activa), **arrastrar entre ventanas**
- **Instantaneas (Snapshots)** + **programador Cron** + **motor de automatizacion**
- **Seguridad**: verificacion de clave SSH, cifrado DPAPI/Keychain, TLS SMTP forzado, ACL de solo propietario
- Traducciones de UI: English, 한국어, 日本語, 中文

## Compilacion

Consulte [README.md](README.md) o [`mac_build.txt`](mac_build.txt) para instrucciones detalladas.

```bash
source env.sh && ./build.sh        # Windows (MSYS2 UCRT64)
./install.sh package               # Instalador NSIS
```

## Licencia

Copyright (c) 2026 TePSEG Co., Ltd. — [LICENSE.txt](LICENSE.txt)

## Contacto

- Desarrollador: ygjeon@tepseg.com
