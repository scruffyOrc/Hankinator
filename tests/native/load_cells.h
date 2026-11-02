#pragma once
#include "config.h"
struct LoadCellSnapshot {uint32_t sequence=0;uint8_t healthyMask=15;};
namespace LoadCells {
LoadCellSnapshot snapshot();float stoppedWeightGrams(const LoadCellSnapshot&);bool tareValid();
}
