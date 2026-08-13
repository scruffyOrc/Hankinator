#include <Arduino.h>
#include <U8g2lib.h>
#include <Adafruit_NeoPixel.h>
#include <EEPROM.h>

#include "pico/time.h"
#include "pico/stdlib.h"

// ==================================================
// PIN CONFIGURATION
// ==================================================

// BTT Mini12864 V1.0
constexpr uint8_t LCD_CS    = 17;
constexpr uint8_t LCD_DC    = 16;
constexpr uint8_t LCD_RESET = 20;

constexpr uint8_t LCD_CLOCK = 18;
constexpr uint8_t LCD_DATA  = 19;

constexpr uint8_t ENC_CLICK = 13;
constexpr uint8_t ENC_A     = 14;
constexpr uint8_t ENC_B     = 15;

constexpr uint8_t BEEPER = 12;
constexpr uint8_t NEOPIXEL_PIN = 11;

// Mini12864 RESET button = STOP / ABORT
constexpr uint8_t STOP_BUTTON = 6;

// BTT TMC2209 V1.2
constexpr uint8_t STEPPER_STEP   = 2;
constexpr uint8_t STEPPER_DIR    = 3;
constexpr uint8_t STEPPER_ENABLE = 4;

constexpr bool WIND_DIRECTION_HIGH = true;

constexpr uint8_t HEARTBEAT_LED = LED_BUILTIN;

// ==================================================
// DISPLAY
// ==================================================

U8G2_UC1701_MINI12864_F_4W_SW_SPI display(
    U8G2_R0,
    LCD_CLOCK,
    LCD_DATA,
    LCD_CS,
    LCD_DC,
    LCD_RESET
);

// ==================================================
// RGB
// ==================================================

constexpr uint8_t NEOPIXEL_COUNT = 3;

Adafruit_NeoPixel pixels(
    NEOPIXEL_COUNT,
    NEOPIXEL_PIN,
    NEO_RGB + NEO_KHZ800
);

// ==================================================
// CONFIGURATION
// ==================================================

namespace Config
{
    // Mechanical
    constexpr uint16_t MOTOR_STEPS_PER_REV = 200;
    constexpr uint8_t MICROSTEPS = 8;

    constexpr uint16_t MOTOR_PULLEY_TEETH = 20;
    constexpr uint16_t HUB_PULLEY_TEETH   = 80;

    constexpr uint32_t STEPS_PER_MOTOR_REV =
        MOTOR_STEPS_PER_REV * MICROSTEPS;

    constexpr uint32_t STEPS_PER_HUB_REV =
        STEPS_PER_MOTOR_REV *
        HUB_PULLEY_TEETH /
        MOTOR_PULLEY_TEETH;

    // Normal winding profile
    constexpr float START_MOTOR_RPS  = 1.0f;
    constexpr float CRUISE_MOTOR_RPS = 4.0f;
    constexpr float END_MOTOR_RPS    = 0.5f;

    constexpr uint32_t START_HOLD_MS = 5000;
    constexpr uint32_t ACCEL_RAMP_MS = 5000;

    constexpr float DECEL_TURNS = 10.0f;

    // ------------------------------------------------
    // PAUSE / RESUME PROFILE
    // ------------------------------------------------

    // When Pause is requested:
    // current speed -> 0.25 motor RPS over 1 hub turn,
    // then stop completely.
    constexpr float PAUSE_END_MOTOR_RPS = 0.25f;
    constexpr float PAUSE_RAMP_TURNS = 1.0f;

    // When Resume is requested:
    // start at 0.5 motor RPS and ramp back to the
    // normal requested speed over 2 hub turns.
    constexpr float RESUME_START_MOTOR_RPS = 0.5f;
    constexpr float RESUME_RAMP_TURNS = 2.0f;

    // Speed trim
    constexpr int SPEED_TRIM_MIN_PERCENT  = 50;
    constexpr int SPEED_TRIM_MAX_PERCENT  = 150;
    constexpr int SPEED_TRIM_STEP_PERCENT = 10;

    // Turns
    constexpr int MIN_TURNS = 1;
    constexpr int MAX_TURNS = 9999;

    // STEP timing
    constexpr uint32_t STEP_PULSE_US = 2;
    constexpr uint32_t IDLE_TIMER_US = 1000;

    // Splash
    constexpr uint32_t SPLASH_TIME_MS = 4000;
    constexpr uint32_t RAINBOW_CYCLE_MS = 1200;

    // EEPROM
    constexpr size_t EEPROM_SIZE = 256;

    constexpr uint32_t SETTINGS_MAGIC =
        0x48414E4B; // HANK

    constexpr uint16_t SETTINGS_VERSION = 1;
}

// ==================================================
// YARN WEIGHTS
// ==================================================

struct YarnWeight
{
    const char* label;
};

constexpr YarnWeight YARN_WEIGHTS[] = {
    { "Lace/Suri" },
    { "Fingering" },
    { "Sport"     },
    { "DK"        },
    { "Worsted"   },
    { "Chunky"    },
    { "Bulky"     },
    { "Jumbo"     }
};

constexpr int YARN_WEIGHT_COUNT =
    sizeof(YARN_WEIGHTS) /
    sizeof(YARN_WEIGHTS[0]);

// ==================================================
// SKEIN SIZES
// ==================================================

struct SkeinSize
{
    const char* label;
    uint16_t factoryDefaultTurns;
};

constexpr SkeinSize SKEIN_SIZES[] = {
    { "Mini", 63  },
    { "Half", 90  },
    { "Full", 110 }
};

constexpr int SKEIN_SIZE_COUNT =
    sizeof(SKEIN_SIZES) /
    sizeof(SKEIN_SIZES[0]);

// ==================================================
// PERSISTENT SETTINGS
// ==================================================

struct PersistentSettings
{
    uint32_t magic;
    uint16_t version;

