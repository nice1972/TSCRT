#!/usr/bin/env bash
# MSYS2 UCRT64에서 tscrt_win 빌드.
#   source env.sh && ./build.sh           # 증분 빌드
#   source env.sh && ./build.sh clean     # 클린 빌드
set -euo pipefail

cd "$(dirname "$0")"

if [[ "${1:-}" == "clean" ]]; then
    rm -rf build
fi

# 실행 중인 exe가 있으면 링크 단계가 Permission denied로 실패하므로 먼저 종료.
if tasklist //FI "IMAGENAME eq tscrt_win.exe" 2>/dev/null | grep -qi tscrt_win.exe; then
    echo "tscrt_win.exe 가 실행 중 — 종료합니다."
    taskkill //F //IM tscrt_win.exe //T >/dev/null 2>&1 || true
fi

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build

echo
echo "빌드 완료: build/tscrt_win.exe"
