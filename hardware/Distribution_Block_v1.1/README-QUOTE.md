# DB Board v1.1 - JLCPCB quotation package

Source: C:\Users\crisd\OneDrive\Documents\DB-Board-Linked\DB Board
Prepared from the supplied v1.1 files on 2026-09-26. QUOTATION / CAD-VALIDATED, not physical first-article approval.

This replaces the previously generated package mistakenly labeled v1.1 from the DB-Board-v1.1 folder. Verified board silk: V1.1. J11 and J10 open left; J9 opens right.

## Upload
- DB-Board-v1.1-CONFIRMED-GERBERS-QUOTE.zip: PCB layers and separate plated/non-plated drills.
- assembly/BOM-JLCPCB.csv: all six connector placements, four part numbers.
- assembly/CPL-JLCPCB.csv: matching six placements, top side, millimetres.

J8 C157972; S1/S2 C144395; J9/J10 C189747; J11 C3097719.
All are through-hole. Request JLCPCB's applicable through-hole/wave-solder assembly quote. Availability and price must be confirmed in the quote; no order was submitted.

## Validation
Native KiCad ERC: 0 errors/warnings. Native DRC: 0 violations, 0 unconnected items, 0 schematic parity issues under the project's saved rules. See reports for disabled checks. All 32 numbered connector pads agree with the schematic and prior accepted net mapping. Motor traces remain 1.0 mm, sensor power and ground 0.5 mm, digital traces 0.2 mm.

Outline: 51.5 x 48.0 mm. Two copper layers, saved stackup 1.6 mm board / 35 um copper. Drills: 44 plated (32 connector pins and 12 vias), 10 non-plated (6 jack pegs, 4 M3 holes). M3 centres relative to lower-left outline: (4,44), (47.5,44), (4,4), (47.5,4), diameter 3.2 mm.

## Export-copy changes
The user's original folder was not modified. The included source copy removes duplicate Library entries and retains project-relative local libraries. J11's edited silk was saved back into its library footprint; its pads/drills are unchanged from the datasheet-derived footprint. The overlapping J8 reference was moved to (201.5,69) mm. Auxiliary export origin was set to the board's lower-left corner (175.5,78.5) mm. No component positions, traces, pad positions, or nets were changed.

## Connector review and assembly orientation
J11 C3097719: manufacturer top-view layout gives 10 mm peg spacing, 2.3 mm NPTH, 0.9 mm contact drills, 2.04 mm same-row pitch, 2.54 mm row spacing. Board pad numbering matches this drawing, including the 270-degree placement.
J9/J10 C189747: mechanical family dimensions match (12 mm peg spacing, 1.02 mm stagger, 2.54 mm rows). User-provided JLC library screenshot shows top row 5,3,1 and lower row 6,4,2, matching the board's normalized footprint numbering. Board contact drills are 0.90 mm versus the generic drawing's 0.79 mm recommendation. This clearance difference is disclosed for assembly review; actual plug-to-contact continuity remains untested.

CPL centres use the F.Fab body outline centre, not pad 1. Rotations are KiCad top-view rotations; JLC's library rotation convention must be checked in its assembly preview. Pin-1 locations relative to export origin are in checks/placement-data.json. This package is suitable for obtaining a quote, not evidence of physical fit or cable continuity. Before fabrication/assembly approval, check the exact plugs end-to-end and JLC's placement preview. J11's body extends 2 mm beyond the left board edge by design; provide housing clearance. Retain the manufacturer's mating and orientation drawings for review.

## Harness pin mapping
J8: 1 GND, 2 3V3, 3 SCK, 4 FL, 5 FR, 6 RL, 7 RR, 8 HE.
J9 RIGHT: 1 GND, 2 SCK, 3 FR, 4 NC, 5 RR, 6 3V3.
J10 LEFT: 1 GND, 2 SCK, 3 FL, 4 NC, 5 RL, 6 3V3.
J11 HALL: 1 GND, 2 HE, 3 NC, 4 3V3.
S1/S2 STEPPER: 1 A2, 2 A1, 3 B1, 4 B2.

Gerbers and CPL were regenerated together; do not mix them with earlier releases. Editable cleaned source, checks, and board preview are included.
