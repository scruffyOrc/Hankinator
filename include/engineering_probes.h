#pragma once
#include <Arduino.h>

// Deliberately intrusive, non-persistent characterization tools. They are
// compiled only in an engineering build and never exposed in product firmware.
namespace EngineeringProbes {
bool handleCommand(const char* command,Print& output,bool allowed);
void poll();
}
