# TSCRT

**TSCRT** e un emulatore di terminale SSH e console seriale multipiattaforma, sviluppato con Qt 6, libssh2 e libvterm. Progettato per ingegneri che lavorano con dispositivi embedded, apparati di rete e server remoti.

[English](README.md) | [한국어](README.ko.md) | [日本語](README.ja.md) | [中文](README.zh.md) | [Español](README.es.md) | [Français](README.fr.md) | [Deutsch](README.de.md) | [Português](README.pt.md) | [Русский](README.ru.md)

![Sessione SSH con pannelli divisi in TSCRT](docs/images/tscrt-split-panes.png)

---

## Funzionalita principali

- Sessioni **SSH** — keepalive, riconnessione automatica, **verifica chiave host (TOFU)**
- Sessioni **Seriale / RS-232**
- Gestione multi-tab con profili persistenti, **tipo di terminale per sessione**
- **Pannelli divisi** (orizzontale/verticale), **broadcast input**
- **Separare la scheda in una nuova finestra** (connessione mantenuta), **trascinare tra finestre**
- **Snapshot** + **schedulatore Cron** + **motore di automazione**
- **Sicurezza**: verifica chiave SSH, crittografia DPAPI/Keychain, TLS SMTP obbligatorio, ACL solo proprietario
- Traduzioni UI: English, 한국어, 日本語, 中文

## Compilazione

Vedi [README.md](README.md) o [`mac_build.txt`](mac_build.txt).

```bash
source env.sh && ./build.sh        # Windows (MSYS2 UCRT64)
./install.sh package               # Installatore NSIS
```

## Licenza

Copyright (c) 2026 TePSEG Co., Ltd. — [LICENSE.txt](LICENSE.txt)

## Contatto

- Sviluppatore: ygjeon@tepseg.com
