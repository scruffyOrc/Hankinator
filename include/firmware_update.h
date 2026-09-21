#pragma once
#include <Arduino.h>

enum class FirmwareUpdateStatus:uint8_t {Off,Starting,Ready,Uploading,Success,Error};

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
}
