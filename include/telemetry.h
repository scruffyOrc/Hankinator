#pragma once
#include <Arduino.h>
enum class PauseState:uint8_t;
enum class RunEnd:uint8_t {Complete,Abort};
namespace Telemetry {
void begin();
void start();
void sample(float rps,uint32_t steps,PauseState pause);
void finish(RunEnd end);
void executeCommand(const char* command,Print& output,bool logAccessAllowed);
}
