#!/usr/bin/env python3.12
"""Generates the two LED bar PCBs (KiCad 7) and their fab files.

    python3 hardware/ledbar/build.py          # needs KiCad 7 (pcbnew + kicad-cli)

Outputs per board in hardware/ledbar/out/<name>/:
  <name>.kicad_pcb, gerbers.zip (Gerbers + drill), bom.csv, cpl.csv, drc.json

Boards
  side  4 × WS2812B, 41.5 × 11 mm — order 2 (left + right of the display).
        First pixel runs ~0.7 V lower through D1 so it accepts the ESP32's
        3.3 V data and re-drives 5 V levels for the rest ("sacrificial
        pixel"), no level-shifter chip needed.
  top  12 × WS2812B, 102.5 × 11 mm — order 1, above the display.

Each bar: wire pads IN (GND, DIN, 5V) on the left, OUT (GND, DOUT, 5V) on
the right. Chain: left bar (bottom→top) → top bar (left→right) → right bar
(top→bottom). F.Cu carries 5V rail + data, B.Cu is a solid GND plane.
"""

import csv
import subprocess
import sys
import zipfile
from pathlib import Path

import pcbnew

FP = Path("/usr/share/kicad/footprints")
OUT = Path(__file__).resolve().parent / "out"
W = 12.0          # board width (mm)
LED_Y = 5.0       # LED centre line
RAIL_Y = 10.7     # 5V rail centre (F.Cu, bottom edge)
RAIL_W = 1.0
CAP_Y = 8.6       # per-LED 100 nF row (below the LED courtyard)
DIODE_Y = 9.0
GND_VIA_Y = 1.5
SIG_W = 0.25
PWR_W = 0.5

PARTS = {  # value -> (description, manufacturer part number)
    "WS2812B": ("RGB LED 5050 with integrated driver", "Worldsemi WS2812B-B/W"),
    "100nF": ("Capacitor 100 nF 16 V X7R 0603", "Samsung CL10B104KO8NNNC"),
    "330R": ("Resistor 330 Ω 1 % 0603", "UNI-ROYAL 0603WAF3300T5E"),
    "1N4148W": ("Diode 1N4148W SOD-123", "Diodes Inc. 1N4148W-7-F"),
}
FOOTPRINTS = {
    "WS2812B": ("LED_SMD", "LED_WS2812B_PLCC4_5.0x5.0mm_P3.2mm"),
    "100nF": ("Capacitor_SMD", "C_0603_1608Metric"),
    "330R": ("Resistor_SMD", "R_0603_1608Metric"),
    "1N4148W": ("Diode_SMD", "D_SOD-123"),
    "PAD": ("TestPoint", "TestPoint_Pad_1.5x1.5mm"),
}


def mm(v: float) -> int:
    return pcbnew.FromMM(v)


def pt(x: float, y: float) -> "pcbnew.VECTOR2I":
    return pcbnew.VECTOR2I(mm(x), mm(y))


