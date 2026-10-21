#!/usr/bin/env python3
"""Parametric case for the Waveshare ESP32-S3-Touch-LCD-4.3 + LED bars.

    pip install manifold3d trimesh
    python3 hardware/case/build.py            # writes hardware/case/out/*.stl

Parts (print the front face-down, no supports needed):
  case_front.stl     front plate + walls, display window, 20 LED windows
  case_back.stl      back lid with 1/4"-20 camera-thread nut pocket
  diffuser_top.stl   1 mm light diffuser behind the 12 top LED windows
  diffuser_side.stl  same for one side (print 2)
  test_frame.stl     first 5 mm of the front only — quick test print to check
                     window, LED windows and fit before the full case

!! The display module dimensions below are from Waveshare's spec table and
!! the standard 4.3" 800×480 panel, NOT measured. Measure your board with a
!! caliper, correct MODULE_* and USB_*, re-run, then order.
"""

from pathlib import Path

import trimesh
from manifold3d import Manifold

OUT = Path(__file__).resolve().parent / "out"

# --- Display module (VERIFY with a caliper) -------------------------------
MODULE_W = 106.1        # PCB/glass outline width
MODULE_H = 67.8         # height
MODULE_T = 12.0         # thickness incl. parts on the back
ACTIVE_W = 95.04        # active area (standard 4.3" 800x480)
ACTIVE_H = 53.86
ACTIVE_DY = 0.0         # active area centre offset from module centre (+ = up)
USB_WALL = "right"      # wall with the USB-C plug: "left", "right", "bottom"
USB_POS = 0.0           # offset along that wall from its centre (mm)
USB_DEPTH = 6.0         # plug centre measured from the module's front glass (mm)

# --- Window: 800×360 of the 800×480 panel (2.22:1, like the simulator) ------
WINDOW_W = ACTIVE_W
WINDOW_H = ACTIVE_H * 360 / 480

# --- LED bars (must match hardware/ledbar/build.py) ------------------------
BAR_W = 12.0
BAR_T = 1.0 + 1.6       # 1.0 mm PCB + WS2812B height
TOP_BAR_L, TOP_LEDS, TOP_PITCH = 104.0, 12, 8.0
SIDE_BAR_L, SIDE_LEDS, SIDE_PITCH = 43.0, 4, 9.0
LED_X0 = 9.5            # first LED from the IN end
LED_LINE = 5.0          # LED centre line from the bar's top edge (local y)

# --- Case -------------------------------------------------------------------
WALL = 2.4
PLATE = 2.0             # front plate thickness
GLASS_GAP = BAR_T + 0.3 # bars sit between plate and display glass
CLEAR = 0.5             # play around the module
BACK = 2.4
DEPTH = PLATE + GLASS_GAP + MODULE_T + 3.0  # inner depth to the lid
LED_WIN = (6.0, 4.0)    # LED window in the front plate (w, h)
INSERT_D = 4.0          # M3 heat-set insert hole
BOSS_D = 7.0
SCREW_D = 3.4
NUT_AF = 11.3           # 1/4"-20 hex nut across flats + play
NUT_T = 5.8

INNER_W = MODULE_W + 2 * (BAR_W - (MODULE_W - ACTIVE_W) / 2 + 1.0) + 2 * CLEAR
INNER_H = MODULE_H + 2 * CLEAR
OUTER_W = INNER_W + 2 * WALL
OUTER_H = INNER_H + 2 * WALL
ACTIVE_CY = ACTIVE_DY   # case origin = module centre


def box(w, h, d, x=0.0, y=0.0, z=0.0) -> Manifold:
    """Axis-aligned box; x/y centred, z from `z` to `z + d`."""
    return Manifold.cube([w, h, d], center=False).translate([x - w / 2, y - h / 2, z])


def cyl(d, h, x=0.0, y=0.0, z=0.0, n=48) -> Manifold:
    return Manifold.cylinder(h, d / 2, d / 2, n).translate([x, y, z])


