#!/usr/bin/env bash
# Reproduz o Build.ps1 do repositório RDR2VulkanFG (Zig 0.16.0, alvo Windows
# x64 GNU — o mesmo alvo que o autor usa) em Linux, sem PowerShell.
#
# Pré-requisitos:
#   - Zig 0.16.0 em $ZIG (default: /tmp/zig-x86_64-linux-0.16.0/zig)
#   - dependências fixadas já clonadas em <repo>/.deps (mesmos commits do
#     Build.ps1; ver README da release).
#
# Uso:  ZIG=/caminho/zig bash build-mor3a-zig-linux.sh [diretorio-saida]
# Saída: <diretorio-saida>/RDR2VulkanFG.asi e RDR2VulkanFGDiagnostics.dll
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../../../RDR2-Vulkan-FG" && pwd)"
ZIG="${ZIG:-$SCRIPT_DIR/../../../tools/zig-x86_64-linux-0.17.1/zig}"
DEPS="$ROOT/.deps"
OUT="${1:-$ROOT/.build-linux}"
[ -x "$ZIG" ] || { echo "zig não encontrado em $ZIG (defina ZIG=)"; exit 1; }
for d in Detours Vulkan-Headers Streamline-2.14.1; do
  [ -d "$DEPS/$d" ] || { echo "falta $DEPS/$d — clone conforme docs/BUILD.md"; exit 1; }
done
mkdir -p "$OUT"

FLAGS=(c++ -std=c++17 -O2 -target x86_64-windows-gnu
  "-ffile-prefix-map=$ROOT=." "-ffile-prefix-map=$DEPS=dependencies"
  -fms-extensions -Wno-macro-redefined -Wno-nullability-completeness
  -DVK_NO_PROTOTYPES -DWIN32_LEAN_AND_MEAN -DNOMINMAX -DDETOURS_X64
  "-I$DEPS/Vulkan-Headers/include" "-I$DEPS/Streamline-2.14.1/include"
  "-I$DEPS/Detours/src" "-I$ROOT/src" "-I$ROOT/vendor/imgui" "-I$ROOT/vendor")

# regressões em tempo de compilação (como no Build.ps1; não executam nada)
for t in frame_history xess_queues gpu_guard bridge_recreation bridge_results; do
  "$ZIG" "${FLAGS[@]}" -c "$ROOT/tests/$t.cpp" -o "$OUT/$t.obj" \
    > "$OUT/$t.log" 2>&1 || { echo "regressão $t falhou:"; tail -25 "$OUT/$t.log"; exit 1; }
done

APP=(dllmain.cpp log.cpp config.cpp vkhooks.cpp slboot.cpp inputs.cpp
  render_reprojection.cpp input_capture.cpp ngx_capture.cpp ngx_capture.S
  ampere_boot.cpp turing_unlock.cpp gpu_guard.cpp rdr_camera.cpp menu.cpp
  hud_capture.cpp native_hud.cpp hud_mask.cpp performance_probe.cpp
  fg_preset.cpp update_check.cpp)
SRC=()
for f in "${APP[@]}"; do SRC+=("$ROOT/src/$f"); done
SRC+=("$ROOT/vendor/xemfg-reference/host_unlock.cpp")
XESS=(xess_bridge_device dlssg_bridge_runtime xess_bridge_runtime xess_output_pacing
      xess_bridge_plan xess_bridge_snapshot xess_bridge_handoff xess_provider
      camera_frame experiment_metrics core_diagnostics ui_synth)
for m in "${XESS[@]}"; do SRC+=("$ROOT/src/$m.cpp"); done
for g in imgui imgui_draw imgui_tables imgui_widgets imgui_impl_win32 imgui_impl_vulkan; do
  SRC+=("$ROOT/vendor/imgui/$g.cpp"); done
SRC+=("$ROOT/src/menu_vulkan.cpp")
for d in detours disasm modules image creatwth; do SRC+=("$DEPS/Detours/src/$d.cpp"); done

"$ZIG" "${FLAGS[@]}" -shared "${SRC[@]}" "$ROOT/vendor/d3dcompiler.lib" \
  -o "$OUT/RDR2VulkanFG.asi" \
  -lbcrypt -luser32 -ladvapi32 -ld3d11 -ld3d12 -ldxgi -lgdi32 -ldwmapi \
  -lcomctl32 -limm32 -lwinhttp -lshell32 -lole32 -lversion \
  -Wl,--dynamicbase -Wl,--nxcompat > "$OUT/build.log" 2>&1 || {
    echo "build falhou:"; tail -40 "$OUT/build.log"; exit 1; }

"$ZIG" c++ -std=c++17 -O2 -target x86_64-windows-gnu -shared -DFGVK_COLLECTOR_DLL \
  -DWIN32_LEAN_AND_MEAN "-I$ROOT/src" "$ROOT/src/tools/fgvk-stack.cpp" \
  -o "$OUT/RDR2VulkanFGDiagnostics.dll" -ldbghelp -lshell32 \
  > "$OUT/diagnostics.log" 2>&1 || { echo "diagnostics falhou:"; tail -25 "$OUT/diagnostics.log"; exit 1; }

echo "OK: $OUT/RDR2VulkanFG.asi"
sha256sum "$OUT/RDR2VulkanFG.asi" "$OUT/RDR2VulkanFGDiagnostics.dll"
