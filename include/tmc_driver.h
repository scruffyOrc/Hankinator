#pragma once
#include <Arduino.h>
namespace TmcDriver { void begin(); void applyCurrent(uint16_t runMa,uint16_t holdMa); bool connected(); uint16_t sgResult(); uint32_t drvStatus(); }
