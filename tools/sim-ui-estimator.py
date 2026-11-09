#!/usr/bin/env python3
"""Offline simulation of the ui_diff.comp alpha estimator (no GPU, no numpy).

Purpose: the 09/11 tint test proved the synthetic UI layer is composited and
UI-localized, and that mode-3 flicker is the per-frame mismatch between the
game's real UI and our estimated alpha. Before touching the field again,
compare estimator variants against synthetic ground truth:
  * V0 current heuristic:  a = clamp(2 * max_channel(B - C))
  * V1/V2 white-model ratio: a = clamp(max(dB) / max(1 - maxC, k))
  * V3 luminance ratio (same, luma-based)
  * V4 per-channel max ratio with epsilon floor
Inputs model the real pipeline: C = captured HUD-less frame, B = backbuffer
alpha-blended over C with true coverage a0 and UI color U, plus per-frame
independent noise on both captures (tone-map/grain class noise). Metrics
measure exactly what the field sees: leakage over background (everywhere
shimmer), per-frame variance of the estimate (the blink source), alpha error
at AA edges (halos), and bias at opaque UI centers (where a=1 means the
generated frame reproduces B exactly and cannot flicker).

Run: python3 -I sim-ui-estimator.py   (stdlib only)
"""
import math
import random
from statistics import pstdev

W, H = 120, 72
FRAMES = 60
NOISE = 0.012  # per-pixel per-frame sigma on both captures
rng = random.Random(20261109)

# ---------- ground truth scene ----------
def smooth_scene():
    """Low-frequency 'world' 0.05..0.95 with two blobs + gradient."""
    img = []
    for y in range(H):
        row = []
        for x in range(W):
            v = 0.28 + 0.30 * x / W
            v += 0.22 * math.exp(-(((x - 0.66*W)**2 + (y - 0.30*H)**2) / (2*(0.18*W)**2)))
            v += 0.18 * math.exp(-(((x - 0.20*W)**2 + (y - 0.75*H)**2) / (2*(0.22*W)**2)))
            row.append(min(0.95, max(0.05, v)))
        img.append(row)
    return img

def aa(v):  # smoothstep-like 1px AA ramp
    return min(1.0, max(0.0, v))

def ground_truth():
    """Per pixel: a0 (coverage) and U (UI color, or None where a0==0)."""
    cx, cy, r0, r1 = 0.5*W, 0.5*H, 0.24*min(W,H), 0.40*min(W,H)
    A = [[0.0]*W for _ in range(H)]
    U = [[None]*W for _ in range(H)]
    # dark-UI case (HUD panels are often dark on bright sky): one corner block, U=(0.15,0.15,0.18)
    for y in range(H):
        for x in range(W):
            r = math.hypot(x - cx, y - cy)
            # wheel ring with AA borders
            a_ring = aa((r - r0)) * aa((r1 - r))
            # minimap: rounded square top-right
            mx, my, mr = W-18, 14, 10
            a_map = aa(min(mr - abs(x-mx), mr - abs(y-my)) + 0.5) * 1.0
            # translucent panel (a=0.5) bottom-left
            a_panel = 0.5 if (8 <= x < 56 and H-16 <= y < H-6) else 0.0
            # dark UI block (dark HUD element over the world) mid-right edge
            a_dark = aa(min(29 - abs(x-(W-24)), 35 - abs(y-(H-24))) + 0.5) * 0.85
            a0 = max(a_ring, a_map, a_panel, a_dark)
            A[y][x] = min(1.0, a0)
            if a0 > 0.01:
                if a_dark >= a_ring and a_dark >= a_map and a_dark >= a_panel:
                    U[y][x] = (0.15, 0.15, 0.18)
                elif (x - cx)**2 + (y - cy)**2 < (r0*1.4)**2 and r < r1:  # colored icons on ring
                    U[y][x] = (1.0, 0.78, 0.30)
                elif r >= r0 and r <= r1:
                    U[y][x] = (0.95, 0.95, 0.95)
                elif a_map:
                    U[y][x] = (0.85, 0.85, 0.90)
                else:
                    U[y][x] = (0.60, 0.60, 0.60)
    return A, U

