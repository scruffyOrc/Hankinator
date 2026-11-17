EESchema Schematic File Version 4
EELAYER 29 0
EELAYER END
$Descr A3 16535 11693
Sheet 1 1
Title "Skein Winder Controller V1A"
Date "2026-09-01"
Rev "V1A PLACEMENT REVIEW"
Comp "Hankinator / Skein Winder"
Comment1 "All components top side; Mini12864 via IDC cables or retained V2 FPC"
Comment2 "Correct physical Pico and BTT TMC2209 numbering; HX711 smart-foot connector"
$EndDescr
Text Notes 700 550 0 100 ~ 20
V1A - CONNECTOR-LEVEL PHYSICAL PIN SCHEMATIC
$Comp
L Connector_Generic:Conn_01x40 U1
U 1 1 7A000001
P 2400 3050
F 0 "U1" H 3300 3150 50  0000 C CNN
F 1 "Raspberry Pi Pico-family physical 40-pin socket" H 3600 2950 50  0000 C CNN
F 2 "" H 2400 3050 50  0001 C CNN
F 3 "" H 2400 3050 50  0001 C CNN
	1    2400 3050
	1    0    0    -1
$EndComp
Text Label 2200 1100 2    40   ~ 0
GP0
Text Label 2200 1200 2    40   ~ 0
GP1
Text Label 2200 1300 2    40   ~ 0
GND
Text Label 2200 1400 2    40   ~ 0
STEP
Text Label 2200 1500 2    40   ~ 0
DIR
Text Label 2200 1600 2    40   ~ 0
EN
NoConn ~ 2200 1700
Text Label 2200 1800 2    40   ~ 0
GND
Text Label 2200 1900 2    40   ~ 0
STOP
Text Label 2200 2000 2    40   ~ 0
TMC_DIAG
Text Label 2200 2100 2    40   ~ 0
TMC_UART_TX
Text Label 2200 2200 2    40   ~ 0
TMC_UART
Text Label 2200 2300 2    40   ~ 0
GND
Text Label 2200 2400 2    40   ~ 0
GP10
Text Label 2200 2500 2    40   ~ 0
RGB
Text Label 2200 2600 2    40   ~ 0
BEEP
Text Label 2200 2700 2    40   ~ 0
ENC_BTN
Text Label 2200 2800 2    40   ~ 0
GND
Text Label 2200 2900 2    40   ~ 0
ENC_A
Text Label 2200 3000 2    40   ~ 0
ENC_B
Text Label 2200 3100 2    40   ~ 0
LCD_RS
Text Label 2200 3200 2    40   ~ 0
LCD_CS
Text Label 2200 3300 2    40   ~ 0
GND
Text Label 2200 3400 2    40   ~ 0
LCD_CLK
Text Label 2200 3500 2    40   ~ 0
LCD_MOSI
Text Label 2200 3600 2    40   ~ 0
LCD_RST
Text Label 2200 3700 2    40   ~ 0
HX_SCK
Text Label 2200 3800 2    40   ~ 0
GND
Text Label 2200 3900 2    40   ~ 0
HX_FL
NoConn ~ 2200 4000
Text Label 2200 4100 2    40   ~ 0
HX_FR
Text Label 2200 4200 2    40   ~ 0
HX_RL
Text Label 2200 4300 2    40   ~ 0
GND
Text Label 2200 4400 2    40   ~ 0
HX_RR
NoConn ~ 2200 4500
Text Label 2200 4600 2    40   ~ 0
3V3
NoConn ~ 2200 4700
Text Label 2200 4800 2    40   ~ 0
GND
Text Label 2200 4900 2    40   ~ 0
5V
NoConn ~ 2200 5000
$Comp
L Connector_Generic:Conn_01x16 U2
U 1 1 7A000002
P 6200 1850
F 0 "U2" H 7100 1950 50  0000 C CNN
F 1 "BTT TMC2209 V1.2 physical socket" H 7400 1750 50  0000 C CNN
F 2 "" H 6200 1850 50  0001 C CNN
F 3 "" H 6200 1850 50  0001 C CNN
	1    6200 1850
	1    0    0    -1
