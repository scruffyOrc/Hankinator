# Hankinator firmware

The firmware is split into configuration/model, settings persistence, motion, display/UI, controls, lighting, TMC2209 access, telemetry, and the application controller. Existing UI, turn matrix/version, splash, motion profiles, and immediate STOP semantics are retained. StallGuard is observational: neither DIAG nor `SG_RESULT` stops motion.

## Startup configuration

Hold STOP and the encoder button together for about 1.5 seconds to enter configuration mode. This works at power-up or later during normal idle operation. If used while winding, STOP aborts motion before configuration can open. The configuration menu provides:

- Bluetooth enable/disable.
- A 60-second discoverable pairing window. Outside that window, already paired computers can connect but the device is not discoverable.
- A Motor submenu containing UART-controlled TMC2209 run current (600-1200 mA RMS in 50 mA steps), hold current (100-600 mA, never above run current), and rotation direction.
- On weight-capable hardware, a Weight Targets submenu for setting the exact Mini, Half, and Full completion weights in 0.1 g increments. Defaults are 20.2 g, 51.0 g, and 101.0 g.
- Clockwise and counter-clockwise direction choices. Clockwise is the default and preserves the original firmware's DIR polarity.

The default 850 mA run current reproduces the measured 1.198 V VREF operating point of the BTT TMC2209 V1.2 module with 0.11 ohm sense resistors. Hold current defaults to 300 mA. Settings versions 1 and 2 are migrated in place so the existing 24-entry turn matrix is preserved; version 2 also preserves its saved currents and Bluetooth setting while defaulting the new direction setting to clockwise.

The launch profile starts at 0.25 motor RPS and uses an eased ramp to 1.0 RPS across two hub turns before the existing startup hold and cruise acceleration. This reduces the initial belt impulse without increasing motor current.

After turn-count selection, the development UI offers a per-run motor cruise speed of 0.5, 1.0, 2.0, 3.0, or 4.0 RPS. It defaults to 0.5 RPS at boot. The selected ceiling is included in live `RUN_START` metadata and applies to the startup hold, cruise, ending profile, and weight-probe resume; 4.0 RPS preserves the original cruise speed.

## Runtime product detection and load cells

`Config::RequiredLoadCells` in `include/config.h` selects the population required for weight-capable hardware and currently defaults to 4. Four HX711 channels are configured: shared `PD_SCK` on GP21 and `DOUT` channels on GP22, GP26, GP27, and GP28. All HX711 boards and the Pico must share ground; power HX711 logic from 3.3 V unless a specific board is verified to level-shift `DOUT` safely.

At startup the firmware observes every configured channel for 1.5 seconds. A channel is detected when it supplies at least two completed 24-bit conversions whose result is not either HX711 saturation code (`0x7FFFFF` or `0x800000`). This conservatively proves a responsive HX711 and a non-saturated analog input, but it cannot conclusively distinguish a correctly attached bridge from every broken or floating load-cell condition. The rule is isolated in `src/load_cells.cpp` for later refinement from collected hardware data.

- Zero detected channels selects `Turninator`; existing winding behavior is unchanged and weight capability remains disabled.
- At least `RequiredLoadCells` selects `Fuhgeddabouditinator` and enables its weight-based winding programs.
- A nonzero count below the requirement selects `LoadCellFault`, displays the detected/required counts, and blocks configuration and winding until restart.

Normal acquisition polls data-ready without waiting. A ready conversion uses one bounded 25-clock transaction (channel A, gain 128), while STEP generation remains on the RP2040/RP2350 hardware alarm. Weight remains observational and never changes motion. Weight-capable hardware performs an initial diagnostic tare at startup and a mandatory tare before every winding run. The motor is enabled without STEP pulses, allowed to settle at standstill/hold current, and a rolling 16-sample window must satisfy the per-channel spread limits in `include/config.h`; STOP cancels, while an unstable timeout blocks winding and offers retry. Tare offsets are held only in RAM and are never reused across power cycles.

