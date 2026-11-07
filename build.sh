#!/usr/bin/env bash
# Windows x64 build via MinGW-w64 (no MSVC needed: the bridge contains no C++
# exception/machinery beyond plain std, and MinGW emits the MSVC x64 ABI).
# Output: build/zz_ngxbridge.asi (+ self-test mocks) - fully static.
set -euo pipefail
cd "$(dirname "$0")"

GCC=x86_64-w64-mingw32-gcc
GXX=x86_64-w64-mingw32-g++
OBJDUMP=x86_64-w64-mingw32-objdump
MH=../vendor/minhook-1.3.3
OUT=build
mkdir -p "$OUT"

# FARPROC<->typed-function casts are inherent to GetProcAddress hooking.
STD="-O2 -g -Wall -Wextra -Wno-unused-parameter -Wno-cast-function-type"
CPP="-std=c++17 -D_WIN32_WINNT=0x0A00 -DUNICODE -D_UNICODE"
MHINC="-I$MH/include -I$MH/src"

# --- MinHook (vendored, MIT) ---
$GCC $STD $MHINC -c $MH/src/buffer.c     -o $OUT/buffer.o
$GCC $STD $MHINC -c $MH/src/hook.c       -o $OUT/hook.o
$GCC $STD $MHINC -c $MH/src/trampoline.c -o $OUT/trampoline.o
$GCC $STD $MHINC -c $MH/src/hde/hde64.c  -o $OUT/hde64.o
MHOBJS="$OUT/buffer.o $OUT/hook.o $OUT/trampoline.o $OUT/hde64.o"

# --- bridge ---
$GXX $STD $CPP -Isrc -c src/mirror_core.cpp -o $OUT/mirror_core_win.o
$GXX $STD $CPP -Isrc -I$MH/include -c src/ngxbridge.cpp -o $OUT/ngxbridge.o
$GXX -shared -static -o $OUT/zz_ngxbridge.asi \
    $OUT/ngxbridge.o $OUT/mirror_core_win.o $MHOBJS -lkernel32

# --- self-test mocks (never installed next to the game) ---
$GXX $STD $CPP -c mock/mock_optiscaler.cpp -o $OUT/mock_optiscaler.o
$GXX -shared -static -o $OUT/mock_optiscaler.dll $OUT/mock_optiscaler.o
$GXX $STD $CPP $MHINC -c mock/mock_mor3a.cpp -o $OUT/mock_mor3a.o
$GXX -shared -static -o $OUT/mock_mor3a.dll $OUT/mock_mor3a.o $MHOBJS -lkernel32
$GXX $STD -static -o $OUT/mock_game.exe mock/mock_game.cpp

# --- sanity: the .asi must be a plain DLL with no exotic imports ---
$OBJDUMP -p $OUT/zz_ngxbridge.asi | grep -E '^File Format|DLL Name' | sort -u > $OUT/zz_ngxbridge.imports.txt

# --- native unit tests (mirror_core logic, runs on the dev machine) ---
g++ -O2 -std=c++17 -Isrc tests/test_mirror.cpp src/mirror_core.cpp -o $OUT/test_mirror -pthread
$OUT/test_mirror

echo "BUILD OK"
ls -la $OUT/*.asi $OUT/*.dll $OUT/*.exe 2>/dev/null
