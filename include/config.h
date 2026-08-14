#pragma once
#include <Arduino.h>

namespace Pins {
constexpr uint8_t Step=2, Dir=3, Enable=4, Stop=6, Diag=7;
// RP2040/RP2350 UART1 alternate function: GP8=TX, GP9=RX.
constexpr uint8_t TmcTx=8, TmcRx=9, NeoPixel=11, Beeper=12;
constexpr uint8_t EncoderClick=13, EncoderA=14, EncoderB=15;
constexpr uint8_t LcdDc=16, LcdCs=17, LcdClock=18, LcdData=19, LcdReset=20;
constexpr uint8_t Heartbeat=LED_BUILTIN;
}

namespace Config {
constexpr uint16_t MotorStepsPerRev=200;
constexpr uint8_t Microsteps=8;
constexpr uint16_t MotorPulleyTeeth=20, HubPulleyTeeth=80;
constexpr uint32_t StepsPerMotorRev=MotorStepsPerRev*Microsteps;
constexpr uint32_t StepsPerHubRev=StepsPerMotorRev*HubPulleyTeeth/MotorPulleyTeeth;
constexpr float StartMotorRps=1.0f, CruiseMotorRps=4.0f, EndMotorRps=0.5f;
constexpr uint32_t StartHoldMs=5000, AccelRampMs=5000;
constexpr float DecelTurns=10.0f, PauseEndMotorRps=.25f, PauseRampTurns=1.0f;
constexpr float ResumeStartMotorRps=.5f, ResumeRampTurns=2.0f;
constexpr int SpeedTrimMin=50, SpeedTrimMax=150, SpeedTrimStep=10;
constexpr int MinTurns=1, MaxTurns=9999;
constexpr uint32_t StepPulseUs=2, IdleTimerUs=1000;
constexpr uint32_t SplashTimeMs=4000, RainbowCycleMs=1200;
constexpr size_t EepromSize=4096;
constexpr uint32_t SettingsMagic=0x48414E4B, LogMagic=0x484C4F47;
constexpr uint16_t SettingsVersion=1, LogVersion=1;
constexpr bool WindDirectionHigh=true;
constexpr float TmcRsense=.11f;
constexpr uint8_t TmcAddress=0, StallThreshold=10;
constexpr uint32_t TelemetryIntervalMs=100;
// Development instrumentation only. Disable for production builds while
// retaining Bluetooth access to persisted LOG commands.
constexpr bool EnableBluetoothLiveTelemetry=true;
}
