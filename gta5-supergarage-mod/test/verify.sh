#!/bin/bash
# Static checks of build/SuperGarageV.asi against the GTA V iOS port's sources.
#   test/verify.sh <path to gta5-ios-port checkout>
set -euo pipefail
cd "$(dirname "$0")/.."
PORT=${1:?path to kevinerikjs/gta5-ios-port}
ASI=build/SuperGarageV.asi
echo "== file";  file "$ASI"; sha256sum "$ASI"
echo "== imported DLLs and functions"
llvm-readobj --coff-imports "$ASI" | grep -E "Name:|Symbol:"
echo "== each import is implemented by the port (app/Source/NativeASI.c import table)"
fail=0
for sym in $(llvm-readobj --coff-imports "$ASI" | awk '/Symbol:/{print $2}'); do
  plain=$(echo "$sym" | sed -E 's/^\?([A-Za-z0-9_]+)@@.*/\1/')
  if grep -q "{\"$plain\"," "$PORT/app/Source/NativeASI.c"; then echo "  ok   $plain"; else echo "  MISSING $plain"; fail=1; fi
done
echo "== each native hash used is in the port's table (app/Scripts/asi_native_hashes.txt)"
for h in $(grep -oE '0x[0-9A-F]{16}ull' src/SuperGarageV.cpp | tr 'A-F' 'a-f' | sed 's/^0x//; s/ull$//' | sort -u); do
  line=$(grep -E "^$h " "$PORT/app/Scripts/asi_native_hashes.txt" || true)
  if [ -n "$line" ]; then echo "  ok   $line"; else echo "  MISSING $h"; fail=1; fi
done
echo "== no CMPSS/CMPPS/CMPSD/CMPPD (mis-emulated by the port's Blink OpCmppsd)"
n=$(llvm-objdump -d --no-show-raw-insn "$ASI" | grep -ciE "cmp[a-z]*(ps|ss|pd|sd)\b" || true)
echo "  float compare instructions: $n"; [ "$n" = 0 ] || fail=1
echo "== no TLS, no C runtime, no static constructors"
llvm-readobj --sections "$ASI" | grep -E "Name: " ; llvm-objdump -p "$ASI" | grep -E "Thread Storage|DLL Name"
[ $fail = 0 ] && echo "ALL STATIC CHECKS PASSED" || { echo "STATIC CHECKS FAILED"; exit 1; }
