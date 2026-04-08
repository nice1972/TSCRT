# source this from MSYS / Git-Bash:  source env.sh
# Sets up MSYS2 UCRT64 toolchain (gcc 15, cmake, Qt6, libssh2, libvterm).

export MSYS2_ROOT="/c/msys64"
export UCRT64="$MSYS2_ROOT/ucrt64"

export PATH="$UCRT64/bin:$UCRT64/share/qt6/bin:$PATH"
export PKG_CONFIG_PATH="$UCRT64/lib/pkgconfig"
export PKG_CONFIG_SYSROOT_DIR="C:/msys64"
export CMAKE_PREFIX_PATH="$UCRT64"

# Convenience aliases
alias bld='cmake -S . -B build -G Ninja && cmake --build build'
alias rebld='rm -rf build && bld'
alias run='./build/tscrt_win.exe'

echo "tscrt_win env ready: $(gcc --version | head -1)"
echo "  Qt6:      $(pkg-config --modversion Qt6Core 2>/dev/null)"
echo "  libssh2:  $(pkg-config --modversion libssh2 2>/dev/null)"
echo "  libvterm: $(pkg-config --modversion vterm 2>/dev/null)"
