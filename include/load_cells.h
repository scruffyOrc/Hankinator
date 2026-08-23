#pragma once
#include <Arduino.h>
#include "config.h"

enum class LoadCellStatus:uint8_t {NoData,Ok,Error};
enum class LoadCellTareStatus:uint8_t {Idle,Collecting,Complete,Failed,Cancelled};

struct LoadCellSnapshot {
    int32_t raw[Config::LoadCellChannelCount]{};
    uint8_t healthyMask=0;
    LoadCellStatus status[Config::LoadCellChannelCount]{};
    uint32_t sequence=0;
};

struct LoadCellProfileTareResult {
    int32_t raw[Config::LoadCellProfileBins][Config::LoadCellChannelCount]{};
    uint8_t samples[Config::LoadCellProfileBins]{};
    LoadCellTareStatus status=LoadCellTareStatus::Idle;
};

struct LoadCellTareResult {
    int32_t offset[Config::LoadCellChannelCount]{};
    int32_t spread[Config::LoadCellChannelCount]{};
    uint8_t samples=0;
    LoadCellTareStatus status=LoadCellTareStatus::Idle;
};

namespace LoadCells {
void begin();
void service();
uint8_t detectAtStartup();
LoadCellSnapshot snapshot();
bool healthy(uint8_t channel);
LoadCellStatus status(uint8_t channel);
const char* statusName(LoadCellStatus status);
void startTare();
void startProfileTare(uint32_t startStep);
LoadCellTareStatus updateProfileTare(uint32_t currentStep);
LoadCellTareStatus updateTare();
void cancelTare();
LoadCellTareStatus tareStatus();
const char* tareStatusName(LoadCellTareStatus status);
LoadCellTareResult tareResult();
bool tareValid();
bool profileTareValid();
LoadCellProfileTareResult profileTareResult();
float weightGrams(const LoadCellSnapshot& snapshot);
float weightGrams();
float profileWeightGrams(const LoadCellSnapshot& snapshot,uint32_t hubStep);
}
