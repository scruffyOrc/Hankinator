# Approval BOM

Per-design quantities below. System quantity is one breakout, two rear and two front boards. Mounting holes/test points are PCB features, not purchased components. Selected modular-jack assembly sourcing remains unresolved; see APPROVAL.md. Passive stock and assembly availability must be refreshed at order time.

## hankinator-lc-breakout

| Reference | Value | Manufacturer | MPN | LCSC |
|---|---|---|---|---|
|J1|CONTROLLER / 8P8C|Wurth Elektronik|615008144221|Unverified|
|J2|LEFT / 6P6C|Wurth Elektronik|615006143621|Unverified|
|J3|RIGHT / 6P6C|Wurth Elektronik|615006143621|Unverified|

## hankinator-lc-rear-smart-foot

| Reference | Value | Manufacturer | MPN | LCSC |
|---|---|---|---|---|
|J1|FROM BREAKOUT / 6P6C|Wurth Elektronik|615006143621|Unverified|
|J2|TO FRONT / 4P4C|Wurth Elektronik|615004143821|Unverified|
|J3|LOAD CELL / JST-XH|JST|B4B-XH-A(LF)(SN)|C144395|
|U1|HX711|Avia Semicon (Xiamen)|HX711|C43656|
|R1|10R|UNI-ROYAL|0603WAF100JT5E|C22859|
|C1|100n / 50V X7R|Samsung|CL10B104KB8NNNC|C1591|
|C2|10u / 25V X5R|Samsung|CL21A106KAYNNNE|C15850|
|C3|100n / 50V X7R|Samsung|CL10B104KB8NNNC|C1591|
|C4|10u / 25V X5R|Samsung|CL21A106KAYNNNE|C15850|
|R2|100R / 1%|UNI-ROYAL|0603WAF1000T5E|C22775|
|R3|100R / 1%|UNI-ROYAL|0603WAF1000T5E|C22775|
|C5|100n differential|Samsung|CL10B104KB8NNNC|C1591|
|R4|100R / DOUT source|UNI-ROYAL|0603WAF1000T5E|C22775|
|R5|100R / SCK stub|UNI-ROYAL|0603WAF1000T5E|C22775|
|R6|100k / SCK idle low|UNI-ROYAL|0603WAF1003T5E|C25803|

## hankinator-lc-front-smart-foot

| Reference | Value | Manufacturer | MPN | LCSC |
|---|---|---|---|---|
|J1|FROM REAR / 4P4C|Wurth Elektronik|615004143821|Unverified|
|J3|LOAD CELL / JST-XH|JST|B4B-XH-A(LF)(SN)|C144395|
|U1|HX711|Avia Semicon (Xiamen)|HX711|C43656|
|R1|10R|UNI-ROYAL|0603WAF100JT5E|C22859|
|C1|100n / 50V X7R|Samsung|CL10B104KB8NNNC|C1591|
|C2|10u / 25V X5R|Samsung|CL21A106KAYNNNE|C15850|
|C3|100n / 50V X7R|Samsung|CL10B104KB8NNNC|C1591|
|C4|10u / 25V X5R|Samsung|CL21A106KAYNNNE|C15850|
|R2|100R / 1%|UNI-ROYAL|0603WAF1000T5E|C22775|
|R3|100R / 1%|UNI-ROYAL|0603WAF1000T5E|C22775|
|C5|100n differential|Samsung|CL10B104KB8NNNC|C1591|
|R4|100R / DOUT source|UNI-ROYAL|0603WAF1000T5E|C22775|
|R5|100R / SCK stub|UNI-ROYAL|0603WAF1000T5E|C22775|
|R6|100k / SCK idle low|UNI-ROYAL|0603WAF1003T5E|C25803|

C1591: Samsung CL10B104KB8NNNC, 100 nF/50 V X7R 0603, [observed LCSC stock 2,115,600](https://www.lcsc.com/product-detail/C1591.html). C15850: CL21A106KAYNNNE, 10 uF/25 V X5R 0805, [JLC basic SMT listing](https://jlcpcb.com/partdetail/C15850); stock not verified. Resistors: [C22859 10 ohm](https://www.lcsc.com/product-detail/C22859.html), [C22775 100 ohm](https://jlcpcb.com/partdetail/C22775), [C25803 100 kohm](https://jlcpcb.com/partdetail/C25803); stock not verified.
