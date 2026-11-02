#pragma once
#include "Arduino.h"
struct TestSettings {char wifiSsid[33]{};char wifiPassword[64]{};};
extern TestSettings settings;
extern bool motionActive;
bool auxiliaryMotionActive();
void saveSettings();
