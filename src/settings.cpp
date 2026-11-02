#include "app_api.h"
#include <math.h>

namespace {
struct LegacySettingsV1{uint32_t magic;uint16_t version;uint16_t turns[YARN_WEIGHT_COUNT][SKEIN_SIZE_COUNT];};
struct LegacySettingsV2{uint32_t magic;uint16_t version;uint16_t turns[YARN_WEIGHT_COUNT][SKEIN_SIZE_COUNT];uint16_t runCurrentMa;uint16_t holdCurrentMa;uint8_t bluetoothEnabled;uint8_t reserved;};
struct LegacySettingsV3{uint32_t magic;uint16_t version;uint16_t turns[YARN_WEIGHT_COUNT][SKEIN_SIZE_COUNT];uint16_t runCurrentMa;uint16_t holdCurrentMa;uint8_t bluetoothEnabled;uint8_t clockwise;};
// Version 7 had one shared commissioning-collar value. Version 8 records the
// four actual masses used by the fitted system model.
struct LegacySettingsV7{uint32_t magic;uint16_t version;uint16_t turns[YARN_WEIGHT_COUNT][SKEIN_SIZE_COUNT];uint16_t runCurrentMa,holdCurrentMa;uint8_t bluetoothEnabled,clockwise;uint16_t weightTargetsCentiGrams[3],autoTurns;uint8_t hallEnabled;char wifiSsid[33],wifiPassword[64];float loadCellGramsPerCount[4];uint16_t commissioningCollarCentiGrams;uint8_t loadCellCalibrationValid;};
// Version 8 is the complete commissioning layout before touch-display
// preferences were appended in version 9.
struct LegacySettingsV8{uint32_t magic;uint16_t version;uint16_t turns[YARN_WEIGHT_COUNT][SKEIN_SIZE_COUNT];uint16_t runCurrentMa,holdCurrentMa;uint8_t bluetoothEnabled,clockwise;uint16_t weightTargetsCentiGrams[3],autoTurns;uint8_t hallEnabled;char wifiSsid[33],wifiPassword[64];float loadCellGramsPerCount[4];uint16_t commissioningCollarCentiGrams[4];uint8_t loadCellCalibrationValid;};
struct LegacySettingsV9{uint32_t magic;uint16_t version;uint16_t turns[YARN_WEIGHT_COUNT][SKEIN_SIZE_COUNT];uint16_t runCurrentMa,holdCurrentMa;uint8_t bluetoothEnabled,clockwise;uint16_t weightTargetsCentiGrams[3],autoTurns;uint8_t hallEnabled;char wifiSsid[33],wifiPassword[64];float loadCellGramsPerCount[4];uint16_t commissioningCollarCentiGrams[4];uint8_t loadCellCalibrationValid,touchDisplaySleepEnabled,touchDisplaySleepMinutes;};
// Version 10 added the touch theme preference. Version 11 appends a final
// center-hub correction to the direct-leg commissioning model.
struct LegacySettingsV10{uint32_t magic;uint16_t version;uint16_t turns[YARN_WEIGHT_COUNT][SKEIN_SIZE_COUNT];uint16_t runCurrentMa,holdCurrentMa;uint8_t bluetoothEnabled,clockwise;uint16_t weightTargetsCentiGrams[3],autoTurns;uint8_t hallEnabled;char wifiSsid[33],wifiPassword[64];float loadCellGramsPerCount[4];uint16_t commissioningCollarCentiGrams[4];uint8_t loadCellCalibrationValid,touchDisplaySleepEnabled,touchDisplaySleepMinutes,touchDisplayDarkMode;};

bool turnsAreValid(const uint16_t turns[YARN_WEIGHT_COUNT][SKEIN_SIZE_COUNT])
{
    for(int weight=0;weight<YARN_WEIGHT_COUNT;weight++)
        for(int size=0;size<SKEIN_SIZE_COUNT;size++)
            if(turns[weight][size]<Config::MIN_TURNS||turns[weight][size]>Config::MAX_TURNS)return false;
    return true;
}
}

