#pragma once

#include <Arduino.h>
#include "config.h"

// The user-interface boundary.  The first slice is intentionally read-only:
// it gives either display implementation one coherent diagnostics snapshot
// without allowing a renderer to reach into the HX711 service directly.
namespace Machine {

enum class LoadCellChannelStatus : uint8_t { NoData, Ok, Error };
enum class TareStatus : uint8_t { Idle, Collecting, Complete, Failed, Cancelled };
// Deliberately mirrors the controller-owned commissioning engine without
// exposing its implementation to either display.
enum class CommissioningState : uint8_t { Ready, Indexing, Taring, PromptLeg,
    CapturingLeg, PromptHubReference, CapturingHubReference, PromptAllFirst, CapturingAllFirst, PromptRotate, Rotating,
    PromptAllSecond, CapturingAllSecond, Complete, Failed };

struct DiagnosticsSnapshot {
    int32_t raw[Config::LoadCellChannelCount]{};
    LoadCellChannelStatus channelStatus[Config::LoadCellChannelCount]{};
    uint8_t healthyMask=0;
    uint32_t sequence=0;
    uint8_t detectedMask=0;
    uint8_t missingReadyMask=0;
    uint8_t suspiciousMask=0;
    uint32_t suspiciousCount=0;
    uint32_t syncTimeoutCount=0;
    uint8_t sampleDelayUs=0;
    float weightGrams=0.0f;
    bool weightValid=false;
    bool weightDiagnosticsSettling=false;
    TareStatus tareStatus=TareStatus::Idle;
};

struct CommissioningSnapshot {
    CommissioningState state=CommissioningState::Ready;
    uint8_t currentLeg=0;
    uint8_t currentReferenceMass=0;
    uint8_t samplesCollected=0;
    uint8_t samplesRequired=0;
    float referenceMassGrams=0.0f;
    float referenceMassesGrams[3]{};
    float expectedTotalGrams=0.0f;
    float firstValidationGrams=0.0f;
    float secondValidationGrams=0.0f;
    float hubGain=1.0f;
    bool firstValidationValid=false;
    bool secondValidationValid=false;
    bool active=false;
    char message[48]{};
};

struct WeightTargetsSnapshot {
    float grams[3]{};
};

// Motor settings are controller-owned for the same reason as the load-cell
// and winding workflows: displays request bounded changes but never talk to
// the TMC2209 or persistent settings directly.
struct MotorSettingsSnapshot {
    uint16_t runCurrentMa=Config::DefaultRunCurrentMa;
    uint16_t holdCurrentMa=Config::DefaultHoldCurrentMa;
    bool clockwise=true;
    uint16_t defaultRunCurrentMa=Config::DefaultRunCurrentMa;
    uint16_t defaultHoldCurrentMa=Config::DefaultHoldCurrentMa;
};

struct ConnectivitySnapshot {
    bool bluetoothEnabled=false;
    bool pairingOpen=false;
    bool clientConnected=false;
    uint32_t pairingSecondsRemaining=0;
};

enum class SystemCheckStage : uint8_t { Idle, CheckingLoadCells, Taring, SeekingClockwise, SeekingCounterClockwise, WaitingForStopButton, Passed, Failed };
struct SystemCheckSnapshot {
    SystemCheckStage stage=SystemCheckStage::Idle;
    bool active=false;
    bool loadCellsPassed=false;
    bool tarePassed=false;
    bool clockwiseHallPassed=false;
    bool counterClockwiseHallPassed=false;
    bool stopButtonPassed=false;
    char message[64]{};
};

// Winding is controller-owned.  Displays only request one of the established
// run types and render this state; they never manipulate timer motion, tare,
// Hall feedback, or the run supervisor directly.
enum class WindingStage : uint8_t { Idle, HallSeeking, HallFailed, Taring,
    TareFailed, LoadYarn, Running, Complete, Fault };

struct WindingSnapshot {
    WindingStage stage=WindingStage::Idle;
    bool weightMode=false;
    bool paused=false;
    bool pauseTransition=false;
    bool requiresClearance=false;
    uint16_t selectedTurns=0;
    float completedTurns=0.0f;
    float targetWeightGrams=NAN;
    float measuredWeightGrams=NAN;
    bool measuredWeightValid=false;
    uint16_t finalTurns=0;
    float finalWeightGrams=NAN;
    bool finalWeightValid=false;
    bool finalAborted=false;
};

void begin();
void service();
const DiagnosticsSnapshot& diagnostics();
const char* channelStatusName(LoadCellChannelStatus status);

// These are the only commands a display needs for the existing controller
// commissioning state machine.  They do not duplicate motion, Hall, tare, or
// persistence logic in a renderer.
bool openCommissioning();
bool confirmCommissioning();
void cancelCommissioning();
const CommissioningSnapshot& commissioning();

// Machine-specific settings exposed to either display.  A renderer never
// edits PersistentSettings or HallSensor state directly.
bool hallFeedbackEnabled();
void setHallFeedbackEnabled(bool enabled);
void toggleHallFeedback();

// Product preset weights are shared persistent settings.  Displays work with
// grams; this boundary validates and normalizes them to the stored 0.1 g
// resolution without exposing PersistentSettings to a renderer.
const WeightTargetsSnapshot& weightTargets();
bool setWeightTargetGrams(uint8_t preset,float grams);
bool resetWeightTargetToDefault(uint8_t preset);
void saveWeightTargets();

const MotorSettingsSnapshot& motorSettings();
bool setRunCurrentMa(uint16_t milliamps);
bool setHoldCurrentMa(uint16_t milliamps);
bool resetRunCurrentToDefault();
bool resetHoldCurrentToDefault();
void setClockwise(bool clockwise);
void toggleClockwise();
// Persist an already-applied motor setting. This is intentionally separate
// from setters so an encoder turn does not write EEPROM on every detent.
void saveMotorSettings();

const ConnectivitySnapshot& connectivity();
void setBluetoothEnabled(bool enabled);
void toggleBluetoothEnabled();
bool openPairingWindow();
void closePairingWindow();
void togglePairingWindow();

bool startSystemCheck();
bool confirmSystemCheckStopButton();
void cancelSystemCheck();
const SystemCheckSnapshot& systemCheck();

// Shared winding commands used by both Mini12864 and touch UI.  They retain
// one appliance workflow: user loads yarn, presses GO, then the controller
// performs Hall seek (when enabled), stationary tare, and hardware-alarm
// motion without a second interaction.
bool prepareCountWinding(uint16_t turns);
bool prepareWeightWinding(uint8_t preset);
bool confirmYarnLoaded();
bool togglePauseResume();
bool confirmSafetyClearance();
bool retryWindingPreparation();
// Exit the current winding workflow.  While active this performs the same
// immediate coast-and-disable abort as the physical STOP path; after a normal
// completion it simply returns the controller to the active mode selector.
bool exitWindingWorkflow();
const WindingSnapshot& winding();

bool startWeightDiagnosticsTare();
void closeWeightDiagnostics();

} // namespace Machine