def edge_mask(A):
    E = [[0]*W for _ in range(H)]
    for y in range(1, H-1):
        for x in range(1, W-1):
            d = max(abs(A[y][x]-A[y+1][x]), abs(A[y][x]-A[y-1][x]),
                    abs(A[y][x]-A[y][x+1]), abs(A[y][x]-A[y][x-1]))
            E[y][x] = 1 if d > 0.05 else 0
    return E

# ---------- estimators (float 0..1, clamped like the shader) ----------
def clamp01(v): return min(1.0, max(0.0, v))

def V0(B, C):
    d = max(B[0]-C[0], B[1]-C[1], B[2]-C[2])
    return clamp01(2.0*d)

def ratio(d, cmax, k):
    return clamp01(d / max(1.0 - cmax, k))

def V1(B, C):  # white-model unmix, conservative floor
    d = max(B[0]-C[0], B[1]-C[1], B[2]-C[2])
    return ratio(max(0.0, d), max(C[0], C[1], C[2]), 0.35)

def V2(B, C):  # sharper floor
    d = max(B[0]-C[0], B[1]-C[1], B[2]-C[2])
    return ratio(max(0.0, d), max(C[0], C[1], C[2]), 0.15)

def V3(B, C):  # luminance form
    lb = 0.2126*B[0]+0.7152*B[1]+0.0722*B[2]
    lc = 0.2126*C[0]+0.7152*C[1]+0.0722*C[2]
    return ratio(max(0.0, lb-lc), lc, 0.35)

def V4(B, C):  # per-channel max ratio, eps floor
    m = 0.0
    for i in range(3):
        if B[i] > C[i]:
            m = max(m, clamp01((B[i]-C[i]) / (1.0 - C[i] + 0.05)))
    return m

def make_v(db):
    def f(B, C):
        d = max(B[0]-C[0], B[1]-C[1], B[2]-C[2]) - db
        return ratio(max(0.0, d), max(C[0], C[1], C[2]), 0.15)
    return f

DB_FINAL = 0.012

def V6(B, C):
    """Two-sided candidate for the shader: white-UI and dark-UI coverage, deadbanded."""
    cmax = max(C[0], C[1], C[2])
    dup = max(B[0]-C[0], B[1]-C[1], B[2]-C[2]) - DB_FINAL
    ddn = max(C[0]-B[0], C[1]-B[1], C[2]-B[2]) - DB_FINAL
    a_up = clamp01(dup / max(1.0 - cmax, 0.15)) if dup > 0.0 else 0.0
    a_dn = clamp01(ddn / max(cmax, 0.15)) if ddn > 0.0 else 0.0
    return max(a_up, a_dn)

ESTS = {"V0-current": V0}
ESTS.update({f"V5-db{db:.3f}": make_v(db) for db in (0.01, 0.015, 0.02)})
ESTS["V6-twosided"] = V6