void loadFactoryDefaults()
{
    settings.magic =
        Config::SETTINGS_MAGIC;

    settings.version =
        Config::SETTINGS_VERSION;

    settings.runCurrentMa=Config::DefaultRunCurrentMa;
    settings.holdCurrentMa=Config::DefaultHoldCurrentMa;
    settings.bluetoothEnabled=1;
    settings.autoTurns=Config::DefaultAutoTurns;
    settings.clockwise=1;
    settings.hallEnabled=0;
    memset(settings.wifiSsid,0,sizeof(settings.wifiSsid));
    memset(settings.wifiPassword,0,sizeof(settings.wifiPassword));
    memcpy(settings.weightTargetsCentiGrams,Config::DefaultWeightTargetsCentiGrams,sizeof(settings.weightTargetsCentiGrams));
    memcpy(settings.loadCellGramsPerCount,Config::LoadCellGramsPerCount,sizeof(settings.loadCellGramsPerCount));
    memcpy(settings.commissioningCollarCentiGrams,Config::DefaultCommissioningCollarCentiGrams,sizeof(settings.commissioningCollarCentiGrams));
    settings.loadCellCalibrationValid=0;
    settings.touchDisplaySleepEnabled=Config::DefaultTouchDisplaySleepEnabled?1:0;
    settings.touchDisplaySleepMinutes=Config::DefaultTouchDisplaySleepMinutes;
    settings.touchDisplayDarkMode=Config::DefaultTouchDisplayDarkMode?1:0;
    settings.loadCellHubGain=1.0f;

    for (
        int weight = 0;
        weight < YARN_WEIGHT_COUNT;
        weight++
    )
    {
        for (
            int size = 0;
            size < SKEIN_SIZE_COUNT;
            size++
        )
        {
            settings.turns[weight][size] =
                SKEIN_SIZES[size]
                    .factoryDefaultTurns;
        }
    }
}

bool settingsAreValid()
{
    if (
        settings.magic !=
        Config::SETTINGS_MAGIC
    )
    {
        return false;
    }

    if (
        settings.version !=
        Config::SETTINGS_VERSION
    )
    {
        return false;
    }

    if(!turnsAreValid(settings.turns))return false;
    if(settings.autoTurns<Config::MinTurns||settings.autoTurns>Config::FuhSafetyTurnCeiling)return false;
    if(settings.runCurrentMa<Config::MinRunCurrentMa||settings.runCurrentMa>Config::MaxRunCurrentMa)return false;
    if(settings.holdCurrentMa<Config::MinHoldCurrentMa||settings.holdCurrentMa>Config::MaxHoldCurrentMa||settings.holdCurrentMa>settings.runCurrentMa)return false;
    for(uint8_t i=0;i<3;i++)if(settings.weightTargetsCentiGrams[i]<Config::MinWeightTargetCentiGrams||settings.weightTargetsCentiGrams[i]>Config::MaxWeightTargetCentiGrams)return false;
    if(settings.bluetoothEnabled>1||settings.clockwise>1||settings.hallEnabled>1||settings.loadCellCalibrationValid>1||settings.touchDisplaySleepEnabled>1||settings.touchDisplayDarkMode>1
        ||settings.touchDisplaySleepMinutes<Config::MinTouchDisplaySleepMinutes||settings.touchDisplaySleepMinutes>Config::MaxTouchDisplaySleepMinutes
        ||!memchr(settings.wifiSsid,0,sizeof(settings.wifiSsid))||!memchr(settings.wifiPassword,0,sizeof(settings.wifiPassword)))return false;
    for(uint8_t collar=0;collar<Config::LoadCellChannelCount;collar++)
        if(settings.commissioningCollarCentiGrams[collar]<100||settings.commissioningCollarCentiGrams[collar]>50000)return false;
    for(uint8_t channel=0;channel<Config::LoadCellChannelCount;channel++)
    {
        const float coefficient=settings.loadCellGramsPerCount[channel];
        if(!isfinite(coefficient)||fabsf(coefficient)<Config::CommissioningMinimumCoefficient||fabsf(coefficient)>Config::CommissioningMaximumCoefficient)return false;
    }
    if(!isfinite(settings.loadCellHubGain)||settings.loadCellHubGain<Config::CommissioningMinimumHubGain||settings.loadCellHubGain>Config::CommissioningMaximumHubGain)return false;
    return true;
}

void saveSettings()
{
    EEPROM.put(
        0,
        settings
    );

    EEPROM.commit();

    Serial.println(
        "Settings saved to flash"
    );
}

