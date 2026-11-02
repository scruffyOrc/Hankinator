#pragma once
#include <Arduino.h>

enum class FirmwareUpdateStatus:uint8_t {Off,Starting,Ready,Uploading,Success,Error,Connecting,Scanning};

namespace FirmwareUpdate {
bool begin();
void end();
void poll();
bool active();
bool uploading();
FirmwareUpdateStatus status();
uint8_t progressPercent();
uint32_t bytesReceived();
uint8_t errorCode();
const char* statusText();
const char* ipAddress();
const char* networkName();
const char* accessCode();
const char* networkMessage();
bool stationMode();
// Bluetooth provisioning only scans and saves credentials. It never starts
// the firmware-upload portal or leaves Wi-Fi enabled after completion.
bool beginBluetoothProvisioning();
bool bluetoothProvisioningActive();
bool selectBluetoothNetwork(uint8_t index); // true => password required
bool submitBluetoothPassword(const char* password);
void clearSavedNetwork();
void useAccessPoint();
void scan();
bool scanning();
uint8_t networkCount();
const char* ssid(uint8_t index);
bool secured(uint8_t index);
bool selectNetwork(uint8_t index); // true => password entry; false => connecting
void editPassword(int delta);
bool acceptPasswordCharacter(); // true => connection started
const char* passwordToken();
uint8_t passwordLength();
void forgetNetwork();
}
