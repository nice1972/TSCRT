@echo off
REM release-win.bat — Windows 배포 빌드 + GitHub Release 등록.
REM
REM 사용법 (MSYS2 UCRT64 / Git Bash 에서 실행):
REM   release-win.bat 1.0.8           # 빌드 + 릴리스
REM   release-win.bat 1.0.8 upload    # 기존 릴리스에 파일만 추가
REM
REM 전제: MSYS2 UCRT64 환경 (source env.sh 완료)
REM       gh auth login (GitHub CLI 인증)
setlocal enabledelayedexpansion

if "%~1"=="" (
    echo Usage: %~nx0 ^<version^> [upload]
    exit /b 1
)

set "VERSION=%~1"
set "MODE=%~2"
if "%MODE%"=="" set "MODE=full"
set "TAG=v%VERSION%"
set "EXE_NAME=tscrt_win-%VERSION%-win64.exe"

cd /d "%~dp0"

REM ---- 1. 버전 치환 ---------------------------------------------------------
if "%MODE%"=="upload" goto :gh_release

echo === 버전 → %VERSION% ===

REM include/tscrt.h
powershell -Command "(Get-Content include\tscrt.h) -replace '#define TSCRT_VERSION\s+\"[^\"]+\"', '#define TSCRT_VERSION        \"%VERSION%\"' | Set-Content include\tscrt.h"

REM CMakeLists.txt
powershell -Command "(Get-Content CMakeLists.txt) -replace '\"[0-9]+\.[0-9]+\.[0-9]+\"', '\"%VERSION%\"' | Set-Content CMakeLists.txt"

REM resources/tscrt_win.rc
set "V4=%VERSION%.0"
set "VNUM=%VERSION:.=,%,0"
powershell -Command "(Get-Content resources\tscrt_win.rc) -replace '#define TSCRT_VERSION_STR\s+\"[^\"]+\"', '#define TSCRT_VERSION_STR  \"%V4%\"' | Set-Content resources\tscrt_win.rc"
powershell -Command "(Get-Content resources\tscrt_win.rc) -replace '#define TSCRT_VERSION_NUM\s+[0-9,]+', '#define TSCRT_VERSION_NUM  %VNUM%' | Set-Content resources\tscrt_win.rc"

REM README 다운로드 링크 (모든 언어)
for %%F in (README*.md) do (
    powershell -Command "(Get-Content '%%F') -replace '/download/v[0-9]+\.[0-9]+\.[0-9]+/', '/download/%TAG%/' -replace 'tscrt_[0-9]+\.[0-9]+\.[0-9]+_amd64\.deb', 'tscrt_%VERSION%_amd64.deb' -replace 'tscrt_win-[0-9]+\.[0-9]+\.[0-9]+-win64\.exe', '%EXE_NAME%' -replace 'tscrt_mac-[0-9]+\.[0-9]+\.[0-9]+-universal\.dmg', 'tscrt_mac-%VERSION%-universal.dmg' | Set-Content '%%F'"
)

REM ---- 2. 빌드 --------------------------------------------------------------
echo === 클린 빌드 ===
if exist build rd /s /q build

REM 실행 중인 exe 가 있으면 링크 실패하므로 종료
tasklist /FI "IMAGENAME eq tscrt_win.exe" 2>nul | find /i "tscrt_win.exe" >nul && (
    echo tscrt_win.exe 실행 중 — 종료합니다.
    taskkill /F /IM tscrt_win.exe /T >nul 2>&1
)

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build

echo === 테스트 ===
ctest --test-dir build --output-on-failure

echo === NSIS 설치 관리자 생성 ===
cmake --install build --prefix build\install
cd build
cpack -G NSIS
cd ..

REM CPack 이 생성한 파일명이 다를 수 있으므로 정규화
if not exist "build\%EXE_NAME%" (
    for %%G in (build\tscrt_win-*.exe) do (
        move "%%G" "build\%EXE_NAME%" >nul
    )
)

echo 결과: build\%EXE_NAME%

REM ---- 3. GitHub Release -----------------------------------------------------
:gh_release
set "EXE_PATH=build\%EXE_NAME%"

gh release view %TAG% >nul 2>&1
if %ERRORLEVEL%==0 (
    echo === 릴리스 %TAG% 이미 존재 — 파일 업로드만 수행 ===
    gh release upload %TAG% "%EXE_PATH%" --clobber
) else (
    echo === 릴리스 %TAG% 생성 + 업로드 ===
    gh release create %TAG% "%EXE_PATH%" --title "TSCRT %VERSION%" --notes "TSCRT %VERSION% release binaries."
)

echo === 릴리스 URL: https://github.com/nice1972/TSCRT/releases/tag/%TAG% ===
echo.
echo 완료. git add/commit/push 는 수동으로 진행하세요.
