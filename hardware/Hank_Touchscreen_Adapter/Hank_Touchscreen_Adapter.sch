EESchema Schematic File Version 4
LIBS:power
EELAYER 29 0
EELAYER END
$Descr A4 11693 8268
Sheet 1 1
Title "Hank Touchscreen Adapter"
Date "2026-11-06"
Rev "A"
Comp "BuzzyToes Creations"
Comment1 "V1A EXP1/EXP2 to ILI9488/XPT2046; GP1 PWM high-side LCD backlight"
$EndDescr
Text Notes 700 700 0 120 ~ 24
Hank Touchscreen Adapter Rev A
Text Notes 700 950 0 60 ~ 12
J1 TFT 1x14: 1 +5V, 2 GND, 3 CS, 4 RST, 5 DC, 6 MOSI, 7 SCK, 8 LED, 9 NC, 10 T_CLK, 11 T_CS, 12 T_DIN, 13 T_DO, 14 T_IRQ
Text Notes 700 1200 0 60 ~ 12
EXP1: 1 T_DO, 2 T_CS, 3 TFT_CS, 4 TFT_DC, 5 TFT_RST, 6 T_IRQ, 9 GND, 10 +5V.
Text Notes 700 1375 0 60 ~ 12
EXP2: 2 TFT_SCK, 3 T_DIN, 5 T_CLK, 6 TFT_MOSI, 9 GND. Other pins are NC.
Text Notes 700 1550 0 60 ~ 12
AUX JST-XH: 1 +3V3, 2 GND, 3 GP1_BL_PWM.
Text Notes 700 1775 0 60 ~ 12
Backlight: GP1_BL_PWM -- R1 1k -- PG2 (Si4599 pin 4); R2 100k from PG2 to +3V3.
Text Notes 700 1950 0 60 ~ 12
Si4599 PS2 pins 5/6 to +3V3; PD2 pins 7/8 to TFT LED (J1 pin 8). N-channel half unused.
Text Notes 700 2200 0 60 ~ 12
This legacy-format source is intentionally included for easy review/import. The authoritative routed manufacturing design is Hank_Touchscreen_Adapter.kicad_pcb.
$EndSCHEMATC