def rounded_box(w, h, d, r, z=0.0) -> Manifold:
    corners = [cyl(2 * r, d, sx * (w / 2 - r), sy * (h / 2 - r), z) for sx in (-1, 1) for sy in (-1, 1)]
    return Manifold.batch_hull(corners)


# --- LED positions in case coordinates (x right, y up) ----------------------
def led_positions():
    win_top = ACTIVE_CY + WINDOW_H / 2
    leds = []
    # Top bar mounted as drawn in KiCad (IN left, wire pads on top): its LED
    # line is LED_LINE below the bar's upper edge.
    top_y = win_top + 1.0 + (BAR_W - LED_LINE)
    for i in range(TOP_LEDS):
        leds.append(("top", -TOP_BAR_L / 2 + LED_X0 + i * TOP_PITCH, top_y))
    # Side bars turned 90°: left one IN at the bottom (chain goes up), right
    # one IN at the top (chain goes down); upper bar edge faces outwards.
    side_cy = ACTIVE_CY - 1.0
    x_left = -WINDOW_W / 2 - 1.0 - (BAR_W - LED_LINE)
    for i in range(SIDE_LEDS):
        leds.append(("left", x_left, side_cy - SIDE_BAR_L / 2 + LED_X0 + i * SIDE_PITCH))
    for i in range(SIDE_LEDS):
        leds.append(("right", -x_left, side_cy + SIDE_BAR_L / 2 - LED_X0 - i * SIDE_PITCH))
    return leds


def bar_rects():
    """Footprints of the three bars (cx, cy, w, h) for locating ribs."""
    win_top = ACTIVE_CY + WINDOW_H / 2
    x_side = WINDOW_W / 2 + 1.0 + BAR_W / 2
    return [
        (0.0, win_top + 1.0 + BAR_W / 2, TOP_BAR_L, BAR_W),
        (-x_side, ACTIVE_CY - 1.0, BAR_W, SIDE_BAR_L),
        (x_side, ACTIVE_CY - 1.0, BAR_W, SIDE_BAR_L),
    ]


BOSSES = [(sx * (INNER_W / 2 - BOSS_D / 2 - 0.2), sy * (INNER_H / 2 - BOSS_D / 2 - 0.2))
          for sx in (-1, 1) for sy in (-1, 1)]


def front() -> Manifold:
    body = rounded_box(OUTER_W, OUTER_H, PLATE + DEPTH, 4.0)
    cavity = box(INNER_W, INNER_H, DEPTH + 1, z=PLATE)
    shell = body - cavity

    # Display window with a 45° chamfer so the edge doesn't shade the image.
    t = PLATE
    window = Manifold.batch_hull([
        box(WINDOW_W + 2 * t, WINDOW_H + 2 * t, 0.01, y=ACTIVE_CY, z=-0.01),
        box(WINDOW_W, WINDOW_H, 0.01, y=ACTIVE_CY, z=t + 0.01),
    ])
    shell -= window
    shell -= box(WINDOW_W, WINDOW_H, t + 1, y=ACTIVE_CY, z=-0.5)

    # LED windows + recess for the diffuser strips on the inside.
    leds = led_positions()
    for kind, x, y in leds:
        w, h = LED_WIN if kind == "top" else LED_WIN[::-1]
        shell -= box(w, h, PLATE + 1, x, y, -0.5)
    for group in ("top", "left", "right"):
        xs = [p[1] for p in leds if p[0] == group]
        ys = [p[2] for p in leds if p[0] == group]
        w, h = LED_WIN if group == "top" else LED_WIN[::-1]
        shell -= box(max(xs) - min(xs) + w + 2, max(ys) - min(ys) + h + 2, 1.0,
                     (max(xs) + min(xs)) / 2, (max(ys) + min(ys)) / 2, PLATE - 1.0)

    # Light separators between LEDs (stop 0.3 mm short of the PCB).
    rib_h = 1.3
    tops = [p for p in leds if p[0] == "top"]
    for a, b in zip(tops, tops[1:]):
        shell += box(1.0, 5.5, rib_h, (a[1] + b[1]) / 2, a[2], PLATE)
    for side in ("left", "right"):
        ps = [p for p in leds if p[0] == side]
        for a, b in zip(ps, ps[1:]):
            shell += box(5.5, 1.0, rib_h, a[1], (a[2] + b[2]) / 2, PLATE)

    # Glass support: a rail along the module's bottom edge; at the top the
    # glass rests on the back of the top LED bar.
    gy = -MODULE_H / 2 + 2.0
    shell += box(MODULE_W - 20, 3.0, GLASS_GAP, 0, gy, PLATE)
    # but never in the window
    shell -= box(WINDOW_W, WINDOW_H, GLASS_GAP + 1, y=ACTIVE_CY, z=PLATE - 0.5)

    # Screw bosses for M3 heat-set inserts.
    for x, y in BOSSES:
        shell += cyl(BOSS_D, DEPTH, x, y, PLATE)
        shell -= cyl(INSERT_D, 8.0, x, y, PLATE + DEPTH - 8.0)

    # USB-C opening, open towards the back so the lid closes over the cable.
    z0 = PLATE + GLASS_GAP + USB_DEPTH - 4.5
    zlen = PLATE + DEPTH - z0 + 1
    if USB_WALL in ("left", "right"):
        sx = 1 if USB_WALL == "right" else -1
        shell -= box(WALL + 2, 14.0, zlen, sx * (INNER_W / 2 + WALL / 2), USB_POS, z0)
    else:
        shell -= box(14.0, WALL + 2, zlen, USB_POS, -(INNER_H / 2 + WALL / 2), z0)
    return shell


