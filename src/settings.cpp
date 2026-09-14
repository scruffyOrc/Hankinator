#include "app_api.h"

namespace {
struct LegacySettingsV1{uint32_t magic;uint16_t version;uint16_t turns[YARN_WEIGHT_COUNT][SKEIN_SIZE_COUNT];};
struct LegacySettingsV2{uint32_t magic;uint16_t version;uint16_t turns[YARN_WEIGHT_COUNT][SKEIN_SIZE_COUNT];uint16_t runCurrentMa;uint16_t holdCurrentMa;uint8_t bluetoothEnabled;uint8_t reserved;};
struct LegacySettingsV3{uint32_t magic;uint16_t version;uint16_t turns[YARN_WEIGHT_COUNT][SKEIN_SIZE_COUNT];uint16_t runCurrentMa;uint16_t holdCurrentMa;uint8_t bluetoothEnabled;uint8_t clockwise;};

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
    memcpy(settings.weightTargetsCentiGrams,Config::DefaultWeightTargetsCentiGrams,sizeof(settings.weightTargetsCentiGrams));

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
    return settings.bluetoothEnabled<=1&&settings.clockwise<=1;
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

    // v5 appends autoTurns; the complete v4 prefix retains its original layout.
    if(settings.magic==Config::SETTINGS_MAGIC&&settings.version==4)
    {
        settings.version=Config::SETTINGS_VERSION;
        settings.autoTurns=Config::DefaultAutoTurns;
        if(settingsAreValid()){saveSettings();Serial.println("Settings v4 migrated; Auto count defaults to 62");return;}
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
