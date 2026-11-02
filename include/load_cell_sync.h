#pragma once
#include <stdint.h>

// No clocks until every member of the fixed startup group is ready.
namespace LoadCellSync {
inline bool canRead(uint8_t expected,uint8_t ready) {
    return expected && (ready & expected)==expected;
}
inline uint8_t missing(uint8_t expected,uint8_t ready) {
    return expected & uint8_t(~ready);
}
}
