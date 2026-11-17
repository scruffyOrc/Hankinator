# HE PCB A2 polarity and assembly orientation review

Viewed from the component side, connector on the LEFT, sensor on the RIGHT.

| Part | Required physical orientation and connection | Review result |
|---|---|---|
| D1 power LED, KT-0603R | Green cathode mark LEFT, pad 1 to GND. Anode RIGHT, pad 2 to PWR_A through R3 to 3.3 V. | Correct; no reversal needed. |
| D2 trigger LED, KT-0603R | Green cathode mark LEFT, pad 1 to GND. Anode RIGHT, pad 2 to TRIG_A through R4 to Q1 drain. | Correct; no reversal needed. |
| Q1 AO3401A P-channel MOSFET | Gate pin 1 UPPER LEFT to HALL_N; source pin 2 LOWER LEFT to 3.3 V; drain pin 3 RIGHT to LED_SWITCHED. | Correct; source and drain are not swapped. |
| U1 DRV5033AJQLPGM | Flat marked sensing face RIGHT. Lead row top to bottom: 1 VCC, 2 GND, 3 OUT. | A1 footprint used bottom-view body outline incorrectly. A2 corrects it and reverses pin 1/3 positions through rotation and rerouting. |
| J1 5501-4P4C-13MM | Mouth LEFT. Contact 1 GND, 2 HE, 3 NC, 4 3.3 V. | Same numbered hole pattern and physical orientation as DH J11. |

Q1 turns ON when HALL_N is low: VGS is approximately -3.3 V and the red trigger LED receives current through R4. With HALL_N high, gate and source are at the same voltage and Q1 turns OFF. The intrinsic diode is oriented from drain toward source, so it does not feed the LED from 3.3 V while Q1 is off.

All resistors are nonpolar. C1 and C2 are ceramic MLCCs and nonpolar. Bare test points and mounting holes have no component polarity. Those exhaust the component population.

## Evidence

- KENTO KT-0603-R manufacturer specification A.1, dated 2017-05-10, printed page 1 (PDF page 2): green mark identifies cathode; package polarity drawing checked visually. Manufacturer document hosted by distributor: https://www.rcscomponents.kiev.ua/datasheets/KT-0603-R.pdf . Included as references/KT-0603R.pdf. JLC C2286 identifies KT-0603R. The signed JLC download URL had expired, so this manufacturer-document mirror was used.
- Alpha & Omega AO3401A Rev 3.1, December 2023, page 1: explicitly labeled TOP VIEW and BOTTOM VIEW, gate/source/drain and intrinsic-diode schematic. https://www.aosmd.com/sites/default/files/res/datasheets/AO3401A.pdf . Included as references/AO3401A.pdf.
- TI DRV5033 datasheet: page 4 lead function, page 10 Figure 11 bottom-view field direction, and LPG0003A package drawing 4221343/C. https://www.ti.com/lit/ds/symlink/drv5033.pdf . Included as references/DRV5033.pdf.
- Actual numbered pads and nets independently asserted from final KiCad PCB; all match the table above and native schematic parity.

## Placement file and JLC preview

CPL angles: D1/D2/Q1 0 degrees, U1 270 degrees, J1 270 degrees. Coordinates use the NEW lower-left origin, CAD (5,20); all X coordinates shift by -5 mm from A1. U1's center is (34.3013,10) in the CPL, and its angle changes from 90 to 270 degrees. Do not reuse an A1 CPL with A2 Gerbers.

CPL angles alone are not a universal assembly orientation convention. The table above is the required final physical result. The user's original JLC screenshot is the old A1 preview. It supports checking the apparent LED anodes on the right, but does not legibly identify every MOSFET pin. The regenerated A2 JLC preview has not been observed here. Check the table against that preview or give this document to JLC's assembly reviewer, particularly U1 flat face right and Q1 gate upper-left. No claim of factory orientation signoff or physical first-article testing is made.
