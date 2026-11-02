# Adaptive StallGuard recovery

This is the first active recovery implementation after hardware-validated commit 0448e3f. Load thresholds and timing are provisional. Test on the machine before treating detection as characterized protection. No count of lost steps is inferred or added back. All RPS settings below refer to the motor; the hub turns four times slower.

## Responses

- Two 50 ms low-load-margin observations trigger a smooth speed cap reduction toward 1.0 motor RPS at 2.0 RPS per second. Existing launch, endpoint and weight-batch limits can demand a still lower speed.
- The first incident requires one uninterrupted healthy commanded hub turn at reduced speed, using the reduced-speed threshold and a 15-count margin. Pauses, missing data, marginal samples and excluded phases reset that turn window. Speed then increases at 0.5 RPS per second toward the selected cruise speed.
- A second distinct incident, including during ramp-up, locks the reduced speed until the run ends. A persistent first event is not counted repeatedly as new events.
- Severe SG (20 or below) or sustained/recurring DIAG evidence for 100 ms stops immediately. Low margin persisting 600 ms also stops. An inconclusive recovery window times out after 10 seconds of eligible motion.
- The display says CHECK YARN / BIND; red LEDs and a clearance prompt replace winding feedback. There is no automatic reverse into an obstruction. The encoder acknowledges clearance; STOP aborts and returns to the active selector. Confirmation requires fresh driver data without thermal/short faults. Two confirmed retries are allowed; the third fault stop aborts.
- Resume stays at reduced speed, with a gentle launch from 0.25 motor RPS. Count mode resumes its remaining commanded turns. Weight mode first takes a tension-relieved stopped measurement, then discards the interrupted segment's grams-per-turn estimate and relearns from a one-turn batch if more weight is needed. Any earlier automatic slowdown also invalidates that segment at the next weighing stop.
- Count uncertainty persists through recovery and completion. It is visible on the final screen and in telemetry. A new run resets recovery and uncertainty.

## Detection limits and tuning

Load-based detection starts at 1.0 motor RPS. Below that, including 0.5 RPS approach and auxiliary tension relief, SG is observational. One motor RPS is the provisional lower usable boundary, not a universal hardware guarantee. At or below the reduced speed, there may be no further speed reduction available; persistence still causes a stop.

SG floors are centralized in config.h: 70 below 1.5 motor RPS, 85 below 2.5, and 100 at/above 2.5. A learned per-band healthy median adds a relative threshold of 35% after eight observations. Classification precedes learning. Suspicious samples and recovery phases cannot lower the learned baseline; unseen bands use the provisional absolute floor. Ordinary detection requires 150 ms of speed stability; severe evidence remains active during production acceleration above the minimum speed. User pause ramps and auxiliary motion are excluded from load classification. Each cached SG conversion is classified at most once.

StallGuard4 is a load indicator in StealthChop, not a hub encoder. The TMC2209 compares SG_RESULT with twice SGTHRS for DIAG, and its usefulness depends on speed, current and the motor. Very low speed is an unreliable operating region. See [ADI TMC2209 datasheet, sections 5.5.3 and 11](https://www.analog.com/media/en/technical-documentation/data-sheets/tmc2209_datasheet_rev1.09.pdf). Configuration retains StealthChop and current settings; TPWMTHRS is explicitly zero. SGTHRS remains 10 and TCOOLTHRS remains 0xFFFFF. Software excludes low-speed DIAG events rather than changing that existing hardware threshold dynamically.

Fresh electrical fault flags (OT/OTPW and short-circuit bits 0–5) abort and disable the driver; open-load bits are not treated as definitive faults. No automatic current increase or chopper-mode change is performed. Protection cannot guarantee a specific number of lost turns or detect every belt slip.

## UART, telemetry and timing

Runtime reads use a nonblocking request/response parser in tmc_driver.cpp. Requests alternate SG_RESULT and DRV_STATUS every 25 ms, with an 8 ms response deadline. It verifies framing, register and CRC, handles TX echo, and distinguishes a valid zero SG from communication failure. Samples older than 200 ms are invalid. Invalid data for 300 ms of production motion causes a clearance stop. Driver data must be available before a run begins; UART failure at boot requires restoring the wiring and restarting. A runtime cable interruption can recover without restarting once valid responses return.

DIAG edges are latched by a minimal ISR and consumed by the supervisor. No UART, logging, display or motor action happens inside that ISR. STEP generation stays on the existing hardware alarm. Fault response latency is bounded by observed loop/poll timing, not a claimed hard realtime guarantee; capture STEP and DIAG during characterization. Driver telemetry shares the cache rather than issuing competing UART reads.

CSV appends stall_recovery and count_uncertain. Stall recovery uses previously unused flag bits; existing v9 logs remain readable and decode as normal/no uncertainty. The subsequent Hall feature adds a v10 sample suffix with a backward-compatible v9 reader; see hall-and-network-update.md. RUN_START advertises stall_recovery. Events include STALL_SLOWDOWN, STALL_HEALTHY_TURN_RAMP_UP, STALL_SLOW_LOCK, STALL_CHECK_YARN_HOLD, STALL_DRIVER_DATA_HOLD, STALL_USER_CLEAR_RESUME_SLOW, WEIGHT_RELEARN_AFTER_STALL and abort reasons. Event streams remain the existing live instrumentation; saved samples retain the recovery state.

## Validation

Run tests/native/run.ps1 with a native g++ compiler. It compiles the actual recovery policy and supervisor against hardware mocks, plus the shared UART frame decoder. Tests cover noise rejection, healthy-turn proof/reset, first restoration, second-event lock, severe/DIAG/persistent/communication stops, retry exhaustion, low-speed exclusion, timer rollover, fault priority, clearance gating, weight remeasurement/relearning, final count weighing, CRC failure and TX echo.

Hardware sequence:

1. Run normal Count and Mini jobs while capturing Bluetooth. Verify no false holds, normal pause/resume, endpoint slowdown, weight accuracy and correct CSV columns. Test a cold and warmed driver.
2. During stable cruise, apply modest controlled drag, release it after slowdown, and confirm a full healthy commanded hub turn precedes ramp-up. Repeat drag during ramp-up, then later in cruise; either recurrence must lock slow speed.
3. Sustain drag or deliberately bind briefly: verify the machine stops with no automatic reverse. Clear it and click. Count resumes slowly with its uncertainty flag; Weight weighs first and relearns. Test STOP while held and during resumed measurement.
4. Repeat faults through the retry limit; verify abort rather than unlimited retries. Test user pause during the healthy-turn window, near a weight batch boundary, and near the count endpoint.
5. Interrupt UART only (do not unplug an energized motor): verify responsive STOP, communication hold and refusal to resume without restored data. Simulated electrical fault flags are preferable to creating a physical short or overtemperature.
6. Mark the hub and compare commanded versus actual movement. Characterize response and observed lost motion, including belt slip. Use these traces to revise the per-speed floors, DIAG threshold and debounce times. Do not presume every detected anomaly represents lost steps, or that every belt skip produces a detectable anomaly.
