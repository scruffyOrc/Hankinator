# Hank Touchscreen Adapter — Rev A

This is a separate retrofit adapter for the existing Hank V1A controller. It
does not change the V1A PCB; it connects the established ILI9488/XPT2046
touchscreen to the V1A EXP headers and adds GP1-controlled true PWM backlight
dimming.

## Final board

- **32.0 mm × 43.0 mm** — within the approximately 56.34 mm × 98 mm LCD PCB
  outline and substantially smaller than the original candidate.
- **Two copper layers** with filled GND pours on both outer layers. FreeRouting
  completed all 22 nets at this layer count; no extra layers or jumpers were
  required.
- All routed traces are 0.20 mm. This is deliberate: the compact J1/J4 escape
  geometry cannot accept a blanket wider rule while preserving 0.20 mm
  clearance. The adapter carries touchscreen logic and backlight current only,
  never motor current; the power runs are short and their return is supported by
  both GND pours.
- J1 is a 1×14 **female** 2.54 mm socket, intended to mate directly with the
  touchscreen's existing male rear header.
- J2/J3 are 2×5 **male** 2.54 mm headers, intended for keyed female-to-female
  harnesses to the existing V1A EXP1 and EXP2 male headers.

## Touchscreen / V1A mapping

| LCD J1 pin | LCD signal | V1A source |
|---:|---|---|
| 1 | VCC | EXP1-10, +5 V |
| 2 | GND | EXP1-9, GND |
| 3 | CS | EXP1-3, GP17 |
| 4 | RESET | EXP1-5, GP20 |
| 5 | DC / RS | EXP1-4, GP16 |
| 6 | SDI / MOSI | EXP2-6, GP19 |
| 7 | SCK | EXP2-2, GP18 |
| 8 | LED | Q1 switched 3.3 V output |
| 9 | SDO / MISO | not connected — the current firmware does not use TFT MISO |
| 10 | T_CLK | EXP2-5, GP14 |
| 11 | T_CS | EXP1-2, GP13 |
| 12 | T_DIN | EXP2-3, GP15 |
| 13 | T_DO | EXP1-1, GP12 |
| 14 | T_IRQ | EXP1-6, GP11 |

The Mini12864 and touchscreen cannot be used concurrently: the listed V1A
signals are deliberately repurposed by the touchscreen firmware.

## J4 AUX connector

| J4 pin | Signal | Purpose |
|---:|---|---|
| 1 | +3V3 | Si4599 source / LCD backlight supply |
| 2 | GND | common reference |
| 3 | GP1_BL_PWM | Pico GP1 active-low backlight command |

J4 is the only extra V1A harness. The V1A side must follow the already proven
JP6/Pico harness: +3V3, GND, and GP1. The exact V1A-side pad positions should
still be continuity-checked before assembly.

## Backlight PWM circuit

Q1 uses the P-channel half of a Si4599DY as a 3.3 V high-side switch:

```text
+3V3 ───── Si4599 PS2 (pins 5,6)
                       PD2 (pins 7,8) ───── LCD LED (J1-8)
GP1 ── R1 1k ── PG2 (pin 4)
                  │
                  R2 100k
                  │
                +3V3
```

The 100 kΩ pull-up makes the backlight **off** while GP1 is floating or the
Pico is resetting. GP1 low turns it on. The circuit supports active-low PWM
dimming without driving LCD current from GP1.

The LCD module already has its own local display/controller decoupling. No
additional capacitors were added to this compact adapter; its power paths are
short and the adapter introduces no active logic beyond Q1.

## Verify before ordering and assembly

1. Confirm the display has the expected **male 1×14, 2.54 mm header**, with
   VCC at pin 1. J1 is a female socket and its pin-1 end is marked on silk.
2. Confirm the EXP harnesses are female-to-female and preserve pin-1 to pin-1
   orientation. J2/J3 are male because the V1A EXP headers are male.
3. Check the J1 socket, its board, and the adapter against the LCD rear SD-card
   socket/touch components before ordering many boards. The board outline fits
   the LCD package, but height and header direction remain an assembly check.
4. Continuity-check the V1A AUX harness for +3V3, GND, and GP1; do not rely on
   historical silkscreen orientation alone.
5. On first power-up, verify J1-1 is +5 V, J1-2 is common ground, and J1-8 is
   initially off until GP1 is asserted low. Then test touch before enabling PWM
   dimming in firmware.

The `manufacturing/2layer` directory contains the current Gerbers, drill files,
DRC report, and ZIP suitable for quoting/upload after the physical connector
checks above. The older fabrication output is retained separately as history,
not as the current release.
