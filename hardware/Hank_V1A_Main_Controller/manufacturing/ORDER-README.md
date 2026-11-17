# V1A Ordering Package

## Bare PCB validation order

Upload `SKEIN-WINDER-CONTROLLER-V1A-GERBERS.zip` as a 2-layer board:

- Board size: 125 x 85 mm
- FR-4: 1.6 mm
- Copper: 1 oz outer layers
- Surface finish: lead-free HASL or ENIG
- Solder mask/silkscreen color: user choice
- Castellations, impedance control and edge plating: no
- Plated slots: yes (DC jack)

The ZIP contains only Gerber/job/drill files. It does not contain assembly
documents or drill-map PDFs.

## Optional JLCPCB SMD assembly

Use `assembly/BOM-JLCPCB-SMD.csv` and
`assembly/CPL-JLCPCB-TOP-SMD.csv`. Assembly is top-side only. The Pico and
TMC2209 sockets/modules, EXP1/EXP2, DC jack, motor/expansion headers, TMC
auxiliary socket, HX711 header and optional test posts are deliberately absent
from the BOM/CPL and are listed in `assembly/HAND-SOLDER-DNP.txt`.

Do not authorize assembly until the JLCPCB placement overlay has been checked
for U3 pin 1, D1/D2 polarity, C7 polarity and J7 pin 1/cable direction.