def back() -> Manifold:
    # Flat inner face (z = 0) goes on the print bed; the nut boss points up.
    lid = rounded_box(OUTER_W, OUTER_H, BACK, 4.0)
    for x, y in BOSSES:
        lid -= cyl(SCREW_D, BACK + 4, x, y, -2)
        lid -= cyl(6.2, 1.6, x, y, BACK - 1.6)       # screw head counterbore
    # 1/4"-20 camera thread: hex pocket opened from the inside, running into
    # an outer boss; the screw pulls the nut against the boss's end wall.
    boss_h = NUT_T + 1.5
    lid += cyl(20.0, boss_h, 0, 0, BACK)
    r_hex = NUT_AF / 2 / 0.8660254
    pocket_top = BACK + boss_h - 1.5
    lid -= Manifold.cylinder(pocket_top + 2.0, r_hex, r_hex, 6).translate([0, 0, -2.0])
    lid -= cyl(6.8, BACK + boss_h + 4, 0, 0, -2)
    return lid


def diffuser(n, pitch) -> Manifold:
    return box((n - 1) * pitch + LED_WIN[0] + 1.6, LED_WIN[1] + 1.6, 0.95)


def export(m: Manifold, name: str) -> Path:
    mesh = m.to_mesh()
    tm = trimesh.Trimesh(vertices=mesh.vert_properties[:, :3], faces=mesh.tri_verts, process=True)
    if not tm.is_watertight:
        raise SystemExit(f"{name}: mesh is not watertight")
    OUT.mkdir(exist_ok=True)
    path = OUT / f"{name}.stl"
    tm.export(path)
    return path


def main() -> int:
    parts = {
        "case_front": front(),
        "case_back": back(),
        "diffuser_top": diffuser(TOP_LEDS, TOP_PITCH),
        "diffuser_side": diffuser(SIDE_LEDS, SIDE_PITCH),
    }
    parts["test_frame"] = parts["case_front"] ^ box(OUTER_W + 2, OUTER_H + 2, PLATE + GLASS_GAP + 0.1)
    for name, m in parts.items():
        b = m.bounding_box()
        size = [b[3] - b[0], b[4] - b[1], b[5] - b[2]]
        print(f"{name:14s} {size[0]:6.1f} x {size[1]:5.1f} x {size[2]:5.1f} mm  -> {export(m, name).name}")
    print(f"window {WINDOW_W:.1f} x {WINDOW_H:.1f} mm, case {OUTER_W:.1f} x {OUTER_H:.1f} x "
          f"{PLATE + DEPTH + BACK:.1f} mm")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
