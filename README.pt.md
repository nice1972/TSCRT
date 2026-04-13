# TSCRT

**TSCRT** e um emulador de terminal SSH e console serial multiplataforma, desenvolvido com Qt 6, libssh2 e libvterm. Projetado para engenheiros que trabalham com dispositivos embarcados, equipamentos de rede e servidores remotos.

[English](README.md) | [한국어](README.ko.md) | [日本語](README.ja.md) | [中文](README.zh.md) | [Español](README.es.md) | [Français](README.fr.md) | [Deutsch](README.de.md) | [Русский](README.ru.md) | [Italiano](README.it.md)

![Sessao SSH com paineis divididos no TSCRT](docs/images/tscrt-split-panes.png)

---

## Principais funcionalidades

- Sessoes **SSH** — keepalive, reconexao automatica, **verificacao de chave de host (TOFU)**
- Sessoes **Serial / RS-232**
- Gerenciamento multi-aba com perfis persistentes, **tipo de terminal por sessao**
- **Paineis divididos** (horizontal/vertical), **broadcast de entrada**
- **Separar aba em nova janela** (conexao mantida), **arrastar entre janelas**
- **Snapshots** + **agendador Cron** + **motor de automacao**
- **Seguranca**: verificacao de chave SSH, criptografia DPAPI/Keychain, TLS SMTP obrigatorio, ACL exclusivo do proprietario
- Traducoes de UI: English, 한국어, 日本語, 中文

## Compilacao

Consulte [README.md](README.md) ou [`mac_build.txt`](mac_build.txt).

```bash
source env.sh && ./build.sh        # Windows (MSYS2 UCRT64)
./install.sh package               # Instalador NSIS
```

## Licenca

Copyright (c) 2026 TePSEG Co., Ltd. — [LICENSE.txt](LICENSE.txt)

## Contato

- Desenvolvedor: ygjeon@tepseg.com
