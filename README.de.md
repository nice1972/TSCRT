# TSCRT

**TSCRT** ist ein plattformuebergreifender SSH- und serieller Konsolen-Terminalemulator, entwickelt mit Qt 6, libssh2 und libvterm. Fuer Ingenieure, die mit eingebetteten Geraeten, Netzwerkgeraeten und Remote-Servern arbeiten.

[English](README.md) | [한국어](README.ko.md) | [日本語](README.ja.md) | [中文](README.zh.md) | [Español](README.es.md) | [Français](README.fr.md) | [Português](README.pt.md) | [Русский](README.ru.md) | [Italiano](README.it.md)

![TSCRT Split-Pane SSH-Sitzung](docs/images/tscrt-split-panes.png)

---

## Hauptfunktionen

- **SSH**-Sitzungen — Keepalive, automatische Wiederverbindung, **Host-Key-Verifizierung (TOFU)**
- **Serielle / RS-232**-Sitzungen
- Multi-Tab-Verwaltung mit persistenten Profilen, **Terminaltyp pro Sitzung**
- **Geteilte Fenster** (horizontal/vertikal), **Eingabe-Broadcast**
- **Tab in neues Fenster trennen** (Verbindung bleibt bestehen), **Tabs zwischen Fenstern ziehen**
- **Snapshots** + **Cron-Planer** + **Automatisierungs-Engine**
- **Sicherheit**: SSH-Host-Key-Verifizierung, DPAPI/Keychain-Verschluesselung, SMTP-TLS erzwungen, Owner-only ACL
- UI-Uebersetzungen: English, 한국어, 日本語, 中文

## Build

Siehe [README.md](README.md) oder [`mac_build.txt`](mac_build.txt).

```bash
source env.sh && ./build.sh        # Windows (MSYS2 UCRT64)
./install.sh package               # NSIS-Installer
```

## Lizenz

Copyright (c) 2026 TePSEG Co., Ltd. — [LICENSE.txt](LICENSE.txt)

## Kontakt

- Entwickler: ygjeon@tepseg.com
