#include "app_api.h"

#include "tmc_driver.h"

#include "telemetry.h"
#include "diagnostics_transport.h"

void resetToYarnWeightSelection()
{
    if (uiState == UiState::WINDING)
        Telemetry::finish(RunEnd::Abort);
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

void beginWinding()
{
    Telemetry::start();
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

    Telemetry::sample(currentMotorRPS, safeCurrentStepCount(), pauseState);

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
        Telemetry::finish(RunEnd::Complete);
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

void App::setup()
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

    // UART diagnostics are nonessential. Initialize them only after the
    // user-visible display is alive so a driver fault cannot hide startup.
    TmcDriver::begin();

    // Let the CYW43 Bluetooth stack initialize its background timing before
    // the continuously rescheduled motion alarm is created.
    DiagnosticsTransport::begin();

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

    Telemetry::begin();
}

void App::loop()
{
    DiagnosticsTransport::poll();
    updateHeartbeat();

    // Immediate abort always wins.
    updateStopButton();

    updateMotor();

    updateEncoder();

    updateButton();
}