The provisional per-channel grams/count coefficients are centralized in `Config::LoadCellGramsPerCount`. Telemetry retains every raw value and additionally records calculated grams, weight spread, tare/weight validity, the full 32-bit TMC2209 `DRV_STATUS`, and the offsets and spreads used for the run. Persisted telemetry format version 9 is intentionally incompatible with older saved logs. Tare is not persisted.

## Fuhgeddabouditinator winding UI

Weight-capable hardware opens directly to four programs: `Mini 20g`, `Half 50g`, `Full 100g`, and `Just Turn`. The labels remain the yarn industry's nominal sizes; their actual default stopping targets are respectively 20.2 g, 51.0 g, and 101.0 g and can be changed independently in configuration. Preset programs run at a fixed 3.0 motor RPS and use 1000 turns only as an internal safety ceiling. Reaching that ceiling before the weight target, or losing a load-cell channel during a preset run, records an abort.

`Just Turn` uses the same empty one-revolution profile and load-yarn pause, but does not use load-cell health or weight to control motion. It runs at 3.0 motor RPS until STOP or the 1000-turn ceiling; encoder click still pauses and resumes. STOP is a successful completion for this program. When a valid tare profile is available, the stopped machine waits for quiet readings and shows settled weight alongside final turns on the completion screen.

The configuration menu includes a read-only Load Cell Diagnostics screen refreshed at 10 Hz. It maps FL=GP22, FR=GP26, RL=GP27, and RR=GP28 and displays each signed raw conversion with `OK`, `No Data`, or `Err`. `No Data` means no conversion has ever arrived. `Err` means a previously responding channel has stopped producing data within the health timeout or its latest conversion is an HX711 saturation result. STOP returns to the configuration menu. All four data pins are captured from one GPIO snapshot on each shared GP21 clock pulse.

## Wi-Fi firmware updates

The configuration menu's Firmware Update entry is available only after the physical STOP+encoder setup gesture. Entering it disables the motor, closes Bluetooth diagnostics, and starts a temporary WPA-protected access point:

> Bluetooth diagnostic build: `HANKINATOR_ENABLE_WIFI_UPDATER=0` is currently set in `platformio.ini`. The updater implementation remains in the source tree, but its Wi-Fi/WebServer code is not linked and the menu reports that updating is disabled. This temporary isolation flag is intended to identify the current Classic Bluetooth SPP regression before Wi-Fi updating is restored.

- Network: `Hankinator-Update`
- Password: `hankinator`
- Browser address: `http://192.168.4.1`

Choose the PlatformIO output `.pio/build/rpipicow/firmware.bin` in the web page and press **Install Update**. The browser and Mini12864 show transfer status. After the Pico OTA layer validates and stages the binary, the machine reboots automatically and the OTA bootloader installs it. STOP cancels update mode before an upload starts or after an error; it is intentionally ignored during an active upload or the reboot handoff.

LittleFS is now 768 KB so it can hold the current firmware image and persisted telemetry together. The first USB/UF2 flash using this layout can cause LittleFS to reformat because its boundary moved from the earlier 128 KB layout; export any required onboard telemetry first. EEPROM remains at the end of flash, but verify saved settings after this one-time migration. USB BOOTSEL remains the recovery path.

The current development updater is protected by the physically entered mode and the temporary WPA access-point password, and the Pico updater performs its normal image-integrity checks. Cryptographic publisher-signature enforcement is not enabled yet; add and securely back up a release signing key before treating Wi-Fi updates as a production distribution channel.

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
- `LOAD CELLS` reports product mode plus the latest raw and health value for every configured channel.
- `MARK <label>` inserts a timestamped `EVENT` record into the live stream while winding.
- `CONTROL STATUS` reports the current stall-detector and weight-approach state.
- `WEIGHT TARGET <grams>` arms the development weight controller on weight-capable hardware; `WEIGHT OFF` disables it.

`LOG DUMP` and `LOG CLEAR` are rejected while winding; `LOG HELP` and `MARK` remain available. Bluetooth live telemetry is development instrumentation controlled by `Config::EnableBluetoothLiveTelemetry` in `include/config.h`. It is currently enabled and streams samples beyond the 2048-entry persisted limit while a client is connected. Each run includes machine-readable `RUN_START` metadata (including current and launch settings), named motion phases, and a `RUN_END` record identifying completion versus STOP/abort. Set the flag to `false` for production while retaining post-run Bluetooth log access.