$EndComp
Text Label 6000 1100 2    40   ~ 0
EN
Text Label 6000 1200 2    40   ~ 0
GND
Text Label 6000 1300 2    40   ~ 0
GND
Text Label 6000 1400 2    40   ~ 0
TMC_UART
NoConn ~ 6000 1500
NoConn ~ 6000 1600
Text Label 6000 1700 2    40   ~ 0
STEP
Text Label 6000 1800 2    40   ~ 0
DIR
Text Label 6000 1900 2    40   ~ 0
12V_PROT
Text Label 6000 2000 2    40   ~ 0
GND
Text Label 6000 2100 2    40   ~ 0
A2
Text Label 6000 2200 2    40   ~ 0
A1
Text Label 6000 2300 2    40   ~ 0
B1
Text Label 6000 2400 2    40   ~ 0
B2
Text Label 6000 2500 2    40   ~ 0
3V3
Text Label 6000 2600 2    40   ~ 0
GND
$Comp
L Connector_Generic:Conn_01x02 J5
U 1 1 7A000003
P 8800 1150
F 0 "J5" H 9700 1250 50  0000 C CNN
F 1 "TMC VREF NC / DIAG" H 10000 1050 50  0000 C CNN
F 2 "" H 8800 1150 50  0001 C CNN
F 3 "" H 8800 1150 50  0001 C CNN
	1    8800 1150
	1    0    0    -1
$EndComp
NoConn ~ 8600 1100
Text Label 8600 1200 2    40   ~ 0
TMC_DIAG
$Comp
L Connector_Generic:Conn_01x02 R1
U 1 1 7A000004
P 10400 1150
F 0 "R1" H 11300 1250 50  0000 C CNN
F 1 "1k TMC UART TX resistor 0805" H 11600 1050 50  0000 C CNN
F 2 "" H 10400 1150 50  0001 C CNN
F 3 "" H 10400 1150 50  0001 C CNN
	1    10400 1150
	1    0    0    -1
$EndComp
Text Label 10200 1100 2    40   ~ 0
TMC_UART_TX
Text Label 10200 1200 2    40   ~ 0
TMC_UART
$Comp
L Connector_Generic:Conn_01x10 J1
U 1 1 7A000005
P 12200 1550
F 0 "J1" H 13100 1650 50  0000 C CNN
F 1 "Mini12864 EXP1 cable header" H 13400 1450 50  0000 C CNN
F 2 "" H 12200 1550 50  0001 C CNN
F 3 "" H 12200 1550 50  0001 C CNN
	1    12200 1550
	1    0    0    -1
$EndComp
Text Label 12000 1100 2    40   ~ 0
BEEP
Text Label 12000 1200 2    40   ~ 0
ENC_BTN
Text Label 12000 1300 2    40   ~ 0
LCD_CS
Text Label 12000 1400 2    40   ~ 0
LCD_RS
Text Label 12000 1500 2    40   ~ 0
LCD_RST
Text Label 12000 1600 2    40   ~ 0
RGB
NoConn ~ 12000 1700
NoConn ~ 12000 1800
Text Label 12000 1900 2    40   ~ 0
GND
Text Label 12000 2000 2    40   ~ 0
5V
$Comp
L Connector_Generic:Conn_01x10 J2
U 1 1 7A000006
P 14200 1550
F 0 "J2" H 15100 1650 50  0000 C CNN
F 1 "Mini12864 EXP2 cable header" H 15400 1450 50  0000 C CNN
F 2 "" H 14200 1550 50  0001 C CNN
F 3 "" H 14200 1550 50  0001 C CNN
	1    14200 1550
	1    0    0    -1
$EndComp
NoConn ~ 14000 1100
Text Label 14000 1200 2    40   ~ 0
LCD_CLK
Text Label 14000 1300 2    40   ~ 0
ENC_B
NoConn ~ 14000 1400
Text Label 14000 1500 2    40   ~ 0
ENC_A
Text Label 14000 1600 2    40   ~ 0
LCD_MOSI
NoConn ~ 14000 1700
Text Label 14000 1800 2    40   ~ 0
STOP
Text Label 14000 1900 2    40   ~ 0
GND
NoConn ~ 14000 2000
$Comp
L Connector_Generic:Conn_01x18 J7
U 1 1 7A000007
P 14200 3850
F 0 "J7" H 15100 3950 50  0000 C CNN
F 1 "Mini12864 V2 FPC AFA07-S18FCC-00 C23826" H 15400 3750 50  0000 C CNN
F 2 "" H 14200 3850 50  0001 C CNN
F 3 "" H 14200 3850 50  0001 C CNN
	1    14200 3850
	1    0    0    -1
