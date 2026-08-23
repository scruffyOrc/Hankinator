#include "app_api.h"

#include "tmc_driver.h"

#include "telemetry.h"
#include "diagnostics_transport.h"
#include "load_cells.h"
#include "product_mode.h"
#include "firmware_update.h"
#include "run_supervisor.h"

namespace {
bool configurationInputReleaseRequired=false;
bool tareSettling=false;
bool tareProfiling=false;
uint32_t tareSettleStartedMs=0;

void printTareResult(const char* context)
{
    const LoadCellTareResult result=LoadCells::tareResult();
    Serial.print("TARE_");Serial.print(LoadCells::tareStatusName(result.status));Serial.print(",context,");Serial.print(context);
    Serial.print(",samples,");Serial.print(result.samples);
    for(uint8_t channel=0;channel<Config::LoadCellChannelCount;channel++)
    {
        Serial.print(",lc");Serial.print(channel+1);Serial.print("_offset,");Serial.print(result.offset[channel]);
        Serial.print(",lc");Serial.print(channel+1);Serial.print("_spread,");Serial.print(result.spread[channel]);
    }
    Serial.println();
}

bool performStartupTare()
{
    display.clearBuffer();display.setFont(u8g2_font_6x10_tf);display.drawStr(25,22,"STARTUP TARE");display.drawStr(17,38,"Do not touch");display.drawStr(16,55,"Motor holding");display.sendBuffer();
    enableMotor();
    const uint32_t settleStarted=millis();
    while(millis()-settleStarted<Config::LoadCellTareMotorSettleMs){LoadCells::service();delay(1);}
    LoadCells::startTare();
    uint32_t lastDraw=0;
    while(LoadCells::tareStatus()==LoadCellTareStatus::Collecting)
    {
        LoadCells::service();
        LoadCells::updateTare();
        if(digitalRead(STOP_BUTTON)==LOW){LoadCells::cancelTare();break;}
        if(millis()-lastDraw>=250){lastDraw=millis();drawTareScreen();}
        delay(1);
    }
    printTareResult("startup");
    disableMotor();
    return LoadCells::tareValid();
}
}

bool configurationButtonsHeld()
{
    if(digitalRead(ENC_CLICK)!=LOW||digitalRead(STOP_BUTTON)!=LOW)return false;
    display.clearBuffer();display.setFont(u8g2_font_6x10_tf);display.drawStr(9,25,"Hold for setup");display.drawFrame(9,35,110,10);display.sendBuffer();
    const uint32_t started=millis();
    while(digitalRead(ENC_CLICK)==LOW&&digitalRead(STOP_BUTTON)==LOW)
    {
        const uint32_t elapsed=millis()-started;
        display.setDrawColor(1);display.drawBox(11,37,min<uint32_t>(106,elapsed*106/Config::ConfigHoldMs),6);display.sendBuffer();
        if(elapsed>=Config::ConfigHoldMs)return true;
        delay(20);
    }
    return false;
}

