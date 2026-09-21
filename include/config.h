#pragma once
#include <Arduino.h>

namespace Pins {
constexpr uint8_t Step=2, Dir=3, Enable=4, Stop=6, Diag=7;
// RP2040/RP2350 UART1 alternate function: GP8=TX, GP9=RX.
constexpr uint8_t TmcTx=8, TmcRx=9, NeoPixel=11, Beeper=12;
constexpr uint8_t EncoderClick=13, EncoderA=14, EncoderB=15;
constexpr uint8_t LcdDc=16, LcdCs=17, LcdClock=18, LcdData=19, LcdReset=20;
constexpr uint8_t LoadCellClock=21;
constexpr uint8_t LoadCellData[]={22,26,27,28};
constexpr uint8_t Heartbeat=LED_BUILTIN;
}

namespace Config {
constexpr char FirmwareVersion[]="0.5.0-dev";
constexpr char FirmwareUpdateSsid[]="Hankinator-Update";
constexpr char FirmwareUpdatePassword[]="hankinator";
constexpr uint16_t MotorStepsPerRev=200;
constexpr uint8_t Microsteps=8;
constexpr uint16_t MotorPulleyTeeth=20, HubPulleyTeeth=80;
constexpr uint32_t StepsPerMotorRev=MotorStepsPerRev*Microsteps;
constexpr uint32_t StepsPerHubRev=StepsPerMotorRev*HubPulleyTeeth/MotorPulleyTeeth;
constexpr float LaunchMotorRps=.25f, LaunchRampTurns=2.0f;
constexpr float StartMotorRps=1.0f, CruiseMotorRps=4.0f, EndMotorRps=0.5f;
constexpr float CruiseSpeedOptions[]={0.5f,1.0f,2.0f,3.0f,4.0f};
constexpr uint8_t CruiseSpeedOptionCount=sizeof(CruiseSpeedOptions)/sizeof(CruiseSpeedOptions[0]);
constexpr uint32_t StartHoldMs=5000, AccelRampMs=5000;
constexpr float DecelTurns=10.0f, PauseEndMotorRps=.25f, PauseRampTurns=1.0f;
constexpr float ResumeStartMotorRps=.5f, ResumeRampTurns=2.0f;
constexpr int SpeedTrimMin=50, SpeedTrimMax=150, SpeedTrimStep=10;
constexpr int MinTurns=1, MaxTurns=9999;
constexpr uint32_t StepPulseUs=2, IdleTimerUs=1000;
constexpr uint32_t SplashTimeMs=4000, RainbowCycleMs=1200;
constexpr size_t EepromSize=4096;
constexpr uint32_t SettingsMagic=0x48414E4B, LogMagic=0x484C4F47;
constexpr uint16_t SettingsVersion=4, LogVersion=8;
constexpr bool WindDirectionHigh=true;
constexpr float TmcRsense=.11f;
constexpr uint8_t TmcAddress=0, StallThreshold=10;
constexpr uint16_t DefaultRunCurrentMa=850,DefaultHoldCurrentMa=300;
constexpr uint16_t MinRunCurrentMa=600,MaxRunCurrentMa=1200,CurrentStepMa=50;
constexpr uint16_t MinHoldCurrentMa=100,MaxHoldCurrentMa=600;
constexpr uint32_t ConfigHoldMs=1500,PairingWindowMs=60000;
constexpr uint8_t LoadCellChannelCount=sizeof(Pins::LoadCellData)/sizeof(Pins::LoadCellData[0]);
constexpr uint8_t RequiredLoadCells=4;
constexpr uint8_t LoadCellDetectionSamples=2;
constexpr uint32_t LoadCellDetectionWindowMs=1500,LoadCellHealthyTimeoutMs=1000;
constexpr uint32_t LoadCellDiagnosticsRefreshMs=100;
constexpr uint8_t LoadCellTareSamples=16;
constexpr uint32_t LoadCellTareTimeoutMs=6000,LoadCellTareMotorSettleMs=750;
constexpr uint8_t LoadCellProfileBins=32;
constexpr uint32_t LoadCellProfileTareTimeoutMs=12000;
constexpr float LoadCellProfileMotorRps=0.5f;
// Normal characterized profile ranges are below about 5k/5k/18k/18k.
// These generous limits reject a disturbed revolution or wiring fault.
constexpr int32_t LoadCellProfileMaxRangeCounts[]={25000,25000,30000,30000};
constexpr int32_t LoadCellTareMaxSpreadCounts[]={3000,3000,4000,4000};
// Provisional grams/count coefficients derived from the four 127 g corner
// calibrations and validated with 23.35 g, 53.98 g, 333.55 g and real hanks.
// RL is intentionally negative because that bridge is wired with opposite polarity.
constexpr float LoadCellGramsPerCount[]={
    0.00235218936f,
    0.00239339701f,
   -0.00264938080f,
    0.00237786342f
};
constexpr uint32_t FirmwareUpdateDisplayRefreshMs=250,FirmwareUpdateRebootDelayMs=1500;
constexpr uint32_t TelemetryIntervalMs=100;
// Development control features. Stall detection is active and logged, but the
// first release does not stop the motor automatically. Weight approach is
// armed by the Fuhgeddabouditinator presets (or diagnostics during development),
// with the selected turn count retained as a safety ceiling.
constexpr bool EnableStallDetection=true,EnableAutomaticStallAbort=false;
constexpr uint32_t ControlSupervisorIntervalMs=100,StallEventCooldownMs=3000;
constexpr float StallDetectionMinimumRps=3.0f;
constexpr uint16_t StallCandidateSg=100,StallSevereSg=20;
constexpr uint8_t StallConfirmSamples=2;
constexpr float MinimumWeightTargetGrams=5.0f,MaximumWeightTargetGrams=2000.0f;
constexpr float WeightApproachMinimumMarginGrams=3.0f,WeightApproachMarginFraction=0.15f;
constexpr float WeightApproachMotorRps=0.5f,WeightApproachDecelRpsPerSecond=1.0f;
constexpr float WeightStopMaxSpreadGrams=5.0f;
constexpr float WeightProbeResumeHysteresisGrams=1.0f,WeightProbeRetryHubTurns=5.0f;
constexpr uint8_t WeightFilterSamples=9,WeightStopConfirmSamples=3,WeightProbeStableSamples=3;
constexpr uint8_t WeightProbeMinimumSamples=16;
constexpr float WeightProbeMaxRangeGrams=15.0f;
constexpr uint8_t WeightProbeMaxConsecutiveRetries=3;
constexpr uint16_t DefaultWeightTargetsCentiGrams[]={2020,5100,10100};
constexpr uint16_t MinWeightTargetCentiGrams=100,MaxWeightTargetCentiGrams=50000,WeightTargetStepCentiGrams=10;
constexpr uint16_t FuhSafetyTurnCeiling=1000;
constexpr float FuhCruiseMotorRps=3.0f;
constexpr uint8_t WeightFinalSettleSamples=8;
constexpr float WeightFinalSettleMaxRangeGrams=2.0f;
constexpr uint32_t WeightFinalSettleTimeoutMs=15000;
// Development instrumentation only. Disable for production builds while
// retaining Bluetooth access to persisted LOG commands.
constexpr bool EnableBluetoothLiveTelemetry=true;
static_assert(RequiredLoadCells>0&&RequiredLoadCells<=LoadCellChannelCount,"RequiredLoadCells must fit the configured HX711 channels");
static_assert(sizeof(LoadCellTareMaxSpreadCounts)/sizeof(LoadCellTareMaxSpreadCounts[0])==LoadCellChannelCount,"Tare spread limits must match load-cell channels");
static_assert(sizeof(LoadCellProfileMaxRangeCounts)/sizeof(LoadCellProfileMaxRangeCounts[0])==LoadCellChannelCount,"Profile range limits must match load-cell channels");
static_assert(sizeof(LoadCellGramsPerCount)/sizeof(LoadCellGramsPerCount[0])==LoadCellChannelCount,"Calibration coefficients must match load-cell channels");
}