$EndComp
Text Label 14000 3000 2    40   ~ 0
5V
Text Label 14000 3100 2    40   ~ 0
GND
NoConn ~ 14000 3200
NoConn ~ 14000 3300
Text Label 14000 3400 2    40   ~ 0
RGB
Text Label 14000 3500 2    40   ~ 0
LCD_RST
Text Label 14000 3600 2    40   ~ 0
LCD_CS
Text Label 14000 3700 2    40   ~ 0
LCD_RS
Text Label 14000 3800 2    40   ~ 0
ENC_BTN
Text Label 14000 3900 2    40   ~ 0
BEEP
Text Label 14000 4000 2    40   ~ 0
STOP
NoConn ~ 14000 4100
Text Label 14000 4200 2    40   ~ 0
LCD_MOSI
Text Label 14000 4300 2    40   ~ 0
ENC_B
NoConn ~ 14000 4400
Text Label 14000 4500 2    40   ~ 0
ENC_A
Text Label 14000 4600 2    40   ~ 0
LCD_CLK
NoConn ~ 14000 4700
$Comp
L Connector_Generic:Conn_01x08 J8
U 1 1 7A000008
P 2400 6850
F 0 "J8" H 3300 6950 50  0000 C CNN
F 1 "HX smart-foot harness" H 3600 6750 50  0000 C CNN
F 2 "" H 2400 6850 50  0001 C CNN
F 3 "" H 2400 6850 50  0001 C CNN
	1    2400 6850
	1    0    0    -1
$EndComp
Text Label 2200 6500 2    40   ~ 0
GND
Text Label 2200 6600 2    40   ~ 0
3V3
Text Label 2200 6700 2    40   ~ 0
HX_SCK
Text Label 2200 6800 2    40   ~ 0
HX_FL
Text Label 2200 6900 2    40   ~ 0
HX_FR
Text Label 2200 7000 2    40   ~ 0
HX_RL
Text Label 2200 7100 2    40   ~ 0
HX_RR
NoConn ~ 2200 7200
$Comp
L Connector_Generic:Conn_01x03 J3
U 1 1 7A000009
P 5000 6600
F 0 "J3" H 5900 6700 50  0000 C CNN
F 1 "DC jack center+ sleeve GND switch NC" H 6200 6500 50  0000 C CNN
F 2 "" H 5000 6600 50  0001 C CNN
F 3 "" H 5000 6600 50  0001 C CNN
	1    5000 6600
	1    0    0    -1
$EndComp
Text Label 4800 6500 2    40   ~ 0
12V_RAW
Text Label 4800 6600 2    40   ~ 0
GND
NoConn ~ 4800 6700
$Comp
L Connector_Generic:Conn_01x04 J4
U 1 1 7A00000A
P 6800 6650
F 0 "J4" H 7700 6750 50  0000 C CNN
F 1 "Motor A2 A1 B1 B2" H 8000 6550 50  0000 C CNN
F 2 "" H 6800 6650 50  0001 C CNN
F 3 "" H 6800 6650 50  0001 C CNN
	1    6800 6650
	1    0    0    -1
$EndComp
Text Label 6600 6500 2    40   ~ 0
A2
Text Label 6600 6600 2    40   ~ 0
A1
Text Label 6600 6700 2    40   ~ 0
B1
Text Label 6600 6800 2    40   ~ 0
B2
$Comp
L Connector_Generic:Conn_01x06 J6
U 1 1 7A00000B
P 8600 6750
F 0 "J6" H 9500 6850 50  0000 C CNN
F 1 "Expansion 5V 3V3 GND GP0 GP1 GP10" H 9800 6650 50  0000 C CNN
F 2 "" H 8600 6750 50  0001 C CNN
F 3 "" H 8600 6750 50  0001 C CNN
	1    8600 6750
	1    0    0    -1
$EndComp
Text Label 8400 6500 2    40   ~ 0
5V
Text Label 8400 6600 2    40   ~ 0
3V3
Text Label 8400 6700 2    40   ~ 0
GND
Text Label 8400 6800 2    40   ~ 0
GP0
Text Label 8400 6900 2    40   ~ 0
GP1
Text Label 8400 7000 2    40   ~ 0
GP10
$Comp
L Connector_Generic:Conn_01x02 F1
U 1 1 7A00000C
P 10800 6550
F 0 "F1" H 11700 6650 50  0000 C CNN
F 1 "1.1A resettable fuse" H 12000 6450 50  0000 C CNN
F 2 "" H 10800 6550 50  0001 C CNN
F 3 "" H 10800 6550 50  0001 C CNN
	1    10800 6550
	1    0    0    -1
