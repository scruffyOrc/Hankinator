"""Correct the layer-count silkscreen after the two-layer redesign."""
from pathlib import Path
import pcbnew

ROOT = Path(__file__).resolve().parents[1]
PCB = ROOT / "Hank_Touchscreen_Adapter.kicad_pcb"

board = pcbnew.LoadBoard(str(PCB))
for drawing in board.GetDrawings():
    if isinstance(drawing, pcbnew.PCB_TEXT) and drawing.GetText() == "REV A  |  6 LAYER":
        drawing.SetText("REV A  |  2 LAYER")
        pcbnew.SaveBoard(str(PCB), board)
        print("Updated layer-count silkscreen")
        break
else:
    raise SystemExit("Existing layer-count silkscreen text was not found")
