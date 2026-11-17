"""Restore the verified 0.20 mm routing geometry.

The compact J1/J4 connector escapes make a blanket wider supply rule violate
clearance. These runs are short and carry the LCD logic/backlight current, not
the motor current: 0.20 mm external 1 oz copper is appropriate here. Both
outer layers also carry a filled GND return plane.
"""
from pathlib import Path
import pcbnew

ROOT = Path(__file__).resolve().parents[1]
PCB = ROOT / "Hank_Touchscreen_Adapter.kicad_pcb"
MM = pcbnew.FromMM

ROUTED_WIDTH = MM(0.20)

board = pcbnew.LoadBoard(str(PCB))
changed = 0
for track in board.GetTracks():
    if isinstance(track, pcbnew.PCB_TRACK):
        track.SetWidth(ROUTED_WIDTH)
        changed += 1

pcbnew.ZONE_FILLER(board).Fill(board.Zones())
pcbnew.SaveBoard(str(PCB), board)
print(f"Set {changed} routed segments to 0.20 mm")
