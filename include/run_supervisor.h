#pragma once
#include <Arduino.h>

enum class StallDetectionState:uint8_t {Monitoring,Candidate,Confirmed};
enum class WeightApproachState:uint8_t {Disabled,Calibration,Bulk,Refinement,Approach,TargetReached};

struct RunSupervisorSnapshot {
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
const char* stallStateName(StallDetectionState state);
const char* weightStateName(WeightApproachState state);
}
