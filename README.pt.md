# TSCRT

**Funciona no Windows, Linux e macOS.**

**TSCRT** é um emulador de terminal SSH e console serial multiplataforma, desenvolvido com Qt 6, libssh2 e libvterm. É destinado a engenheiros que precisam de um console multi-sessão confiável e roteirizável para trabalhar com dispositivos embarcados, equipamentos de rede e servidores remotos.

[English](README.md) | [한국어](README.ko.md) | [日本語](README.ja.md) | [中文](README.zh.md) | [Español](README.es.md) | [Français](README.fr.md) | [Deutsch](README.de.md) | **Português** | [Русский](README.ru.md) | [Italiano](README.it.md)

![Sessão SSH com painéis divididos no TSCRT](docs/images/tscrt-split-panes.png)

---

## Recursos

### Sessões e terminal
- Sessões **SSH** via libssh2 com keepalive, reconexão automática e **verificação de chave de host (TOFU)**
- Sessões **Serial / RS-232** para alvos embarcados
- Gerenciamento de sessões multi-aba com perfis persistentes
- **Tipo de terminal por sessão** (xterm-256color, vt100, etc.) com valor padrão global
- **Painéis divididos** (horizontal/vertical) por aba
- **Broadcast de entrada** para todos os painéis de uma aba (`Ctrl+Shift+B`)
- **Barra de busca** com regex e sensível a maiúsculas/minúsculas em todo o scrollback (`Ctrl+F`)
- Destaque persistente em todo o terminal / recurso **Mark**
- Modo tela cheia (`F11`) — todos os atalhos permanecem ativos

### Multi-janela
- **Separar aba em nova janela** — clique com o botão direito em uma aba e escolha "Separar em nova janela" para movê-la a uma instância TSCRT separada mantendo a conexão ativa
- **Arrastar abas entre janelas** — arraste uma aba da barra de abas de uma janela e solte em outra
- Menu de contexto da aba: Renomear, Duplicar, Fixar, Executar snapshot, Separar, Fechar

### Automação
- **Motor de automação** — ações de inicialização, ações disparadas por padrão (com limite de taxa), tarefas periódicas
- **Snapshots** — execute comandos shell na conexão, por padrão ou conforme agendamento, capturando a saída
- **Agendador Cron** — expressões cron de 5 campos para agendar snapshots
- **Barra de botões** — botões de ação rápida personalizáveis por sessão
- **Widget de linha de comando** para execução inline de ações / scripts

### Segurança
- **Verificação de chave SSH de host** — modelo TOFU com arquivo `known_hosts`; chaves divergentes são rejeitadas estritamente
- Credenciais criptografadas em repouso: **DPAPI** (Windows), **Keychain** (macOS), fallback em texto puro (Linux)
- Perfil do Windows protegido por **ACL somente do proprietário**
- SMTP: **validação de certificado TLS obrigatória**, AUTH em texto puro rejeitado, injeção de cabeçalho bloqueada
- Dumps de falha: permissões somente do proprietário, exclusão automática após 7 dias
- Gatilhos de automação: limitação de taxa por padrão e global
- Reconexão: atraso base mínimo de 250 ms, limite máximo de 100 tentativas

### Logs e confiabilidade
- Logger de sessão por painel com diálogo dedicado de configurações de log
- **Cliente SMTP** para notificações por e-mail (STARTTLS/SMTPS obrigatório para autenticação)
- **Tratador de falhas** com suporte a símbolos de depuração
- Reconexão automática com backoff exponencial e jitter

### Internacionalização
- Traduções de UI embutidas: **English**, **한국어**, **日本語**, **中文**, **Deutsch**, **Español**, **Français**, **Italiano**, **Português**, **Русский**
- Guias de ajuda HTML integrados por idioma

---

## Requisitos

| Componente | Versão |
|-----------|---------|
| CMake     | >= 3.20  |
| C / C++   | C11 / C++17 |
| Qt        | >= 6.5 (Core, Gui, Widgets, Network, LinguistTools, Test) |
| libssh2   | mais recente  |
| libvterm  | mais recente  |
| pkg-config | obrigatório |

**Windows**: toolchain MSYS2 UCRT64 (gcc 15+).
**macOS**: 12+ com Homebrew.

---

## Build — Windows (MSYS2 UCRT64)

```bash
pacman -S --needed \
  mingw-w64-ucrt-x86_64-{gcc,cmake,ninja,pkgconf,qt6-base,qt6-tools,libssh2,libvterm}

source env.sh
./build.sh              # incremental (use `clean` para rebuild completo)
./install.sh            # prepara exe + DLLs do Qt
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

## Testes

```bash
ctest --test-dir build --output-on-failure
```

Suítes: `test_action_parser`, `test_cron`, `test_profile`.

---

## Estrutura do projeto

```
core/         C portável — E/S de perfil, abstração de plataforma
gui/          Qt/C++ — janela principal, sessões, diálogos, automação, snapshots
include/      cabeçalhos públicos compartilhados entre core e gui
resources/    ícone, qrc, NSIS .rc, ajuda HTML
translations/ arquivos Qt .ts (en/ko/ja/zh/de/es/fr/it/pt/ru)
tests/        testes unitários Qt Test
```

---

## Licença

Copyright (c) 2026 TePSEG Co., Ltd. — veja [LICENSE.txt](LICENSE.txt).

Terceiros: Qt 6 (LGPL/GPL), libssh2 (BSD-3), libvterm (MIT), OpenSSL (Apache 2.0), zlib, runtime MinGW, ICU, HarfBuzz, FreeType, libpng, libjpeg.

---

## Contato

- Desenvolvedor: ygjeon@tepseg.com
