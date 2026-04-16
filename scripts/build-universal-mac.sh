#!/usr/bin/env bash
# Build a Universal (arm64 + x86_64) macOS DMG of TSCRT without a Qt account.
#
# Requirements (auto-installed via Homebrew if missing):
#   brew, cmake, ninja, pkg-config, libtool, pipx
#
# Output:
#   build-universal/tscrt_mac.dmg

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEPS="$ROOT/universal-deps"
PREFIX="$DEPS/prefix"
QT_VERSION="${QT_VERSION:-6.8.2}"
QT_DIR="$HOME/Qt/$QT_VERSION/macos"
ARCHS="arm64;x86_64"

LIBSSH2_VER=1.11.1
LIBVTERM_VER=0.3.3
MBEDTLS_VER=3.6.2

ensure_brew_pkg() {
  for pkg in "$@"; do
    brew list --versions "$pkg" >/dev/null 2>&1 || brew install "$pkg"
  done
}

echo "==> Checking host tools"
command -v brew >/dev/null || { echo "Homebrew required"; exit 1; }
ensure_brew_pkg cmake ninja pkg-config libtool pipx

echo "==> Installing aqtinstall"
pipx list 2>/dev/null | grep -q aqtinstall || pipx install aqtinstall
export PATH="$HOME/.local/bin:$PATH"

if [ ! -d "$QT_DIR" ]; then
  echo "==> Downloading Qt $QT_VERSION (universal) via aqt"
  mkdir -p "$HOME/Qt"
  (cd "$HOME/Qt" && aqt install-qt mac desktop "$QT_VERSION" clang_64)
fi

mkdir -p "$DEPS" "$PREFIX"
cd "$DEPS"

if [ ! -f "$PREFIX/lib/libmbedtls.a" ]; then
  echo "==> Building mbedTLS $MBEDTLS_VER (universal)"
  [ -d "mbedtls-$MBEDTLS_VER" ] || {
    curl -sL -o mbedtls.tar.bz2 "https://github.com/Mbed-TLS/mbedtls/releases/download/mbedtls-$MBEDTLS_VER/mbedtls-$MBEDTLS_VER.tar.bz2"
    tar xjf mbedtls.tar.bz2
  }
  cmake -S "mbedtls-$MBEDTLS_VER" -B "mbedtls-$MBEDTLS_VER/build" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_ARCHITECTURES="$ARCHS" \
    -DCMAKE_INSTALL_PREFIX="$PREFIX" \
    -DENABLE_TESTING=OFF -DENABLE_PROGRAMS=OFF -DUSE_SHARED_MBEDTLS_LIBRARY=OFF
  cmake --build "mbedtls-$MBEDTLS_VER/build" --target install
fi

if [ ! -f "$PREFIX/lib/libssh2.dylib" ]; then
  echo "==> Building libssh2 $LIBSSH2_VER (universal, mbedTLS backend)"
  [ -d "libssh2-$LIBSSH2_VER" ] || {
    curl -sL -o libssh2.tar.gz "https://www.libssh2.org/download/libssh2-$LIBSSH2_VER.tar.gz"
    tar xzf libssh2.tar.gz
  }
  cmake -S "libssh2-$LIBSSH2_VER" -B "libssh2-$LIBSSH2_VER/build" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_ARCHITECTURES="$ARCHS" \
    -DCMAKE_INSTALL_PREFIX="$PREFIX" \
    -DCMAKE_PREFIX_PATH="$PREFIX" \
    -DCRYPTO_BACKEND=mbedTLS \
    -DBUILD_SHARED_LIBS=ON \
    -DBUILD_EXAMPLES=OFF -DBUILD_TESTING=OFF
  cmake --build "libssh2-$LIBSSH2_VER/build" --target install
fi

if [ ! -f "$PREFIX/lib/libvterm.dylib" ]; then
  echo "==> Building libvterm $LIBVTERM_VER (universal)"
  [ -d "libvterm-$LIBVTERM_VER" ] || {
    curl -sL -o libvterm.tar.gz "https://www.leonerd.org.uk/code/libvterm/libvterm-$LIBVTERM_VER.tar.gz"
    tar xzf libvterm.tar.gz
  }
  (cd "libvterm-$LIBVTERM_VER" && \
    make clean >/dev/null 2>&1 || true; \
    make CFLAGS="-O2 -arch arm64 -arch x86_64" \
         LDFLAGS="-arch arm64 -arch x86_64" \
         PREFIX="$PREFIX" install)
fi

echo "==> Configuring TSCRT (universal)"
export PATH="$QT_DIR/bin:$PATH"
export PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig"
export CMAKE_PREFIX_PATH="$QT_DIR;$PREFIX"

cd "$ROOT"
rm -rf build-universal
cmake -S . -B build-universal -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES="$ARCHS"

echo "==> Building TSCRT"
cmake --build build-universal

echo "==> Bundling Qt frameworks + creating DMG"
rm -f build-universal/tscrt_mac.dmg
macdeployqt build-universal/tscrt_mac.app -dmg

echo "==> Done"
file build-universal/tscrt_mac.app/Contents/MacOS/tscrt_mac
ls -lh build-universal/tscrt_mac.dmg
