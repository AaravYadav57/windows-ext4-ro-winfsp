cmake_minimum_required(VERSION 3.16)
project(ext4fsp C)

set(CMAKE_C_STANDARD 11)
# Default install path. Override with: cmake -DWINFSP_ROOT="C:\path\to\winfsp"
if(NOT DEFINED WINFSP_ROOT)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(WINFSP_ROOT "C:/Program Files (x86)/WinFsp")
    else()
        set(WINFSP_ROOT "C:/Program Files (x86)/WinFsp")
    endif()
endif()

# Header path
set(WINFSP_INC "${WINFSP_ROOT}/inc")

# Library path
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(WINFSP_LIB "${WINFSP_ROOT}/lib/winfsp-x64.lib")
    set(TARGET_ARCH x64)
else()
    set(WINFSP_LIB "${WINFSP_ROOT}/lib/winfsp-x86.lib")
    set(TARGET_ARCH x86)
endif()

# Validate WinFsp presence at configure time
if(NOT EXISTS "${WINFSP_INC}/winfsp/winfsp.h")
    message(FATAL_ERROR
        "WinFsp headers not found at: ${WINFSP_INC}/winfsp/winfsp.h\n"
        "Install WinFsp from https://winfsp.dev/rel/ and/or set WINFSP_ROOT.\n"
        "Example: cmake -DWINFSP_ROOT=\"C:/Program Files (x86)/WinFsp\" ..")
endif()

if(NOT EXISTS "${WINFSP_LIB}")
    message(FATAL_ERROR
        "WinFsp library not found at: ${WINFSP_LIB}\n"
        "Install WinFsp (the SDK/development package) and/or set WINFSP_ROOT.")
endif()

message(STATUS "WinFsp root : ${WINFSP_ROOT}")
message(STATUS "WinFsp inc  : ${WINFSP_INC}")
message(STATUS "WinFsp lib  : ${WINFSP_LIB}")
message(STATUS "Target arch : ${TARGET_ARCH}")

# ── Sources 
add_executable(ext4fsp
    src/ext4fsp.c
    src/ext4fs.c
    src/diskio.c
)

target_include_directories(ext4fsp PRIVATE
    include
    ${WINFSP_INC}
)

target_link_libraries(ext4fsp
    ${WINFSP_LIB}
    ntdll
)

# ── Compiler flags ──
if(MSVC)
    target_compile_options(ext4fsp PRIVATE
        /W3
        /WX-
        /wd4996   # suppress deprecated CRT warnings
    )
    target_compile_definitions(ext4fsp PRIVATE
        _CRT_SECURE_NO_WARNINGS
        _WIN32_WINNT=0x0601  # Windows 7+
        WIN32_LEAN_AND_MEAN
        UNICODE
        _UNICODE
    )
    # Static CRT for standalone binary
    set_property(TARGET ext4fsp PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded")
elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    # MinGW / Clang on Windows
    target_compile_options(ext4fsp PRIVATE
        -Wall -Wextra -Wno-unused-parameter
    )
    target_compile_definitions(ext4fsp PRIVATE
        _WIN32_WINNT=0x0601
        WIN32_LEAN_AND_MEAN
        UNICODE
        _UNICODE
    )
    target_link_libraries(ext4fsp ntdll)
endif()

# ── Install ───
install(TARGETS ext4fsp DESTINATION bin)
