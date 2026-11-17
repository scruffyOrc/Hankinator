#pragma once
#include <Arduino.h>
#include "stall_recovery.h"

enum class StallDetectionState:uint8_t {Monitoring,Candidate,Confirmed};
enum class ClearanceReason:uint8_t {None,Load,DriverData,DriverMode,Hall};
enum class WeightApproachState:uint8_t {Disabled,Calibration,Bulk,Refinement,Approach,TargetReached};

struct RunSupervisorSnapshot {
    ClearanceReason clearanceReason=ClearanceReason::None;
    StallRecovery::Phase recovery=StallRecovery::Phase::Normal;
    bool countUncertain=false;
    uint8_t stallIncidents=0;
    StallDetectionState stallState=StallDetectionState::Monitoring;
    WeightApproachState weightState=WeightApproachState::Disabled;
    uint16_t sgResult=0xFFFF;
    uint16_t sgBaseline=0;
    float filteredWeightGrams=NAN;
    // This is display-only: the last settled reading projected forward by the
    // learned grams-per-turn and physical STEP count.  It must never be used
    // as an in-motion load-cell reading or as a completion decision.
    float predictedWeightGrams=NAN;
    bool predictedWeightValid=false;
    float weightSpreadGrams=NAN;
    float targetWeightGrams=NAN;
    bool weightArmed=false;
    bool settlingFinalWeight=false;
    bool finalWeightMeasured=false;
    // The first eight-turn probe was unable to establish a usable positive
    // yarn slope, and the one permitted re-tare/retry also failed.
    bool weightEstimateUnavailable=false;
    float gramsPerTurn=NAN;
    uint16_t plannedStageTurns=0;
    uint8_t approachBatch=0;
};

namespace RunSupervisor {
void beginRun();
void setRecoveryRestartLimit(uint8_t limit);
bool waitingForClearance();
bool confirmClearance();
// Hall feedback is a physical count reconciler, not the bind detector.  A
// run may continue after a tracking discontinuity, but callers mark the
// commanded count as uncertain so the UI and telemetry stay honest.
void markCountUncertain();
void holdForHallFault(const char* reason);
void service();
float limitMotorRps(float requestedRps);
RunSupervisorSnapshot snapshot();
bool armWeightTarget(float grams);
void disarmWeightTarget();
bool weightTargetArmed();
float weightTargetGrams();
bool completionIsAbort();
bool completionPending();
bool requestFinalMeasurement();
const char* recoveryName(StallRecovery::Phase phase);
const char* stallStateName(StallDetectionState state);
const char* weightStateName(WeightApproachState state);
}
