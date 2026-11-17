# V1A Release Validation

- Native KiCad schematic ERC: 0 violations.
- Routed PCB DRC: 0 violations and 0 unconnected items.
- Schematic/PCB pad-net parity differences: 0.
- Bottom-side component count: 0.
- Pico and TMC2209 spacing: physically confirmed on 1:1 print.
- DC-jack slot pattern/orientation: physically confirmed, then moved 2.5 mm
  farther inward without changing its relative blade geometry.
- Mini12864 V2 J7 FPC retained; component-side pin 1 is 5 V at the left.
- Manufacturing archive contains two copper layers, two masks, two silkscreens,
  board outline, PTH/NPTH drill files and the Gerber job file.

This release is suitable for the intended first bare-PCB validation order. SMD
assembly remains subject to the board-house placement-overlay polarity review.
