#!/usr/bin/env bash
# Run the Windows self-test suite under Wine (Linux development machine).
set -euo pipefail
cd "$(dirname "$0")/.."
ST=build/selftest
rm -rf "$ST"; mkdir -p "$ST"
cp build/mock_optiscaler.dll build/mock_mor3a.dll build/zz_ngxbridge.asi build/mock_game.exe "$ST/"
cp selftest/zz_ngxbridge.ini "$ST/zz_ngxbridge.ini"
cd "$ST"
WINEDEBUG=-all timeout 60 wine mock_game.exe > stdout.log 2> stderr.log || true
echo "----- mock_game stdout -----"
cat stdout.log
echo "----- bridge log -----"
[ -f RDR2VulkanFG/ngxbridge.log ] && cat RDR2VulkanFG/ngxbridge.log || echo "(no bridge log!)"
echo "----- verdict -----"
if grep -q "OBSERVER-OK" stdout.log && grep -q "ACCEPT - installing" stdout.log \
   && grep -q "MIRROR-CROSS-TYPE-OK" stdout.log \
   && grep -q "STALE-GUARD-OK" stdout.log \
   && grep -q "spoofed provider path" RDR2VulkanFG/ngxbridge.log \
   && grep -q "vtable .* patched" RDR2VulkanFG/ngxbridge.log; then
    echo "SELFTEST: PASS"
else
    echo "SELFTEST: FAIL"
    exit 1
fi
