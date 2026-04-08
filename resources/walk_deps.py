#!/usr/bin/env python3
"""
walk_deps.py - resolve all transitive DLL dependencies of one or more
PE binaries (objdump-based BFS), filtering out OS/runtime DLLs that
ship with Windows. Used to keep CMakeLists.txt's bundled-DLL list in
sync with the MSYS2 ucrt64 toolchain.

Usage:
    python resources/walk_deps.py path/to/exe [more.dll ...]

Re-run after upgrading Qt or adding new external libraries; paste the
sorted output into CMakeLists.txt's _TP_DLLS list.
"""
import subprocess
import sys
from pathlib import Path

OBJDUMP    = r"C:\msys64\ucrt64\bin\objdump.exe"
SEARCH_DIR = Path(r"C:\msys64\ucrt64\bin")

# DLLs that ship with Windows / Universal CRT - never bundle these.
SYSTEM_PREFIXES = (
    "kernel32", "user32", "gdi32", "advapi32", "shell32", "shlwapi",
    "ole32", "oleaut32", "comctl32", "comdlg32", "ws2_32", "winspool",
    "uxtheme", "version", "imm32", "winmm", "iphlpapi", "userenv",
    "psapi", "netapi32", "secur32", "crypt32", "wintrust", "dwmapi",
    "dwrite", "d2d1", "d3d", "dxgi", "msimg32", "msvcrt", "msvcr",
    "ucrtbase", "api-ms-win", "ext-ms-", "rpcrt4", "setupapi", "ntdll",
    "powrprof", "authz", "wtsapi32", "mpr", "credui", "cfgmgr32",
    "bcrypt", "ncrypt", "dbghelp", "dnsapi", "iertutil", "urlmon",
    "wininet", "winhttp", "wldap32", "gdiplus", "propsys", "oleacc",
    "msftedit", "usp10", "fwpuclnt", "mswsock",
)


def is_system(name: str) -> bool:
    n = name.lower()
    return any(n.startswith(p) for p in SYSTEM_PREFIXES)


def imports_of(path: Path) -> list[str]:
    out = subprocess.run([OBJDUMP, "-p", str(path)],
                         capture_output=True, text=True, errors="replace")
    deps = []
    for line in out.stdout.splitlines():
        s = line.strip()
        if s.startswith("DLL Name:"):
            deps.append(s.split(":", 1)[1].strip())
    return deps


def main() -> None:
    if len(sys.argv) < 2:
        print(__doc__, file=sys.stderr)
        sys.exit(1)

    queue: list[Path] = [Path(p) for p in sys.argv[1:]]
    seen: set[str] = set()
    bundled: set[str] = set()

    while queue:
        cur = queue.pop()
        if str(cur) in seen:
            continue
        seen.add(str(cur))
        for dep in imports_of(cur):
            if is_system(dep):
                continue
            dep_lower = dep.lower()
            if dep_lower in bundled:
                continue
            cand = SEARCH_DIR / dep
            if not cand.exists():
                for f in SEARCH_DIR.iterdir():
                    if f.name.lower() == dep_lower:
                        cand = f
                        break
            if cand.exists():
                bundled.add(dep_lower)
                queue.append(cand)

    for d in sorted(bundled):
        # Print the actual on-disk filename casing.
        for f in SEARCH_DIR.iterdir():
            if f.name.lower() == d:
                print(f.name)
                break


if __name__ == "__main__":
    main()