bool updateConfigurationShortcut()
{
    static bool active=false;
    static bool entered=false;
    static uint32_t started=0,lastDraw=0;
    const bool bothHeld=digitalRead(ENC_CLICK)==LOW&&digitalRead(STOP_BUTTON)==LOW;

    // Tare owns STOP as an immediate cancel and must never be replaced by the
    // configuration shortcut while the motor is energized.
    if(uiState==UiState::TARING||uiState==UiState::TARE_FAILED||uiState==UiState::LOAD_YARN)return false;

    const bool alreadyInConfiguration=uiState==UiState::CONFIG_MENU||uiState==UiState::CONFIG_MOTOR_MENU||uiState==UiState::CONFIG_RUN_CURRENT||uiState==UiState::CONFIG_HOLD_CURRENT||uiState==UiState::CONFIG_DIRECTION||uiState==UiState::CONFIG_WEIGHT_MENU||uiState==UiState::CONFIG_WEIGHT_VALUE||uiState==UiState::CONFIG_PAIRING||uiState==UiState::CONFIG_LOAD_CELL_DIAGNOSTICS||uiState==UiState::CONFIG_FIRMWARE_UPDATE;
    if(alreadyInConfiguration)
    {
        if(configurationInputReleaseRequired)
        {
            if(digitalRead(ENC_CLICK)==LOW||digitalRead(STOP_BUTTON)==LOW)return true;
            lastClick=HIGH;
            lastStop=HIGH;
            configurationInputReleaseRequired=false;
            active=false;entered=false;started=0;
        }
        return false;
    }

    if(!bothHeld)
    {
        if(active&&!entered)drawCurrentScreen();
        active=false;entered=false;started=0;
        return false;
    }

    if(!active)
    {
        active=true;started=millis();lastDraw=0;
        if(uiState==UiState::WINDING)resetToYarnWeightSelection();
    }

    const uint32_t elapsed=millis()-started;
    if(!entered&&elapsed>=Config::ConfigHoldMs)
    {
        entered=true;
        configurationInputReleaseRequired=true;
        uiState=UiState::CONFIG_MENU;
        configMenuSelection=0;
        drawCurrentScreen();
    }
    else if(!entered&&(lastDraw==0||millis()-lastDraw>=50))
    {
        lastDraw=millis();
        display.clearBuffer();display.setFont(u8g2_font_6x10_tf);display.drawStr(9,25,"Hold for setup");display.drawFrame(9,35,110,10);
        display.drawBox(11,37,min<uint32_t>(106,elapsed*106/Config::ConfigHoldMs),6);display.sendBuffer();
    }
    return true;
}

void resetToYarnWeightSelection()
{
    if(uiState==UiState::TARING)LoadCells::cancelTare();
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

    uiState=weightCapabilityEnabled?UiState::FUH_PROGRAM_SELECT:UiState::YARN_WEIGHT_SELECT;
    if(!weightCapabilityEnabled)loadSelectedTurnCount();

    speedTrimPercent = 100;

    currentMotorRPS =
        Config::LAUNCH_MOTOR_RPS;

    launchCompleteElapsedMs=UINT32_MAX;

    lastTurnEncoderTime = 0;

    repeatYes = true;

    beep(100);

    drawCurrentScreen();

    Serial.println(
        "STOP - TMC2209 disabled"
    );
}

void requestWindingStart()
{
    if(!weightCapabilityEnabled)
    {
        beginWinding();
        return;
    }
    motionActive=false;
    requestedStepRateHz=0;
    enableMotor();
    tareSettling=true;
    tareProfiling=false;
    tareSettleStartedMs=millis();
    uiState=UiState::TARING;
    drawTareScreen();
    Serial.println("TARE_START,context,pre_winding,mode,one_hub_revolution,motor,energized");
}

void updateTareProcess()
{
    if(uiState!=UiState::TARING)return;
    if(tareSettling)
    {
        if(millis()-tareSettleStartedMs<Config::LoadCellTareMotorSettleMs)return;
        tareSettling=false;
        tareProfiling=true;
        digitalWrite(STEPPER_DIR,(settings.clockwise?WIND_DIRECTION_HIGH:!WIND_DIRECTION_HIGH)?HIGH:LOW);
        currentStepCount=0;targetStepCount=Config::StepsPerHubRev;
        currentMotorRPS=Config::LoadCellProfileMotorRps;
        requestedStepRateHz=stepsPerSecondForMotorRPS(currentMotorRPS);
        LoadCells::startProfileTare(0);
        motionActive=true;
    }

    const LoadCellTareStatus status=tareProfiling?LoadCells::updateProfileTare(safeCurrentStepCount()):LoadCells::updateTare();
    static uint32_t lastDraw=0;
    if(millis()-lastDraw>=100){lastDraw=millis();drawTareScreen();}
    if(status==LoadCellTareStatus::Complete)
    {
        motionActive=false;requestedStepRateHz=0;tareProfiling=false;
        printTareResult("pre_winding");
        uiState=UiState::LOAD_YARN;
        Serial.println("TARE_PROFILE_COMPLETE,load_yarn_then_click");
        drawLoadYarnScreen();
    }
    else if(status==LoadCellTareStatus::Failed)
    {
        motionActive=false;requestedStepRateHz=0;tareProfiling=false;
        printTareResult("pre_winding");
        if(activeFuhProgram==FuhProgram::JustTurn)
        {
            Serial.println("TARE_PROFILE_FAILED,just_turn_continues_without_weight");
            uiState=UiState::LOAD_YARN;drawLoadYarnScreen();return;
        }
        disableMotor();
        uiState=UiState::TARE_FAILED;
        beep(150);
        drawTareScreen();
    }
}

