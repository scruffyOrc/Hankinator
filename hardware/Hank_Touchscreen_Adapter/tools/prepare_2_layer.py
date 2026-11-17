"""Prepare the manually packed Hank touchscreen adapter for FreeRouting.

The component placement is deliberately left untouched. This only replaces
the obsolete 54 x 55 mm outline, removes the former six-layer routing/zones,
and makes a two-copper-layer candidate for the autorouter.
"""
from pathlib import Path
import pcbnew

ROOT = Path(__file__).resolve().parents[1]
PCB = ROOT / "Hank_Touchscreen_Adapter.kicad_pcb"
MM = pcbnew.FromMM
board = pcbnew.LoadBoard(str(PCB))

# Packed component/silkscreen envelope plus practical assembly clearance.
left, top, right, bottom = 122.0, 105.0, 154.0, 148.0

# New routing attempt; retain the user's footprint placement.
for item in list(board.GetTracks()):
    board.Remove(item)
for zone in list(board.Zones()):
    board.Remove(zone)
for item in list(board.GetDrawings()):
    if item.GetLayer() == pcbnew.Edge_Cuts:
        board.Remove(item)

# Two copper layers for the first routing attempt.
board.SetCopperLayerCount(2)

def vec(x, y):
    return pcbnew.VECTOR2I(MM(x), MM(y))

for a, b in zip(
    [(left, top), (right, top), (right, bottom), (left, bottom)],
    [(right, top), (right, bottom), (left, bottom), (left, top)],
):
    edge = pcbnew.PCB_SHAPE(board)
    edge.SetShape(pcbnew.SHAPE_T_SEGMENT)
    edge.SetStart(vec(*a))
    edge.SetEnd(vec(*b))
    edge.SetWidth(MM(0.10))
    edge.SetLayer(pcbnew.Edge_Cuts)
    board.Add(edge)

pcbnew.SaveBoard(str(PCB), board)
print(f"Prepared {PCB}: {right-left:.1f} x {bottom-top:.1f} mm, two copper layers")
