# V1A Physical Pin and Schematic/PCB Parity Audit

- Bottom-side components: **0**
- Schematic/PCB pad-net differences: **0**


## Pico physical socket

| Pin | Net |
|---:|---|
| 1 | GP0 |
| 2 | GP1 |
| 3 | GND |
| 4 | STEP |
| 5 | DIR |
| 6 | EN |
| 7 | NC |
| 8 | GND |
| 9 | STOP |
| 10 | TMC_DIAG |
| 11 | TMC_UART_TX |
| 12 | TMC_UART |
| 13 | GND |
| 14 | GP10 |
| 15 | RGB |
| 16 | BEEP |
| 17 | ENC_BTN |
| 18 | GND |
| 19 | ENC_A |
| 20 | ENC_B |
| 21 | LCD_RS |
| 22 | LCD_CS |
| 23 | GND |
| 24 | LCD_CLK |
| 25 | LCD_MOSI |
| 26 | LCD_RST |
| 27 | HX_SCK |
| 28 | GND |
| 29 | HX_FL |
| 30 | NC |
| 31 | HX_FR |
| 32 | HX_RL |
| 33 | GND |
| 34 | HX_RR |
| 35 | NC |
| 36 | 3V3 |
| 37 | NC |
| 38 | GND |
| 39 | 5V |
| 40 | NC |

## BTT TMC2209 main socket

| Pin | Net |
|---:|---|
| 1 | EN |
| 2 | GND |
| 3 | GND |
| 4 | TMC_UART |
| 5 | NC |
| 6 | NC |
| 7 | STEP |
| 8 | DIR |
| 9 | 12V_PROT |
| 10 | GND |
| 11 | A2 |
| 12 | A1 |
| 13 | B1 |
| 14 | B2 |
| 15 | 3V3 |
| 16 | GND |

## Mini12864 EXP1 cable header

| Pin | Net |
|---:|---|
| 1 | BEEP |
| 2 | ENC_BTN |
| 3 | LCD_CS |
| 4 | LCD_RS |
| 5 | LCD_RST |
| 6 | RGB |
| 7 | NC |
| 8 | NC |
| 9 | GND |
| 10 | 5V |

## Mini12864 EXP2 cable header

| Pin | Net |
|---:|---|
| 1 | NC |
| 2 | LCD_CLK |
| 3 | ENC_B |
| 4 | NC |
| 5 | ENC_A |
| 6 | LCD_MOSI |
| 7 | NC |
| 8 | STOP |
| 9 | GND |
| 10 | NC |

## HX711 smart-foot harness

| Pin | Net |
|---:|---|
| 1 | GND |
| 2 | 3V3 |
| 3 | HX_SCK |
| 4 | HX_FL |
| 5 | HX_FR |
| 6 | HX_RL |
| 7 | HX_RR |
| 8 | NC |

## Hard assertions

- PASS: Pico GP2 STEP is physical pin 4
- PASS: Pico GP3 DIR is physical pin 5
- PASS: Pico GP4 ENABLE is physical pin 6
- PASS: Pico GP7 DIAG is physical pin 10
- PASS: Pico GP8 UART TX is physical pin 11
- PASS: Pico GP9 UART RX/common is physical pin 12
- PASS: Pico GP21 HX clock is physical pin 27
- PASS: Pico physical pin 28 is GND
- PASS: Pico VSYS physical pin 39 is 5V
- PASS: Pico VBUS physical pin 40 is NC
- PASS: TMC row spacing is exactly 12.70 mm
- PASS: TMC PDN_UART pad 4 is UART
- PASS: TMC DIAG auxiliary pad 2 is DIAG
- PASS: All components are on F.Cu
- PASS: Schematic and PCB pad nets agree