void beginWinding()
{
    finalRunStepCount=0;finalRunWeightGrams=NAN;finalRunWeightValid=false;finalRunAborted=false;
    Telemetry::start();
    RunSupervisor::beginRun();
    uint32_t steps =
        calculateTargetSteps();

    speedTrimPercent = 100;

    currentMotorRPS =
        Config::LAUNCH_MOTOR_RPS;

    launchCompleteElapsedMs=UINT32_MAX;

    windingStartTime =
        millis();

    totalPausedTime  = 0;
    pauseStartedTime = 0;
    pauseTimeRecorded = false;

    pauseState =
        PauseState::RUNNING;

    digitalWrite(
        STEPPER_DIR,
        (settings.clockwise?WIND_DIRECTION_HIGH:!WIND_DIRECTION_HIGH)
            ? HIGH
            : LOW
    );

    currentStepCount = 0;
    targetStepCount  = steps;

    requestedStepRateHz =
        stepsPerSecondForMotorRPS(
            Config::LAUNCH_MOTOR_RPS
        );

    enableMotor();

    motionActive = true;
    requestStepGeneratorWake();

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

    RunSupervisor::service();

    updateWindingSpeed();

    currentMotorRPS=RunSupervisor::limitMotorRps(currentMotorRPS);
    requestedStepRateHz=currentMotorRPS>0.0f?stepsPerSecondForMotorRPS(currentMotorRPS):0;

    const int wakeResult=serviceStepGeneratorWake();
    if(wakeResult)
    {
        Telemetry::event(wakeResult>0?(wakeResult==1?"STEP_RESTART_ARMED":"STEP_RESTART_ARMED_WITHOUT_CANCEL"):"STEP_RESTART_FAILED");
    }
    uint32_t firstStepDelayUs=0,firstStep=0;
    if(consumeFirstStepAfterWake(firstStepDelayUs,firstStep))
    {
        char event[64]{};snprintf(event,sizeof(event),"STEP_RESTART_FIRST_STEP_delay_us_%lu_step_%lu",(unsigned long)firstStepDelayUs,(unsigned long)firstStep);
        Telemetry::event(event);
    }

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
        !motionActive&&!RunSupervisor::completionPending()
    )
    {
        if(weightCapabilityEnabled&&activeFuhProgram==FuhProgram::JustTurn&&RunSupervisor::requestFinalMeasurement())return;
        finalRunStepCount=safeCurrentStepCount();
        const RunSupervisorSnapshot finalControl=RunSupervisor::snapshot();
        finalRunWeightValid=finalControl.finalWeightMeasured&&!isnan(finalControl.filteredWeightGrams);
        finalRunWeightGrams=finalRunWeightValid?finalControl.filteredWeightGrams:NAN;
        finalRunAborted=RunSupervisor::completionIsAbort();
        Telemetry::finish(finalRunAborted?RunEnd::Abort:RunEnd::Complete);
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

    // Panel lighting is required in both normal and configuration startup.
    // The splash temporarily replaces these colors, then restores them.
    setPanelLights();

    // LCD
    display.begin();

    display.setContrast(
        255
    );

    // Capture the deliberate two-button setup gesture before load-cell detection.
    const bool configurationRequested=configurationButtonsHeld();
    Serial.print("STOP+encoder configuration request: ");Serial.println(configurationRequested?1:0);

    display.clearBuffer();display.setFont(u8g2_font_6x10_tf);display.drawStr(11,31,"Checking hardware");display.sendBuffer();
    LoadCells::begin();
    Product::resolve(LoadCells::detectAtStartup());

    Serial.print("Load cells: required=");Serial.print(Config::RequiredLoadCells);
    Serial.print(" detected=");Serial.print(detectedLoadCellCount);
    Serial.print(" product_mode=");Serial.println(Product::name());
    const LoadCellSnapshot startupLoadCells=LoadCells::snapshot();
    for(uint8_t channel=0;channel<Config::LoadCellChannelCount;channel++)
    {
        Serial.print("HX711 channel ");Serial.print(channel+1);Serial.print(": raw=");Serial.print(startupLoadCells.raw[channel]);
        Serial.print(" healthy=");Serial.println((startupLoadCells.healthyMask&(1u<<channel))?1:0);
    }

    if(configurationRequested)
    {
        configurationInputReleaseRequired=true;
        uiState=UiState::CONFIG_MENU;
        configMenuSelection=0;
    }
    else if(productMode==ProductMode::LoadCellFault)
    {
        uiState=UiState::LOAD_CELL_FAULT;
        drawLoadCellFaultScreen();
    }
    else
    {
        runStartupSplash();
    }

    // UART diagnostics are nonessential. Initialize them only after the
    // user-visible display is alive so a driver fault cannot hide startup.
    TmcDriver::begin();

    // This initial diagnostic zero is replaced by the mandatory tare taken
    // immediately before each winding run.
    if(!configurationRequested&&weightCapabilityEnabled)performStartupTare();

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

    if(productMode==ProductMode::Fuhgeddabouditinator)uiState=UiState::FUH_PROGRAM_SELECT;
    drawCurrentScreen();

    // Debug
    Serial.println();

    Serial.print(Product::name());Serial.println(productMode==ProductMode::LoadCellFault?" blocked by load-cell fault":" ready");

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
    LoadCells::service();
    updateTareProcess();
    FirmwareUpdate::poll();
    DiagnosticsTransport::poll();
    if(updateConfigurationShortcut())
    {
        updateHeartbeat();
        return;
    }
    if(productMode==ProductMode::LoadCellFault&&uiState==UiState::LOAD_CELL_FAULT)
    {
        updateHeartbeat();
        return;
    }
    if(uiState==UiState::CONFIG_PAIRING)
    {
        static uint32_t lastPairingDraw=0;
        if(!DiagnosticsTransport::pairingOpen()){uiState=UiState::CONFIG_MENU;drawCurrentScreen();}
        else if(millis()-lastPairingDraw>=1000){lastPairingDraw=millis();drawCurrentScreen();}
    }
    if(uiState==UiState::CONFIG_LOAD_CELL_DIAGNOSTICS)
    {
        static uint32_t lastLoadCellDiagnosticsDraw=0;
        if(millis()-lastLoadCellDiagnosticsDraw>=Config::LoadCellDiagnosticsRefreshMs)
        {
            lastLoadCellDiagnosticsDraw=millis();
            drawLoadCellDiagnosticsScreen();
        }
    }
    if(uiState==UiState::CONFIG_FIRMWARE_UPDATE)
    {
        static uint32_t lastFirmwareUpdateDraw=0;
        if(millis()-lastFirmwareUpdateDraw>=Config::FirmwareUpdateDisplayRefreshMs)
        {
            lastFirmwareUpdateDraw=millis();
            drawFirmwareUpdateScreen();
        }
    }
    updateHeartbeat();

    // Immediate abort always wins.
    updateStopButton();

    updateMotor();

    updateEncoder();

    updateButton();
}
