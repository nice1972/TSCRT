# TSCRT

**TSCRT** 는 Qt 6, libssh2, libvterm 으로 구현한 크로스플랫폼 SSH / 시리얼 콘솔 터미널 에뮬레이터입니다. 임베디드 장비, 네트워크 장비, 원격 서버를 다루는 엔지니어를 위해 안정적이고 스크립트로 자동화할 수 있는 다중 세션 콘솔을 목표로 합니다.

[English README](README.md)

---

## 주요 기능

### 세션 / 터미널
- libssh2 기반 **SSH** 세션, keepalive 와 자동 재접속 지원
- 임베디드 타깃을 위한 **시리얼 / RS-232** 세션
- 영속 프로파일 기반 다중 탭 세션 관리
- 탭 단위 **분할창** (수평/수직)
- 탭 안의 모든 패인에 입력 동시 전송 — **브로드캐스트** (`Ctrl+Shift+B`)
- 전체 스크롤백 대상 정규식·대소문자 옵션 **검색바** (`Ctrl+F`)
- 터미널 전체에 유지되는 하이라이트 / **Mark** 기능
- 전체화면 (`F11`), 빠른 탭 이동 (`Ctrl+Alt+←/→`)

### 자동화
- **Automation Engine** — 시작 액션, 수신 바이트 패턴 트리거, 주기 실행
- **Snapshots** — 접속 시 / 패턴 발생 시 / 스케줄 시점에 셸 명령을 실행하고 결과 캡처
- **Cron 스케줄러** — 5필드 cron 표현식으로 스냅샷 시각 지정
- **Button bar** — 세션별 사용자 정의 단축 버튼
- 인라인 액션·스크립트 실행을 위한 **Command-line widget**

### 로깅 / 안정성
- 패인 단위 세션 로거 + 전용 로그 설정 다이얼로그
- 이메일 알림용 **SMTP 클라이언트**
- **크래시 핸들러** (Windows 의 경우 dbghelp 심볼 지원)
- 자동 재접속과 억제 옵션

### 보안
- 자격 증명은 Windows 에서 **DPAPI** 로 암호화 저장
- macOS 는 Security.framework 사용, Linux 는 평문 폴백 (마이그레이션 후 재입력 필요)

### 다국어
- 내장 UI 번역: **English**, **한국어**, **日本語**, **中文**
- 언어별 인앱 HTML 도움말

---

## 요구 사항

| 구성 요소 | 버전 |
|-----------|------|
| CMake     | ≥ 3.20 |
| C / C++   | C11 / C++17 |
| Qt        | ≥ 6.5 (Core, Gui, Widgets, Network, LinguistTools, Test) |
| libssh2   | 최신 |
| libvterm  | 최신 |
| pkg-config | 필수 |

**Windows**: MSYS2 UCRT64 툴체인 (gcc 15+).
**macOS**: 12 이상 + Homebrew.

---

## 빌드 — Windows (MSYS2 UCRT64)

의존성 설치:

```bash
pacman -S --needed \
  mingw-w64-ucrt-x86_64-gcc \
  mingw-w64-ucrt-x86_64-cmake \
  mingw-w64-ucrt-x86_64-ninja \
  mingw-w64-ucrt-x86_64-pkgconf \
  mingw-w64-ucrt-x86_64-qt6-base \
  mingw-w64-ucrt-x86_64-qt6-tools \
  mingw-w64-ucrt-x86_64-libssh2 \
  mingw-w64-ucrt-x86_64-libvterm
```

빌드 및 설치:

```bash
source env.sh           # MSYS2 UCRT64 PATH/PKG_CONFIG 설정
./build.sh              # 증분 Ninja 빌드 ( `clean` 인자로 클린 빌드 )
./install.sh            # build/install/ 에 tscrt_win.exe + Qt DLL 스테이징
./install.sh package    # NSIS 인스톨러까지 생성 (build/tscrt_win-*-win64.exe)
```

링크 단계 `Permission denied` 를 막기 위해 `build.sh` 는 실행 중인 `tscrt_win.exe` 를 자동으로 종료합니다.

---

## 빌드 — macOS

자세한 내용은 [`mac_build.txt`](mac_build.txt) 참고. 요약:

```bash
brew install qt@6 libssh2 libvterm pkg-config cmake ninja
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
cd build && cpack -G DragNDrop   # .dmg 생성
```

---

## 테스트

Qt Test 기반 단위 테스트는 기본으로 빌드됩니다:

```bash
ctest --test-dir build --output-on-failure
```

스위트: `test_action_parser`, `test_cron`, `test_profile`.

---

## 디렉터리 구조

```
core/         포터블 C — 프로파일 I/O, 플랫폼 추상화
gui/          Qt/C++ — 메인 윈도우, 세션, 다이얼로그, 자동화, 스냅샷
include/      core / gui 공용 헤더
resources/    아이콘, qrc, NSIS .rc, HTML 도움말, walk_deps.py
translations/ Qt .ts 파일 (en/ko/ja/zh)
tests/        Qt Test 단위 테스트
```

---

## 라이선스

Copyright © 2026 TePSEG Co., Ltd. — [LICENSE.txt](LICENSE.txt) 참고.

서드파티: Qt 6 (LGPL/GPL), libssh2 (BSD-3), libvterm (MIT), OpenSSL (Apache 2.0), zlib, MinGW runtime, ICU, HarfBuzz, FreeType, libpng, libjpeg.

---

## 연락처

- 개발자: ygjeon@tepseg.com
