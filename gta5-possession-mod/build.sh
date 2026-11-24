#!/bin/bash
# Builds PossessionV.asi (64-bit Windows DLL) with clang + lld-link, no MSVC CRT.
# Requires: clang, lld-link, llvm-dlltool (LLVM 15+). Output: build/PossessionV.asi
set -euo pipefail
cd "$(dirname "$0")"
OBJ=build/obj; mkdir -p "$OBJ" build
for d in ScriptHookV kernel32 user32; do
  llvm-dlltool -m i386:x86-64 -d lib/$d.def -l "$OBJ/$d.lib"
done
clang++ -target x86_64-pc-windows-msvc -std=c++17 -O2 -ffreestanding -fno-builtin \
  -fno-exceptions -fno-rtti -fno-stack-protector -fno-unwind-tables -fno-asynchronous-unwind-tables \
  -mno-stack-arg-probe -Wall -Wextra -Werror -c src/PossessionV.cpp -o "$OBJ/PossessionV.obj"
lld-link /dll /nodefaultlib /entry:DllMain /machine:x64 /subsystem:windows \
  /out:build/PossessionV.asi "$OBJ/PossessionV.obj" "$OBJ/ScriptHookV.lib" "$OBJ/kernel32.lib" "$OBJ/user32.lib"
rm -f build/PossessionV.lib build/PossessionV.exp
echo "built build/PossessionV.asi ($(stat -c %s build/PossessionV.asi) bytes)"
