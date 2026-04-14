# TSCRT

**可在 Windows、Linux 和 macOS 上运行。**

**TSCRT** 是基于 Qt 6、libssh2 和 libvterm 构建的跨平台 SSH / 串口控制台终端仿真器。面向需要可靠、可脚本化的多会话控制台，用于处理嵌入式设备、网络设备和远程服务器的工程师。

[English](README.md) | [한국어](README.ko.md) | [日本語](README.ja.md) | **中文** | [Español](README.es.md) | [Français](README.fr.md) | [Deutsch](README.de.md) | [Português](README.pt.md) | [Русский](README.ru.md) | [Italiano](README.it.md)

![TSCRT 分割窗格 SSH 会话](docs/images/tscrt-split-panes.png)

---

## 功能

### 会话与终端
- 基于 libssh2 的 **SSH** 会话，支持 keepalive、自动重连和**主机密钥验证 (TOFU)**
- 面向嵌入式目标的**串口 / RS-232** 会话
- 带持久化配置的多标签页会话管理
- **每会话终端类型**（xterm-256color、vt100 等），并支持全局默认回退
- 每个标签页支持**分割窗格**（水平/垂直）
- 向标签页中所有窗格**广播输入**（`Ctrl+Shift+B`）
- 对整个滚动回看内容支持正则和区分大小写的**搜索栏**（`Ctrl+F`）
- 终端范围持久高亮 / **Mark** 功能
- 全屏模式（`F11`）— 所有快捷键保持可用

### 多窗口
- **分离标签页到新窗口** — 在标签页上右键选择"分离到新窗口"，在保持连接的情况下将其移至独立的 TSCRT 实例
- **在窗口间拖拽标签页** — 从一个窗口的标签栏拖拽标签页并放入另一个窗口
- 标签页上下文菜单：重命名、复制、固定、运行快照、分离、关闭

### 自动化
- **自动化引擎** — 启动动作、模式触发动作（带频率限制）、定时任务
- **快照 (Snapshots)** — 在连接时、匹配到模式时或按计划执行 shell 命令并捕获输出
- **Cron 调度器** — 用于快照定时的 5 字段 cron 表达式
- **按钮栏** — 每个会话可自定义的快速操作按钮
- **命令行部件** — 用于内联执行动作 / 脚本

### 安全
- **SSH 主机密钥验证** — 基于 `known_hosts` 文件的 TOFU 模型；不匹配的密钥会被严格拒绝
- 凭据静态加密：**DPAPI**（Windows）、**Keychain**（macOS）、明文回退（Linux）
- Windows 配置文件受**仅所有者 ACL** 保护
- SMTP：**强制 TLS 证书验证**，拒绝明文 AUTH，阻止头部注入
- 崩溃转储：仅所有者权限，7 天后自动删除
- 自动化触发器：按模式和全局频率限制
- 重连：最小基准延迟 250 ms，硬性上限 100 次

### 日志与可靠性
- 每窗格会话日志器，配有专用日志设置对话框
- 用于邮件通知的 **SMTP 客户端**（认证需要 STARTTLS/SMTPS）
- 支持调试符号的**崩溃处理器**
- 带指数退避和抖动的自动重连

### 国际化
- 内置 UI 翻译：**English**、**한국어**、**日本語**、**中文**、**Deutsch**、**Español**、**Français**、**Italiano**、**Português**、**Русский**
- 每种语言的应用内 HTML 帮助

---

## 要求

| 组件 | 版本 |
|-----------|---------|
| CMake     | >= 3.20  |
| C / C++   | C11 / C++17 |
| Qt        | >= 6.5 (Core, Gui, Widgets, Network, LinguistTools, Test) |
| libssh2   | 最新  |
| libvterm  | 最新  |
| pkg-config | 必需 |

**Windows**：MSYS2 UCRT64 工具链（gcc 15+）。
**macOS**：12+，配合 Homebrew。

---

## 构建 — Windows (MSYS2 UCRT64)

```bash
pacman -S --needed \
  mingw-w64-ucrt-x86_64-{gcc,cmake,ninja,pkgconf,qt6-base,qt6-tools,libssh2,libvterm}

source env.sh
./build.sh              # 增量（完整重建使用 `clean`）
./install.sh            # 暂存 exe 和 Qt DLL
./install.sh package    # NSIS 安装包
```

## 构建 — macOS

参见 [`mac_build.txt`](mac_build.txt)。

```bash
brew install qt@6 libssh2 libvterm pkg-config cmake ninja
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
cd build && cpack -G DragNDrop
```

---

## 测试

```bash
ctest --test-dir build --output-on-failure
```

套件：`test_action_parser`、`test_cron`、`test_profile`。

---

## 项目结构

```
core/         可移植 C — 配置 I/O、平台抽象
gui/          Qt/C++ — 主窗口、会话、对话框、自动化、快照
include/      core 和 gui 共享的公共头文件
resources/    图标、qrc、NSIS .rc、HTML 帮助
translations/ Qt .ts 文件 (en/ko/ja/zh/de/es/fr/it/pt/ru)
tests/        Qt Test 单元测试
```

---

## 许可证

Copyright (c) 2026 TePSEG Co., Ltd. — 参见 [LICENSE.txt](LICENSE.txt)。

第三方：Qt 6 (LGPL/GPL), libssh2 (BSD-3), libvterm (MIT), OpenSSL (Apache 2.0), zlib, MinGW runtime, ICU, HarfBuzz, FreeType, libpng, libjpeg。

---

## 联系方式

- 开发者：ygjeon@tepseg.com
