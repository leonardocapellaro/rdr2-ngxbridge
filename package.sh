#!/usr/bin/env bash
# Monta release/ e o ZIP do pacote a partir de build/. Idempotente.
set -euo pipefail
cd "$(dirname "$0")"
REL=release
ST=$REL/selftest-windows

[ -f build/zz_ngxbridge.asi ] || { echo "rode build.sh antes"; exit 1; }
mkdir -p "$ST"

# artefatos da ponte
cp -f build/zz_ngxbridge.asi "$REL/zz_ngxbridge.asi"
# bundle de self-test Windows: ponte + mocks + game + ini do self-test
cp -f build/zz_ngxbridge.asi      "$ST/zz_ngxbridge.asi"
cp -f build/mock_optiscaler.dll   "$ST/mock_optiscaler.dll"
cp -f build/mock_mor3a.dll        "$ST/mock_mor3a.dll"
cp -f build/mock_game.exe         "$ST/mock_game.exe"
cp -f selftest/zz_ngxbridge.ini   "$ST/zz_ngxbridge.ini"

# scripts .bat / .ini com CRLF (cmd lida goto/label com confiança só assim)
for f in "$REL/install.bat" "$REL/uninstall.bat" "$ST/run_selftest.bat" \
         "$REL/zz_ngxbridge.ini" "$ST/zz_ngxbridge.ini"; do
    sed -i 's/\r\?$/\r/' "$f"
done

# licenciamento (MinHook MIT vendorizado; nada proprietario incluso)
{
  echo "zz_ngxbridge - licencas de terceiros"
  echo "Este pacote contem codigo de terceiros: somente o MinHook 1.3.3 (MIT),"
  echo "vendorizado e linkado estaticamente. Texto integral da licenca abaixo."
  echo "Nenhum componente da NVIDIA, Intel, Rockstar ou Microsoft e incluido."
  echo "======================================================================"
  cat ../vendor/minhook-1.3.3/LICENSE.txt
} > "$REL/THIRD-PARTY-NOTICES.txt"

# checksums de tudo (exceto o proprio arquivo de soma)
(cd "$REL" && find . -type f ! -name SHA-256SUMS.txt -print0 \
   | sort -z | xargs -0 sha256sum > SHA-256SUMS.txt)

# zip do pacote (nome com versao; -X sem metadados extra)
VER=$(grep -oP '#define NGXBRIDGE_VERSION "\K[^"]+' src/ngxbridge.cpp)
ZIP=ngxbridge-v${VER}-rdr2-xefg-compat.zip
rm -f "$ZIP"
zip -rqX "$ZIP" release
echo "PACOTE OK: $(pwd)/$ZIP"
sha256sum "$ZIP"
unzip -l "$ZIP" | tail -3
