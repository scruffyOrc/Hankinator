#pragma once
#include "runtime.h"
void loadFactoryDefaults(); bool settingsAreValid(); void saveSettings(); void loadSettings();
void updatePanelLights();void signalStoppedLights();void setPanelLights(); void drawStartupSplash(); void runStartupSplash(); void beep(uint16_t duration=30);
uint32_t calculateTargetSteps(); uint32_t stepsPerSecondForMotorRPS(float); void enableMotor(); void disableMotor();
int64_t stepAlarmCallback(alarm_id_t,void*); uint32_t safeCurrentStepCount(); float turnsCompleted(); float turnsRemaining();
void requestStepGeneratorWake();int serviceStepGeneratorWake();bool consumeFirstStepAfterWake(uint32_t& delayUs,uint32_t& step);
bool startAuxiliaryHubMotion(bool forward,uint32_t steps,float motorRps);bool consumeAuxiliaryMotionComplete();bool auxiliaryMotionActive();void cancelAuxiliaryMotion();
uint32_t activeWindingElapsedMs(); float startupProfileMotorRPS(); float endingProfileMotorRPS(); float normalRequestedMotorRPS();
MotionPhase currentMotionPhase();
void updateWindingSpeed(); void requestPause(); void requestResume();
void drawWindingScreen();
void drawFuhProgramScreen();
void drawCompleteScreen();void drawRepeatPromptScreen();void drawCurrentScreen();
void drawConfigScreen();bool configurationButtonsHeld();bool updateConfigurationShortcut();
void drawLoadCellFaultScreen();
void drawLoadCellDiagnosticsScreen();
void drawTareScreen();
void drawLoadYarnScreen();
void drawFirmwareUpdateScreen();
int getTurnIncrement();void updateEncoder();void updateButton();void updateStopButton();
void resetToYarnWeightSelection();void requestWindingStart();void beginWinding();void updateTareProcess();void updateHeartbeat();void updatePauseState();void updateMotor();
namespace App {void setup();void loop();}
