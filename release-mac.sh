#!/usr/bin/env bash
# release-mac.sh — macOS 배포 빌드 + GitHub Release 등록.
#
# 사용법 (macOS 에서 실행):
#   ./release-mac.sh 1.0.8           # 빌드 + 릴리스 업로드
#   ./release-mac.sh 1.0.8 upload    # 기존 릴리스에 파일만 추가
#
# 전제: brew install qt@6 libssh2 libvterm pkg-config cmake ninja
#       gh auth login (GitHub CLI 인증)
set -euo pipefail
cd "$(dirname "$0")"

VERSION="${1:?Usage: $0 <version> [upload]}"
MODE="${2:-full}"
TAG="v${VERSION}"

DMG_NAME="tscrt_mac-${VERSION}-universal.dmg"

# Homebrew Qt 경로 (Apple Silicon / Intel 양쪽 대응)
QT_PREFIX="$(brew --prefix qt@6 2>/dev/null || echo /usr/local/opt/qt@6)"
export PATH="${QT_PREFIX}/bin:$PATH"
export PKG_CONFIG_PATH="$(brew --prefix libssh2)/lib/pkgconfig:$(brew --prefix libvterm)/lib/pkgconfig"
export CMAKE_PREFIX_PATH="${QT_PREFIX}"

# ---- 1. 버전 치환 -----------------------------------------------------------
bump_version() {
    echo ">>> 버전 → ${VERSION}"

    sed -i '' "s/#define TSCRT_VERSION .*/#define TSCRT_VERSION        \"${VERSION}\"/" include/tscrt.h
    sed -i '' -E "s/\"[0-9]+\.[0-9]+\.[0-9]+\"/\"${VERSION}\"/g" CMakeLists.txt

    local V4="${VERSION}.0"
    local VNUM="${VERSION//./, },0"
    sed -i '' "s/#define TSCRT_VERSION_STR .*/#define TSCRT_VERSION_STR  \"${V4}\"/" resources/tscrt_win.rc
    sed -i '' "s/#define TSCRT_VERSION_NUM .*/#define TSCRT_VERSION_NUM  ${VNUM}/" resources/tscrt_win.rc

    for f in README*.md; do
        sed -i '' -E "s|/download/v[0-9]+\.[0-9]+\.[0-9]+/|/download/${TAG}/|g" "$f"
        sed -i '' -E "s|tscrt_[0-9]+\.[0-9]+\.[0-9]+_amd64\.deb|tscrt_${VERSION}_amd64.deb|g" "$f"
        sed -i '' -E "s|tscrt_win-[0-9]+\.[0-9]+\.[0-9]+-win64\.exe|tscrt_win-${VERSION}-win64.exe|g" "$f"
        sed -i '' -E "s|tscrt_mac-[0-9]+\.[0-9]+\.[0-9]+-universal\.dmg|${DMG_NAME}|g" "$f"
    done
}

# ---- 2. 빌드 ----------------------------------------------------------------
build() {
    echo ">>> 클린 빌드"
    rm -rf build
    cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
    cmake --build build

    echo ">>> 테스트"
    ctest --test-dir build --output-on-failure

    echo ">>> DMG 패키지 생성"
    cmake --install build --prefix build/install
    (cd build && cpack -G DragNDrop)

    # DMG 이름 정규화 (CPack 이 자동 생성한 이름이 다를 수 있음)
    local GENERATED
    GENERATED="$(ls build/tscrt_mac-*.dmg 2>/dev/null | head -1)"
    if [[ -n "$GENERATED" && "$GENERATED" != "build/${DMG_NAME}" ]]; then
        mv "$GENERATED" "build/${DMG_NAME}"
    fi
    ls -lh "build/${DMG_NAME}"
}

# ---- 3. GitHub Release -------------------------------------------------------
gh_release() {
    local DMG_PATH="build/${DMG_NAME}"

    if gh release view "${TAG}" >/dev/null 2>&1; then
        echo ">>> 릴리스 ${TAG} 이미 존재 — 파일 업로드만 수행"
        gh release upload "${TAG}" "${DMG_PATH}" --clobber
    else
        echo ">>> 릴리스 ${TAG} 생성 + 업로드"
        gh release create "${TAG}" "${DMG_PATH}" \
            --title "TSCRT ${VERSION}" \
            --notes "TSCRT ${VERSION} release binaries."
    fi
    echo ">>> 릴리스 URL: https://github.com/nice1972/TSCRT/releases/tag/${TAG}"
}

# ---- 실행 -------------------------------------------------------------------
if [[ "$MODE" == "upload" ]]; then
    gh_release
else
    bump_version
    build
    gh_release
    echo
    echo "완료. git add/commit/push 는 수동으로 진행하세요."
fi
