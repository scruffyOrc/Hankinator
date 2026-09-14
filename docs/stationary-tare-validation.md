# Stationary tare and startup checks

Startup no longer tares or energizes the motor for weighing. The existing 1.5-second HX711 detection window now requires each detected channel to produce at least five non-saturated samples, at least two distinct valid raw values, and a valid latest sample no older than one second. Identical adjacent readings are allowed. Counts and timing are centralized in config.h.

A responding channel that fails these checks is latched as Err until restart and blocks normal startup, even if every responding channel fails. No responses at all still selects Turninator. Partial healthy hardware selects the existing load-cell fault screen; four healthy channels select Fuhgeddabouditinator with the default required count. Detection proves ADC activity and variation, not intact wiring between the bridge and HX711. A disconnected bridge can still generate varying data.

Before a weight-capable run, the motor is enabled without STEP pulses, allowed to settle for the same three seconds used for stopped weighing, and a fresh 16-sample stationary tare is collected using existing stability limits and timeout. The driver uses its existing configured standstill current. Successful tare opens LOAD YARN and waits for a click. STOP cancels. Tare failure retains retry/cancel and the existing Just Turn exception.

No initial full revolution, reverse relief, or forward recovery is performed during tare. During actual winding, stopped measurement tension relief and recovery remain in place. Weight control and telemetry use the stationary reference; the unused profile fields remain in persisted logs for format compatibility and have zero samples. Moving weight telemetry remains observational.

## Hardware checks before relying on the change

1. Boot with four connected sensors: expect hardware check and normal startup, no tare. Boot with none: expect Turninator. Boot with one channel disconnected: expect a fault. A test source repeating one fixed raw code should also fault; repeated values interspersed with different valid samples should pass.
2. Start Mini with an empty hub: verify motor holding, no rotation, then LOAD YARN. Verify STOP during settling and sample collection; verify retry after disturbing a tare.
3. Load yarn and wind: confirm the initial batch, stopped weighing with reverse relief, bulk winding, final approach, and final weight all work. Compare several completed hanks with an external scale and the previous baseline, including different initial hub positions.
4. Capture telemetry: stationary tare valid, profile tare invalid, usable weight readings after tare, and no pre-run rotation events. Check Bluetooth export and retained logs.
