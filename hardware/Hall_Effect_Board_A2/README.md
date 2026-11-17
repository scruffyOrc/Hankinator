# Hankinator HE PCB A2

37 x 20 mm, two copper layers, nominal 1.6 mm FR4 / 1 oz copper. Separate project; DH/RL/FL and controller projects are unchanged.

## A2 changes

Removed 5 mm from the left end. The jack mouth is at CAD X=1, giving approximately 4 mm overhang past the new X=5 board edge. Both 2.3 mm locating holes remain intact; minimum laminate from each hole rim to the left edge is 0.85 mm. Connector mating clearance still needs housing review.

Corrected the sensor footprint from manufacturer bottom view to component-side view, rotated U1 by 180 degrees, and rerouted its supply/output pins. The required flat sensing face points RIGHT. A1's asserted orientation was incorrect; use the A2 Gerbers and CPL together. See POLARITY-REVIEW.md for the complete sensor, LED, MOSFET, and connector review and the required JLC preview orientation.

Added back-side silkscreen: ©Buzzytoes Creations 2026.

## Function

U1 is TI DRV5033AJQLPGM, JLCPCB C201608. Rated 2.5-38 V by the TI datasheet and used here at 3.3 V. It is a continuous digital omnipolar switch, not an analog sensor or a latching switch. Either pole can trigger it. AJ nominal switching thresholds are 6.9 mT operate / 3.5 mT release, with worst-case operate up to 12 mT. Do not assume the same trigger gap as the existing unidentified 3144E module.

D1 PWR and D2 TRIG are red 0603 SMD LEDs, individually labeled. D1 lights with supply. D2 lights while the magnet is detected and goes out when it leaves. It has no pulse stretcher: short detections can be hard to see, and a stationary magnet holds it on.

U1 pulls HALL_N low when detected. R1 is a local 4.7k pull-up to 3.3 V; R2 is 100 ohms in series with the HE cable signal. AO3401A Q1 switches the trigger LED supply, so its LED current does not flow in the sensor output or HE wire. Q1 does add gate capacitance; firmware should measure actual pulse timing if required. C1 is 100 nF near U1; C2 is 10 uF at the input. Each LED has a 1k current-limiting resistor.

## Connector and cable

J1 is the same EVERCOM 5501-4P4C-13MM / C3097719 jack as DH J11. It opens out of the left end. Use a cable verified contact n to contact n; telephone handset cables may reverse contacts.

| Jack contact | DH J11 | Hall J1 |
|---|---|---|
| 1 | GND | GND |
| 2 | HE | HE, active low |
| 3 | Unused | No connect |
| 4 | 3V3 | 3V3_SENSOR |

This is the HALL port mapping, not the smart-foot 4P4C mapping. Connect only to DH J11. The controller Hall allocation remains GP10; verify the installed controller harness actually routes HE there. No firmware or main controller change is included.

## Physical pin check

Component-side PCB coordinates in mm, retaining the A1 CAD origin, X right and Y down. The new outline spans X=5 to 42 and Y=0 to 20; subtract 5 from X to measure from the new left edge:

| J1 contact | X | Y | Function |
|---|---:|---:|---|
| 1, square pad | 9.30 | 11.53 | GND |
| 2 | 11.84 | 10.51 | HE |
| 3 | 9.30 | 9.49 | NC |
| 4 | 11.84 | 8.47 | 3.3 V |

Jack pegs are at (7,5) and (7,15), 2.3 mm NPTH. Signal holes are 0.9 mm. These numbered pad offsets and the 270-degree jack orientation were compared directly against DH J11 and match exactly. The selected manufacturer drawing is included. Do not interpret the staggered solder tails as the plug contact order or mirror this component-side table for fabrication. On the solder side the visual order is reversed. Verify plug contact continuity to the square GND pad and each numbered pad before applying power.

U1 has 1.27 mm pitch and TI's recommended 1.07 x 1.70 mm pads / 0.75 mm drill, from drawing 4221343/C. Pins are 1 VCC, 2 GND, 3 OUT. On this PCB, their coordinates are (40,8.73), (40,10), (40,11.27). The upright flat sensing face points toward the right board edge, as marked MAGNET >. The face is approximately 1.46 mm inside the board edge. Present the magnet toward that face, parallel to the board plane. Mounting height, magnet gap and housing clearance require a prototype check; there is no verified detection-distance claim.

