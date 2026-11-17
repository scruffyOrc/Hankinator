#pragma once
#include <Arduino.h>
#include "config.h"
namespace HallSensor {
enum class State:uint8_t {Disabled,Seeking,Absent,Acquiring,Locked,Fault,CommissioningSeeking,CommissioningIndexed};
void begin();
// ignoreEdges is used when reversing immediately after an indexed magnet: the
// first edge is the magnet we just left, not the landmark we want to park on.
void startSeek(bool forward=true,float motorRps=Config::HallSeekMotorRps,uint8_t ignoreEdges=0);
void cancel();
void service();
bool seekFinished();
bool seekFailed();
// Diagnostic movement can intentionally suppress Hall feedback so Hall
// tracking cannot become another variable in a mechanical characterization.
void beginRun(bool suppressFeedback=false);
void endRun();
// Any forced recovery stop can leave the commanded STEP position slightly out
// of phase with the physical hub.  Reacquire the five-magnet pattern before
// applying strict landmark-order or missing-edge rules again.
void resumeAfterClearance();
State state();
const char* stateName();
const char* stateName(State value);
uint32_t pulseCount();
uint32_t correctionSteps();
bool activeLevel();
void startCommissioningIndex(float motorRps=Config::CommissioningIndexMotorRps);
bool commissioningIndexFinished();
bool commissioningIndexFailed();
// A utility-mode tracker: Hall edges are counted without participating in
// winding correction or fault logic while the user turns the hub by hand.
// When firstEdgeIsClosePair is true, the hub is parked at the first magnet of
// the close pair and manual winding begins in the configured winding direction.
void beginManualTracking(bool firstEdgeIsClosePair=false);
void endManualTracking();
uint32_t manualPulseCount();
// Full turns are derived from the known five-magnet pattern after the hub is
// parked at the first close-pair magnet. The sequence is interpreted according
// to the direction selected when manual tracking begins.
uint32_t manualQuarterTurns();
}