    uint16_t turns[
        YARN_WEIGHT_COUNT
    ][
        SKEIN_SIZE_COUNT
    ];
};

PersistentSettings settings;

// ==================================================
// UI STATE
// ==================================================

enum class UiState
{
    YARN_WEIGHT_SELECT,
    SKEIN_SELECT,
    TURN_SELECT,
    READY,
    WINDING,
    COMPLETE,
    REPEAT_PROMPT
};

UiState uiState =
    UiState::YARN_WEIGHT_SELECT;

// ==================================================
// PAUSE STATE
// ==================================================

enum class PauseState : uint8_t
{
    RUNNING,
    RAMPING_DOWN,
    PAUSED,
    RAMPING_UP
};

volatile PauseState pauseState =
    PauseState::RUNNING;

// Pause-down tracking
volatile uint32_t pauseRampStartStep = 0;
volatile uint32_t pauseRampStopStep  = 0;

float pauseRampStartRPS =
    Config::START_MOTOR_RPS;

// Resume tracking
volatile uint32_t resumeRampStartStep = 0;
volatile uint32_t resumeRampEndStep   = 0;

// Time actually spent stationary
uint32_t pauseStartedTime = 0;
uint32_t totalPausedTime  = 0;

bool pauseTimeRecorded = false;

// ==================================================
// CURRENT SELECTIONS
// ==================================================

int selectedYarnWeight = 0;
int selectedSkeinSize  = 0;

int selectedTurns = 63;

bool repeatYes = true;

int speedTrimPercent = 100;

float currentMotorRPS =
    Config::START_MOTOR_RPS;

uint32_t windingStartTime = 0;

// ==================================================
// MOTOR STATE
// ==================================================

volatile bool motionActive = false;

volatile uint32_t currentStepCount = 0;
volatile uint32_t targetStepCount  = 0;

volatile uint32_t requestedStepRateHz = 0;

alarm_id_t stepAlarmId = 0;

// ==================================================
// INPUT STATE
// ==================================================

int lastA = HIGH;

bool lastClick = HIGH;
bool lastStop  = HIGH;

uint32_t lastTurnEncoderTime = 0;

// ==================================================
// HEARTBEAT
// ==================================================

bool heartbeatState = false;
uint32_t lastHeartbeat = 0;

// ==================================================
// PERSISTENT SETTINGS
// ==================================================

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

// ==================================================
// PANEL LIGHTING
// ==================================================

void setPanelLights()
{
    pixels.clear();

    pixels.setPixelColor(
        0,
        pixels.Color(
            180,
            180,
            180
        )
    );

    pixels.setPixelColor(
        1,
        pixels.Color(
            40,
            80,
            180
        )
    );

    pixels.setPixelColor(
        2,
        pixels.Color(
            40,
            80,
            180
        )
    );

    pixels.show();
}

// ==================================================
// STARTUP SPLASH
// ==================================================

void drawStartupSplash()
{
    display.clearBuffer();

    display.setFont(
        u8g2_font_ncenB24_tr
    );

    const char* text =
        "Behold!";

    int width =
        display.getStrWidth(
            text
        );

    display.drawStr(
        (128 - width) / 2,
        43,
        text
    );

    display.sendBuffer();
}

void runStartupSplash()
{
    drawStartupSplash();

    uint32_t startTime =
        millis();

    while (
        millis() - startTime <
        Config::SPLASH_TIME_MS
    )
    {
        uint32_t elapsed =
            millis() -
            startTime;

        uint16_t baseHue =
            (uint16_t)(
                (
                    elapsed *
                    65536UL
                ) /
                Config::RAINBOW_CYCLE_MS
            );

        for (
            uint8_t i = 0;
            i < NEOPIXEL_COUNT;
            i++
        )
        {
            uint16_t hue =
                baseHue +
                (uint16_t)(
                    i *
                    (
                        65536UL /
                        NEOPIXEL_COUNT
                    )
                );

            uint32_t color =
                pixels.gamma32(
                    pixels.ColorHSV(
                        hue,
                        255,
                        180
                    )
                );

            pixels.setPixelColor(
                i,
                color
            );
        }

        pixels.show();

        delay(20);
    }

    setPanelLights();
}

// ==================================================
// BASIC HELPERS
// ==================================================

void beep(
    uint16_t duration = 30
)
{
    digitalWrite(
        BEEPER,
        HIGH
    );

    delay(duration);

    digitalWrite(
        BEEPER,
        LOW
    );
}

uint32_t calculateTargetSteps()
{
    return
        (uint32_t)selectedTurns *
        Config::STEPS_PER_HUB_REV;
}

uint32_t stepsPerSecondForMotorRPS(
    float motorRPS
)
{
    float result =
        motorRPS *
        Config::STEPS_PER_MOTOR_REV;

    if (result < 1.0f)
    {
        result = 1.0f;
    }

    return
        (uint32_t)lround(
            result
        );
}

void enableMotor()
{
    digitalWrite(
        STEPPER_ENABLE,
        LOW
    );
}

void disableMotor()
{
    digitalWrite(
        STEPPER_ENABLE,
        HIGH
    );
}

// ==================================================
// STEP TIMER
// ==================================================

