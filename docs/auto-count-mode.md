# Auto HankWinder count and weight selection

With load cells, startup opens Count based / Weight based (Weight highlighted initially). Count opens a plain turn dial; Weight opens Mini / Half / Full / Just Turn. STOP from either child selector returns to the mode choice. Without load cells, HankWinder opens the same count dial directly, including after configuration, STOP and Repeat No. It skips tare, loading confirmation and final weighing. The old yarn-category, skein-size and speed-selection workflow is no longer reachable in normal use. Legacy saved turn matrices remain intact for compatibility.

Count defaults to 62 on fresh settings and when migrating v4 settings. Confirming a different count saves it in EEPROM once, preserving it across runs and power cycles. Valid range is 1–1000 turns, retaining the Auto safety ceiling. Existing calibration matrix, motor, Bluetooth, direction, and weight-target settings are preserved during v4 migration.

Count uses the stationary energized tare and LOAD YARN confirmation. It uses the existing motion profile at the established Auto cruise setting of 3.0 motor RPS, including normal launch, deceleration, pause and resume. Short runs may never reach cruise. Weight targets are disabled for count mode, including stale diagnostic targets. No measurement batch interrupts the count run.

At the exact commanded count, count mode performs the existing 1/16-turn auxiliary reverse, waits for settling, and measures weight. It leaves tension released for unloading rather than recovering forward. Auxiliary steps do not change the production turn count. Missing or unstable final weight ends as an abort/unavailable result, with no automatic measurement retry loop. STOP remains an immediate abort.

Completion retains the turn count and final measured weight and prompts removal of the hank followed by an encoder click. Click opens Repeat. Yes starts a fresh tare and loading cycle with the same mode/settings. No returns to the count dial or weight presets, according to the active mode.

## Hardware verification

- Upgrade existing v4 settings: confirm current, direction, Bluetooth, turn matrix and weight targets persist; Count starts at 62. Set 75, confirm and cancel tare; restart and confirm Count still shows 75.
- Check Count/Weight selection, STOP back-navigation, Count repeat Yes/No, and Weight repeat Yes/No. Confirm count selection after a weight run still uses the saved count, not the weight safety ceiling.
- Run a short count (e.g. 3), then 62 or 75: verify launch, cruise when reachable, deceleration, exact production turns, one reverse relief, settled weight and persistent unload screen. Test pause/resume and STOP during winding and final weighing.
- Disturb tare: expect retry/cancel. Disconnect a sensor after tare: expect final measurement failure rather than indefinite retries or a claimed valid weight. Repeat with excessive final sensor movement.
- Verify Mini and Just Turn retain their weight-mode behavior. Without sensors, verify direct count selection, saved count after reboot, immediate start on confirmation, normal ramps, pause/resume, STOP back to count, completion without weight, Repeat Yes and Repeat No, and configuration exit back to count. No weight/count mode chooser should appear.

Build validation covers both rpipico2w and rpipicow. Physical motion, weight accuracy, EEPROM migration on a device and panel interactions require hardware validation.
