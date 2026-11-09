#!/usr/bin/env bash
# Regenerates RDR2-Vulkan-FG/src/ui_smooth_spv.h from src/ui_smooth.comp.
# Prerequisite: tools/glslang/glslangValidator (KhronosGroup/glslang release 16.6.0,
# linux-x86_64-release asset) installed in the workspace.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../../../RDR2-Vulkan-FG" && pwd)"
GLSLANG="${GLSLANG:-$SCRIPT_DIR/../../../tools/glslang/glslangValidator}"
[ -x "$GLSLANG" ] || { echo "glslang not found at $GLSLANG"; exit 1; }
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
"$GLSLANG" -V "$ROOT/src/ui_smooth.comp" -e main -o "$TMP/ui_smooth.spv" >/dev/null
python3 -I - "$ROOT" "$TMP/ui_smooth.spv" <<'EOF'
import pathlib, sys
root, spv = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2])
data = spv.read_bytes()
assert len(data) % 4 == 0
w = [int.from_bytes(data[i:i+4], 'little') for i in range(0, len(data), 4)]
lines = ['#pragma once', '#include <cstddef>', '#include <cstdint>', 'namespace fgvk {',
         'alignas(4) static constexpr uint32_t UiSmoothSpirv[] = {']
lines += ['  ' + ', '.join('0x%08xu' % x for x in w[i:i+8]) + ',' for i in range(0, len(w), 8)]
lines += ['};', 'static constexpr size_t UiSmoothSpirvSize = sizeof(UiSmoothSpirv);', '}', '']
out = root / 'src' / 'ui_smooth_spv.h'
out.write_text('\n'.join(lines))
print(f'{out} ({len(w)} SPIR-V words)')
EOF