int64_t stepAlarmCallback(
    alarm_id_t id,
    void* userData
)
{
    if (!motionActive)
    {
        return
            -(int64_t)
            Config::IDLE_TIMER_US;
    }

    // Completely stationary pause.
    if (
        pauseState ==
        PauseState::PAUSED
    )
    {
        return
            -(int64_t)
            Config::IDLE_TIMER_US;
    }

    if (
        currentStepCount >=
        targetStepCount
    )
    {
        motionActive = false;

        return
            -(int64_t)
            Config::IDLE_TIMER_US;
    }

    // Generate STEP pulse
    gpio_put(
        STEPPER_STEP,
        1
    );

    busy_wait_us_32(
        Config::STEP_PULSE_US
    );

    gpio_put(
        STEPPER_STEP,
        0
    );

    currentStepCount++;

    // ------------------------------------------------
    // Pause ramp reached its exact stop position
    // ------------------------------------------------

    if (
        pauseState ==
            PauseState::RAMPING_DOWN &&
        currentStepCount >=
            pauseRampStopStep
    )
    {
        pauseState =
            PauseState::PAUSED;

        requestedStepRateHz = 0;

        return
            -(int64_t)
            Config::IDLE_TIMER_US;
    }

    // ------------------------------------------------
    // Resume ramp finished
    // ------------------------------------------------

    if (
        pauseState ==
            PauseState::RAMPING_UP &&
        currentStepCount >=
            resumeRampEndStep
    )
    {
        pauseState =
            PauseState::RUNNING;
    }

    // Normal job completion
    if (
        currentStepCount >=
        targetStepCount
    )
    {
        motionActive = false;

        return
            -(int64_t)
            Config::IDLE_TIMER_US;
    }

    uint32_t rate =
        requestedStepRateHz;

    if (rate < 1)
    {
        rate = 1;
    }

    uint32_t intervalUs =
        1000000UL /
        rate;

    if (intervalUs < 50)
    {
        intervalUs = 50;
    }

    return
        -(int64_t)intervalUs;
}

// ==================================================
// TURN ENCODER ACCELERATION
// ==================================================

int getTurnIncrement()
{
    uint32_t now =
        millis();

    if (
        lastTurnEncoderTime == 0
    )
    {
        lastTurnEncoderTime =
            now;

        return 1;
    }

    uint32_t elapsed =
        now -
        lastTurnEncoderTime;

    lastTurnEncoderTime =
        now;

    if (elapsed <= 45)
        return 25;

    if (elapsed <= 100)
        return 10;

    if (elapsed <= 220)
        return 5;

    return 1;
}

// ==================================================
// WINDING PROGRESS
// ==================================================

uint32_t safeCurrentStepCount()
{
    return currentStepCount;
}

float turnsCompleted()
{
    return
        safeCurrentStepCount() /
        (float)
        Config::STEPS_PER_HUB_REV;
}

float turnsRemaining()
{
    float remaining =
        selectedTurns -
        turnsCompleted();

    if (remaining < 0.0f)
    {
        remaining = 0.0f;
    }

    return remaining;
}

// ==================================================
// ACTIVE WINDING TIME
// ==================================================

uint32_t activeWindingElapsedMs()
{
    uint32_t pausedTime =
        totalPausedTime;

    if (
        pauseState ==
        PauseState::PAUSED &&
        pauseTimeRecorded
    )
    {
        pausedTime +=
            millis() -
            pauseStartedTime;
    }

    return
        millis() -
        windingStartTime -
        pausedTime;
}

// ==================================================
// NORMAL AUTOMATIC PROFILE
// ==================================================

float startupProfileMotorRPS()
{
    uint32_t elapsed =
        activeWindingElapsedMs();

    if (
        elapsed <=
        Config::START_HOLD_MS
    )
    {
        return
            Config::START_MOTOR_RPS;
    }

    uint32_t rampElapsed =
        elapsed -
        Config::START_HOLD_MS;

    if (
        rampElapsed <
        Config::ACCEL_RAMP_MS
    )
    {
        float progress =
            rampElapsed /
            (float)
            Config::ACCEL_RAMP_MS;

        return
            Config::START_MOTOR_RPS +
            (
                Config::CRUISE_MOTOR_RPS -
                Config::START_MOTOR_RPS
            ) *
            progress;
    }

    return
        Config::CRUISE_MOTOR_RPS;
}

float endingProfileMotorRPS()
{
    float remaining =
        turnsRemaining();

    if (
        remaining >=
        Config::DECEL_TURNS
    )
    {
        return
            Config::CRUISE_MOTOR_RPS;
    }

    float progress =
        1.0f -
        (
            remaining /
            Config::DECEL_TURNS
        );

    if (progress < 0.0f)
    {
        progress = 0.0f;
    }

    if (progress > 1.0f)
    {
        progress = 1.0f;
    }

    return
        Config::CRUISE_MOTOR_RPS -
        (
            Config::CRUISE_MOTOR_RPS -
            Config::END_MOTOR_RPS
        ) *
        progress;
}

float normalRequestedMotorRPS()
{
    float automaticRPS =
        min(
            startupProfileMotorRPS(),
            endingProfileMotorRPS()
        );

    float trimMultiplier =
        speedTrimPercent /
        100.0f;

    float result =
        automaticRPS *
        trimMultiplier;

    if (result < 0.25f)
    {
        result = 0.25f;
    }

    return result;
}

// ==================================================
// PAUSE / RESUME SPEED PROFILE
// ==================================================