Bluetooth uses automatic "Just Works" confirmation because the prototype has no numeric-confirmation interface, but discoverability is restricted to the configuration menu's timed pairing window.

Every predictive decision uses a stopped measurement. Eight fresh corrected readings must settle within a 2 g range; an unstable 15-second timeout aborts a weight-controlled run rather than trusting a noisy estimate. Batch boundaries are enforced at the exact STEP in the hardware-alarm callback, and every batch contains a whole number of hub turns so all measurements occur at the same hub angle. The completion screen latches final turns and settled measured weight until the encoder is clicked. Telemetry emits `STEP_RESTART_ARMED` and `STEP_RESTART_FIRST_STEP_delay_us_...` markers so commanded restart time can be compared with the first generated STEP.

## Development winding supervision

The first-pass StallGuard detector runs only during steady cruise at 3.0 motor RPS or faster. It learns a per-run normal `SG_RESULT` baseline, marks a candidate below either 100 or 35% of that baseline, and confirms after two consecutive 100 ms samples. Candidate and confirmed states are recorded in telemetry and emitted as live `EVENT` rows. `Config::EnableAutomaticStallAbort` is deliberately `false`, so a detection does not stop or alter motion yet. Thresholds are provisional and must be evaluated against real winding data before enabling intervention.

Before every Fuhgeddabouditinator run, the energized hub turns through one complete slow revolution while empty and captures a 32-bin raw baseline indexed by hub angle. It then stops with the motor still energized and displays `LOAD YARN`; attach the yarn and click before winding begins, or press STOP to cancel. A preset then winds an eight-turn learning batch and stops to calculate that run's grams per turn. It winds 80% of the estimated remaining turns, stops and updates the estimate from the observed weight gain, then winds to four turns short of the revised estimate. Final approach uses batches of at most four turns at 0.5 RPS, with a stopped measurement and recalculation after every batch until the configured target is met. Batch starts and endings ramp across two turns to reduce belt impulse. Invalid initial slope, unhealthy load cells, measurement timeout, twelve unsuccessful approach batches, or the 1000-turn ceiling aborts the run. The winding screen retains the nominal 20 g, 50 g, or 100 g label rather than exposing the configured overage target.

## Characterization test

1. With the motor mechanically unloaded, boot and confirm `TMC2209 UART online`. If unavailable, check common ground, address straps, and the TX resistor topology.
2. Run each normal winding speed/trim and source type at least three times. Dump and label each CSV externally before the next run overwrites it.
3. Repeat with gentle intermittent drag, a sustained bind, and a deliberate near-stall. Never use fingers near the rotating assembly; apply load through yarn or a safe fixture and keep STOP accessible.
4. Compare `SG_RESULT` by steady velocity. Exclude acceleration, deceleration, and very-low-speed regions when choosing candidate thresholds. DIAG pulses may be missed by 10 Hz sampling, so the first characterization pass should treat SG_RESULT as primary; a later build can latch DIAG edges in an interrupt.
5. Only after distributions show separation across temperature, yarn/source types, and 50-150% trim should automatic response be designed.
6. For weight-control trials, select Mini, Half, or Full and keep STOP accessible. Confirm the `WEIGHT_CALIBRATION_BATCH`, `WEIGHT_BULK_BATCH`, `WEIGHT_REFINEMENT_BATCH`, `WEIGHT_APPROACH_BATCH`, stopped-measurement, and target events against the raw and filtered weight columns.

Primary reference: [Analog Devices TMC2209 datasheet Rev. 1.09](https://www.analog.com/media/en/technical-documentation/data-sheets/tmc2209_datasheet_rev1.09.pdf). StallGuard4 is enabled in StealthChop when `TCOOLTHRS >= TSTEP > TPWMTHRS`; its reading varies with motor, load, current, and velocity and updates once per full step.
