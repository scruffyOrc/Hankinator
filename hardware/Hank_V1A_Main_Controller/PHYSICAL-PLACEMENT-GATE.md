# V1A Physical Placement Gate

This project is **not routed and is not order-ready**.  The 1:1 placement sheet
must be printed at 100% / actual size before routing.

## Viewing convention

Every component mounts on `F.Cu`.  The placement PDF is a direct component-side
view with no mirroring.  There are no bottom-side components.

R1 is an 0805 SMD assembly part. Pico and TMC2209 sockets, EXP1/EXP2, the DC
jack, and the JST-style headers are reserved for hand/wave soldering. J7
remains an SMD assembly part.

## Place these real parts on the 1:1 print

- Raspberry Pi Pico W or Pico 2 W: USB at the left board edge; physical pin 1
  at the lower-left square pad.
- BTT TMC2209 V1.2: VM row toward the board top; both eight-pin rows must drop
  onto 12.70 mm row centers without bending.
- TMC auxiliary socket: only the inline VREF and DIAG pins enter J5.  The offset
  INDEX pin must not encounter a hole.
- Both 2x5 keyed IDC box headers and the intended ribbon-cable plugs.
- The measured Amazon barrel jack: verify all three blade centers and slot
  orientations. With the jack mouth facing away from you and its blades down,
  the side/switched blade must be on the left and its PCB slot must be vertical.
- J7 Mini12864 V2 FPC: C23826/AFA07-S18FCC-00, cable exit toward the lower
  board edge, component-side pin 1 at the left and carrying 5 V.
  orientations.  Pin 1 is center-positive, pin 2 is sleeve/GND, and the side
  switched blade is unconnected.
- Four-position motor connector.
- Six-position expansion connector.
- Eight-position HX711 smart-foot connector.
- AP63205 TSOT26 package: normal component-side placement, pin-1 dot at the
  upper-left pad.
- Inductor, C7 electrolytic, D1 and D2 package bodies.

## Electrical orientation checks

- D1 pad 1 / banded cathode -> `12V_PROT`; pad 2 / anode -> `12V_FUSED`.
- D2 pad 1 / banded cathode -> `12V_PROT`; pad 2 / anode -> GND.
- C7 pad 1 positive -> `12V_PROT`; pad 2 negative stripe -> GND.
- U3 pins: 1 FB/5V, 2 EN/12V_PROT, 3 VIN/12V_PROT, 4 GND, 5 SW, 6 BST.
- Pico pin 39 VSYS -> 5V; Pico pin 40 VBUS is unconnected.

## Cable convention

J1 and J2 use straight-through ten-conductor IDC cables.  Pin 1 at the
controller must arrive at pin 1 on the Mini12864.  No electrical mirroring or
conductor reversal is allowed.

## Approval consequence

Routing, copper pours, final DRC, Gerbers, BOM and CPL remain blocked until the
real components pass this print-fit inspection.
