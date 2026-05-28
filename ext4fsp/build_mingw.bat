@echo off
setlocal
set WINFSP_ROOT=C:\Program Files (x86)\WinFsp
set OUT=build\ext4fsp_mingw.exe

if not exist build mkdir build

gcc -O2 -Wall -Wextra -Wno-unused-parameter ^
    -Iinclude -I"%WINFSP_ROOT%\inc" ^
    src/ext4fsp.c src/ext4fs.c src/diskio.c ^
    -o %OUT% ^
    "%WINFSP_ROOT%\lib\winfsp-x64.lib" ^
    -lntdll ^
    -D_WIN32_WINNT=0x0601 ^
    -DWIN32_LEAN_AND_MEAN ^
    -DUNICODE -D_UNICODE ^
    -municode

if %ERRORLEVEL% neq 0 (
    echo [FAILED]
    exit /b 1
)
echo [SUCCESS] %OUT%
