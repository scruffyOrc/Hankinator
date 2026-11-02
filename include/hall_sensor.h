#pragma once
#include <Arduino.h>
namespace HallSensor {
enum class State:uint8_t {Disabled,Seeking,Absent,Acquiring,Locked,Fault,CommissioningSeeking,CommissioningIndexed};
void begin();
void startSeek(bool forward=true);
void cancel();
void service();
bool seekFinished();
bool seekFailed();
void beginRun();
void endRun();
void resumeAfterClearance();
State state();
const char* stateName();
const char* stateName(State value);
uint32_t pulseCount();
uint32_t correctionSteps();
bool activeLevel();
void startCommissioningIndex();
bool commissioningIndexFinished();
bool commissioningIndexFailed();
}
