# TSCRT

**TSCRT** — кроссплатформенный эмулятор терминала SSH и последовательной консоли, построенный на Qt 6, libssh2 и libvterm. Предназначен для инженеров, работающих со встраиваемыми устройствами, сетевым оборудованием и удалёнными серверами.

[English](README.md) | [한국어](README.ko.md) | [日本語](README.ja.md) | [中文](README.zh.md) | [Español](README.es.md) | [Français](README.fr.md) | [Deutsch](README.de.md) | [Português](README.pt.md) | [Italiano](README.it.md)

![TSCRT: SSH-сессия с разделёнными панелями](docs/images/tscrt-split-panes.png)

---

## Основные возможности

- Сессии **SSH** — keepalive, автоматическое переподключение, **проверка ключа хоста (TOFU)**
- Сессии **Serial / RS-232**
- Управление вкладками с сохраняемыми профилями, **тип терминала для каждой сессии**
- **Разделённые панели** (горизонтально/вертикально), **трансляция ввода**
- **Отделение вкладки в новое окно** (соединение сохраняется), **перетаскивание между окнами**
- **Снимки (Snapshots)** + **планировщик Cron** + **движок автоматизации**
- **Безопасность**: проверка SSH-ключей, шифрование DPAPI/Keychain, обязательный TLS для SMTP, ACL только для владельца
- Переводы интерфейса: English, 한국어, 日本語, 中文

## Сборка

Подробности см. в [README.md](README.md) или [`mac_build.txt`](mac_build.txt).

```bash
source env.sh && ./build.sh        # Windows (MSYS2 UCRT64)
./install.sh package               # NSIS-установщик
```

## Лицензия

Copyright (c) 2026 TePSEG Co., Ltd. — [LICENSE.txt](LICENSE.txt)

## Контакт

- Разработчик: ygjeon@tepseg.com
