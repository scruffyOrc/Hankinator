# Panel feedback

Pixel 2 is assigned as the backlight and remains the steady white display backlight outside the existing startup rainbow. Pixels 0 and 1 indicate state:

- Ready/selecting: green.
- Tare or stopped weight measurement: breathing blue.
- Load yarn, complete/unload, repeat prompt: breathing green.
- Winding: offset rainbow hues on a three-second cycle; configured counterclockwise direction reverses the sequence.
- Pause ramp: amber; paused: breathing amber; resume: amber fading to blue.
- STOP/reset: amber for half a second, then the current state.
- Fault, failed tare, aborted completion or firmware update error: breathing red.
- Configuration: purple.

Updates run at most every 50 ms, skip unchanged colors and never redraw the LCD. PanelPixels retains Adafruit's color buffer but submits bytes directly to a claimed PIO state machine with interrupts enabled, avoiding the stock show() interrupt mask. FIFO submission can briefly wait for space; STEP interrupts remain enabled throughout. It uses the same RGB ordering and 800 kHz PIO program as the prior output.

On hardware, verify pixel 2 is the backlight, both encoder LEDs have different winding colors, changing configured direction reverses the sequence, and pause/resume/fault indications match the display. Check a full-speed run with LED animation for stutters and compare STEP timing if a logic analyzer is available. Two physical LEDs give a changing color sequence rather than an unambiguous circular chase.

Weight presets now contain only Mini, Half and Full. Just Turn and its special STOP/tare handling have been removed; Count mode remains available.
