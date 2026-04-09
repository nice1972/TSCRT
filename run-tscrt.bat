@echo off
rem Launch tscrt_win.exe with MSYS2 UCRT64 DLLs available on PATH.
rem Double-click from Explorer or run from cmd.exe — no install needed.
rem Edit MSYS2 path below if MSYS2 is not at C:\msys64.
set "PATH=C:\msys64\ucrt64\bin;C:\msys64\ucrt64\share\qt6\bin;%PATH%"
start "" "%~dp0build\tscrt_win.exe"