def main():
    base = smooth_scene()
    A, U = ground_truth()
    E = edge_mask(A)
    ui = [(y, x) for y in range(H) for x in range(W) if A[y][x] > 0.001]
    bg = [(y, x) for y in range(H) for x in range(W) if A[y][x] <= 0.001]
    band = [(y, x) for y in range(H) for x in range(W) if E[y][x] and A[y][x] < 0.99]
    n_ui, n_bg, n_band = len(ui), len(bg), len(band)

    stats = {name: {"mae_ui":0.0,"mae_bg":0.0,"edge":0.0,"center":0.0,
                    "framesum":[0.0]*0, "vals":[]} for name in ESTS}
    per_frame_bg = {name: [] for name in ESTS}   # mean bg estimate per frame (drift probe)
    bg_pix_var = {name: [] for name in ESTS}     # per-pixel std over frames, avg

    sample_bg = bg[::97]  # deterministic subset for variance tracking
    sample_ui = [p for p in ui][::59]
    all_samples = sample_bg + sample_ui
    acc_frames = {name: {p: [] for p in all_samples} for name in ESTS}

    for f in range(FRAMES):
        # world drifts slightly between frames (also what the interpolator differs on)
        drift = 0.03 * math.sin(f / 6.0)
        Ct = {}; Bt = {}
        for y in range(H):
            for x in range(W):
                c = base[y][x] + drift*(0.3 + 0.7*x/W)
                c = min(0.95, max(0.05, c))
                c3 = (min(1.0, max(0.0, c + rng.gauss(0.0, NOISE))),) * 3
                a0 = A[y][x]
                if U[y][x]:
                    u = U[y][x]
                    b3 = tuple(a0*u[i] + (1-a0)*c3[i] + rng.gauss(0,NOISE) for i in range(3))
                    b3 = tuple(min(1.0,max(0.0,v)) for v in b3)
                else:
                    # B and C agree in content but NOT in noise: two captures of the
                    # same world carry independent tone-map/grain noise, and THAT is
                    # what every estimator must survive.
                    b3 = (min(1.0, max(0.0, c + rng.gauss(0.0, NOISE))),) * 3
                Ct[(y,x)] = c3; Bt[(y,x)] = b3
        for name, fn in ESTS.items():
            s = stats[name]
            bgvals_now = []
            for (y, x) in ui:
                a = fn(Bt[(y,x)], Ct[(y,x)])
                s["mae_ui"] += abs(a - A[y][x])
                if A[y][x] > 0.99:
                    s["center"] += (a - 1.0)
                if E[y][x] and A[y][x] < 0.99:
                    s["edge"] += (a - A[y][x])**2
            for (y, x) in bg:
                a = fn(Bt[(y,x)], Ct[(y,x)])
                s["mae_bg"] += a  # true a0==0: any estimate > 0 is leak
                bgvals_now.append(a)
            per_frame_bg[name].append(sum(bgvals_now)/len(bgvals_now))
            for p in all_samples:
                acc_frames[name][p].append(fn(Bt[p], Ct[p]))
    print(f"scene {W}x{H} frames {FRAMES} noise {NOISE}  UI px {n_ui}  edge px {n_band}  bg px {n_bg}")
    print(f"{'variant':<12} {'mae_ui':>8} {'edge_rmse':>9} {'center_bias':>11} {'leak_bg':>8} "
          f"{'ema_bg':>8} {'raw_bg':>8} {'ema_ui':>8} {'raw_ui':>8}")
    for name in ESTS:
        s = stats[name]
        mae_ui = s["mae_ui"]/ (FRAMES*n_ui)
        edge = math.sqrt(s["edge"]/(FRAMES*n_band))
        center = s["center"]/(FRAMES*max(1,sum(1 for (y,x) in ui if A[y][x]>0.99)))
        leak = s["mae_bg"]/(FRAMES*n_bg)
        flick_bg = sum(pstdev(acc_frames[name][p]) for p in sample_bg)/len(sample_bg)
        flick_ui = sum(pstdev(acc_frames[name][p]) for p in sample_ui)/len(sample_ui)
        def ema_std(ps):
            out = []
            for p in ps:
                v = acc_frames[name][p][0]; seq = [v]
                for x in acc_frames[name][p][1:]:
                    v += 0.35*(x - v); seq.append(v)
                out.append(pstdev(seq[5:]))
            return sum(out)/len(out)
        print(f"{name:<12} {mae_ui:8.4f} {edge:9.4f} {center:11.4f} {leak:8.4f} "
              f"{ema_std(sample_bg):9.4f} {flick_bg:9.4f} {ema_std(sample_ui):9.4f} {flick_ui:9.4f}")

if __name__ == "__main__":
    main()
