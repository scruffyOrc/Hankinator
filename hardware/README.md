# Hank hardware sources

This directory contains the editable KiCad sources and the corresponding
released manufacturing artifacts for the hardware used by Hank Winder V1.

| Directory | Board | Release source |
| --- | --- | --- |
| `Hank_V1A_Main_Controller` | Hank V1A main controller | V1A released controller package |
| `Load_Cell_System` | HX711 breakout and front/rear smart-foot load-cell boards | Load-cell-system release package |
| `Distribution_Block_v1.1` | Load-cell distribution block | v1.1 confirmed JLC release |
| `Hall_Effect_Board_A2` | Hall-effect sensor board | A2 release package |
| `Hank_Touchscreen_Adapter` | V1A-to-touchscreen adapter | Rev A two-layer package |

KiCad local state files (`*.kicad_prl`), lock files, and local `.history`
directories are intentionally excluded. They are machine-specific and are not
authoritative design data.
