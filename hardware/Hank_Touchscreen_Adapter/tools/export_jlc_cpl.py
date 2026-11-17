"""Create a JLCPCB-ready CPL from the final KiCad board.

KiCad's generic CSV position export mirrors B.Cu coordinates, which JLC's
upload parser does not accept as a normal CPL.  This export keeps the actual
positive board centroids and uses JLC's recognized column names.
"""
from pathlib import Path
import csv
import pcbnew

ROOT = Path(__file__).resolve().parents[1]
PCB = ROOT / "Hank_Touchscreen_Adapter.kicad_pcb"
OUT = ROOT / "manufacturing" / "2layer" / "Hank_Touchscreen_Adapter-CPL.csv"

board = pcbnew.LoadBoard(str(PCB))
rows = []
for footprint in board.GetFootprints():
    # Only the three SMD parts are eligible for assembly. The connectors are
    # intentionally through-hole and are hand-installed.
    if not (footprint.GetAttributes() & pcbnew.FP_SMD):
        continue
    pos = footprint.GetPosition()
    rows.append({
        "Designator": footprint.GetReference(),
        "Mid X": f"{pcbnew.ToMM(pos.x):.6f}",
        "Mid Y": f"{pcbnew.ToMM(pos.y):.6f}",
        "Layer": "Bottom" if footprint.IsFlipped() else "Top",
        "Rotation": f"{footprint.GetOrientation().AsDegrees():.1f}",
    })

rows.sort(key=lambda row: row["Designator"])
with OUT.open("w", newline="", encoding="utf-8") as handle:
    writer = csv.DictWriter(handle, fieldnames=["Designator", "Mid X", "Mid Y", "Layer", "Rotation"])
    writer.writeheader()
    writer.writerows(rows)

if {row["Designator"] for row in rows} != {"Q1", "R1", "R2"}:
    raise SystemExit("Unexpected SMD population; inspect the board before using this CPL")
print(f"Wrote JLCPCB CPL with {len(rows)} placements: {OUT}")
