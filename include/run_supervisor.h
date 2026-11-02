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
    float weightSpreadGrams=NAN;
    float targetWeightGrams=NAN;
    bool weightArmed=false;
    bool settlingFinalWeight=false;
    bool finalWeightMeasured=false;
    float gramsPerTurn=NAN;
    uint16_t plannedStageTurns=0;
    uint8_t approachBatch=0;
};

namespace RunSupervisor {
void beginRun();
bool waitingForClearance();
bool confirmClearance();
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
