# Hankinator firmware

The firmware is split into configuration/model, settings persistence, motion, display/UI, controls, lighting, TMC2209 access, telemetry, and the application controller. Existing UI, turn matrix/version, splash, motion profiles, and immediate STOP semantics are retained. StallGuard is observational: neither DIAG nor `SG_RESULT` stops motion.

## Breadboard wiring

- Pico GP7 <- TMC2209 `DIAG`. Both are 3.3 V logic. DIAG is push-pull, so no external pull-up is required; firmware uses an internal pulldown.
- Pico GP8 (UART1 TX) -> **1 kOhm series resistor** -> TMC2209 `PDN_UART`.
- Pico GP9 (UART1 RX) -> TMC2209 `PDN_UART` directly, on the driver side of the resistor.
- Pico GND <-> driver GND. The existing STEP, DIR, EN, VM, VIO, and motor wiring remains unchanged.
- Verify the module's `PDN_UART` pad and MS1/MS2 address straps; this firmware assumes address 0. Do not add a conventional strong pull-up to DIAG.

The 1 kOhm resistor isolates TX from RX during replies in the datasheet's single-wire UART circuit. GP8/GP9 are UART1-capable pins on Pico-family boards and were unused in the supplied firmware. Some StepStick boards route or strap `PDN_UART` differently; confirm continuity against the exact BTT V1.2 module before soldering.

## Offline telemetry

Samples are captured at 10 Hz in a 2048-entry RAM buffer (about 205 seconds). At completion or STOP, the run is committed once to a LittleFS file in a reserved 128 KiB flash region. A new completed/aborted run replaces the previous saved run, avoiding continuous flash writes while the motor is running.

Commands are available over USB Serial at 115200 baud and over the Pico W's Bluetooth Classic SPP connection. Pair the Windows development machine with the MAC-suffixed `Hankinator` device and open its outgoing COM port:

- `LOG DUMP` exports CSV.
- `LOG CLEAR` clears the saved run.
- `LOG HELP` lists commands.

`LOG DUMP` and `LOG CLEAR` are rejected while winding; `LOG HELP` remains available. Bluetooth live telemetry is development instrumentation controlled by `Config::EnableBluetoothLiveTelemetry` in `include/config.h`. It is currently enabled and streams samples beyond the 2048-entry persisted limit while a client is connected. Each run includes machine-readable `RUN_START` metadata and a `RUN_END` record identifying completion versus STOP/abort. Set the flag to `false` for production while retaining post-run Bluetooth log access.

Bluetooth currently uses automatic "Just Works" pairing because the prototype has no numeric-confirmation interface. Production firmware should restrict discoverability and pairing to a deliberate physical action such as holding STOP during startup.

## Characterization test

1. With the motor mechanically unloaded, boot and confirm `TMC2209 UART online`. If unavailable, check common ground, address straps, and the TX resistor topology.
2. Run each normal winding speed/trim and source type at least three times. Dump and label each CSV externally before the next run overwrites it.
3. Repeat with gentle intermittent drag, a sustained bind, and a deliberate near-stall. Never use fingers near the rotating assembly; apply load through yarn or a safe fixture and keep STOP accessible.
4. Compare `SG_RESULT` by steady velocity. Exclude acceleration, deceleration, and very-low-speed regions when choosing candidate thresholds. DIAG pulses may be missed by 10 Hz sampling, so the first characterization pass should treat SG_RESULT as primary; a later build can latch DIAG edges in an interrupt.
5. Only after distributions show separation across temperature, yarn/source types, and 50-150% trim should automatic response be designed.

Primary reference: [Analog Devices TMC2209 datasheet Rev. 1.09](https://www.analog.com/media/en/technical-documentation/data-sheets/tmc2209_datasheet_rev1.09.pdf). StallGuard4 is enabled in StealthChop when `TCOOLTHRS >= TSTEP > TPWMTHRS`; its reading varies with motor, load, current, and velocity and updates once per full step.
