# Shared-clock / live weight characterization

This change tests a synchronization hypothesis, not a proven diagnosis of the
large raw-value jumps. Raw readings and calibration coefficients are unchanged.

## Acquisition

At boot, hold GP21 low for the existing 1500 ms discovery window and record
which DOUT pins ever go low. Freeze that responding group until restart. Then
perform the existing sample/change validation for another 1500 ms. A low pin
is only a candidate response, not proof of a working bridge or HX711.

During operation, issue the shared 25-pulse A/128 transaction only when every
member of that group is ready. All data lines are sampled together. Waiting is
non-blocking. After 1000 ms without a complete group, report the missing mask
and increment a timeout counter; do not clock a partial group or drop a member.
The existing freshness check eventually marks the held samples stale. A group
with no responders produces no clock pulses. Restart after reconnecting hardware.

GP21 remains 2 mA, slow slew. Each clock pulse uses matched high and low phases
under interrupt protection; the existing STEP generation remains unchanged.
Reference: [HX711 datasheet](https://cdn.sparkfun.com/datasheets/Sensors/ForceFlex/hx711_english.pdf).

Raw deltas greater than 10000 counts are flagged as suspicious, not discarded,
smoothed, or classified as proven corruption. Real load changes can trigger this
flag. Existing healthy status is still startup/freshness/saturation based, not a
guarantee of plausible weight. Config constants centralize timeout and threshold.
LC_SYNC_v1 events report cumulative timeouts/jumps, at most once per second;
LOAD CELLS also prints these counters. Masks use bits FL=1, FR=2, RL=4, RR=8.

## Weight Diagnostics

Configuration > Weight Diagnostics holds the motor stationary, waits the
existing motor-settle interval, and collects the existing stable tare window.
The screen refreshes at 10 Hz and shows signed grams with no added smoothing.
Encoder press re-tares; STOP cancels collection, releases the motor, and returns
to configuration. Tare is temporary; normal winding still performs its own tare.
Missing or stale readings must show an error/no valid weight, not a frozen weight.

## Hardware comparison

### Adjustable sample delay (single-flash test)

Configuration > Load Cell Timing is available even in partial-cell fault mode.
The motor is disabled on entry. Turn the encoder to choose 1, 2, 3, 5, 8 or
10 microseconds; click to apply and restart the elapsed time and trial jump/
timeout counters. Selection alone does not apply. Re-click the active value
to start another trial. Lifetime counters remain intact. STOP returns to setup.

This changes the matched high and low phase delay for every clock pulse. Data is
sampled at the end of the high phase, and the 25th gain-select pulse uses the
same timing. The 2 mA drive and slow slew are unchanged. The applied value
remains active after leaving the screen, but is RAM-only and resets to 5
microseconds on reboot. Reboot before normal winding to ensure the baseline
timing is restored. The screen itself cannot start motion.

Live raw/status rows refresh at 10 Hz. J counts channel excursions (not frames),
TO counts group readiness timeouts. LOAD CELLS includes LC_TIMING with active
delay, elapsed milliseconds and trial counters. Keep wiring and load unchanged;
compare 60-second trials at 1, 2, 5 and 10 microseconds, then repeat 1 microsecond.
Record elapsed time rather than comparing cumulative counts across restarts.

### Automatic terminal capture

Opening raw Load Cell Diagnostics, Weight Diagnostics, or Load Cell Timing starts
a live-only 10 Hz CSV stream to connected USB Serial and Bluetooth terminals.
Leaving the screen ends the stream. No diagnostic samples are buffered for later
export or written to flash; LOG DUMP still refers to the normal winding log.
Enable PuTTY session logging before opening diagnostics.

LC_BEGIN/LC_END delimit visits. Screen IDs: 1=raw, 2=weight, 3=timing.
LC_COLUMNS describes LC_DATA fields: elapsed milliseconds, acquisition sequence,
active sample delay in microseconds, FL/FR/RL/RR signed raw values, healthy/missing/
jump/expected masks, cumulative jump and timeout counts, tare state, weight in
centigrams, weight-valid flag, and required count. Ignore weight when valid=0.
Tare states: 0 idle, 1 collecting, 2 complete, 3 failed, 4 cancelled.
Sequence lets analysis distinguish repeated snapshots from new conversions.
The schema/context repeats every 50 rows to support mid-session connection.

USB skips a whole line when its output buffer has insufficient room. Bluetooth
uses Arduino-Pico SerialBT's connection flag (availableForWrite returns 0 or 1,
not a byte capacity). SerialBT writes are blocking, so a slow Bluetooth receiver
can delay acquisition and UI handling on these stopped diagnostic screens.
This is not a non-blocking transport guarantee and does not change winding
telemetry. There is no replay. Row elapsed times expose gaps. LC_END rows is the
number attempted, not guaranteed delivered.
The existing cumulative counters are not reset by entering a screen, so compare
their differences over each timed interval. Timing changes appear in every row.

1. Keep cables, supply, load, and machine position unchanged. Capture stationary
   raw telemetry on the previous firmware, then this firmware. Record firmware
   identity and LOAD CELLS output for both. Do not recalibrate between captures.
2. Open Weight Diagnostics empty. Allow tare completion; watch/log at least
   60 seconds. Record drift, peak-to-peak weight, jump counters and timeouts.
3. Place a known weight, wait, remove it, then re-tare with the encoder. Confirm
   the expected sign/approximate magnitude and return near zero. Large deliberate
   load steps can legitimately increment the suspicious counter.
4. With motion stopped, disconnect a DOUT lead. Verify timeout mask, stale/no
   valid weight, responsive STOP, and no partial-group clock reads with a logic
   analyzer. Restore the lead and confirm recovery; reboot for changed membership.
5. Verify no-cell boot, partial-cell fault, and all-cell boot, then run an empty
   count-mode cycle. Confirm normal STEP/Hall behavior and compare raw excursions.

Native tests exhaust all 256 expected/ready mask combinations. The actual
load-cell module is also tested with simulated GPIO for zero, one, two, and four
responders, partial-group timeout without clocks, stale status, recovery, signed
raw data, unfiltered jump reporting, and tare/re-tare cancellation. Builds and host
tests do not validate cable signal integrity, physical pulse timing, or weight
accuracy; the comparison above is still required.
