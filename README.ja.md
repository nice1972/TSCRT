# TSCRT

**TSCRT** は Qt 6、libssh2、libvterm で構築されたクロスプラットフォームの SSH / シリアルコンソール端末エミュレータです。組み込み機器、ネットワーク機器、リモートサーバーを扱うエンジニア向けに設計されています。

[English](README.md) | [한국어](README.ko.md) | [中文](README.zh.md) | [Español](README.es.md) | [Français](README.fr.md) | [Deutsch](README.de.md) | [Português](README.pt.md) | [Русский](README.ru.md) | [Italiano](README.it.md)

![TSCRT 分割ペイン SSH セッション](docs/images/tscrt-split-panes.png)

---

## 主な機能

- **SSH** セッション — keepalive、自動再接続、**ホスト鍵検証 (TOFU)**
- **シリアル / RS-232** セッション
- マルチタブ管理、**セッションごとの端末タイプ**設定
- **分割ペイン**（水平/垂直）、**入力ブロードキャスト**
- **タブの分離** — 新しい TSCRT ウィンドウへ移動（接続維持）
- **ウィンドウ間タブドラッグ**
- **スナップショット** + **Cron スケジューラ** + **自動化エンジン**
- **セキュリティ**: SSH ホスト鍵検証、DPAPI/Keychain 資格情報暗号化、SMTP TLS 強制、owner-only ACL
- UI 翻訳: English, 한국어, 日本語, 中文

## ビルド

詳細は [README.md](README.md) または [`mac_build.txt`](mac_build.txt) を参照。

```bash
source env.sh && ./build.sh        # Windows (MSYS2 UCRT64)
./install.sh package               # NSIS インストーラー
```

## ライセンス

Copyright (c) 2026 TePSEG Co., Ltd. — [LICENSE.txt](LICENSE.txt)

## 連絡先

- 開発者: ygjeon@tepseg.com
