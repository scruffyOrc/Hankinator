"""Import the FreeRouting result into the two-layer KiCad board."""
from pathlib import Path
import pcbnew

ROOT = Path(__file__).resolve().parents[1]
PCB = ROOT / "Hank_Touchscreen_Adapter.kicad_pcb"
SES = ROOT / "routing" / "Hank_Touchscreen_Adapter_2layer.ses"

if not SES.exists():
    raise SystemExit(f"Missing FreeRouting session: {SES}")

board = pcbnew.LoadBoard(str(PCB))
if not pcbnew.ImportSpecctraSES(board, str(SES)):
    raise SystemExit("Specctra SES import failed")

if not list(board.GetTracks()):
    raise SystemExit("SES import produced no tracks")

pcbnew.SaveBoard(str(PCB), board)
print(f"Imported {len(list(board.GetTracks()))} routed segments")
