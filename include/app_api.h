#pragma once
#include "runtime.h"
void loadFactoryDefaults(); bool settingsAreValid(); void saveSettings(); void loadSettings();
uint16_t getSavedTurns(); void loadSelectedTurnCount(); void saveSelectedTurnCount();
void setPanelLights(); void drawStartupSplash(); void runStartupSplash(); void beep(uint16_t duration=30);
uint32_t calculateTargetSteps(); uint32_t stepsPerSecondForMotorRPS(float); void enableMotor(); void disableMotor();
int64_t stepAlarmCallback(alarm_id_t,void*); uint32_t safeCurrentStepCount(); float turnsCompleted(); float turnsRemaining();
uint32_t activeWindingElapsedMs(); float startupProfileMotorRPS(); float endingProfileMotorRPS(); float normalRequestedMotorRPS();
void updateWindingSpeed(); void requestPause(); void requestResume();
void drawYarnWeightScreen();void drawSkeinScreen();void drawTurnScreen();void drawReadyScreen();void drawWindingScreen();
void drawCompleteScreen();void drawRepeatPromptScreen();void drawCurrentScreen();
int getTurnIncrement();void updateEncoder();void updateButton();void updateStopButton();
void resetToYarnWeightSelection();void beginWinding();void updateHeartbeat();void updatePauseState();void updateMotor();
namespace App {void setup();void loop();}