$EndComp
Text Label 10600 6500 2    40   ~ 0
12V_RAW
Text Label 10600 6600 2    40   ~ 0
12V_FUSED
$Comp
L Connector_Generic:Conn_01x02 D1
U 1 1 7A00000D
P 12400 6550
F 0 "D1" H 13300 6650 50  0000 C CNN
F 1 "SS54 series reverse diode" H 13600 6450 50  0000 C CNN
F 2 "" H 12400 6550 50  0001 C CNN
F 3 "" H 12400 6550 50  0001 C CNN
	1    12400 6550
	1    0    0    -1
$EndComp
Text Label 12200 6500 2    40   ~ 0
12V_PROT
Text Label 12200 6600 2    40   ~ 0
12V_FUSED
$Comp
L Connector_Generic:Conn_01x02 D2
U 1 1 7A00000E
P 14000 6550
F 0 "D2" H 14900 6650 50  0000 C CNN
F 1 "SMBJ18A TVS cathode to protected VIN" H 15200 6450 50  0000 C CNN
F 2 "" H 14000 6550 50  0001 C CNN
F 3 "" H 14000 6550 50  0001 C CNN
	1    14000 6550
	1    0    0    -1
$EndComp
Text Label 13800 6500 2    40   ~ 0
12V_PROT
Text Label 13800 6600 2    40   ~ 0
GND
$Comp
L Connector_Generic:Conn_01x06 U3
U 1 1 7A00000F
P 2400 8450
F 0 "U3" H 3300 8550 50  0000 C CNN
F 1 "AP63205 pins 1 FB 2 EN 3 VIN 4 GND 5 SW 6 BST" H 3600 8350 50  0000 C CNN
F 2 "" H 2400 8450 50  0001 C CNN
F 3 "" H 2400 8450 50  0001 C CNN
	1    2400 8450
	1    0    0    -1
$EndComp
Text Label 2200 8200 2    40   ~ 0
5V
Text Label 2200 8300 2    40   ~ 0
12V_PROT
Text Label 2200 8400 2    40   ~ 0
12V_PROT
Text Label 2200 8500 2    40   ~ 0
GND
Text Label 2200 8600 2    40   ~ 0
SW
Text Label 2200 8700 2    40   ~ 0
BST
$Comp
L Connector_Generic:Conn_01x02 L1
U 1 1 7A000010
P 4800 8250
F 0 "L1" H 5700 8350 50  0000 C CNN
F 1 "4.7uH" H 6000 8150 50  0000 C CNN
F 2 "" H 4800 8250 50  0001 C CNN
F 3 "" H 4800 8250 50  0001 C CNN
	1    4800 8250
	1    0    0    -1
$EndComp
Text Label 4600 8200 2    40   ~ 0
SW
Text Label 4600 8300 2    40   ~ 0
5V
$Comp
L Connector_Generic:Conn_01x02 C1
U 1 1 7A000011
P 6500 8250
F 0 "C1" H 7400 8350 50  0000 C CNN
F 1 "C1 capacitor" H 7700 8150 50  0000 C CNN
F 2 "" H 6500 8250 50  0001 C CNN
F 3 "" H 6500 8250 50  0001 C CNN
	1    6500 8250
	1    0    0    -1
$EndComp
Text Label 6300 8200 2    40   ~ 0
12V_PROT
Text Label 6300 8300 2    40   ~ 0
GND
$Comp
L Connector_Generic:Conn_01x02 C2
U 1 1 7A000012
P 7600 8250
F 0 "C2" H 8500 8350 50  0000 C CNN
F 1 "C2 capacitor" H 8800 8150 50  0000 C CNN
F 2 "" H 7600 8250 50  0001 C CNN
F 3 "" H 7600 8250 50  0001 C CNN
	1    7600 8250
	1    0    0    -1
$EndComp
Text Label 7400 8200 2    40   ~ 0
12V_PROT
Text Label 7400 8300 2    40   ~ 0
GND
$Comp
L Connector_Generic:Conn_01x02 C3
U 1 1 7A000013
P 8700 8250
F 0 "C3" H 9600 8350 50  0000 C CNN
F 1 "C3 capacitor" H 9900 8150 50  0000 C CNN
F 2 "" H 8700 8250 50  0001 C CNN
F 3 "" H 8700 8250 50  0001 C CNN
	1    8700 8250
	1    0    0    -1