void updateWindingSpeed()
{
    // ------------------------------------------------
    // Fully paused
    // ------------------------------------------------

    if (
        pauseState ==
        PauseState::PAUSED
    )
    {
        requestedStepRateHz = 0;
        currentMotorRPS = 0.0f;

        return;
    }

    // ------------------------------------------------
    // Ramping down to pause
    // ------------------------------------------------

    if (
        pauseState ==
        PauseState::RAMPING_DOWN
    )
    {
        uint32_t current =
            safeCurrentStepCount();

        uint32_t totalRampSteps =
            pauseRampStopStep -
            pauseRampStartStep;

        uint32_t completedRampSteps = 0;

        if (
            current >
            pauseRampStartStep
        )
        {
            completedRampSteps =
                current -
                pauseRampStartStep;
        }

        float progress = 1.0f;

        if (totalRampSteps > 0)
        {
            progress =
                completedRampSteps /
                (float)totalRampSteps;
        }

        if (progress < 0.0f)
            progress = 0.0f;

        if (progress > 1.0f)
            progress = 1.0f;

        currentMotorRPS =
            pauseRampStartRPS -
            (
                pauseRampStartRPS -
                Config::PAUSE_END_MOTOR_RPS
            ) *
            progress;

        if (
            currentMotorRPS <
            Config::PAUSE_END_MOTOR_RPS
        )
        {
            currentMotorRPS =
                Config::PAUSE_END_MOTOR_RPS;
        }

        requestedStepRateHz =
            stepsPerSecondForMotorRPS(
                currentMotorRPS
            );

        return;
    }

    // ------------------------------------------------
    // Ramping back up after pause
    // ------------------------------------------------

    if (
        pauseState ==
        PauseState::RAMPING_UP
    )
    {
        uint32_t current =
            safeCurrentStepCount();

        uint32_t totalRampSteps =
            resumeRampEndStep -
            resumeRampStartStep;

        uint32_t completedRampSteps = 0;

        if (
            current >
            resumeRampStartStep
        )
        {
            completedRampSteps =
                current -
                resumeRampStartStep;
        }

        float progress = 1.0f;

        if (totalRampSteps > 0)
        {
            progress =
                completedRampSteps /
                (float)totalRampSteps;
        }

        if (progress < 0.0f)
            progress = 0.0f;

        if (progress > 1.0f)
            progress = 1.0f;

        float normalRPS =
            normalRequestedMotorRPS();

        // Ramp from 0.5 RPS toward whatever the
        // normal profile currently wants.
        float rampRPS =
            Config::RESUME_START_MOTOR_RPS +
            (
                normalRPS -
                Config::RESUME_START_MOTOR_RPS
            ) *
            progress;

        // If the normal finishing profile happens
        // to be below our resume-start speed, honor
        // the lower normal speed instead.
        if (
            normalRPS <
            Config::RESUME_START_MOTOR_RPS
        )
        {
            rampRPS =
                normalRPS;
        }

        currentMotorRPS =
            rampRPS;

        requestedStepRateHz =
            stepsPerSecondForMotorRPS(
                currentMotorRPS
            );

        return;
    }

    // ------------------------------------------------
    // Normal running
    // ------------------------------------------------

    currentMotorRPS =
        normalRequestedMotorRPS();

    requestedStepRateHz =
        stepsPerSecondForMotorRPS(
            currentMotorRPS
        );
}

// ==================================================
// PAUSE / RESUME COMMANDS
// ==================================================

void requestPause()
{
    if (
        uiState !=
            UiState::WINDING ||
        !motionActive ||
        pauseState !=
            PauseState::RUNNING
    )
    {
        return;
    }

    pauseRampStartStep =
        safeCurrentStepCount();

    uint32_t requestedStopStep =
        pauseRampStartStep +
        (uint32_t)(
            Config::PAUSE_RAMP_TURNS *
            Config::STEPS_PER_HUB_REV
        );

    // Never pause beyond the actual end of the job.
    if (
        requestedStopStep >
        targetStepCount
    )
    {
        requestedStopStep =
            targetStepCount;
    }

    pauseRampStopStep =
        requestedStopStep;

    pauseRampStartRPS =
        currentMotorRPS;

    pauseState =
        PauseState::RAMPING_DOWN;

    pauseTimeRecorded = false;

    beep(40);

    Serial.println(
        "Pause requested - ramping down"
    );
}

void requestResume()
{
    if (
        uiState !=
            UiState::WINDING ||
        !motionActive ||
        pauseState !=
            PauseState::PAUSED
    )
    {
        return;
    }

    // Account only for the time actually spent
    // stationary, not the ramp-down time.
    if (pauseTimeRecorded)
    {
        totalPausedTime +=
            millis() -
            pauseStartedTime;
    }

    pauseTimeRecorded = false;

    resumeRampStartStep =
        safeCurrentStepCount();

    uint32_t requestedEndStep =
        resumeRampStartStep +
        (uint32_t)(
            Config::RESUME_RAMP_TURNS *
            Config::STEPS_PER_HUB_REV
        );

    if (
        requestedEndStep >
        targetStepCount
    )
    {
        requestedEndStep =
            targetStepCount;
    }

    resumeRampEndStep =
        requestedEndStep;

    pauseState =
        PauseState::RAMPING_UP;

    currentMotorRPS =
        Config::RESUME_START_MOTOR_RPS;

    requestedStepRateHz =
        stepsPerSecondForMotorRPS(
            currentMotorRPS
        );

    beep(30);

    Serial.println(
        "Resuming - ramping up"
    );
}

// ==================================================
// DISPLAY: YARN WEIGHT
// ==================================================

void drawYarnWeightScreen()
{
    display.clearBuffer();

    display.setFont(
        u8g2_font_6x12_tf
    );

    display.drawStr(
        19,
        11,
        "THE HANKINATOR"
    );

    display.drawHLine(
        0,
        15,
        128
    );

    display.drawStr(
        32,
        29,
        "Yarn Weight"
    );

    const char* label =
        YARN_WEIGHTS[
            selectedYarnWeight
        ].label;

    display.setFont(
        u8g2_font_ncenB14_tr
    );

    int width =
        display.getStrWidth(
            label
        );

    display.drawStr(
        (128 - width) / 2,
        53,
        label
    );

    display.setFont(
        u8g2_font_5x8_tf
    );

    display.drawStr(
        2,
        63,
        "Turn"
    );

    display.drawStr(
        96,
        63,
        "Click"
    );

    display.sendBuffer();
}

// ==================================================
// DISPLAY: SKEIN SIZE
// ==================================================

