#pragma once

// The ILI9488/XPT2046 production diagnostics surface.  The implementation is
// compiled only by the touch-diagnostics PlatformIO environment.
namespace TouchDiagnosticsUi {
// A deliberately static preflight splash.  It is safe to show before the
// blocking HX711 discovery pass because it does no display work while that
// pass is running.
void showBootSplash();
// A short, local transition after hardware discovery.  This is intentionally
// called only after the HX711 loops have finished.
void finishBootSplash();
void begin();
void service();
// Queues the safe physical-STOP view transition.  The controller owns the
// actual motor disable; the UI only redraws on the next LVGL service pass.
void requestEmergencyStopView();
}
