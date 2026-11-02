#pragma once
#include "config.h"
enum class PauseState:uint8_t {RUNNING,RAMPING_DOWN,PAUSED,RAMPING_UP};
extern bool weightCapabilityEnabled,autoCountMode,motionActive;
extern uint32_t targetStepCount,motionSegmentStopStep,requestedStepRateHz;
extern float currentMotorRPS,selectedCruiseMotorRPS;
extern PauseState pauseState;
uint32_t safeCurrentStepCount();
bool startAuxiliaryHubMotion(bool,uint32_t,float);
bool consumeAuxiliaryMotionComplete();
bool auxiliaryMotionActive();
void cancelAuxiliaryMotion();
uint32_t auxiliaryStepsTravelled();
void requestStepGeneratorWake();
uint32_t stepsPerSecondForMotorRPS(float);
void disableMotor();
void enableMotor();
int serviceStepGeneratorWake();
extern uint32_t currentStepCount;
struct HallTestSettings {uint8_t hallEnabled=0;};
extern HallTestSettings settings;