$EndComp
Text Label 8500 8200 2    40   ~ 0
5V
Text Label 8500 8300 2    40   ~ 0
GND
$Comp
L Connector_Generic:Conn_01x02 C4
U 1 1 7A000014
P 9800 8250
F 0 "C4" H 10700 8350 50  0000 C CNN
F 1 "C4 capacitor" H 11000 8150 50  0000 C CNN
F 2 "" H 9800 8250 50  0001 C CNN
F 3 "" H 9800 8250 50  0001 C CNN
	1    9800 8250
	1    0    0    -1
$EndComp
Text Label 9600 8200 2    40   ~ 0
5V
Text Label 9600 8300 2    40   ~ 0
GND
$Comp
L Connector_Generic:Conn_01x02 C5
U 1 1 7A000015
P 10900 8250
F 0 "C5" H 11800 8350 50  0000 C CNN
F 1 "C5 capacitor" H 12100 8150 50  0000 C CNN
F 2 "" H 10900 8250 50  0001 C CNN
F 3 "" H 10900 8250 50  0001 C CNN
	1    10900 8250
	1    0    0    -1
$EndComp
Text Label 10700 8200 2    40   ~ 0
BST
Text Label 10700 8300 2    40   ~ 0
SW
$Comp
L Connector_Generic:Conn_01x02 C6
U 1 1 7A000016
P 12000 8250
F 0 "C6" H 12900 8350 50  0000 C CNN
F 1 "C6 capacitor" H 13200 8150 50  0000 C CNN
F 2 "" H 12000 8250 50  0001 C CNN
F 3 "" H 12000 8250 50  0001 C CNN
	1    12000 8250
	1    0    0    -1
$EndComp
Text Label 11800 8200 2    40   ~ 0
5V
Text Label 11800 8300 2    40   ~ 0
GND
$Comp
L Connector_Generic:Conn_01x02 C7
U 1 1 7A000017
P 13100 8250
F 0 "C7" H 14000 8350 50  0000 C CNN
F 1 "C7 capacitor" H 14300 8150 50  0000 C CNN
F 2 "" H 13100 8250 50  0001 C CNN
F 3 "" H 13100 8250 50  0001 C CNN
	1    13100 8250
	1    0    0    -1
$EndComp
Text Label 12900 8200 2    40   ~ 0
12V_PROT
Text Label 12900 8300 2    40   ~ 0
GND
$Comp
L Connector_Generic:Conn_01x01 TP1
U 1 1 7A000018
P 6500 9600
F 0 "TP1" H 7400 9700 50  0000 C CNN
F 1 "12V_RAW test point" H 7700 9500 50  0000 C CNN
F 2 "" H 6500 9600 50  0001 C CNN
F 3 "" H 6500 9600 50  0001 C CNN
	1    6500 9600
	1    0    0    -1
$EndComp
Text Label 6300 9600 2    40   ~ 0
12V_RAW
$Comp
L Connector_Generic:Conn_01x01 TP2
U 1 1 7A000019
P 7300 9600
F 0 "TP2" H 8200 9700 50  0000 C CNN
F 1 "12V_PROT test point" H 8500 9500 50  0000 C CNN
F 2 "" H 7300 9600 50  0001 C CNN
F 3 "" H 7300 9600 50  0001 C CNN
	1    7300 9600
	1    0    0    -1
$EndComp
Text Label 7100 9600 2    40   ~ 0
12V_PROT
$Comp
L Connector_Generic:Conn_01x01 TP3
U 1 1 7A00001A
P 8100 9600
F 0 "TP3" H 9000 9700 50  0000 C CNN
F 1 "5V test point" H 9300 9500 50  0000 C CNN
F 2 "" H 8100 9600 50  0001 C CNN
F 3 "" H 8100 9600 50  0001 C CNN
	1    8100 9600
	1    0    0    -1
$EndComp
Text Label 7900 9600 2    40   ~ 0
5V
$Comp
L Connector_Generic:Conn_01x01 TP4
U 1 1 7A00001B
P 8900 9600
F 0 "TP4" H 9800 9700 50  0000 C CNN
F 1 "3V3 test point" H 10100 9500 50  0000 C CNN
F 2 "" H 8900 9600 50  0001 C CNN
F 3 "" H 8900 9600 50  0001 C CNN
	1    8900 9600
	1    0    0    -1
