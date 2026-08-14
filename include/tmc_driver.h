#pragma once
#include <Arduino.h>
namespace TmcDriver { void begin(); bool connected(); uint16_t sgResult(); uint32_t drvStatus(); }
