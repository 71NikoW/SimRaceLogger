# Prototype order (PCBWay)

Everything for one prototype display, as uploaded to PCBWay. Build the
package with:

```bash
python3.12 hardware/ledbar/build.py && python3 hardware/case/build.py
cd hardware && zip -r SimRaceLogger-prototype.zip ORDER.md ledbar/out/*/gerbers.zip \
    ledbar/out/*/bom.csv ledbar/out/*/cpl.csv case/out/case_*.st* case/out/diffuser_*.st*
```

## 1. LED bars — PCB + assembly (two separate orders)

| | top | side |
|---|---|---|
| Files | `ledbar/out/top/gerbers.zip`, `bom.csv`, `cpl.csv` | `ledbar/out/side/…` |
| Size | 104 × 12 mm | 43 × 12 mm |
| PCB qty | 5 | 5 |
| Assembled qty | 2 (1 + spare) | 4 (2 + 2 spare) |
| Layers / thickness | 2 / **1.0 mm** | 2 / **1.0 mm** |
| Solder mask / silk | black / white | black / white |
| Surface | HASL lead free | HASL lead free |
| Assembly | Turnkey, top side, 1 unique part side | same |

Check PCBWay's DFM and part-orientation review: pin 1 of every LED must sit
at the corner marked "1" on the silkscreen.

## 2. Case — 3D printing

Upload the `.step` files (PCBWay's 3D printing form takes STEP; the `.stl`
files are the same parts for your own printer).

| File | Qty | Material |
|------|-----|----------|
| `case_front.step` | 1 | MJF/SLS **PA12 nylon, black** (or FDM PETG black) |
| `case_back.step` | 1 | same |
| `diffuser_top.step` | 1 | SLA **white resin** or SLS PA12 white (unpainted) |
| `diffuser_side.step` | 2 | same |

**Risk:** the case uses datasheet dimensions of the Waveshare
ESP32-S3-Touch-LCD-4.3 (106.1 × 67.8 mm), not a measured board. If the board
differs, the case won't fit — measure first if you can.

## 3. Not at PCBWay

Waveshare ESP32-S3-Touch-LCD-4.3, USB-C data cable, 4× M3 heat-set inserts
(4.0 mm hole), 4× M3×8 screws, 1/4"-20 nut, 26 AWG wire — see docs/BUILD.md.
