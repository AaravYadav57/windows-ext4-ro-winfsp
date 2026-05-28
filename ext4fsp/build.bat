@echo off
REM =========================================================================
REM  build.bat - Quick build script for ext4fsp using MSVC (cl.exe)
REM
REM  Prerequisites:
REM    1. Visual Studio Build Tools OR Visual Studio (any edition)
REM       https://visualstudio.microsoft.com/downloads/
REM    2. WinFsp installed (SDK/development option checked)
REM       https://winfsp.dev/rel/
REM
REM  Usage:
REM    build.bat              -> builds x64 release
REM    build.bat debug        -> builds x64 debug
REM    build.bat x86          -> builds x86 release
REM =========================================================================

setlocal enabledelayedexpansion

set BUILD_TYPE=release
set ARCH=x64
if /i "%1"=="debug" set BUILD_TYPE=debug
if /i "%1"=="x86"   set ARCH=x86

set VSWHERE="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist %VSWHERE% (
    echo [ERROR] vswhere.exe not found. Install Visual Studio Build Tools.
    echo         https://visualstudio.microsoft.com/downloads/
    exit /b 1
)

for /f "usebackq tokens=*" %%i in (
    `%VSWHERE% -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`
) do set VS_PATH=%%i

if not defined VS_PATH (
    echo [ERROR] No Visual Studio with C++ tools found.
    exit /b 1
)
echo [INFO] Visual Studio: !VS_PATH!
if /i "!ARCH!"=="x64" (
    call "!VS_PATH!\VC\Auxiliary\Build\vcvars64.bat" > nul
) else (
    call "!VS_PATH!\VC\Auxiliary\Build\vcvars32.bat" > nul
)
set WINFSP_ROOT=C:\Program Files (x86)\WinFsp
if not exist "!WINFSP_ROOT!\inc\winfsp\winfsp.h" (
    echo [ERROR] WinFsp headers not found at: !WINFSP_ROOT!\inc\winfsp\winfsp.h
    echo         Install WinFsp: https://winfsp.dev/rel/
    echo         Or set WINFSP_ROOT to your WinFsp install directory.
    exit /b 1
)

if /i "!ARCH!"=="x64" (
    set WINFSP_LIB="!WINFSP_ROOT!\lib\winfsp-x64.lib"
) else (
    set WINFSP_LIB="!WINFSP_ROOT!\lib\winfsp-x86.lib"
)
echo [INFO] WinFsp: !WINFSP_ROOT!

if not exist build mkdir build
set OUT=build\ext4fsp.exe
set SOURCES=src\ext4fsp.c src\ext4fs.c src\diskio.c
set CFLAGS=/nologo /W3 /MT /utf-8 /D_AMD64_
set CFLAGS=!CFLAGS! /D_CRT_SECURE_NO_WARNINGS
set CFLAGS=!CFLAGS! /D_WIN32_WINNT=0x0601
set CFLAGS=!CFLAGS! /DWIN32_LEAN_AND_MEAN
set CFLAGS=!CFLAGS! /DUNICODE /D_UNICODE

if /i "!BUILD_TYPE!"=="debug" (
    set CFLAGS=!CFLAGS! /Zi /Od
    set LDFLAGS=/DEBUG
) else (
    set CFLAGS=!CFLAGS! /O2
    set LDFLAGS=
)
set INCLUDES=/I include /I "!WINFSP_ROOT!\inc"
set LIBS=!WINFSP_LIB! ntdll.lib
echo [BUILD] cl !CFLAGS! !INCLUDES! !SOURCES! /Fe:!OUT! /link !LDFLAGS! !LIBS!
cl !CFLAGS! !INCLUDES! !SOURCES! /Fe:!OUT! /link !LDFLAGS! !LIBS!
if !ERRORLEVEL! neq 0 (
    echo [FAILED] Build failed.
    exit /b !ERRORLEVEL!
)
echo.
echo [SUCCESS] Built: !OUT!
echo.
echo Usage examples:
echo   !OUT! image.img *
echo   !OUT! image.img Z:
echo   !OUT! image.img -p 1 *
echo   !OUT! \\.\PhysicalDrive1 -p 2 *
echo   !OUT! image.img --list-partitions
echo.
