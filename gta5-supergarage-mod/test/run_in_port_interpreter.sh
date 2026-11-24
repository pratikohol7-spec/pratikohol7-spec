#!/bin/bash
# Builds the GTA V iOS port's own ASI interpreter (app/Source/NativeASI.c + vendored Blink) for
# Linux and runs build/SuperGarageV.asi in it against a simulated game world (test/fake_game_host.c).
#   test/run_in_port_interpreter.sh <path to gta5-ios-port checkout> [work dir]
# The port's source is only read, never modified. Three macOS-only Blink files are replaced by
# test/linux_stubs.c, and <xlocale.h> by a one-line shim.
set -euo pipefail
cd "$(dirname "$0")/.."
PORT=$(cd "${1:?path to kevinerikjs/gta5-ios-port}" && pwd)/app
W=${2:-$(mktemp -d)}; mkdir -p "$W/blink" "$W/shim" "$W/mod"
echo '#include <locale.h>' > "$W/shim/xlocale.h"
if [ ! -f "$W/libblink.a" ]; then
  for f in "$PORT"/Vendor/blink/blink/*.c; do
    case $(basename "$f") in cpucount.c|ioctl.c|sysinfo.c) continue;; esac
    clang -c -O2 -DNDEBUG -w -D_GNU_SOURCE -I "$PORT/Vendor/blink" -o "$W/blink/$(basename "$f" .c).o" "$f"
  done
  llvm-ar rcs "$W/libblink.a" "$W"/blink/*.o
fi
clang -O1 -g -w -D_GNU_SOURCE -I "$W/shim" -I "$PORT/Source" -I "$PORT/Vendor/blink" -o "$W/fake_game_host" \
  test/fake_game_host.c test/linux_stubs.c "$PORT/Source/NativeASI.c" "$W/libblink.a" -lpthread -lm
cp build/SuperGarageV.asi src/SuperGarageV.ini "$W/mod/"
"$W/fake_game_host" "$W/mod/SuperGarageV.asi" "$W/mod"
