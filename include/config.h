#pragma once
#include <Arduino.h>

namespace Pins {
constexpr uint8_t Step=2, Dir=3, Enable=4, Stop=6, Diag=7;
// Temporary V1A touch-panel backlight control.  This drives the gate of the
// external P-channel MOSFET: LOW = LED rail on, HIGH = LED rail off.
constexpr uint8_t TouchBacklight=1;
// RP2040/RP2350 UART1 alternate function: GP8=TX, GP9=RX.
constexpr uint8_t TmcTx=8, TmcRx=9, NeoPixel=11, Beeper=12;
constexpr uint8_t EncoderClick=13, EncoderA=15, EncoderB=14;
constexpr uint8_t LcdDc=16, LcdCs=17, LcdClock=18, LcdData=19, LcdReset=20;
constexpr uint8_t LoadCellClock=21;
constexpr uint8_t Hall=10; // A3144 active-low, 3.3 V logic ONLY.
constexpr uint8_t LoadCellData[]={22,26,27,28};
constexpr uint8_t Heartbeat=LED_BUILTIN;
}

namespace Config {
constexpr char FirmwareVersion[]="0.99.12";
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
// The mechanism has a pronounced resonance near 1.0 motor RPS.  Keep the
// gentle launch profile, but cross that band rather than dwelling in it.
constexpr uint32_t StartHoldMs=750, AccelRampMs=5000;
constexpr float DecelTurns=10.0f, PauseEndMotorRps=.25f, PauseRampTurns=1.0f;
// Resume gently after an ordinary user pause, but traverse the known 1 RPS
// mechanical resonance before it can build. These values are independent of
// hub travel and do not affect Hall-fault recovery.
constexpr float ResumeStartMotorRps=.5f,ResumeBaseRampRpsPerSecond=.5f;
constexpr float ResumeResonanceCrossRpsPerSecond=3.0f;
constexpr float ResumeResonanceLowMotorRps=.85f,ResumeResonanceHighMotorRps=1.20f;
constexpr int SpeedTrimMin=50;
constexpr int SpeedTrimMax=150;
constexpr int SpeedTrimStep=10;
constexpr int MinTurns=1, MaxTurns=9999;
constexpr uint32_t StepPulseUs=2, IdleTimerUs=1000;
constexpr uint32_t SplashTimeMs=4000, RainbowCycleMs=1200;
constexpr size_t EepromSize=4096;
constexpr uint32_t SettingsMagic=0x48414E4B, LogMagic=0x484C4F47;
constexpr uint16_t SettingsVersion=11, LogVersion=10;
// Leave sleep off until the external backlight MOSFET is fitted.  The UI can
// be configured in advance, but the direct 3.3 V LED wiring cannot be shut
// down in software.
constexpr bool DefaultTouchDisplaySleepEnabled=false;
constexpr bool DefaultTouchDisplayDarkMode=false;
constexpr uint8_t DefaultTouchDisplaySleepMinutes=5;
constexpr uint8_t MinTouchDisplaySleepMinutes=1,MaxTouchDisplaySleepMinutes=60;
// Five magnets: symmetric pair about zero, plus 90/180/270 degrees.
// Characterize pulse edges before changing these provisional tolerances.
constexpr uint16_t HallPairGapDegrees=20;
constexpr float HallSeekMotorRps=0.5f;
constexpr uint32_t HallSeekMaxSteps=StepsPerHubRev/3,HallSeekTimeoutMs=6000;
constexpr uint32_t HallDebounceUs=1000,HallAcquisitionMaxSteps=StepsPerHubRev*3;
constexpr uint32_t HallPositionToleranceSteps=64,HallMaxCorrectionSteps=StepsPerHubRev/4;
constexpr uint32_t WifiConnectTimeoutMs=20000,WifiScanTimeoutMs=15000,WifiStationStartDelayMs=500;
constexpr uint8_t WifiStationConnectAttempts=2;
constexpr uint8_t WifiMaxNetworks=16;
constexpr bool WindDirectionHigh=true;
constexpr float TmcRsense=.11f;
constexpr uint8_t TmcAddress=0, StallThreshold=10;
constexpr uint16_t DefaultRunCurrentMa=850,DefaultHoldCurrentMa=300;
constexpr uint16_t MinRunCurrentMa=600,MaxRunCurrentMa=1200,CurrentStepMa=50;
constexpr uint16_t MinHoldCurrentMa=100,MaxHoldCurrentMa=600;
constexpr uint32_t ConfigHoldMs=1500,PairingWindowMs=60000;
constexpr uint8_t LoadCellChannelCount=sizeof(Pins::LoadCellData)/sizeof(Pins::LoadCellData[0]);
constexpr uint8_t RequiredLoadCells=4;
// Temporary panel profile while the Makerbase Mini12864 is installed.  Its
// three WS2812s are wired in a different physical sequence from the BTT
// Mini12864: display backlight, upper encoder LED, lower encoder LED.
// Set this false when the BTT replacement is fitted.
constexpr bool UseMakerbaseMini12864=true;
constexpr uint8_t PanelBacklightPixel=UseMakerbaseMini12864?0:2;
constexpr uint8_t PanelEncoderFirstPixel=UseMakerbaseMini12864?1:0;
constexpr uint8_t PanelEncoderSecondPixel=UseMakerbaseMini12864?2:1;
// The Makerbase buzzer is excessively loud for normal appliance use.  Keep
// the beep call sites intact so this can be restored with one setting.
constexpr bool EnableBeeper=false;
constexpr uint32_t StatusLedRefreshMs=50,StatusLedPulseMs=2000,StatusLedRainbowMs=3000,StatusLedStopMs=500;
constexpr uint16_t DefaultAutoTurns=62;
constexpr uint8_t LoadCellDetectionSamples=5;
constexpr uint32_t LoadCellDetectionWindowMs=1500,LoadCellHealthyTimeoutMs=1000;
constexpr uint32_t LoadCellDiagnosticsRefreshMs=100;
constexpr uint32_t LoadCellSyncTimeoutMs=1000;
// Keep both PD_SCK phases conservative on the long harness. The data bit is
// sampled at the end of the high phase, and pulse 25 uses the same timing.
constexpr uint8_t LoadCellSampleDelayUs=5;
// Observational only: a large delta is not proof of corruption (a real load can change).
constexpr int32_t LoadCellSuspiciousDeltaCounts=10000;
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
// Commissioning first measures each HX711 directly above its associated leg
// with these three known reference masses. Those same masses are then measured
// at the hub center to derive one machine-level hub gain. The four collars are
// independent validation fixtures in two hub orientations.
constexpr uint16_t CommissioningReferenceMassCentiGrams[]={7943,11159,33353};
constexpr uint8_t CommissioningReferenceMassCount=sizeof(CommissioningReferenceMassCentiGrams)/sizeof(CommissioningReferenceMassCentiGrams[0]);
constexpr uint8_t CommissioningDirectSampleCount=CommissioningReferenceMassCount*LoadCellChannelCount;
constexpr uint16_t DefaultCommissioningCollarCentiGrams[]={4806,4807,4807,4808};
constexpr uint32_t CommissioningMotorSettleMs=1000,CommissioningLoadSettleMs=1500;
constexpr uint8_t CommissioningSamples=40;
constexpr float CommissioningIndexMotorRps=0.5f;
constexpr int32_t CommissioningMinimumFirstChangeCounts=2000;
constexpr float CommissioningMinimumCoefficient=0.00001f,CommissioningMaximumCoefficient=0.02f;
constexpr float CommissioningMinimumHubGain=0.90f,CommissioningMaximumHubGain=1.10f;
constexpr float CommissioningValidationMinimumGrams=5.0f,CommissioningValidationFraction=0.05f;
constexpr uint32_t FirmwareUpdateDisplayRefreshMs=250,FirmwareUpdateRebootDelayMs=1500;
constexpr uint32_t TelemetryIntervalMs=100;
// Adaptive StallGuard recovery; provisional thresholds require characterization.
constexpr bool EnableStallDetection=true;
constexpr uint32_t ControlSupervisorIntervalMs=100;
constexpr uint16_t StallSevereSg=20;
constexpr bool EnableStallRecovery=true;
constexpr uint32_t StallPollMs=50,StallUartRequestMs=25,StallUartTimeoutMs=8,StallDataFreshMs=200;
// A confirmed recovery slows below cruise, but must not park at the 1.0 RPS
// mechanical resonance.  1.5 RPS is the first characterization target.
constexpr float StallRecoveryMinimumRps=1.0f,StallReducedMotorRps=1.5f;
constexpr float StallSlowdownRate=2.0f,StallRecoveryRampRate=0.5f;
constexpr uint32_t StallConfirmMs=100,StallSevereMs=100,StallPersistentMs=600;
constexpr uint32_t StallInvalidMs=300,StallStableMs=150,StallProofTimeoutMs=10000;
constexpr uint16_t StallSpeedFloors[]={70,85,100}; // <1.5, <2.5, >=2.5 motor RPS
constexpr uint16_t StallHealthyHysteresis=15;
constexpr uint8_t StallMaxHolds=3; // Two confirmed retries; third stop aborts.
constexpr float MinimumWeightTargetGrams=5.0f,MaximumWeightTargetGrams=2000.0f;
constexpr float WeightApproachMinimumMarginGrams=3.0f,WeightApproachMarginFraction=0.15f;
constexpr float WeightApproachMotorRps=0.5f,WeightApproachDecelRpsPerSecond=1.0f;
constexpr float WeightStopMaxSpreadGrams=5.0f;
constexpr float WeightProbeResumeHysteresisGrams=1.0f,WeightProbeRetryHubTurns=5.0f;
constexpr uint8_t WeightFilterSamples=9,WeightStopConfirmSamples=3,WeightProbeStableSamples=3;
constexpr uint8_t WeightProbeMinimumSamples=16;
constexpr float WeightProbeMaxRangeGrams=15.0f;
constexpr uint8_t WeightProbeMaxConsecutiveRetries=3;
constexpr uint8_t WeightEstimateCalibrationTurns=8;
// The initial eight-turn weight probe must also avoid the 1.0 RPS resonance.
constexpr float WeightEstimateCalibrationMotorRps=1.5f;
// A weight-estimation batch ends at a measurement boundary. Decelerate before
// it reaches that boundary instead of letting the STEP alarm halt at speed.
constexpr float WeightBatchDecelTurns=2.0f,WeightBatchEndMotorRps=0.5f;
constexpr float WeightEstimateBulkFraction=0.80f;
constexpr uint8_t WeightEstimateFinalReserveTurns=4;
constexpr uint8_t WeightEstimateMaxApproachTurns=4,WeightEstimateMaxApproachBatches=12;
constexpr float WeightEstimateMinimumGramsPerTurn=0.01f;
constexpr uint16_t DefaultWeightTargetsCentiGrams[]={2150,5300,11000};
constexpr uint16_t MinWeightTargetCentiGrams=100,MaxWeightTargetCentiGrams=50000,WeightTargetStepCentiGrams=10;
constexpr uint16_t FuhSafetyTurnCeiling=1000;
constexpr float FuhCruiseMotorRps=3.0f;
constexpr uint8_t WeightFinalSettleSamples=8;
constexpr float WeightFinalSettleMaxRangeGrams=2.0f;
constexpr uint32_t WeightFinalSettleTimeoutMs=15000;
// The frame and four load-cell mounts continue redistributing load for several
// seconds after the hub stops. Do not begin the quiet-window test until that
// initial mechanical relaxation has passed.
constexpr uint32_t WeightStoppedMechanicalSettleMs=3000;
// Let the hub and belt come fully to rest before the short reverse used to
// release yarn tension.  Reversing immediately after a controlled stop was
// abrupt enough to skip the belt on the completed machine.
constexpr uint32_t WeightTensionReliefPreReverseSettleMs=750;
constexpr uint32_t WeightTensionReliefSteps=StepsPerHubRev/16;
constexpr float WeightTensionReliefMotorRps=0.5f;
// Development instrumentation only. Disable for production builds while
// retaining Bluetooth access to persisted LOG commands.
constexpr bool EnableBluetoothLiveTelemetry=true;
static_assert(RequiredLoadCells>0&&RequiredLoadCells<=LoadCellChannelCount,"RequiredLoadCells must fit the configured HX711 channels");
static_assert(sizeof(LoadCellTareMaxSpreadCounts)/sizeof(LoadCellTareMaxSpreadCounts[0])==LoadCellChannelCount,"Tare spread limits must match load-cell channels");
static_assert(sizeof(LoadCellProfileMaxRangeCounts)/sizeof(LoadCellProfileMaxRangeCounts[0])==LoadCellChannelCount,"Profile range limits must match load-cell channels");
static_assert(sizeof(LoadCellGramsPerCount)/sizeof(LoadCellGramsPerCount[0])==LoadCellChannelCount,"Calibration coefficients must match load-cell channels");
static_assert(CommissioningReferenceMassCount>=2,"Commissioning needs at least two reference masses");
static_assert(sizeof(DefaultCommissioningCollarCentiGrams)/sizeof(DefaultCommissioningCollarCentiGrams[0])==LoadCellChannelCount,"Commissioning collar masses must match load-cell channels");
}
