# FIELD-B390-3 — v2: multiplier investigation + HudMask NO-GO (2026-11-08)

## 1. Weapon/item wheel flickering → the HudMask cannot be "switched back on" (NO-GO)
I mapped the generated frame's lifecycle (Explore agent + my own reading):
- `CreatePresentation(..., XEFG_SWAPCHAIN_UI_MODE_BACKBUFFER_HUDLESS)`
  (xess_provider.cpp:534) — the **SDK itself** composites the HUD from the real frames onto
  the interpolated ones via backbuffer−hudless diff (which is why the static HUD is OK; the
  translucent animated wheel flickers — a temporal limitation of the SDK's
  recomposition).
- The generated frame **never exists as a VkImage**: it is born inside
  `swapchain_->Present()` (xess_bridge_runtime.cpp:181-186), flip-model D3D12,
  no NT handle, no output-resource tag in the API (xefg_swapchain.h:105-110).
- `HudMaskApply` (hud_mask.cpp:119) is orphaned in the 0.82 and re-wiring it has nowhere to go:
  the only controllable Vulkan command (`f.command`) runs BEFORE the SDK's Present
  (:774). The agent's alternative (masking the hudless INPUT before
  RecordColor) would smear HUD over time — it would make things worse. The EXTERNAL↔family
  barriers on the SharedImages would also conflict with the mask's IGNORED ones.
Action: the right question was asked in NEXUS-REPORT-draft (the SDK's UI mode + the
HudMask as an abandoned precursor). Nothing implemented — correctly not implemented.

## 2. 3x/4x multiplier: the UI exists, the event never arrived
The "Multiplier" combo (menu.cpp:435, 2x/3x/4x Experimental labels) is top-level
of the card, and the runtime re-reads `Rt().frames` on every present with clamp +
`SetInterpolations` (:677-681). But the session log doesn't have a SINGLE
`menu:`/`hotkey:` line → neither the click nor End registered. Chain: imgui only receives
messages via the WH_GETMESSAGE hook (`menu_input::Install`,
ProcessGameMessage:110-124; the polling fallback only runs without the hook, and without mouse-move).
v2 adds logs that section the chain: `menu: open/close`,
`menu input: left click observed...`, `hotkey: ... ignored — foreground...`.

## 3. Perf: nothing free in the public code; the real win is config
CPU on the present: proxyPresent 10-16ms average (bound by the SDK's XeLLSleep), not
optimizable from here. The real cost = SDK inference + bridge copies. **Free:**
`DebugLogging=0` (removes ~one line/present in the log; the writing is already gated
in log.cpp:145, but the line parsing exists) — recommend to the user.
`LogDebug` is already conditional; don't touch.

## Artifacts
- branch `hud-transfer-capture`: a1ce133 (transfer) + **41726f0** (v2 diag)
- exported patch: 175 lines; build `0.82+hudfield-v2`
  (sha f375dede→**3ea4cbe1…**), Diagnostics 6dc0e215→**e882fa19…**,
  zip **8dec95ef…**; strings verified; wine smoke OK
- zig now in `tools/zig-x86_64-linux-0.17.1/` (permanent; /tmp disappears on
  reboot — Windows.h→windows.h shim redone; document it in the script)