void drawSkeinScreen()
{
    display.clearBuffer();

    display.setFont(
        u8g2_font_6x12_tf
    );

    display.drawStr(
        19,
        11,
        "THE HANKINATOR"
    );

    display.drawHLine(
        0,
        15,
        128
    );

    display.drawStr(
        35,
        29,
        "Skein Size"
    );

    display.setFont(
        u8g2_font_ncenB18_tr
    );

    const char* label =
        SKEIN_SIZES[
            selectedSkeinSize
        ].label;

    int width =
        display.getStrWidth(
            label
        );

    display.drawStr(
        (128 - width) / 2,
        53,
        label
    );

    display.setFont(
        u8g2_font_5x8_tf
    );

    display.drawStr(
        2,
        63,
        "Turn"
    );

    display.drawStr(
        96,
        63,
        "Click"
    );

    display.sendBuffer();
}

// ==================================================
// DISPLAY: TURN COUNT
// ==================================================

void drawTurnScreen()
{
    display.clearBuffer();

    display.setFont(
        u8g2_font_6x12_tf
    );

    display.drawStr(
        19,
        11,
        "THE HANKINATOR"
    );

    display.drawHLine(
        0,
        15,
        128
    );

    char context[32];

    snprintf(
        context,
        sizeof(context),
        "%s / %s",
        YARN_WEIGHTS[
            selectedYarnWeight
        ].label,
        SKEIN_SIZES[
            selectedSkeinSize
        ].label
    );

    display.setFont(
        u8g2_font_5x8_tf
    );

    int contextWidth =
        display.getStrWidth(
            context
        );

    display.drawStr(
        (128 - contextWidth) / 2,
        25,
        context
    );

    display.setFont(
        u8g2_font_6x12_tf
    );

    display.drawStr(
        25,
        36,
        "Number of Turns"
    );

    char buffer[16];

    snprintf(
        buffer,
        sizeof(buffer),
        "%d",
        selectedTurns
    );

    display.setFont(
        u8g2_font_ncenB18_tr
    );

    int width =
        display.getStrWidth(
            buffer
        );

    display.drawStr(
        (128 - width) / 2,
        58,
        buffer
    );

    display.sendBuffer();
}

// ==================================================
// DISPLAY: READY
// ==================================================

void drawReadyScreen()
{
    display.clearBuffer();

    display.setFont(
        u8g2_font_6x12_tf
    );

    display.drawStr(
        45,
        11,
        "READY"
    );

    display.drawHLine(
        0,
        15,
        128
    );

    char line[32];

    snprintf(
        line,
        sizeof(line),
        "%s / %s",
        YARN_WEIGHTS[
            selectedYarnWeight
        ].label,
        SKEIN_SIZES[
            selectedSkeinSize
        ].label
    );

    display.setFont(
        u8g2_font_5x8_tf
    );

    int width =
        display.getStrWidth(
            line
        );

    display.drawStr(
        (128 - width) / 2,
        29,
        line
    );

    snprintf(
        line,
        sizeof(line),
        "Turns: %d",
        selectedTurns
    );

    display.setFont(
        u8g2_font_6x12_tf
    );

    width =
        display.getStrWidth(
            line
        );

    display.drawStr(
        (128 - width) / 2,
        44,
        line
    );

    display.setFont(
        u8g2_font_5x8_tf
    );

    display.drawStr(
        28,
        62,
        "Click to start"
    );

    display.sendBuffer();
}

// ==================================================
// DISPLAY: WINDING / PAUSE
// ==================================================

void drawWindingScreen()
{
    display.clearBuffer();

    uint32_t target =
        targetStepCount;

    uint32_t current =
        safeCurrentStepCount();

    int percent = 0;

    if (target > 0)
    {
        percent =
            (int)(
                (
                    (uint64_t)current *
                    100ULL
                ) /
                target
            );
    }

    if (percent > 100)
    {
        percent = 100;
    }

    int completedTurns =
        (int)(
            current /
            Config::STEPS_PER_HUB_REV
        );

    char buffer[32];

    // ------------------------------------------------
    // PAUSING
    // ------------------------------------------------

    if (
        pauseState ==
        PauseState::RAMPING_DOWN
    )
    {
        display.setFont(
            u8g2_font_ncenB14_tr
        );

        const char* text =
            "PAUSING...";

        int width =
            display.getStrWidth(
                text
            );

        display.drawStr(
            (128 - width) / 2,
            24,
            text
        );

        display.setFont(
            u8g2_font_6x12_tf
        );

        snprintf(
            buffer,
            sizeof(buffer),
            "%d / %d turns",
            completedTurns,
            selectedTurns
        );

        width =
            display.getStrWidth(
                buffer
            );

        display.drawStr(
            (128 - width) / 2,
            42,
            buffer
        );

        display.setFont(
            u8g2_font_5x8_tf
        );

        display.drawStr(
            22,
            61,
            "Slowing to stop"
        );

        display.sendBuffer();
        return;
    }

    // ------------------------------------------------
    // PAUSED
    // ------------------------------------------------

    if (
        pauseState ==
        PauseState::PAUSED
    )
    {
        display.setFont(
            u8g2_font_ncenB18_tr
        );

        const char* text =
            "PAUSED";

        int width =
            display.getStrWidth(
                text
            );

        display.drawStr(
            (128 - width) / 2,
            25,
            text
        );

        display.setFont(
            u8g2_font_6x12_tf
        );

        snprintf(
            buffer,
            sizeof(buffer),
            "%d / %d turns",
            completedTurns,
            selectedTurns
        );

        width =
            display.getStrWidth(
                buffer
            );

        display.drawStr(
            (128 - width) / 2,
            42,
            buffer
        );

        display.setFont(
            u8g2_font_5x8_tf
        );

        display.drawStr(
            22,
            61,
            "Click to resume"
        );

        display.sendBuffer();
        return;
    }

    // ------------------------------------------------
    // RESUMING
    // ------------------------------------------------

    if (
        pauseState ==
        PauseState::RAMPING_UP
    )
    {
        display.setFont(
            u8g2_font_ncenB14_tr
        );

        const char* text =
            "RESUMING...";

        int width =
            display.getStrWidth(
                text
            );

        display.drawStr(
            (128 - width) / 2,
            24,
            text
        );

        display.setFont(
            u8g2_font_6x12_tf
        );

        snprintf(
            buffer,
            sizeof(buffer),
            "%d / %d turns",
            completedTurns,
            selectedTurns
        );

        width =
            display.getStrWidth(
                buffer
            );

        display.drawStr(
            (128 - width) / 2,
            42,
            buffer
        );

        display.setFont(
            u8g2_font_5x8_tf
        );

        display.drawStr(
            21,
            61,
            "Ramping to speed"
        );

        display.sendBuffer();
        return;
    }

    // ------------------------------------------------
    // NORMAL WINDING
    // ------------------------------------------------

    display.setFont(
        u8g2_font_6x12_tf
    );

    display.drawStr(
        39,
        11,
        "WINDING"
    );

    display.drawHLine(
        0,
        15,
        128
    );

    snprintf(
        buffer,
        sizeof(buffer),
        "%d / %d",
        completedTurns,
        selectedTurns
    );

    display.setFont(
        u8g2_font_ncenB14_tr
    );

    int width =
        display.getStrWidth(
            buffer
        );

    display.drawStr(
        (128 - width) / 2,
        34,
        buffer
    );

    snprintf(
        buffer,
        sizeof(buffer),
        "%.1f r/s  %d%%",
        currentMotorRPS,
        speedTrimPercent
    );

    display.setFont(
        u8g2_font_5x8_tf
    );

    width =
        display.getStrWidth(
            buffer
        );

    display.drawStr(
        (128 - width) / 2,
        45,
        buffer
    );

    display.drawFrame(
        8,
        49,
        112,
        8
    );

    int barWidth =
        (108 * percent) /
        100;

    display.drawBox(
        10,
        51,
        barWidth,
        4
    );

    display.setFont(
        u8g2_font_4x6_tf
    );

    display.drawStr(
        2,
        64,
        "Click=Pause  RESET=Abort"
    );

    display.sendBuffer();
}

