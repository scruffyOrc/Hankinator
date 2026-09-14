#pragma once
#include <Arduino.h>
#include <U8g2lib.h>
#include "panel_pixels.h"
#include <EEPROM.h>
#include "pico/time.h"
#include "config.h"

constexpr uint8_t LCD_CS=Pins::LcdCs,LCD_DC=Pins::LcdDc,LCD_RESET=Pins::LcdReset,LCD_CLOCK=Pins::LcdClock,LCD_DATA=Pins::LcdData;
constexpr uint8_t ENC_CLICK=Pins::EncoderClick,ENC_A=Pins::EncoderA,ENC_B=Pins::EncoderB,BEEPER=Pins::Beeper,NEOPIXEL_PIN=Pins::NeoPixel,STOP_BUTTON=Pins::Stop;
constexpr uint8_t STEPPER_STEP=Pins::Step,STEPPER_DIR=Pins::Dir,STEPPER_ENABLE=Pins::Enable,HEARTBEAT_LED=Pins::Heartbeat;
constexpr bool WIND_DIRECTION_HIGH=Config::WindDirectionHigh;
constexpr uint8_t NEOPIXEL_COUNT=3;

namespace Config {
constexpr uint32_t STEPS_PER_MOTOR_REV=StepsPerMotorRev,STEPS_PER_HUB_REV=StepsPerHubRev;
constexpr float LAUNCH_MOTOR_RPS=LaunchMotorRps,LAUNCH_RAMP_TURNS=LaunchRampTurns;
constexpr float START_MOTOR_RPS=StartMotorRps,CRUISE_MOTOR_RPS=CruiseMotorRps,END_MOTOR_RPS=EndMotorRps;
constexpr uint32_t START_HOLD_MS=StartHoldMs,ACCEL_RAMP_MS=AccelRampMs;
constexpr float DECEL_TURNS=DecelTurns,PAUSE_END_MOTOR_RPS=PauseEndMotorRps,PAUSE_RAMP_TURNS=PauseRampTurns;
constexpr float RESUME_START_MOTOR_RPS=ResumeStartMotorRps,RESUME_RAMP_TURNS=ResumeRampTurns;
constexpr int SPEED_TRIM_MIN_PERCENT=SpeedTrimMin,SPEED_TRIM_MAX_PERCENT=SpeedTrimMax,SPEED_TRIM_STEP_PERCENT=SpeedTrimStep;
constexpr int MIN_TURNS=MinTurns,MAX_TURNS=MaxTurns;
constexpr uint32_t STEP_PULSE_US=StepPulseUs,IDLE_TIMER_US=IdleTimerUs,SPLASH_TIME_MS=SplashTimeMs,RAINBOW_CYCLE_MS=RainbowCycleMs;
constexpr size_t EEPROM_SIZE=EepromSize;
constexpr uint32_t SETTINGS_MAGIC=SettingsMagic;
constexpr uint16_t SETTINGS_VERSION=SettingsVersion;
}

struct YarnWeight{const char* label;};
struct SkeinSize{const char* label;uint16_t factoryDefaultTurns;};
inline constexpr YarnWeight YARN_WEIGHTS[]={{"Lace/Suri"},{"Fingering"},{"Sport"},{"DK"},{"Worsted"},{"Chunky"},{"Bulky"},{"Jumbo"}};
inline constexpr SkeinSize SKEIN_SIZES[]={{"Mini",63},{"Half",90},{"Full",110}};
inline constexpr int YARN_WEIGHT_COUNT=sizeof(YARN_WEIGHTS)/sizeof(YARN_WEIGHTS[0]);
inline constexpr int SKEIN_SIZE_COUNT=sizeof(SKEIN_SIZES)/sizeof(SKEIN_SIZES[0]);
struct PersistentSettings{uint32_t magic;uint16_t version;uint16_t turns[YARN_WEIGHT_COUNT][SKEIN_SIZE_COUNT];uint16_t runCurrentMa;uint16_t holdCurrentMa;uint8_t bluetoothEnabled;uint8_t clockwise;uint16_t weightTargetsCentiGrams[3];uint16_t autoTurns;};
enum class ProductMode:uint8_t{Turninator,Fuhgeddabouditinator,LoadCellFault};
enum class FuhProgram:uint8_t{Mini,Half,Full};
enum class UiState{AUTO_MODE_SELECT,TURN_SELECT,FUH_PROGRAM_SELECT,TARING,TARE_FAILED,LOAD_YARN,WINDING,COMPLETE,REPEAT_PROMPT,CONFIG_MENU,CONFIG_MOTOR_MENU,CONFIG_RUN_CURRENT,CONFIG_HOLD_CURRENT,CONFIG_DIRECTION,CONFIG_WEIGHT_MENU,CONFIG_WEIGHT_VALUE,CONFIG_PAIRING,CONFIG_LOAD_CELL_DIAGNOSTICS,CONFIG_FIRMWARE_UPDATE,LOAD_CELL_FAULT};
enum class PauseState:uint8_t{RUNNING,RAMPING_DOWN,PAUSED,RAMPING_UP};
enum class MotionPhase:uint8_t{LAUNCH,START_HOLD,ACCEL,CRUISE,DECEL,PAUSE_DOWN,PAUSED,RESUME};

extern U8G2_UC1701_MINI12864_F_4W_SW_SPI display;
extern PanelPixels pixels;
extern PersistentSettings settings;
extern ProductMode productMode;
extern bool weightCapabilityEnabled;
extern uint8_t detectedLoadCellCount;
extern UiState uiState;
extern volatile PauseState pauseState;
extern volatile uint32_t pauseRampStartStep,pauseRampStopStep,resumeRampStartStep,resumeRampEndStep;
extern float pauseRampStartRPS;
extern uint32_t pauseStartedTime,totalPausedTime;
extern bool pauseTimeRecorded;
extern int selectedYarnWeight,selectedSkeinSize,selectedTurns;
extern int selectedCruiseSpeedIndex;
extern float selectedCruiseMotorRPS;
extern FuhProgram selectedFuhProgram,activeFuhProgram;
extern bool repeatYes;
extern bool autoCountMode;
extern int speedTrimPercent;
extern float currentMotorRPS;
extern uint32_t windingStartTime;
extern uint32_t finalRunStepCount;
extern float finalRunWeightGrams;
extern bool finalRunWeightValid;
extern bool finalRunAborted;
extern uint32_t launchCompleteElapsedMs;
extern volatile bool motionActive;
extern volatile uint32_t currentStepCount,targetStepCount,requestedStepRateHz;
extern volatile uint32_t motionSegmentStopStep;
extern alarm_id_t stepAlarmId;
extern int lastA;
extern bool lastClick,lastStop,heartbeatState;
extern uint32_t lastTurnEncoderTime,lastHeartbeat;
extern int configMenuSelection;
extern int motorMenuSelection;
extern int weightTargetMenuSelection;