class Bar:
    def __init__(self, name: str, leds: int, pitch: float, first_pixel_diode: bool):
        self.name, self.n, self.pitch, self.diode = name, leds, pitch, first_pixel_diode
        self.board = pcbnew.BOARD()
        self.nets: dict[str, pcbnew.NETINFO_ITEM] = {}
        self.parts: list[tuple[str, str, float, float, float, str]] = []  # ref, value, x, y, rot, side
        self.x0 = 9.5
        self.length = self.x0 + (leds - 1) * pitch + 6.5

    # -- primitives ---------------------------------------------------------
    def net(self, name: str):
        if name not in self.nets:
            n = pcbnew.NETINFO_ITEM(self.board, name)
            self.board.Add(n)
            self.nets[name] = n
        return self.nets[name]

    def place(self, ref: str, value: str, x: float, y: float, rot: float = 0.0, pads=None):
        lib, fpname = FOOTPRINTS[value]
        fp = pcbnew.FootprintLoad(str(FP / f"{lib}.pretty"), fpname)
        fp.SetReference(ref)
        fp.SetValue(value)
        fp.SetPosition(pt(x, y))
        fp.SetOrientationDegrees(rot)
        fp.Reference().SetVisible(False)
        fp.Value().SetVisible(False)
        self.board.Add(fp)
        for num, netname in (pads or {}).items():
            fp.FindPadByNumber(str(num)).SetNet(self.net(netname))
        if value != "PAD":
            self.parts.append((ref, value, x, y, rot, "top"))
        return fp

    @staticmethod
    def pad(fp, num) -> tuple[float, float]:
        p = fp.FindPadByNumber(str(num)).GetPosition()
        return pcbnew.ToMM(p.x), pcbnew.ToMM(p.y)

    def track(self, netname: str, *points, width: float = SIG_W, layer=pcbnew.F_Cu):
        for (x1, y1), (x2, y2) in zip(points, points[1:]):
            t = pcbnew.PCB_TRACK(self.board)
            t.SetStart(pt(x1, y1))
            t.SetEnd(pt(x2, y2))
            t.SetWidth(mm(width))
            t.SetLayer(layer)
            t.SetNet(self.net(netname))
            self.board.Add(t)

    def via(self, netname: str, x: float, y: float):
        v = pcbnew.PCB_VIA(self.board)
        v.SetPosition(pt(x, y))
        v.SetDrill(mm(0.3))
        v.SetWidth(mm(0.6))
        v.SetNet(self.net(netname))
        self.board.Add(v)

    def text(self, s: str, x: float, y: float, size: float = 0.8, layer=pcbnew.F_SilkS):
        t = pcbnew.PCB_TEXT(self.board)
        t.SetText(s)
        t.SetPosition(pt(x, y))
        t.SetLayer(layer)
        t.SetTextSize(pcbnew.VECTOR2I(mm(size), mm(size)))
        t.SetTextThickness(mm(size * 0.15))
        self.board.Add(t)

    # -- layout -------------------------------------------------------------
    def build(self):
        L, b = self.length, self.board
        ds = b.GetDesignSettings()
        ds.SetCopperLayerCount(2)

        edge = pcbnew.PCB_SHAPE(b)
        edge.SetShape(pcbnew.SHAPE_T_RECT)
        edge.SetStart(pt(0, 0))
        edge.SetEnd(pt(L, W))
        edge.SetLayer(pcbnew.Edge_Cuts)
        edge.SetWidth(mm(0.1))
        b.Add(edge)

        # Wire pads: IN on the left, OUT on the right.
        pin_gnd = self.place("TP1", "PAD", 1.4, 1.6, pads={1: "GND"})
        pin_din = self.place("TP2", "PAD", 1.4, 6.0, pads={1: "DIN"})
        pin_5v = self.place("TP3", "PAD", 1.4, 10.4, pads={1: "+5V"})
        out_gnd = self.place("TP4", "PAD", L - 1.4, 1.6, pads={1: "GND"})
        out_dout = self.place("TP5", "PAD", L - 1.4, 6.0, pads={1: f"D{self.n}"})
        out_5v = self.place("TP6", "PAD", L - 1.4, 10.4, pads={1: "+5V"})
        for fp in (pin_gnd, out_gnd):
            x, y = self.pad(fp, 1)
            self.via("GND", x, y)

        # 5V rail along the bottom edge.
        self.track("+5V", (1.4, RAIL_Y), (L - 1.4, RAIL_Y), width=RAIL_W)
        for fp in (pin_5v, out_5v):
            x, y = self.pad(fp, 1)
            self.track("+5V", (x, y), (x, RAIL_Y), width=PWR_W)

        # Series resistor into the first pixel.
        r1 = self.place("R1", "330R", 4.3, 6.0, pads={1: "DIN", 2: "D0"})
        self.track("DIN", self.pad(pin_din, 1), self.pad(r1, 1))

        prev_dout = self.pad(r1, 2)
        for i in range(self.n):
            x = self.x0 + i * self.pitch
            vdd = "VDD1" if (i == 0 and self.diode) else "+5V"
            # Rotated 180°: DIN bottom-left, DOUT top-right, VSS top-left, VDD bottom-right.
            led = self.place(f"LED{i + 1}", "WS2812B", x, LED_Y, 180,
                             pads={1: vdd, 2: f"D{i + 1}", 3: "GND", 4: f"D{i}"})
            c = self.place(f"C{i + 1}", "100nF", x + 0.6, CAP_Y, 0, pads={1: "GND", 2: vdd})
            # data in
            self.track(f"D{i}", prev_dout, self.pad(led, 4))
            prev_dout = self.pad(led, 2)
            # ground: VSS up to a via, cap GND to a via
            vx, vy = self.pad(led, 3)
            self.track("GND", (vx, vy), (vx, GND_VIA_Y), width=PWR_W)
            self.via("GND", vx, GND_VIA_Y)
            cgx, cgy = self.pad(c, 1)
            self.track("GND", (cgx, cgy), (cgx - 1.1, cgy))
            self.via("GND", cgx - 1.1, cgy)
            # supply
            px, py = self.pad(led, 1)
            cvx, cvy = self.pad(c, 2)
            if vdd == "+5V":
                self.track("+5V", (px, py), (px, RAIL_Y), width=PWR_W)
                self.track("+5V", (cvx, cvy), (cvx, RAIL_Y), width=PWR_W)
            else:
                # Cathode (pad 1) to the pixel, anode to the 5V rail.
                d = self.place("D1", "1N4148W", x + 4.5, DIODE_Y, 0, pads={1: "VDD1", 2: "+5V"})
                kx, ky = self.pad(d, 1)
                ax, ay = self.pad(d, 2)
                self.track("VDD1", (px, py), (px, ky), (kx, ky), width=PWR_W)
                self.track("VDD1", (cvx, cvy), (px, cvy), width=PWR_W)
                self.track("+5V", (ax, ay), (ax, RAIL_Y), width=PWR_W)
        self.track(f"D{self.n}", prev_dout, self.pad(out_dout, 1))

        # Labels on the back, behind the wire pads (front is full of parts).
        back = pcbnew.B_SilkS
        for x, tag in ((1.4, "IN"), (L - 1.4, "OUT")):
            self.text("G", x, 1.6, 1.0, back)
            self.text("D", x, 6.0, 1.0, back)
            self.text("5V", x, 10.4, 1.0, back)
            self.text(tag, x + (3.0 if tag == "IN" else -3.2), 6.0, 1.0, back)
        self.text(f"SimRaceLogger LED {self.name} v1", L / 2, 3.6, 1.0, back)

        # Solid GND plane on the bottom.
        z = pcbnew.ZONE(b)
        z.SetLayer(pcbnew.B_Cu)
        z.SetNet(self.net("GND"))
        z.SetLocalClearance(mm(0.3))
        z.SetMinThickness(mm(0.25))
        z.SetPadConnection(pcbnew.ZONE_CONNECTION_FULL)
        o = z.Outline()
        o.NewOutline()
        for x, y in ((0.3, 0.3), (L - 0.3, 0.3), (L - 0.3, W - 0.3), (0.3, W - 0.3)):
            o.Append(mm(x), mm(y))
        b.Add(z)
        return self

    # -- outputs ------------------------------------------------------------
    def write(self) -> Path:
        d = OUT / self.name
        d.mkdir(parents=True, exist_ok=True)
        pcb = d / f"ledbar-{self.name}.kicad_pcb"
        self.board.Save(str(pcb))
        # Zone filling needs a board loaded with its project settings; a bare
        # BOARD() built in Python crashes the filler.
        loaded = pcbnew.LoadBoard(str(pcb))
        pcbnew.ZONE_FILLER(loaded).Fill(loaded.Zones())
        loaded.Save(str(pcb))
        self._bom(d / "bom.csv")
        self._cpl(d / "cpl.csv")
        return pcb

    def _bom(self, path: Path):
        groups: dict[str, list[str]] = {}
        for ref, value, *_ in self.parts:
            groups.setdefault(value, []).append(ref)
        with path.open("w", newline="") as f:
            w = csv.writer(f)
            w.writerow(["Item", "Designator", "Qty", "Value", "Footprint", "Description",
                        "Manufacturer Part Number"])
            for i, (value, refs) in enumerate(groups.items(), start=1):
                desc, mpn = PARTS[value]
                w.writerow([i, ",".join(refs), len(refs), value, FOOTPRINTS[value][1], desc, mpn])

    def _cpl(self, path: Path):
        # Placement from the actual footprints so it always matches the PCB.
        with path.open("w", newline="") as f:
            w = csv.writer(f)
            w.writerow(["Designator", "Mid X(mm)", "Mid Y(mm)", "Layer", "Rotation"])
            for fp in self.board.GetFootprints():
                if fp.GetValue() == "PAD":
                    continue
                p = fp.GetPosition()
                # KiCad's Y grows downwards; fab CPLs use Y up from the board origin.
                w.writerow([fp.GetReference(), f"{pcbnew.ToMM(p.x):.3f}", f"{W - pcbnew.ToMM(p.y):.3f}",
                            "Top", f"{fp.GetOrientationDegrees() % 360:.0f}"])


