#include "app_api.h"

void loadFactoryDefaults()
{
    settings.magic =
        Config::SETTINGS_MAGIC;

    settings.version =
        Config::SETTINGS_VERSION;

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
            uint16_t turns =
                settings.turns[weight][size];

            if (
                turns < Config::MIN_TURNS ||
                turns > Config::MAX_TURNS
            )
            {
                return false;
            }
        }
    }

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
        "Turn settings saved to flash"
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
            "Turn settings loaded from flash"
        );
    }
}

uint16_t getSavedTurns()
{
    return settings.turns[
        selectedYarnWeight
    ][
        selectedSkeinSize
    ];
}

void loadSelectedTurnCount()
{
    selectedTurns =
        getSavedTurns();
}

void saveSelectedTurnCount()
{
    uint16_t currentSaved =
        settings.turns[
            selectedYarnWeight
        ][
            selectedSkeinSize
        ];

    if (
        currentSaved ==
        selectedTurns
    )
    {
        return;
    }

    settings.turns[
        selectedYarnWeight
    ][
        selectedSkeinSize
    ] =
        selectedTurns;

    saveSettings();

    Serial.print(
        "Updated "
    );

    Serial.print(
        YARN_WEIGHTS[
            selectedYarnWeight
        ].label
    );

    Serial.print(
        " / "
    );

    Serial.print(
        SKEIN_SIZES[
            selectedSkeinSize
        ].label
    );

    Serial.print(
        " = "
    );

    Serial.print(
        selectedTurns
    );

    Serial.println(
        " turns"
    );
}
