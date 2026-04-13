# TSCRT

**TSCRT** 是基于 Qt 6、libssh2 和 libvterm 构建的跨平台 SSH / 串口控制台终端仿真器，面向嵌入式设备、网络设备和远程服务器工程师。

[English](README.md) | [한국어](README.ko.md) | [日本語](README.ja.md) | [Español](README.es.md) | [Français](README.fr.md) | [Deutsch](README.de.md) | [Português](README.pt.md) | [Русский](README.ru.md) | [Italiano](README.it.md)

![TSCRT 分割窗格 SSH 会话](docs/images/tscrt-split-panes.png)

---

## 主要功能

- **SSH** 会话 — keepalive、自动重连、**主机密钥验证 (TOFU)**
- **串口 / RS-232** 会话
- 多标签页管理，**每会话终端类型**设置
- **分割窗格**（水平/垂直）、**输入广播**
- **标签页分离** — 移至新 TSCRT 窗口（保持连接）
- **窗口间标签页拖拽**
- **快照** + **Cron 调度器** + **自动化引擎**
- **安全**: SSH 主机密钥验证、DPAPI/Keychain 凭据加密、SMTP TLS 强制、owner-only ACL
- UI 翻译: English, 한국어, 日本語, 中文

## 构建

详情见 [README.md](README.md) 或 [`mac_build.txt`](mac_build.txt)。

```bash
source env.sh && ./build.sh        # Windows (MSYS2 UCRT64)
./install.sh package               # NSIS 安装包
```

## 许可证

Copyright (c) 2026 TePSEG Co., Ltd. — [LICENSE.txt](LICENSE.txt)

## 联系方式

- 开发者: ygjeon@tepseg.com