// ==================================================
// DISPLAY: COMPLETE
// ==================================================

void drawCompleteScreen()
{
    display.clearBuffer();

    display.setFont(
        u8g2_font_6x12_tf
    );

    display.drawStr(
        36,
        12,
        "COMPLETE"
    );

    display.drawHLine(
        0,
        16,
        128
    );

    display.drawStr(
        31,
        34,
        "Remove yarn"
    );

    display.drawStr(
        10,
        56,
        "Click once finished"
    );

    display.sendBuffer();
}

// ==================================================
// DISPLAY: REPEAT
// ==================================================

void drawRepeatPromptScreen()
{
    display.clearBuffer();

    display.setFont(
        u8g2_font_6x12_tf
    );

    display.drawStr(
        11,
        13,
        "Repeat with same"
    );

    display.drawStr(
        36,
        26,
        "settings?"
    );

    display.drawHLine(
        0,
        31,
        128
    );

    display.setFont(
        u8g2_font_ncenB14_tr
    );

    constexpr int YES_X = 18;
    constexpr int NO_X  = 82;
    constexpr int TEXT_Y = 53;

    if (repeatYes)
    {
        int width =
            display.getStrWidth(
                "YES"
            );

        display.drawBox(
            YES_X - 3,
            37,
            width + 6,
            19
        );

        display.setDrawColor(0);

        display.drawStr(
            YES_X,
            TEXT_Y,
            "YES"
        );

        display.setDrawColor(1);
    }
    else
    {
        display.drawStr(
            YES_X,
            TEXT_Y,
            "YES"
        );
    }

    display.drawStr(
        61,
        TEXT_Y,
        "/"
    );

    if (!repeatYes)
    {
        int width =
            display.getStrWidth(
                "NO"
            );

        display.drawBox(
            NO_X - 3,
            37,
            width + 6,
            19
        );

        display.setDrawColor(0);

        display.drawStr(
            NO_X,
            TEXT_Y,
            "NO"
        );

        display.setDrawColor(1);
    }
    else
    {
        display.drawStr(
            NO_X,
            TEXT_Y,
            "NO"
        );
    }

    display.setFont(
        u8g2_font_5x8_tf
    );

    display.drawStr(
        22,
        63,
        "Turn / Click"
    );

    display.sendBuffer();
}

// ==================================================
// DRAW CURRENT SCREEN
// ==================================================

void drawCurrentScreen()
{
    switch (uiState)
    {
        case UiState::YARN_WEIGHT_SELECT:
            drawYarnWeightScreen();
            break;

        case UiState::SKEIN_SELECT:
            drawSkeinScreen();
            break;

        case UiState::TURN_SELECT:
            drawTurnScreen();
            break;

        case UiState::READY:
            drawReadyScreen();
            break;

        case UiState::WINDING:
            drawWindingScreen();
            break;

        case UiState::COMPLETE:
            drawCompleteScreen();
            break;

        case UiState::REPEAT_PROMPT:
            drawRepeatPromptScreen();
            break;
    }
}

// ==================================================
// STOP / ABORT
// ==================================================

void resetToYarnWeightSelection()
{
    motionActive = false;

    pauseState =
        PauseState::RUNNING;

    requestedStepRateHz = 0;

    disableMotor();

    totalPausedTime  = 0;
    pauseStartedTime = 0;
    pauseTimeRecorded = false;

    uiState =
        UiState::YARN_WEIGHT_SELECT;

    loadSelectedTurnCount();

    speedTrimPercent = 100;

    currentMotorRPS =
        Config::START_MOTOR_RPS;

    lastTurnEncoderTime = 0;

    repeatYes = true;

    beep(100);

    drawCurrentScreen();

    Serial.println(
        "STOP - TMC2209 disabled"
    );
}

// ==================================================
// BEGIN WINDING
// ==================================================

