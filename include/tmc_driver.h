#pragma once
#include <Arduino.h>
namespace TmcDriver { void begin(); void applyCurrent(uint16_t runMa,uint16_t holdMa); bool connected(); uint16_t sgResult(); uint32_t drvStatus(); bool hasCachedDrvStatus(); uint32_t cachedDrvStatus(); }
namespace TmcDriver {
struct Sample {uint16_t sg=0xFFFF;uint32_t status=0;bool valid=false;uint32_t sequence=0;bool statusValid=false;};
void poll();
Sample sample();
bool consumeDiag();
}
