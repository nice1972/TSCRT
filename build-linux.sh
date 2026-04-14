#!/usr/bin/env bash
# Ubuntu/Debian에서 tscrt_linux 빌드 및 .deb 패키지 생성.
#   ./build-linux.sh           # 증분 빌드 + deb
#   ./build-linux.sh clean     # 클린 빌드
#   ./build-linux.sh notest    # 테스트 스킵
set -euo pipefail

cd "$(dirname "$0")"

MODE="${1:-}"
if [[ "$MODE" == "clean" ]]; then
    rm -rf build
fi

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build

if [[ "$MODE" != "notest" ]]; then
    ctest --test-dir build --output-on-failure
fi

(cd build && cpack -G DEB)

echo
echo "빌드 완료: build/tscrt_linux"
echo "패키지:    $(ls -1 build/tscrt_*.deb 2>/dev/null | tail -1)"