def kicad_cli(*args):
    return subprocess.run(["kicad-cli", *args], check=False, capture_output=True, text=True)


def fabricate(pcb: Path) -> dict:
    d = pcb.parent
    drc = d / "drc.txt"
    pcbnew.WriteDRCReport(pcbnew.LoadBoard(str(pcb)), str(drc), pcbnew.EDA_UNITS_MILLIMETRES, True)
    errors = parse_drc(drc.read_text())
    g = d / "gerbers"
    g.mkdir(exist_ok=True)
    for f in g.iterdir():
        f.unlink()
    kicad_cli("pcb", "export", "gerbers", "--layers", "F.Cu,B.Cu,F.Paste,F.SilkS,B.SilkS,F.Mask,B.Mask,Edge.Cuts",
              "--subtract-soldermask", "-o", str(g) + "/", str(pcb))
    kicad_cli("pcb", "export", "drill", "--format", "excellon", "--excellon-separate-th", "-o", str(g) + "/", str(pcb))
    with zipfile.ZipFile(d / "gerbers.zip", "w", zipfile.ZIP_DEFLATED) as z:
        for f in sorted(g.iterdir()):
            z.write(f, f.name)
    kicad_cli("pcb", "export", "svg", "--layers", "F.Cu,F.SilkS,F.Mask,Edge.Cuts", "--exclude-drawing-sheet", "--page-size-mode", "2",
              "-o", str(d / "preview-front.svg"), str(pcb))
    kicad_cli("pcb", "export", "svg", "--layers", "B.Cu,B.SilkS,Edge.Cuts", "--exclude-drawing-sheet", "--page-size-mode", "2", "--mirror",
              "-o", str(d / "preview-back.svg"), str(pcb))
    return {"board": pcb.name, "errors": len(errors), "details": errors}


# Checks that only matter inside the KiCad GUI (library table) are skipped.
IGNORED = {"lib_footprint_issues", "lib_footprint_mismatch"}


def parse_drc(text: str) -> list[str]:
    """Error-severity DRC entries (violations + unconnected) as text blocks."""
    blocks, cur = [], []
    for line in text.splitlines():
        if line.startswith("["):
            if cur:
                blocks.append(cur)
            cur = [line]
        elif cur and line.startswith("    "):
            cur.append(line)
    if cur:
        blocks.append(cur)
    out = []
    for b in blocks:
        kind = b[0][1:b[0].index("]")]
        if kind in IGNORED or not any("Severity: error" in l for l in b):
            continue
        out.append("\n".join(b))
    return out


def main() -> int:
    ok = True
    for bar in (Bar("side", 4, 9.0, first_pixel_diode=True), Bar("top", 12, 8.0, first_pixel_diode=False)):
        pcb = bar.build().write()
        res = fabricate(pcb)
        print(f"{res['board']}: {bar.length:.1f} x {W:.1f} mm, DRC errors: {res['errors']}")
        for e in res["details"][:25]:
            print("   ", e.replace("\n", "\n    "))
        ok &= res["errors"] == 0
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
