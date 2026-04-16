#!/usr/bin/env bash
# release-win.sh — Windows 배포 빌드 + GitHub Release 등록.
#
# 사용법 (MSYS2 UCRT64 에서 실행):
#   source env.sh
#   ./release-win.sh 1.0.10           # 빌드 + 릴리스
#   ./release-win.sh 1.0.10 upload    # 기존 릴리스에 파일만 추가
#
# 전제: MSYS2 UCRT64 환경 (source env.sh 완료)
#       gh auth login (GitHub CLI 인증 — 없으면 업로드 건너뜀)
set -euo pipefail

if [[ $# -lt 1 ]]; then
    echo "Usage: $0 <version> [upload]"
    exit 1
fi

VERSION="$1"
MODE="${2:-full}"
TAG="v${VERSION}"
EXE_NAME="tscrt_win-${VERSION}-win64.exe"

cd "$(dirname "$0")"

# ---- 1. 버전 치환 -----------------------------------------------------------
if [[ "$MODE" != "upload" ]]; then

echo "=== 버전 → ${VERSION} ==="

# include/tscrt.h
sed -i "s/#define TSCRT_VERSION .*\".*\"/#define TSCRT_VERSION        \"${VERSION}\"/" include/tscrt.h

# CMakeLists.txt — 버전 문자열만 치환 (따옴표 내부)
sed -i "s/\"[0-9]\+\.[0-9]\+\.[0-9]\+\"/\"${VERSION}\"/g" CMakeLists.txt

# resources/tscrt_win.rc
V4="${VERSION}.0"
VNUM=$(echo "${VERSION}" | sed 's/\./, /g')", 0"
sed -i "s/#define TSCRT_VERSION_STR  \".*\"/#define TSCRT_VERSION_STR  \"${V4}\"/" resources/tscrt_win.rc
sed -i "s/#define TSCRT_VERSION_NUM  .*/#define TSCRT_VERSION_NUM  ${VNUM}/" resources/tscrt_win.rc

# README 다운로드 링크 (모든 언어)
for f in README*.md; do
    sed -i \
        -e "s|/download/v[0-9]\+\.[0-9]\+\.[0-9]\+/|/download/${TAG}/|g" \
        -e "s|tscrt_[0-9]\+\.[0-9]\+\.[0-9]\+_amd64\.deb|tscrt_${VERSION}_amd64.deb|g" \
        -e "s|tscrt_win-[0-9]\+\.[0-9]\+\.[0-9]\+-win64\.exe|${EXE_NAME}|g" \
        -e "s|tscrt_mac-[0-9]\+\.[0-9]\+\.[0-9]\+-universal\.dmg|tscrt_mac-${VERSION}-universal.dmg|g" \
        "$f"
done

# ---- 2. 빌드 ----------------------------------------------------------------
echo "=== 클린 빌드 ==="
rm -rf build

# 실행 중인 exe 가 있으면 링크 실패하므로 종료
if tasklist //FI "IMAGENAME eq tscrt_win.exe" 2>/dev/null | grep -qi tscrt_win.exe; then
    echo "tscrt_win.exe 실행 중 — 종료합니다."
    taskkill //F //IM tscrt_win.exe //T >/dev/null 2>&1 || true
fi

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build

echo "=== 테스트 ==="
ctest --test-dir build --output-on-failure

echo "=== NSIS 설치 관리자 생성 ==="
cmake --install build --prefix "$(pwd)/build/install"
( cd build && cpack -G NSIS )

# CPack 이 생성한 파일명이 다를 수 있으므로 정규화
if [[ ! -f "build/${EXE_NAME}" ]]; then
    for g in build/tscrt_win-*.exe; do
        [[ -f "$g" ]] && mv "$g" "build/${EXE_NAME}"
        break
    done
fi

echo "결과: build/${EXE_NAME}"

fi  # MODE != upload

# ---- 3. GitHub Release -------------------------------------------------------
EXE_PATH="build/${EXE_NAME}"

if ! command -v gh &>/dev/null; then
    echo "⚠ gh (GitHub CLI) 가 설치되어 있지 않아 릴리스 업로드를 건너뜁니다."
    echo "  수동 업로드: gh release create ${TAG} \"${EXE_PATH}\" --title \"TSCRT ${VERSION}\""
else
    if gh release view "${TAG}" &>/dev/null; then
        echo "=== 릴리스 ${TAG} 이미 존재 — 파일 업로드만 수행 ==="
        gh release upload "${TAG}" "${EXE_PATH}" --clobber
    else
        echo "=== 릴리스 ${TAG} 생성 + 업로드 ==="
        gh release create "${TAG}" "${EXE_PATH}" --title "TSCRT ${VERSION}" --notes "TSCRT ${VERSION} release binaries."
    fi
    echo "=== 릴리스 URL: https://github.com/nice1972/TSCRT/releases/tag/${TAG} ==="
fi

echo
echo "완료. git add/commit/push 는 수동으로 진행하세요."
