#pragma once
#include <Arduino.h>
#include "load_cells.h"
#include "run_supervisor.h"

namespace DiagnosticsTransport {
void begin();
void poll();
void setEnabled(bool enabled);
bool enabled();
bool connected();
void openPairingWindow();
void closePairingWindow();
bool pairingOpen();
uint32_t pairingSecondsRemaining();
void beginTelemetryStream(const char* yarnWeight,const char* skeinSize,uint16_t targetTurns);
void streamTelemetry(uint32_t ms,uint16_t rps100,uint32_t steps,uint16_t sg,uint8_t diag,uint8_t pause,uint8_t phase,uint32_t status,const LoadCellSnapshot& loadCells,int32_t weightCentiGrams,uint16_t weightSpreadCentiGrams,bool tareValid,bool weightValid,const RunSupervisorSnapshot& control);
void endTelemetryStream(const char* result,uint32_t durationMs,uint32_t totalSamples,uint16_t persistedSamples);
void streamEvent(uint32_t ms,const char* label);
}
