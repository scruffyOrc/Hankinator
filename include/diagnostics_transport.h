#pragma once
#include <Arduino.h>

namespace DiagnosticsTransport {
void begin();
void poll();
void beginTelemetryStream(const char* yarnWeight,const char* skeinSize,uint16_t targetTurns);
void streamTelemetry(uint32_t ms,uint16_t rps100,uint32_t steps,uint16_t sg,uint8_t diag,uint8_t pause,uint16_t status);
void endTelemetryStream(const char* result,uint32_t durationMs,uint32_t totalSamples,uint16_t persistedSamples);
}