void beginWinding()
{
    uint32_t steps =
        calculateTargetSteps();

    speedTrimPercent = 100;

    currentMotorRPS =
        Config::START_MOTOR_RPS;

    windingStartTime =
        millis();

    totalPausedTime  = 0;
    pauseStartedTime = 0;
    pauseTimeRecorded = false;

    pauseState =
        PauseState::RUNNING;

    digitalWrite(
        STEPPER_DIR,
        WIND_DIRECTION_HIGH
            ? HIGH
            : LOW
    );

    currentStepCount = 0;
    targetStepCount  = steps;

    requestedStepRateHz =
        stepsPerSecondForMotorRPS(
            Config::START_MOTOR_RPS
        );

    enableMotor();

    motionActive = true;

    uiState =
        UiState::WINDING;

    drawWindingScreen();

    Serial.println();

    Serial.print(
        "Yarn weight: "
    );

    Serial.println(
        YARN_WEIGHTS[
            selectedYarnWeight
        ].label
    );

    Serial.print(
        "Skein size: "
    );

    Serial.println(
        SKEIN_SIZES[
            selectedSkeinSize
        ].label
    );

    Serial.print(
        "Turns: "
    );

    Serial.println(
        selectedTurns
    );
}

// ==================================================
// HEARTBEAT
// ==================================================

void updateHeartbeat()
{
    if (
        millis() -
        lastHeartbeat >=
        500
    )
    {
        lastHeartbeat =
            millis();

        heartbeatState =
            !heartbeatState;

        digitalWrite(
            HEARTBEAT_LED,
            heartbeatState
        );
    }
}

// ==================================================
// PAUSE STATE HOUSEKEEPING
// ==================================================

void updatePauseState()
{
    // The timer interrupt changes RAMPING_DOWN
    // to PAUSED at the exact target step.
    //
    // Record the start of stationary time here
    // in normal application context.

    if (
        pauseState ==
            PauseState::PAUSED &&
        !pauseTimeRecorded
    )
    {
        pauseStartedTime =
            millis();

        pauseTimeRecorded =
            true;

        currentMotorRPS =
            0.0f;

        Serial.println(
            "Winding paused"
        );

        drawWindingScreen();
    }
}

// ==================================================
// ENCODER
// ==================================================

void updateEncoder()
{
    int currentA =
        digitalRead(
            ENC_A
        );

    if (
        currentA != lastA &&
        currentA == LOW
    )
    {
        bool clockwise =
            digitalRead(ENC_B) ==
            currentA;

        // ------------------------------------------
        // WINDING SPEED TRIM
        // ------------------------------------------

        if (
            uiState ==
            UiState::WINDING
        )
        {
            // Don't alter speed trim while pause
            // or resume behavior is underway.
            if (
                pauseState ==
                PauseState::RUNNING
            )
            {
                if (clockwise)
                {
                    speedTrimPercent +=
                        Config::
                        SPEED_TRIM_STEP_PERCENT;
                }
                else
                {
                    speedTrimPercent -=
                        Config::
                        SPEED_TRIM_STEP_PERCENT;
                }

                if (
                    speedTrimPercent >
                    Config::
                    SPEED_TRIM_MAX_PERCENT
                )
                {
                    speedTrimPercent =
                        Config::
                        SPEED_TRIM_MAX_PERCENT;
                }

                if (
                    speedTrimPercent <
                    Config::
                    SPEED_TRIM_MIN_PERCENT
                )
                {
                    speedTrimPercent =
                        Config::
                        SPEED_TRIM_MIN_PERCENT;
                }

                updateWindingSpeed();

                drawWindingScreen();
            }
        }

        // ------------------------------------------
        // YARN WEIGHT
        // ------------------------------------------

        else if (
            uiState ==
            UiState::YARN_WEIGHT_SELECT
        )
        {
            if (clockwise)
            {
                selectedYarnWeight++;

                if (
                    selectedYarnWeight >=
                    YARN_WEIGHT_COUNT
                )
                {
                    selectedYarnWeight = 0;
                }
            }
            else
            {
                selectedYarnWeight--;

                if (
                    selectedYarnWeight < 0
                )
                {
                    selectedYarnWeight =
                        YARN_WEIGHT_COUNT - 1;
                }
            }

            loadSelectedTurnCount();

            drawCurrentScreen();
        }

        // ------------------------------------------
        // SKEIN SIZE
        // ------------------------------------------

        else if (
            uiState ==
            UiState::SKEIN_SELECT
        )
        {
            if (clockwise)
            {
                selectedSkeinSize++;

                if (
                    selectedSkeinSize >=
                    SKEIN_SIZE_COUNT
                )
                {
                    selectedSkeinSize = 0;
                }
            }
            else
            {
                selectedSkeinSize--;

                if (
                    selectedSkeinSize < 0
                )
                {
                    selectedSkeinSize =
                        SKEIN_SIZE_COUNT - 1;
                }
            }

            loadSelectedTurnCount();

            drawCurrentScreen();
        }

        // ------------------------------------------
        // TURN COUNT
        // ------------------------------------------

        else if (
            uiState ==
            UiState::TURN_SELECT
        )
        {
            int increment =
                getTurnIncrement();

            if (clockwise)
            {
                selectedTurns +=
                    increment;
            }
            else
            {
                selectedTurns -=
                    increment;
            }

            if (
                selectedTurns <
                Config::MIN_TURNS
            )
            {
                selectedTurns =
                    Config::MIN_TURNS;
            }

            if (
                selectedTurns >
                Config::MAX_TURNS
            )
            {
                selectedTurns =
                    Config::MAX_TURNS;
            }

            drawCurrentScreen();
        }

        // ------------------------------------------
        // REPEAT
        // ------------------------------------------

        else if (
            uiState ==
            UiState::REPEAT_PROMPT
        )
        {
            repeatYes =
                !repeatYes;

            drawCurrentScreen();
        }
    }

    lastA =
        currentA;
}