$EndComp
Text Label 8700 9600 2    40   ~ 0
3V3
$Comp
L Connector_Generic:Conn_01x01 TP5
U 1 1 7A00001C
P 9700 9600
F 0 "TP5" H 10600 9700 50  0000 C CNN
F 1 "GND test point" H 10900 9500 50  0000 C CNN
F 2 "" H 9700 9600 50  0001 C CNN
F 3 "" H 9700 9600 50  0001 C CNN
	1    9700 9600
	1    0    0    -1
$EndComp
Text Label 9500 9600 2    40   ~ 0
GND
$Comp
L Connector_Generic:Conn_01x01 TP6
U 1 1 7A00001D
P 10500 9600
F 0 "TP6" H 11400 9700 50  0000 C CNN
F 1 "STEP test point" H 11700 9500 50  0000 C CNN
F 2 "" H 10500 9600 50  0001 C CNN
F 3 "" H 10500 9600 50  0001 C CNN
	1    10500 9600
	1    0    0    -1
$EndComp
Text Label 10300 9600 2    40   ~ 0
STEP
$Comp
L Connector_Generic:Conn_01x01 TP7
U 1 1 7A00001E
P 11300 9600
F 0 "TP7" H 12200 9700 50  0000 C CNN
F 1 "DIR test point" H 12500 9500 50  0000 C CNN
F 2 "" H 11300 9600 50  0001 C CNN
F 3 "" H 11300 9600 50  0001 C CNN
	1    11300 9600
	1    0    0    -1
$EndComp
Text Label 11100 9600 2    40   ~ 0
DIR
$Comp
L Connector_Generic:Conn_01x01 TP8
U 1 1 7A00001F
P 12100 9600
F 0 "TP8" H 13000 9700 50  0000 C CNN
F 1 "EN test point" H 13300 9500 50  0000 C CNN
F 2 "" H 12100 9600 50  0001 C CNN
F 3 "" H 12100 9600 50  0001 C CNN
	1    12100 9600
	1    0    0    -1
$EndComp
Text Label 11900 9600 2    40   ~ 0
EN
$Comp
L Connector_Generic:Conn_01x01 TP9
U 1 1 7A000020
P 12900 9600
F 0 "TP9" H 13800 9700 50  0000 C CNN
F 1 "TMC_UART test point" H 14100 9500 50  0000 C CNN
F 2 "" H 12900 9600 50  0001 C CNN
F 3 "" H 12900 9600 50  0001 C CNN
	1    12900 9600
	1    0    0    -1
$EndComp
Text Label 12700 9600 2    40   ~ 0
TMC_UART
$Comp
L Connector_Generic:Conn_01x01 TP10
U 1 1 7A000021
P 13700 9600
F 0 "TP10" H 14600 9700 50  0000 C CNN
F 1 "TMC_DIAG test point" H 14900 9500 50  0000 C CNN
F 2 "" H 13700 9600 50  0001 C CNN
F 3 "" H 13700 9600 50  0001 C CNN
	1    13700 9600
	1    0    0    -1
$EndComp
Text Label 13500 9600 2    40   ~ 0
TMC_DIAG
$Comp
L Connector_Generic:Conn_01x01 TP11
U 1 1 7A000022
P 14500 9600
F 0 "TP11" H 15400 9700 50  0000 C CNN
F 1 "HX_SCK test point" H 15700 9500 50  0000 C CNN
F 2 "" H 14500 9600 50  0001 C CNN
F 3 "" H 14500 9600 50  0001 C CNN
	1    14500 9600
	1    0    0    -1
$EndComp
Text Label 14300 9600 2    40   ~ 0
HX_SCK
Text Notes 700 10500 0 60 ~ 12
Pico: GP21 is physical pin 27; pin 28 is GND. VBUS pin 40 is NC; VSYS pin 39 receives regulated 5V.
Text Notes 700 10700 0 60 ~ 12
TMC UART: GP8 pin11 -> R1 1k -> GP9 pin12 / TMC PDN_UART. DIAG -> GP7 pin10. VREF NC; INDEX omitted.
Text Notes 700 10900 0 60 ~ 12
Mini12864 uses straight-through 10-conductor IDC cables: controller pin N connects display pin N.
$EndSCHEMATC
