#!/usr/bin/env bash
# release-linux.sh — Linux 배포 빌드 + GitHub Release 등록.
#
# 사용법:
#   ./release-linux.sh 1.0.8           # 빌드 + 릴리스 생성
#   ./release-linux.sh 1.0.8 upload    # 기존 릴리스에 파일만 추가
#
# 전제: cmake, ninja, cpack, gh (GitHub CLI, 인증 완료), sed, git
set -euo pipefail
cd "$(dirname "$0")"

VERSION="${1:?Usage: $0 <version> [upload]}"
MODE="${2:-full}"
TAG="v${VERSION}"

DEB_NAME="tscrt_${VERSION}_amd64.deb"

# ---- 1. 버전 치환 -----------------------------------------------------------
bump_version() {
    echo ">>> 버전 → ${VERSION}"

    # include/tscrt.h
    sed -i "s/#define TSCRT_VERSION .*/#define TSCRT_VERSION        \"${VERSION}\"/" include/tscrt.h

    # CMakeLists.txt (CPACK_PACKAGE_VERSION, MACOSX_BUNDLE)
    sed -i -E "s/\"[0-9]+\.[0-9]+\.[0-9]+\"/\"${VERSION}\"/g" CMakeLists.txt

    # resources/tscrt_win.rc
    local V4="${VERSION}.0"
    local VNUM="${VERSION//./, },0"
    sed -i "s/#define TSCRT_VERSION_STR .*/#define TSCRT_VERSION_STR  \"${V4}\"/" resources/tscrt_win.rc
    sed -i "s/#define TSCRT_VERSION_NUM .*/#define TSCRT_VERSION_NUM  ${VNUM}/" resources/tscrt_win.rc

    # README 다운로드 링크 (모든 언어)
    for f in README*.md; do
        sed -i -E "s|/download/v[0-9]+\.[0-9]+\.[0-9]+/|/download/${TAG}/|g" "$f"
        sed -i -E "s|tscrt_[0-9]+\.[0-9]+\.[0-9]+_amd64\.deb|${DEB_NAME}|g" "$f"
        sed -i -E "s|tscrt_win-[0-9]+\.[0-9]+\.[0-9]+-win64\.exe|tscrt_win-${VERSION}-win64.exe|g" "$f"
        sed -i -E "s|tscrt_mac-[0-9]+\.[0-9]+\.[0-9]+-universal\.dmg|tscrt_mac-${VERSION}-universal.dmg|g" "$f"
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

    echo ">>> .deb 패키지 생성"
    (cd build && cpack -G DEB)
    ls -lh "build/${DEB_NAME}"
}

# ---- 3. GitHub Release -------------------------------------------------------
gh_release() {
    local DEB_PATH="build/${DEB_NAME}"

    if gh release view "${TAG}" >/dev/null 2>&1; then
        echo ">>> 릴리스 ${TAG} 이미 존재 — 파일 업로드만 수행"
        gh release upload "${TAG}" "${DEB_PATH}" --clobber
    else
        echo ">>> 릴리스 ${TAG} 생성 + 업로드"
        gh release create "${TAG}" "${DEB_PATH}" \
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