void loadSettings()
{
    EEPROM.begin(
        Config::EEPROM_SIZE
    );

    EEPROM.get(
        0,
        settings
    );

    if(settings.magic==Config::SETTINGS_MAGIC&&settings.version==1)
    {
        LegacySettingsV1 legacy{};
        EEPROM.get(0,legacy);
        if(turnsAreValid(legacy.turns))
        {
            loadFactoryDefaults();
            memcpy(settings.turns,legacy.turns,sizeof(settings.turns));
            saveSettings();
            Serial.println("Settings v1 migrated; turn matrix preserved");
            return;
        }
    }

    if(settings.magic==Config::SETTINGS_MAGIC&&settings.version==2)
    {
        LegacySettingsV2 legacy{};
        EEPROM.get(0,legacy);
        if(turnsAreValid(legacy.turns)&&legacy.runCurrentMa>=Config::MinRunCurrentMa&&legacy.runCurrentMa<=Config::MaxRunCurrentMa&&legacy.holdCurrentMa>=Config::MinHoldCurrentMa&&legacy.holdCurrentMa<=Config::MaxHoldCurrentMa&&legacy.holdCurrentMa<=legacy.runCurrentMa&&legacy.bluetoothEnabled<=1)
        {
            loadFactoryDefaults();
            memcpy(settings.turns,legacy.turns,sizeof(settings.turns));
            settings.runCurrentMa=legacy.runCurrentMa;
            settings.holdCurrentMa=legacy.holdCurrentMa;
            settings.bluetoothEnabled=legacy.bluetoothEnabled;
            saveSettings();
            Serial.println("Settings v2 migrated; direction defaults to clockwise");
            return;
        }
    }

    if(settings.magic==Config::SETTINGS_MAGIC&&settings.version==3)
    {
        LegacySettingsV3 legacy{};EEPROM.get(0,legacy);
        if(turnsAreValid(legacy.turns)&&legacy.runCurrentMa>=Config::MinRunCurrentMa&&legacy.runCurrentMa<=Config::MaxRunCurrentMa&&legacy.holdCurrentMa>=Config::MinHoldCurrentMa&&legacy.holdCurrentMa<=Config::MaxHoldCurrentMa&&legacy.holdCurrentMa<=legacy.runCurrentMa&&legacy.bluetoothEnabled<=1&&legacy.clockwise<=1)
        {
            loadFactoryDefaults();memcpy(settings.turns,legacy.turns,sizeof(settings.turns));settings.runCurrentMa=legacy.runCurrentMa;settings.holdCurrentMa=legacy.holdCurrentMa;settings.bluetoothEnabled=legacy.bluetoothEnabled;settings.clockwise=legacy.clockwise;saveSettings();Serial.println("Settings v3 migrated; weight targets set to product defaults");return;
        }
    }

    if(settings.magic==Config::SETTINGS_MAGIC&&settings.version==7)
    {
        LegacySettingsV7 legacy{};EEPROM.get(0,legacy);
        if(turnsAreValid(legacy.turns)&&legacy.autoTurns>=Config::MinTurns&&legacy.autoTurns<=Config::FuhSafetyTurnCeiling
            &&legacy.runCurrentMa>=Config::MinRunCurrentMa&&legacy.runCurrentMa<=Config::MaxRunCurrentMa
            &&legacy.holdCurrentMa>=Config::MinHoldCurrentMa&&legacy.holdCurrentMa<=Config::MaxHoldCurrentMa&&legacy.holdCurrentMa<=legacy.runCurrentMa
            &&legacy.bluetoothEnabled<=1&&legacy.clockwise<=1&&legacy.hallEnabled<=1&&legacy.loadCellCalibrationValid<=1)
        {
            loadFactoryDefaults();
            memcpy(settings.turns,legacy.turns,sizeof(settings.turns));
            settings.runCurrentMa=legacy.runCurrentMa;settings.holdCurrentMa=legacy.holdCurrentMa;
            settings.bluetoothEnabled=legacy.bluetoothEnabled;settings.clockwise=legacy.clockwise;
            memcpy(settings.weightTargetsCentiGrams,legacy.weightTargetsCentiGrams,sizeof(settings.weightTargetsCentiGrams));
            settings.autoTurns=legacy.autoTurns;settings.hallEnabled=legacy.hallEnabled;
            memcpy(settings.wifiSsid,legacy.wifiSsid,sizeof(settings.wifiSsid));memcpy(settings.wifiPassword,legacy.wifiPassword,sizeof(settings.wifiPassword));
            memcpy(settings.loadCellGramsPerCount,legacy.loadCellGramsPerCount,sizeof(settings.loadCellGramsPerCount));
            settings.loadCellCalibrationValid=legacy.loadCellCalibrationValid;
            saveSettings();Serial.println("Settings v7 migrated; individual commissioning collars set");return;
        }
    }

    if(settings.magic==Config::SETTINGS_MAGIC&&settings.version==8)
    {
        LegacySettingsV8 legacy{};EEPROM.get(0,legacy);
        if(turnsAreValid(legacy.turns)&&legacy.autoTurns>=Config::MinTurns&&legacy.autoTurns<=Config::FuhSafetyTurnCeiling
            &&legacy.runCurrentMa>=Config::MinRunCurrentMa&&legacy.runCurrentMa<=Config::MaxRunCurrentMa
            &&legacy.holdCurrentMa>=Config::MinHoldCurrentMa&&legacy.holdCurrentMa<=Config::MaxHoldCurrentMa&&legacy.holdCurrentMa<=legacy.runCurrentMa
            &&legacy.bluetoothEnabled<=1&&legacy.clockwise<=1&&legacy.hallEnabled<=1&&legacy.loadCellCalibrationValid<=1)
        {
            loadFactoryDefaults();
            memcpy(settings.turns,legacy.turns,sizeof(settings.turns));
            settings.runCurrentMa=legacy.runCurrentMa;settings.holdCurrentMa=legacy.holdCurrentMa;
            settings.bluetoothEnabled=legacy.bluetoothEnabled;settings.clockwise=legacy.clockwise;
            memcpy(settings.weightTargetsCentiGrams,legacy.weightTargetsCentiGrams,sizeof(settings.weightTargetsCentiGrams));
            settings.autoTurns=legacy.autoTurns;settings.hallEnabled=legacy.hallEnabled;
            memcpy(settings.wifiSsid,legacy.wifiSsid,sizeof(settings.wifiSsid));memcpy(settings.wifiPassword,legacy.wifiPassword,sizeof(settings.wifiPassword));
            memcpy(settings.loadCellGramsPerCount,legacy.loadCellGramsPerCount,sizeof(settings.loadCellGramsPerCount));
            memcpy(settings.commissioningCollarCentiGrams,legacy.commissioningCollarCentiGrams,sizeof(settings.commissioningCollarCentiGrams));
            settings.loadCellCalibrationValid=legacy.loadCellCalibrationValid;
            saveSettings();Serial.println("Settings v8 migrated; display sleep defaults applied");return;
        }
    }

    if(settings.magic==Config::SETTINGS_MAGIC&&settings.version==9)
    {
        LegacySettingsV9 legacy{};EEPROM.get(0,legacy);
        if(turnsAreValid(legacy.turns)&&legacy.autoTurns>=Config::MinTurns&&legacy.autoTurns<=Config::FuhSafetyTurnCeiling
            &&legacy.runCurrentMa>=Config::MinRunCurrentMa&&legacy.runCurrentMa<=Config::MaxRunCurrentMa
            &&legacy.holdCurrentMa>=Config::MinHoldCurrentMa&&legacy.holdCurrentMa<=Config::MaxHoldCurrentMa&&legacy.holdCurrentMa<=legacy.runCurrentMa
            &&legacy.bluetoothEnabled<=1&&legacy.clockwise<=1&&legacy.hallEnabled<=1&&legacy.loadCellCalibrationValid<=1
            &&legacy.touchDisplaySleepEnabled<=1&&legacy.touchDisplaySleepMinutes>=Config::MinTouchDisplaySleepMinutes&&legacy.touchDisplaySleepMinutes<=Config::MaxTouchDisplaySleepMinutes)
        {
            loadFactoryDefaults();
            memcpy(settings.turns,legacy.turns,sizeof(settings.turns));
            settings.runCurrentMa=legacy.runCurrentMa;settings.holdCurrentMa=legacy.holdCurrentMa;
            settings.bluetoothEnabled=legacy.bluetoothEnabled;settings.clockwise=legacy.clockwise;
            memcpy(settings.weightTargetsCentiGrams,legacy.weightTargetsCentiGrams,sizeof(settings.weightTargetsCentiGrams));
            settings.autoTurns=legacy.autoTurns;settings.hallEnabled=legacy.hallEnabled;
            memcpy(settings.wifiSsid,legacy.wifiSsid,sizeof(settings.wifiSsid));memcpy(settings.wifiPassword,legacy.wifiPassword,sizeof(settings.wifiPassword));
            memcpy(settings.loadCellGramsPerCount,legacy.loadCellGramsPerCount,sizeof(settings.loadCellGramsPerCount));
            memcpy(settings.commissioningCollarCentiGrams,legacy.commissioningCollarCentiGrams,sizeof(settings.commissioningCollarCentiGrams));
            settings.loadCellCalibrationValid=legacy.loadCellCalibrationValid;
            settings.touchDisplaySleepEnabled=legacy.touchDisplaySleepEnabled;settings.touchDisplaySleepMinutes=legacy.touchDisplaySleepMinutes;
            saveSettings();Serial.println("Settings v9 migrated; display theme defaults applied");return;
        }
    }

    if(settings.magic==Config::SETTINGS_MAGIC&&settings.version==10)
    {
        LegacySettingsV10 legacy{};EEPROM.get(0,legacy);
        if(turnsAreValid(legacy.turns)&&legacy.autoTurns>=Config::MinTurns&&legacy.autoTurns<=Config::FuhSafetyTurnCeiling
            &&legacy.runCurrentMa>=Config::MinRunCurrentMa&&legacy.runCurrentMa<=Config::MaxRunCurrentMa
            &&legacy.holdCurrentMa>=Config::MinHoldCurrentMa&&legacy.holdCurrentMa<=Config::MaxHoldCurrentMa&&legacy.holdCurrentMa<=legacy.runCurrentMa
            &&legacy.bluetoothEnabled<=1&&legacy.clockwise<=1&&legacy.hallEnabled<=1&&legacy.loadCellCalibrationValid<=1
            &&legacy.touchDisplaySleepEnabled<=1&&legacy.touchDisplayDarkMode<=1&&legacy.touchDisplaySleepMinutes>=Config::MinTouchDisplaySleepMinutes&&legacy.touchDisplaySleepMinutes<=Config::MaxTouchDisplaySleepMinutes)
        {
            loadFactoryDefaults();
            memcpy(settings.turns,legacy.turns,sizeof(settings.turns));
            settings.runCurrentMa=legacy.runCurrentMa;settings.holdCurrentMa=legacy.holdCurrentMa;
            settings.bluetoothEnabled=legacy.bluetoothEnabled;settings.clockwise=legacy.clockwise;
            memcpy(settings.weightTargetsCentiGrams,legacy.weightTargetsCentiGrams,sizeof(settings.weightTargetsCentiGrams));
            settings.autoTurns=legacy.autoTurns;settings.hallEnabled=legacy.hallEnabled;
            memcpy(settings.wifiSsid,legacy.wifiSsid,sizeof(settings.wifiSsid));memcpy(settings.wifiPassword,legacy.wifiPassword,sizeof(settings.wifiPassword));
            memcpy(settings.loadCellGramsPerCount,legacy.loadCellGramsPerCount,sizeof(settings.loadCellGramsPerCount));
            memcpy(settings.commissioningCollarCentiGrams,legacy.commissioningCollarCentiGrams,sizeof(settings.commissioningCollarCentiGrams));
            settings.loadCellCalibrationValid=legacy.loadCellCalibrationValid;
            settings.touchDisplaySleepEnabled=legacy.touchDisplaySleepEnabled;settings.touchDisplaySleepMinutes=legacy.touchDisplaySleepMinutes;settings.touchDisplayDarkMode=legacy.touchDisplayDarkMode;
            // Existing commissioning is retained but deliberately marked as
            // having no center-hub correction until recommissioned.
            settings.loadCellHubGain=1.0f;
            saveSettings();Serial.println("Settings v10 migrated; hub gain defaults to 1.0");return;
        }
    }

    // Preserve the unchanged v4-v6 prefix; initialize only newly added fields.
    if(settings.magic==Config::SETTINGS_MAGIC&&(settings.version==4||settings.version==5||settings.version==6))
    {
        if(settings.version==4)settings.autoTurns=Config::DefaultAutoTurns;
        if(settings.version<6)
        {
            settings.hallEnabled=0;
            memset(settings.wifiSsid,0,sizeof(settings.wifiSsid));
            memset(settings.wifiPassword,0,sizeof(settings.wifiPassword));
        }
        memcpy(settings.loadCellGramsPerCount,Config::LoadCellGramsPerCount,sizeof(settings.loadCellGramsPerCount));
        memcpy(settings.commissioningCollarCentiGrams,Config::DefaultCommissioningCollarCentiGrams,sizeof(settings.commissioningCollarCentiGrams));
        settings.loadCellCalibrationValid=0;
        settings.version=Config::SETTINGS_VERSION;
        if(settingsAreValid()){saveSettings();Serial.println("Settings migrated; load-cell commissioning available");return;}
    }

    if (!settingsAreValid())
    {
        Serial.println(
            "No valid settings found; loading defaults"
        );

        loadFactoryDefaults();
        saveSettings();
    }
    else
    {
        Serial.println(
            "Settings loaded from flash"
        );
    }
}
