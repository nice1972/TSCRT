#!/usr/bin/env bash
# tscrt_win 설치 + NSIS 인스톨러 생성.
#   source env.sh && ./install.sh             # build/install 디렉터리에 스테이징
#   source env.sh && ./install.sh package     # NSIS .exe 인스톨러까지 생성
set -euo pipefail

cd "$(dirname "$0")"

if [[ ! -f build/build.ninja ]]; then
    echo "build/ 가 없어서 먼저 빌드합니다."
    ./build.sh
fi

# windeployqt 가 Qt DLL/플러그인을 install prefix 에 스테이징.
INSTALL_PREFIX="$(pwd)/build/install"
cmake --install build --prefix "$INSTALL_PREFIX"

echo
echo "스테이징 완료: $INSTALL_PREFIX"

if [[ "${1:-}" == "package" ]]; then
    echo
    echo "NSIS 인스톨러 생성 중..."
    ( cd build && cpack -G NSIS )
    echo
    echo "인스톨러 생성 완료:"
    ls -1 build/tscrt_win-*-win64.exe 2>/dev/null || true
fi
