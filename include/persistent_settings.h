#pragma once
#include <stdint.h>
namespace SettingsLayout {
constexpr int YarnCount=8,SizeCount=3;
}
// Append-only layout. Keep the v5 prefix unchanged for EEPROM migration.
struct PersistentSettings {
    uint32_t magic;
    uint16_t version;
    uint16_t turns[SettingsLayout::YarnCount][SettingsLayout::SizeCount];
    uint16_t runCurrentMa,holdCurrentMa;
    uint8_t bluetoothEnabled,clockwise;
    uint16_t weightTargetsCentiGrams[3],autoTurns;
    uint8_t hallEnabled;
    char wifiSsid[33],wifiPassword[64];
    float loadCellGramsPerCount[4];
    uint16_t commissioningCollarCentiGrams[4];
    uint8_t loadCellCalibrationValid;
    uint8_t touchDisplaySleepEnabled;
    uint8_t touchDisplaySleepMinutes;
    uint8_t touchDisplayDarkMode;
    // Arm-use correction fitted after the direct-leg model using the three
    // reference masses at front/middle/rear arm positions.
    float loadCellHubGain;
    // Experimental distributed-payload model. Rows are front, middle, rear
    // collar positions across all four arms; columns are FL, FR, RL, RR
    // signed counts per gram.
    float loadCellArmCountsPerGram[3][4];
    uint8_t loadCellArmModelValid;
    // 0 = unlimited user-cleared recovery restarts for the current run.
    uint8_t stallRestartLimit;
};
