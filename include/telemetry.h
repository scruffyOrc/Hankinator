#pragma once
#include <Arduino.h>
enum class PauseState:uint8_t;
enum class RunEnd:uint8_t {Complete,Abort};
namespace Telemetry {
void begin();
void diagnosticPoll(uint8_t screen); // 0=off, 1=raw, 2=weight, 3=timing
void start();
void sample(float rps,uint32_t steps,PauseState pause);
void finish(RunEnd end);
void event(const char* label);
void executeCommand(const char* command,Print& output,bool logAccessAllowed);
}
