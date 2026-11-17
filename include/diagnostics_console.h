#pragma once

// Handles USB and Bluetooth diagnostics-console input, including Wi-Fi setup.
// Bluetooth lifecycle and telemetry streaming remain owned by the transport.
namespace DiagnosticsConsole {
void poll(bool bluetoothConnected);
}