Two 3.2 mm M3 NPTH mounting holes are centered at (18.5,3.5) and (18.5,16.5), 13 mm apart. Use nonmagnetic hardware around the Hall sensor. Leave jack latch and plug withdrawal clearance. The jack is approximately 14.5 mm tall; housing geometry has not been verified.

## Current and traces

All traces are at least 0.20 mm; the short bottom HE connection is 0.25 mm. B.Cu has a continuous ground reference around the short signal route. At 3.3 V, each red LED is approximately 0.9-1.5 mA using the listed 1.8-2.4 V forward range; brightness at these currents must be checked. U1 is 2.7 mA typical, 3.6 mA maximum at 125 C. The active pull-up draws approximately 0.70 mA. The combined conservative operating budget is below 10 mA, excluding external loads on HE.

For a conservative 50 mm long, 0.20 mm wide, 35 um copper trace, resistance is approximately 0.123 ohm at 20 C. At 10 mA this is 1.23 mV drop and 12.3 uW dissipation. These trace widths comfortably support the board's current. Supply drop in the external harness and magnetic switching behavior remain separate checks.

## Files and validation

- `HE PCB/`: editable KiCad schematic, PCB and project; project-local libraries in `libraries/`.
- `HE-PCB-Gerbers.zip`: fabrication layers and separate PTH/NPTH drills.
- `assembly/BOM-JLCPCB.csv` and `assembly/CPL-JLCPCB.csv`: 11 populated components; mounting holes and bare test pads excluded.
- `checks/`: native ERC, native DRC with schematic parity, XML netlist, connector/pad-net audit and assembly inspections.
- `DRILL-COORDINATE-AUDIT.txt`: every exported drill coordinate compared against the PCB.
- `review/`: schematic and board views. Bottom view is shown through the board from the component side, so lettering appears mirrored.

KiCad 10.0.5: ERC 0 errors / 0 warnings; DRC 0 violations / 0 unconnected pads / 0 schematic parity issues under saved project rules. Default ignored check categories are listed in the reports; no individual violations were excluded. CAD checks do not establish physical jack fit, cable continuity, magnet gap, or assembled operation.

## JLCPCB quote / assembly

Upload the Gerber ZIP as a new board item with the BOM and CPL. CPL uses the NEW lower-left board origin at CAD (5,20), millimeters, X right and Y up. Its X coordinates are 5 mm smaller than A1. J1 and U1 coordinates are corrected to their body centers; native KiCad anchor positions are retained in checks/native-positions.csv for comparison. Verify JLC's placement preview: J1 mouth left, U1 flat face right, LED cathodes/pin 1 left. J1 and U1 require through-hole assembly; small-board panel/fixture requirements and costs are determined by JLC.

C201608 has a live JLC listing for wave-solder assembly, but a reliable current stock count was not exposed on the retrieved page. Recheck availability during quoting; do not substitute an unspecified 3144E. Other BOM stock has not been freshly counted for this package. No order has been placed. These files support quoting and first-article fabrication; assembly orientation and actual cable/magnet checks remain required before production use.

## Sources

- TI DRV5033 datasheet: https://www.ti.com/lit/ds/symlink/drv5033.pdf
- JLC sensor listing: https://jlcpcb.com/partdetail/TexasInstruments-DRV5033AJQLPGM/C201608
- AO3401A manufacturer datasheet: https://www.aosmd.com/sites/default/files/res/datasheets/AO3401A.pdf
- JLC Q1 listing: https://jlcpcb.com/partdetail/Alpha_OmegaSemicon-AO3401A/C15127
- JLC LED listing: https://jlcpcb.com/partdetail/Hubei_KentoElec-KT0603R/C2286
- EVERCOM drawing: https://atta.szlcsc.com/upload/public/pdf/source/20220705/442A4794DE2D8659C3975B8A2EE8C1A4.pdf

Prepared 2026-09-30.
