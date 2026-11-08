# Draft report for Mor3a (Nexus post/message) — ready to paste, English

Target: https://www.nexusmods.com/reddeadredemption2/mods/ … (user's mod page),
author **Mor3a**. Written for the author; logs available on request.

---

**Subject: RDR2 + OptiScaler (Vulkan SR) + Intel Arc — HUD-less capture never
satisfies; mod blocks on "HUD-less post-tone-map pass missing" forever. Also:
FSRFG needs 3 pacing queues Intel's driver doesn't expose; DLSS-G menu option
is a trap on Arc.**

Hi Mor3a — first, this mod is remarkable, and it got closer than anything else
to real frame gen on Intel Arc. Testing setup: RDR2 1.0.1491.50 (Epic), Vulkan
renderer, **Intel Arc B390** (Win11 26300), **OptiScaler 0.9.4** doing the
upscaling (XeSS 2.0.2.68 through it), your mod 1.0beta "With Amd Support",
FGProvider=XeSS. One extra piece of open-source glue on my side
(`zz_ngxbridge`, MIT — attached) exists only to make the game's
`LoadLibrary("nvngx.dll")` return a path ending in `nvngx.dll` instead of
OptiScaler.asi (your capture-module gate rejects the OptiScaler HMODULE —
that's why stock setups show FG-109 with OptiScaler; with the bridge the gate
passes and inputs flow, so you can ignore FG-109 below, it's solved).

**Symptom (100% reproducible):** the NGX/DLSS-SR input pipeline works perfectly
— `FG input pipeline v2: accepted SR handle=... render=2293x960 output=3440x1440`,
`NGX inputs first valid: depth 2293x960 fmt=130, mvec fmt=83, jitter=(0.0000,-0.1667)
mvScale=(1.0,1.0)`, telemetry `input=` climbs past 700 — but then, forever:
`HUD-less post-tone-map pass missing for over 500 ms` (xess_provider.cpp:691),
`enabled=0`, `hud=0`, no FG frames ever. No INI key avoids this
(TagHUDLess/TagUI are Streamline/DLSS-G paths).

**Root cause (reading the 0.82 source you published — strings say 1.0beta
behaves the same):** `hud_capture.cpp` only marks a frame HUD-less from a
**render pass** (WrapBeginRenderPass/2/Rendering, hud_capture.cpp:130-148 and
277-295) whose subpass-0 color attachment view resolves to a bridge virtual
swapchain image, full extent, exact swapchain format, samples=1,
loadOp==LOAD, initialLayout≠UNDEFINED, layerCount==1 — and the submit must be
in the same acquire epoch. That's exactly right when the *game* renders to
your virtual swapchain (your intended DX12-bridge flow). But when a
**third-party Vulkan upscaler (OptiScaler XeSS/DLSR)** is in the middle, the
final tone-mapped frame reaches the swapchain image via
`vkCmdBlitImage`/`vkCmdCopyImage` (SR output image → swapchain image), never
as a direct render-pass attachment write. The signature can never fire, so the
gate can never open. This is generic to any external Vulkan SR, not just
OptiScaler.

**Proposed fix (I built it — patch attached, opt-in via
`CapturePresentTransfers=1`, default off, three files: config.h/config.cpp/
hud_capture.cpp):** wrap `vkCmdCopyImage`/`vkCmdBlitImage` in the same device
chain as your existing `HudCaptureWrap` entries; when the destination is a
virtual swapchain image and there's a single full-extent color region
(mip 0, layer 0, depth 1), take the capture **after** the real call, using the
transfer's dst layout (TRANSFER_DST is required during the blit by spec;
`RecordCapture` restores the old layout afterwards so the game's barriers stay
consistent). Honest tradeoff I couldn't solve from outside: a present transfer
is *as-presented*, so if the game already composited HUD into the image, the
HUD is inside the snapshot (ghosting risk) — you know the pipeline well enough
to decide whether an overlay-draw heuristic or a per-swapchain-image epoch rule
could filter that; my patch is a functional proof-of-concept, not necessarily
the shape you'd ship.

**Two other findings from the same field run:**
1. **FSRFG on Intel Arc (your "With Amd Support"):** fails in-menu with
   FG-3xx "FSR Vulkan features unavailable … Three independent pacing queues
   are unavailable". FidelityFX Frame Generation Vulkan needs 3 independent
   queues; Intel's driver doesn't expose them. Consider a note in your docs —
   Arc users will hit this.
2. **DLSS-G menu on Arc:** selecting DLSS-G + restart in the in-game menu hits
   `Live DLSS-G setup failed` (xess_provider.cpp:493) — ampere_boot only
   accepts RTX families, so on Arc this is guaranteed, and the error is sticky
   until a full game restart (looked like a crash-of-FG in my logs, FG-201 at
   99 s; it was self-inflicted by the menu). A one-line "not supported on this
   GPU" pre-check in the menu would save people the confusion.

Everything above has log evidence (RDR2VulkanFG.log 517 KB + the bridge log);
I'll paste or send whatever's useful. Willing to test any build or idea you
have for the HUD-less problem — I can build the public source myself now
(zig cross-compile on Linux works fine, FYI).

---

Attachments to include when sending: `patches/hud-capture-present-transfer.patch`,
the two logs, and (optionally) the zz_ngxbridge release zip.

---

**UPDATE after field test (same hardware, same night — FG now RUNS at 2x):**
I rebuilt from your public 0.82 source and **the HUD-less capture matches**
(`HUD capture first success: frame=61 ...` → `enabled=1`, `displayed=2`,
~36 real / ~72 displayed fps at 3440x1440 on the B390). So the blocker above
is specific to the **1.0beta binary**, which never matched the signature under
the same OptiScaler delivery (`hud=0` forever, identical INI/setup). Likely a
regression in the beta's capture gate for third-party Vulkan SR delivery —
worth checking on your side. (My `CapturePresentTransfers` blit-capture patch
ended up unnecessary in practice: 0 hits — the normal render-pass signature
worked fine on 0.82. Keep or drop as you see fit.)

Two follow-ups from the working run:
1. **Frame-multiplier switching never reached the mod in my run.** The combo is
   implemented for XeSS (menu.cpp, 2x/3x/4x labels, request encoding looks right, and
   `Rt().frames` is re-read + `SetInterpolations` re-called every present, so runtime
   switching should work). But my session log contains zero `menu:` / `hotkey:` lines — no
   click or End-key event ever registered. My build now logs menu open/close, first clicks
   seen in the game's message queue, and focus-rejected hotkeys to find where it drops;
   happy to report which leg fails. (INI `DLSSGFrames` works fine as a fallback;
   `XeSS MFG experimental unlock: validated maximum=3 (4x)`.)
2. **Animated/translucent UI (RDR2's weapon and item wheels) flickers at 2x.** Static
   HUD is fine, so the SDK's `XEFG_SWAPCHAIN_UI_MODE_BACKBUFFER_HUDLESS` compositing works
   for opaque HUD; the semi-transparent animated radial wheels don't survive temporal
   reconstruction on interpolated frames and blink. I considered wiring the orphaned
   `HudMaskApply` (hud_mask.cpp:119, no callers in public 0.82) as an alternative and
   concluded it cannot help here: generated frames never exist as a readable resource
   (they are produced inside `Present()` on the D3D12 side), and masking the *input*
   HUD-less color would smear HUD temporally instead. Any guidance on wheel artifacts
   under BACKBUFFER_HUDLESS? (Or is `HudMaskApply` the abandoned precursor of exactly this?)

Thanks either way — this is the first frame generation I've seen running on
Intel Arc under Vulkan, on a real game.
