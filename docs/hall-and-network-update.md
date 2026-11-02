# Optional Hall feedback and network firmware updates

This is a development implementation, not a physically validated release. Existing
StallGuard recovery changes remain included. Nothing is flashed automatically.
Pico 2 W remains the default build; Pico W remains supported. No load-cell wiring
is changed. The hardware-alarm STEP generator is unchanged.

## Hall setup

Configuration -> **Hall feedback** toggles ON/OFF and saves immediately. The
factory/migration default is **OFF**. With OFF there is no seek or Hall-based
motion intervention. GP10 is reserved for an **active-low digital A3144** signal.
It is NOT an analog input.

### Electrical gate before connecting

The original Allegro A3144 needs a 4.5–24 V supply and has an open-collector
output. Powering the bare sensor at 5 V with its output pulled up to 3.3 V is
possible. The purchased module's onboard LED/pull-up circuit may instead pull
the signal toward 5 V. Verify the actual module schematic/continuity and signal
levels, then remove/reconfigure a conflicting pull-up or use a suitable level
translator. **Never apply a 5 V module output directly to GP10.** Share ground.
Firmware enables the Pico's internal pull-up; an appropriate external pull-up
to 3.3 V may be needed for the harness. Do not assume a module's supply-voltage
advertisement specifies its output voltage.