// ==================================================
// ENCODER CLICK
// ==================================================

void updateButton()
{
    bool click =
        digitalRead(
            ENC_CLICK
        );

    if (
        lastClick == HIGH &&
        click == LOW
    )
    {
        // ------------------------------------------
        // WINDING PAUSE / RESUME
        // ------------------------------------------

        if (
            uiState ==
            UiState::WINDING
        )
        {
            if (
                pauseState ==
                PauseState::RUNNING
            )
            {
                requestPause();
            }
            else if (
                pauseState ==
                PauseState::PAUSED
            )
            {
                requestResume();
            }

            // During RAMPING_DOWN or RAMPING_UP,
            // further clicks are intentionally ignored.

            drawWindingScreen();

            delay(120);

            lastClick =
                click;

            return;
        }

        beep();

        switch (uiState)
        {
            case UiState::YARN_WEIGHT_SELECT:

                loadSelectedTurnCount();

                uiState =
                    UiState::SKEIN_SELECT;

                break;

            case UiState::SKEIN_SELECT:

                loadSelectedTurnCount();

                lastTurnEncoderTime = 0;

                uiState =
                    UiState::TURN_SELECT;

                break;

            case UiState::TURN_SELECT:

                saveSelectedTurnCount();

                uiState =
                    UiState::READY;

                break;

            case UiState::READY:

                beginWinding();

                break;

            case UiState::WINDING:

                break;

            case UiState::COMPLETE:

                disableMotor();

                repeatYes = true;

                uiState =
                    UiState::REPEAT_PROMPT;

                break;

            case UiState::REPEAT_PROMPT:

                if (repeatYes)
                {
                    speedTrimPercent = 100;

                    uiState =
                        UiState::READY;
                }
                else
                {
                    loadSelectedTurnCount();

                    speedTrimPercent = 100;

                    lastTurnEncoderTime = 0;

                    uiState =
                        UiState::YARN_WEIGHT_SELECT;
                }

                break;
        }

        drawCurrentScreen();

        delay(120);
    }

    lastClick =
        click;
}

// ==================================================
// STOP BUTTON
// ==================================================

void updateStopButton()
{
    bool stop =
        digitalRead(
            STOP_BUTTON
        );

    if (
        lastStop == HIGH &&
        stop == LOW
    )
    {
        // RESET remains a genuine immediate abort.
        // Unlike Pause, it intentionally does NOT ramp.
        resetToYarnWeightSelection();

        delay(120);
    }

    lastStop =
        stop;
}

// ==================================================
// MOTOR UPDATE
// ==================================================

void updateMotor()
{
    if (
        uiState !=
        UiState::WINDING
    )
    {
        return;
    }

    updatePauseState();

    updateWindingSpeed();

    static uint32_t lastDisplayUpdate = 0;

    if (
        millis() -
        lastDisplayUpdate >=
        200
    )
    {
        lastDisplayUpdate =
            millis();

        drawWindingScreen();
    }

    // A requested pause that happens to reach the
    // actual end of the winding job should finish
    // normally rather than becoming "paused forever."

    if (
        !motionActive
    )
    {
        pauseState =
            PauseState::RUNNING;

        requestedStepRateHz = 0;

        uiState =
            UiState::COMPLETE;

        beep(200);
        delay(75);
        beep(200);

        drawCompleteScreen();

        Serial.println(
            "Winding complete"
        );
    }
}

// ==================================================
// SETUP
// ==================================================

void setup()
{
    pinMode(
        HEARTBEAT_LED,
        OUTPUT
    );

    Serial.begin(
        115200
    );

    // Encoder
    pinMode(
        ENC_A,
        INPUT_PULLUP
    );

    pinMode(
        ENC_B,
        INPUT_PULLUP
    );

    pinMode(
        ENC_CLICK,
        INPUT_PULLUP
    );

    // STOP
    pinMode(
        STOP_BUTTON,
        INPUT_PULLUP
    );

    lastA =
        digitalRead(
            ENC_A
        );

    lastClick =
        digitalRead(
            ENC_CLICK
        );

    lastStop =
        digitalRead(
            STOP_BUTTON
        );

    // Beeper
    pinMode(
        BEEPER,
        OUTPUT
    );

    digitalWrite(
        BEEPER,
        LOW
    );

    // TMC2209
    pinMode(
        STEPPER_STEP,
        OUTPUT
    );

    pinMode(
        STEPPER_DIR,
        OUTPUT
    );

    pinMode(
        STEPPER_ENABLE,
        OUTPUT
    );

    digitalWrite(
        STEPPER_STEP,
        LOW
    );

    digitalWrite(
        STEPPER_DIR,
        WIND_DIRECTION_HIGH
            ? HIGH
            : LOW
    );

    disableMotor();

    // Persistent settings
    loadSettings();

    loadSelectedTurnCount();

    // RGB
    pixels.begin();

    pixels.setBrightness(
        100
    );

    // LCD
    display.begin();

    display.setContrast(
        255
    );

    // Dramatic entrance
    runStartupSplash();

    // Motion timer
    stepAlarmId =
        add_alarm_in_us(
            Config::IDLE_TIMER_US,
            stepAlarmCallback,
            nullptr,
            true
        );

    drawCurrentScreen();

    // Debug
    Serial.println();

    Serial.println(
        "The Hankinator ready"
    );

    Serial.println(
        "Pause profile:"
    );
 
    Serial.println(
        "Ramp down = 1 hub turn"
    );

    Serial.println(
        "Ramp up = 2 hub turns"
    );

    Serial.println(
        "24 independent yarn calibrations"
    );
}

// ==================================================
// LOOP
// ==================================================

void loop()
{
    updateHeartbeat();

    // Immediate abort always wins.
    updateStopButton();

    updateMotor();

    updateEncoder();

    updateButton();
}