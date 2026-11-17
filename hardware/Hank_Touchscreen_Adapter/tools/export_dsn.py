"""Export the current two-layer adapter placement to Specctra DSN."""
from pathlib import Path
import pcbnew

ROOT = Path(__file__).resolve().parents[1]
PCB = ROOT / "Hank_Touchscreen_Adapter.kicad_pcb"
DSN = ROOT / "routing" / "Hank_Touchscreen_Adapter_2layer.dsn"
DSN.parent.mkdir(exist_ok=True)

board = pcbnew.LoadBoard(str(PCB))
if board.GetCopperLayerCount() != 2:
    raise SystemExit("Refusing to export: this FreeRouting attempt is meant to be two-layer.")
if not pcbnew.ExportSpecctraDSN(board, str(DSN)):
    raise SystemExit("Specctra DSN export failed")
print(DSN)