Primary reference: [Allegro A3141–A3144 datasheet](https://www.allegromicro.com/~/media/Files/Datasheets/A3141-2-3-4-Datasheet.ashx).

### Magnet geometry and acquisition

- Default layout: identical magnets at 350°, 10°, 90°, 180°, 270°, all with the
  same pole toward the sensor and comparable clearance. The 0° arm is between
  the pair. The maximum nominal gap is 90°, not five equal 72° sectors.
- `Config::HallPairGapDegrees` is the **total** pair separation, default 20°.
  The decoder assumes a symmetric pair. If the real placement differs, update
  that constant; this is not automatic geometry calibration. Both commanded
  directions use the symmetric pattern. One sensor cannot independently verify
  actual direction or distinguish every missed pulse from actual lost motion.
- Both inactive and active pulse widths must exceed 1 ms. The paired magnets
  must produce two distinct pulses with an inactive gap. A merged pair cannot
  index. Entry STEP positions are captured by a small CHANGE ISR; a completed
  pulse validates the position. No serial, display, or driver I/O runs in it.
- Each enabled run slowly seeks the first stable active edge at 0.5 motor RPS,
  capped at 120° of commanded hub movement and six seconds. Starting over a
  magnet requires leaving it and encountering the next. STOP cancels immediately.
  A seek failure cannot keep the motor turning indefinitely.
- After seeking, normal stationary, energized tare runs (when load cells exist),
  followed by LOAD YARN. Without load cells, Hall-enabled Count still prompts for
  loading after seek. Seek movement is auxiliary, not counted as winding.
- There is no unsolicited rotation on power-up. Digital presence cannot be
  established reliably at rest, so optional-sensor detection is deferred to this
  pre-run seek. If no pulse has ever been seen since boot and the input stays
  inactive, continue with current step-count/StallGuard behavior. The loading
  screen indicates HALL NOT DETECTED. This cannot distinguish absent hardware
  from a stuck-high/broken sensor. Once a pulse has been seen this boot, a later
  failed seek is a fault; a stuck-active seek is also a fault. Restart or disable
  Hall explicitly to return to optional-hardware fallback.
- During winding, count passages immediately and confirm two complete, matching
  five-gap cycles before index lock. Nominal gap tolerance is 64 commanded steps
  (~3.6°); full-cycle travel must also match within this tolerance. Acquisition
  is bounded to three commanded hub turns. Missing pulse detection is based on
  commanded travel, so it also operates at speeds below StallGuard's useful
  range, without timing out a deliberate stationary pause.
- Tension relief and recovery are excluded. After auxiliary motion, reacquire
  the pattern rather than count reverse/forward passages as winding. Paused and
  idle passages are not production counts. Uncontrolled hand movement is not a
  supported way to reposition the hub during a job.

### Recovery and correction boundaries

After index lock, a small positive position deficit must persist across two
successive landmarks before it is eligible for correction. A 64-step deadband
is deliberately left uncompensated. Individual ambiguous gaps (beyond half the
short pair interval), extra/missing edges, queue overflow, or an acquisition
timeout cause a clearance hold, **not guessed replacement turns**.

A confirmed small deficit extends only a Count job's commanded target, records
the correction, and stops for clearance. Encoder confirmation resumes gently
at reduced speed using the existing recovery controller; STOP aborts. There is
no automatic reverse into a bind. After confirmation, reacquire Hall lock. At
most two clear-and-resume retries are allowed before abort. Total correction
is capped at 1/4 hub turn and never extends the absolute 1000-commanded-turn
safety ceiling. Count display subtracts the accepted correction from commanded
progress; it remains marked uncertain, not advertised as exact encoder position.

Weight mode never adds compensating turns to its target. A Hall disturbance
uses the existing clear/confirm, tension-relieved weighing, and slope-relearning
path. Authoritative final weight remains a stopped measurement. Blind movement
before lock, after the last marker, or during auxiliary motion cannot be fully
verified. Large slips need investigation; this is not a full quadrature encoder.

### Telemetry

`HALL STATUS` works over USB/Bluetooth. CSV live and saved logs append
`hall_state,hall_pulses,hall_correction_steps`; `steps`/`turns` remain **commanded**
values for compatibility. For corrected Count progress subtract
`hall_correction_steps / 6400` from commanded turns. Live `HALL_EDGE` events
include the ISR-captured STEP position and pulse number for geometry inspection.
Other events mark seek, fallback, lock, faults, and correction clearance.
Saved sample format is v10; the reader still accepts v9 logs (Hall fields read
as off/zero). No flash write occurs for every Hall edge: samples still buffer
in RAM and commit at run end. Detailed events are live, not persisted.

## Wi-Fi update workflow

1. Enter Configuration -> **Firmware Update**. Bluetooth is suspended for update
   mode as before; motor motion is disabled.
2. Click the encoder (**Networks**) to scan visible SSIDs. Turn to select one;
   click to enter its password. Scan results are deduplicated and limited to 16.
3. For secured networks, turn through printable characters and click to append.
   The character wheel also has **DELETE** and **CONNECT**. Password contents
   are not shown or logged; the screen shows character count and selected token.
   Choose CONNECT when done. Supported passphrases are 8–63 printable ASCII
   characters. Open networks connect without password entry. STOP cancels.
4. On association **and DHCP success**, credentials are saved to EEPROM. The
   update screen shows `http://<assigned IP>` and a session login:
   username **admin**, password the eight-character code displayed on the LCD.
5. Open that address from a computer on the same reachable network, authenticate,
   and upload the matching target's `firmware.bin`. Successful update reboots
   automatically, as before. Do not disconnect power during upload/install.

Future visits to Firmware Update attempt the saved network automatically.
There is no Wi-Fi connection or update server during normal winding. Connection
and DHCP attempts are asynchronous with a 20-second limit; scans have a
15-second limit. Failure or loss of an idle update connection falls back to the
existing **Hankinator-Update** hotspot, password **hankinator**, address
**192.168.4.1**. Failed/cancelled attempts never overwrite the saved network.
Networks also offers **Scan again**, **Use update hotspot**, and **Forget saved
Wi-Fi**. STOP from Networks returns to the hotspot screen; STOP from the update
screen shuts Wi-Fi down and restores configured Bluetooth operation. Navigation
is locked during upload/success until completion or upload failure.

Use a reachable 2.4 GHz personal Wi-Fi network supported by the Pico stack.
Enterprise login, captive portals, hidden-SSID entry, and static IP setup are
not implemented. Guest/client isolation can prevent access despite DHCP success.
Credentials are stored unencrypted in local EEPROM flash, not in source control
or telemetry. The LAN update page uses HTTP with per-session Basic authentication,
not TLS or secure boot: use a trusted network; do not port-forward it. The
existing password-protected direct hotspot workflow remains available.

Settings migrate from v4/v5 to v6 without changing current/direction/turn/weight
values; older supported migrations remain. No partition change or filesystem
format is performed. Older firmware may reset settings it does not recognize
if you later roll back to a pre-v6 build. Export logs before rolling back;
pre-v10 firmware will not understand newly recorded Hall logs.

## Verification and machine checks

`tests/native/run.ps1` compiles the actual Hall and updater modules against
hardware mocks plus the pure Hall decoder, EEPROM migration, StallGuard recovery,
supervisor, and UART framing suites. These are software tests, not evidence of
physical timing, signal integrity, radio interoperability, or weighing accuracy.

Before normal use:

1. Leave Hall OFF. Confirm normal Count/Weight tare, load, pause, STOP, completion,
   Bluetooth logging, and original hotspot update still work.
2. Verify Hall signal voltage before connecting GP10. Enable Hall on an empty
   machine, keep hands clear, and capture Bluetooth/USB. Test an absent input,
   a valid first passage, starting over a magnet, and STOP during seek.
3. Verify five separate pulses per hub revolution and index lock in both
   directions at reduced and normal speed. Use HALL_EDGE intervals to check
   geometry; adjust sensor clearance/pair separation or the documented constants
   before expecting correction to work reliably. Keep both pair pulses distinct.
4. Safely simulate missed/extra Hall pulses or disconnect only the sensor signal
   after lock. Verify clearance hold, STOP priority, reduced-speed retry and
   retry limit. Never unplug an energized stepper. Do not create shorts or put
   fingers near moving belts/hub arms to test a bind.
5. Compare a marked hub's actual turns with count, correction telemetry and
   final results; check Weight still remeasures at rest after recovery. Test
   near an endpoint and across pause and tension relief.
6. Scan/connect with a correct password, wrong password, and unavailable AP;
   cancel entry; verify fallback, forget, reboot persistence, displayed DHCP IP,
   rejected unauthenticated LAN upload, successful update/reboot, and restored
   Bluetooth after leaving updater. Keep USB/BOOTSEL recovery available.
