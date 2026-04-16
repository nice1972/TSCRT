# TSCRT

**Windows / Linux / macOS で動作します。**

**TSCRT** は Qt 6、libssh2、libvterm で構築されたクロスプラットフォームの SSH / シリアルコンソール端末エミュレータです。組み込み機器、ネットワーク機器、リモートサーバーを扱うために、信頼性が高くスクリプト可能なマルチセッション コンソールを必要とするエンジニアを対象としています。

[English](README.md) | [한국어](README.ko.md) | **日本語** | [中文](README.zh.md) | [Español](README.es.md) | [Français](README.fr.md) | [Deutsch](README.de.md) | [Português](README.pt.md) | [Русский](README.ru.md) | [Italiano](README.it.md)

![TSCRT 分割ペイン SSH セッション](docs/images/tscrt-split-panes.png)

---

## ダウンロード

ビルド済みバイナリは [GitHub Releases](https://github.com/nice1972/TSCRT/releases/latest) で公開しています。

| プラットフォーム | ファイル | 備考 |
|------------------|----------|------|
| **Windows x64** | [tscrt_win-1.0.8-win64.exe](https://github.com/nice1972/TSCRT/releases/download/v1.0.8/tscrt_win-1.0.8-win64.exe) | NSIS インストーラー |
| **Ubuntu / Debian (amd64)** | [tscrt_1.0.8_amd64.deb](https://github.com/nice1972/TSCRT/releases/download/v1.0.8/tscrt_1.0.8_amd64.deb) | `sudo apt install ./tscrt_1.0.8_amd64.deb` |
| **macOS (Universal)** | [tscrt_mac-1.0.8-universal.dmg](https://github.com/nice1972/TSCRT/releases/download/v1.0.8/tscrt_mac-1.0.8-universal.dmg) | `tscrt_mac.app` を `/Applications` にドラッグ |

---

## 機能

### セッション & 端末
- libssh2 ベースの **SSH** セッション — keepalive、自動再接続、**ホスト鍵検証 (TOFU)**
- 組み込みターゲット向けの **シリアル / RS-232** セッション
- 永続プロファイル付きマルチタブ セッション管理
- **セッションごとの端末タイプ**（xterm-256color、vt100 など）とグローバル既定値へのフォールバック
- タブごとの **分割ペイン**（水平/垂直）
- タブ内のすべてのペインへの **入力ブロードキャスト**（`Ctrl+Shift+B`）
- 全スクロールバックにわたる正規表現と大文字小文字の区別に対応した **検索バー**（`Ctrl+F`）
- 端末全体の永続ハイライト / **Mark** 機能
- 全画面モード（`F11`）— すべてのショートカットは有効なまま

### マルチウィンドウ
- **タブを新しいウィンドウに切り離す** — タブを右クリックして「新しいウィンドウに切り離す」を選ぶと、接続を維持したまま別の TSCRT インスタンスに移動します
- **ウィンドウ間でタブをドラッグ** — あるウィンドウのタブバーからタブをドラッグして、別のウィンドウにドロップします
- タブのコンテキストメニュー: 名前変更、複製、ピン留め、スナップショット実行、切り離し、閉じる

### 自動化
- **自動化エンジン** — 起動時アクション、パターン トリガー アクション（レート制限あり）、定期タスク
- **スナップショット** — 接続時、パターン発生時、スケジュールに従ってシェルコマンドを実行し出力を取得
- **Cron スケジューラ** — スナップショット タイミング用の 5 フィールド cron 式
- **ボタンバー** — セッションごとにカスタマイズ可能なクイックアクション ボタン
- **コマンドライン ウィジェット** でインライン アクション / スクリプト実行

### セキュリティ
- **SSH ホスト鍵検証** — `known_hosts` ファイルによる TOFU モデル。一致しない鍵はハード拒否
- 資格情報の保存時暗号化: **DPAPI**（Windows）、**Keychain**（macOS）、平文フォールバック（Linux）
- Windows プロファイルは **所有者のみ ACL** で保護
- SMTP: **TLS 証明書検証を強制**、平文 AUTH を拒否、ヘッダー インジェクションをブロック
- クラッシュ ダンプ: 所有者のみの権限、7 日後に自動削除
- 自動化トリガー: パターンごとおよびグローバルのレート制限
- 再接続: 最低 250 ms の基本遅延、最大 100 回の強制上限

### ロギング & 信頼性
- 専用ログ設定ダイアログを備えたペインごとのセッション ロガー
- メール通知用 **SMTP クライアント**（認証には STARTTLS/SMTPS が必須）
- デバッグ シンボル対応の **クラッシュ ハンドラ**
- 指数バックオフとジッター付きの自動再接続

### 国際化
- UI 翻訳内蔵: **English**、**한국어**、**日本語**、**中文**、**Deutsch**、**Español**、**Français**、**Italiano**、**Português**、**Русский**
- 言語ごとのアプリ内 HTML ヘルプ

---

## 要件

| コンポーネント | バージョン |
|-----------|---------|
| CMake     | >= 3.20  |
| C / C++   | C11 / C++17 |
| Qt        | >= 6.5 (Core, Gui, Widgets, Network, LinguistTools, Test) |
| libssh2   | 最新  |
| libvterm  | 最新  |
| pkg-config | 必須 |

**Windows**: MSYS2 UCRT64 ツールチェーン（gcc 15+）。
**macOS**: 12+ と Homebrew。

---

## ビルド — Windows (MSYS2 UCRT64)

```bash
pacman -S --needed \
  mingw-w64-ucrt-x86_64-{gcc,cmake,ninja,pkgconf,qt6-base,qt6-tools,libssh2,libvterm}

source env.sh
./build.sh              # 増分ビルド（完全再ビルドは `clean`）
./install.sh            # exe と Qt DLL をステージング
./install.sh package    # NSIS インストーラー
```

## ビルド — macOS

[`mac_build.txt`](mac_build.txt) を参照。

```bash
brew install qt@6 libssh2 libvterm pkg-config cmake ninja
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
cd build && cpack -G DragNDrop
```

---

## テスト

```bash
ctest --test-dir build --output-on-failure
```

スイート: `test_action_parser`、`test_cron`、`test_profile`。

---

## プロジェクト構成

```
core/         ポータブル C — プロファイル I/O、プラットフォーム抽象化
gui/          Qt/C++ — メインウィンドウ、セッション、ダイアログ、自動化、スナップショット
include/      core と gui で共有される公開ヘッダー
resources/    アイコン、qrc、NSIS .rc、HTML ヘルプ
translations/ Qt .ts ファイル (en/ko/ja/zh/de/es/fr/it/pt/ru)
tests/        Qt Test ユニットテスト
```

---

## ライセンス

Copyright (c) 2026 TePSEG Co., Ltd. — [LICENSE.txt](LICENSE.txt) を参照。

サードパーティ: Qt 6 (LGPL/GPL), libssh2 (BSD-3), libvterm (MIT), OpenSSL (Apache 2.0), zlib, MinGW runtime, ICU, HarfBuzz, FreeType, libpng, libjpeg。

---

## 連絡先

- 開発者: ygjeon@tepseg.com
