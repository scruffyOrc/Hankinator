"""Add and fill external-layer GND pours for the compact routed board."""
from pathlib import Path
import pcbnew

ROOT = Path(__file__).resolve().parents[1]
PCB = ROOT / "Hank_Touchscreen_Adapter.kicad_pcb"
MM = pcbnew.FromMM

board = pcbnew.LoadBoard(str(PCB))
if board.GetCopperLayerCount() != 2:
    raise SystemExit("This pour script is specific to the two-layer release candidate")

for zone in list(board.Zones()):
    board.Remove(zone)

gnd = board.FindNet("GND")
if gnd is None:
    raise SystemExit("GND net was not found")

def point(x, y):
    return pcbnew.VECTOR2I(MM(x), MM(y))

for layer in (pcbnew.F_Cu, pcbnew.B_Cu):
    zone = pcbnew.ZONE(board)
    zone.SetNet(gnd)
    zone.SetLayer(layer)
    zone.SetLocalClearance(MM(0.25))
    zone.SetMinThickness(MM(0.20))
    zone.SetPadConnection(pcbnew.ZONE_CONNECTION_FULL)
    outline = zone.Outline()
    outline.NewOutline()
    for x, y in ((122.4, 105.4), (153.6, 105.4), (153.6, 147.6), (122.4, 147.6)):
        outline.Append(point(x, y))
    board.Add(zone)

pcbnew.ZONE_FILLER(board).Fill(board.Zones())
pcbnew.SaveBoard(str(PCB), board)
print("Added and filled F.Cu and B.Cu GND pours")
