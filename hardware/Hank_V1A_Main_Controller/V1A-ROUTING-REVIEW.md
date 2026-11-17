# Skein Winder Controller V1A Routing Review

Status: released for the first bare-PCB validation order.

## Confirmed mechanical changes

- Pico and BTT TMC2209 socket spacing physically confirmed on the 1:1 print.
- DC jack moved 2.5 mm farther inward after physical fit confirmation.
- DC jack switched side blade is left/vertical; center and sleeve blades use
  horizontal slots.
- All components remain on the top/component side.
- Mini12864 EXP1 and EXP2 use independent keyed 2x5 cable headers.
- Mini12864 V2 J7 FPC interface is retained using the C23826 /
  AFA07-S18FCC-00 land pattern.

## Assembly split

Board-house SMD assembly includes R1 (1 kOhm, 0805), the buck/protection
components, and J7. Pico and TMC2209 sockets, EXP1/EXP2, the DC jack, motor and
expansion connectors, and JST-style headers are intentionally hand/wave
soldered.

## Routing result

- Two copper layers; components only on F.Cu.
- Power and motor nets use wider assigned net classes than logic signals.
- Final PCB DRC: 0 violations, 0 unconnected items.
- Bare-PCB Gerbers and PTH/NPTH drill files have been generated.

## Release gate result

The native KiCad schematic now contains J7 and the 0805 R1. ERC is clean and
the native schematic netlist has zero pad-net differences from the routed PCB.
The Gerber ZIP is therefore released for the first bare-board validation run.
Any SMD assembly run still requires inspection of the board-house placement
overlay before authorization.
