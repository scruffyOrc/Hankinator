#include "app_api.h"

#include "tmc_driver.h"

#include "telemetry.h"
#include "diagnostics_transport.h"
#include "load_cells.h"
#include "product_mode.h"
#include "firmware_update.h"
#include "run_supervisor.h"
#include "hall_sensor.h"
#include "commissioning.h"
#include "machine_interface.h"
#include "touch_diagnostics_ui.h"

namespace {
bool configurationInputReleaseRequired=false;
bool tareSettling=false;
uint32_t tareSettleStartedMs=0;
uint32_t diagnosticsTareStarted=0;

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
    if(uiState==UiState::TARING||uiState==UiState::TARE_FAILED||uiState==UiState::LOAD_YARN||uiState==UiState::HALL_SEEK||uiState==UiState::HALL_FAILED)return false;

    const bool alreadyInConfiguration=uiState==UiState::CONFIG_MENU||uiState==UiState::CONFIG_CONNECTIVITY_MENU||uiState==UiState::CONFIG_MACHINE_SETTINGS_MENU||uiState==UiState::CONFIG_DIAGNOSTICS_MENU||uiState==UiState::CONFIG_MOTOR_MENU||uiState==UiState::CONFIG_RUN_CURRENT||uiState==UiState::CONFIG_HOLD_CURRENT||uiState==UiState::CONFIG_DIRECTION||uiState==UiState::CONFIG_WEIGHT_MENU||uiState==UiState::CONFIG_WEIGHT_VALUE||uiState==UiState::CONFIG_PAIRING||uiState==UiState::CONFIG_LOAD_CELL_DIAGNOSTICS||uiState==UiState::CONFIG_FIRMWARE_UPDATE||uiState==UiState::CONFIG_WEIGHT_DIAGNOSTICS||uiState==UiState::CONFIG_COMMISSIONING;
    if(alreadyInConfiguration||uiState==UiState::CONFIG_LOAD_CELL_TIMING)
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
    signalStoppedLights();
    if(uiState==UiState::TARING)LoadCells::cancelTare();
    const bool abortedRun=uiState==UiState::WINDING;
    cancelAuxiliaryMotion();
    motionActive = false;

    pauseState =
        PauseState::RUNNING;

    requestedStepRateHz = 0;

    disableMotor();
    HallSensor::cancel();

    // Stop STEP generation and disable the driver before flash/log work.
    if(abortedRun)Telemetry::finish(RunEnd::Abort);

    totalPausedTime  = 0;
    pauseStartedTime = 0;
    pauseTimeRecorded = false;

    uiState=weightCapabilityEnabled&&!autoCountMode?UiState::FUH_PROGRAM_SELECT:UiState::TURN_SELECT;
    if(!weightCapabilityEnabled){autoCountMode=true;selectedTurns=settings.autoTurns;}

    speedTrimPercent = 100;

    currentMotorRPS =
        Config::LAUNCH_MOTOR_RPS;

    launchCompleteElapsedMs=UINT32_MAX;

    lastTurnEncoderTime = 0;

    repeatYes = true;

    beep(100);

#if !defined(HANKWINDER_TOUCH_DIAGNOSTICS)
    drawCurrentScreen();
#endif

    Serial.println(
        "STOP - TMC2209 disabled"
    );
}

void requestWindingStart()
{
    if(settings.hallEnabled)
    {
        motionActive=false;requestedStepRateHz=0;
        uiState=UiState::HALL_SEEK;HallSensor::startSeek();
#if !defined(HANKWINDER_TOUCH_DIAGNOSTICS)
        drawCurrentScreen();
#endif
        return;
    }
    startTareAfterHall();
}

void startTareAfterHall()
{
    if(!weightCapabilityEnabled)
    {
        // Yarn was explicitly loaded before this preparation sequence began.
        // Turninator still indexes the hub when Hall feedback is enabled, but
        // has no load-cell tare to perform before starting.
        beginWinding();
        return;
    }
    motionActive=false;
    requestedStepRateHz=0;
    enableMotor();
    tareSettling=true;
    tareSettleStartedMs=millis();
    uiState=UiState::TARING;
#if !defined(HANKWINDER_TOUCH_DIAGNOSTICS)
    drawTareScreen();
#endif
    Serial.println("TARE_START,context,pre_winding,mode,stationary,motor,energized");
}

void updateTareProcess()
{
    if(uiState!=UiState::TARING)return;
    if(tareSettling)
    {
        // Match the settling interval used for authoritative stopped weighing.
        if(millis()-tareSettleStartedMs<Config::WeightStoppedMechanicalSettleMs)return;
        tareSettling=false;
        LoadCells::startTare();
        Serial.println("TARE_STATIONARY_START,context,pre_winding,motor,energized");
    }
    const LoadCellTareStatus status=LoadCells::updateTare();
#if !defined(HANKWINDER_TOUCH_DIAGNOSTICS)
    static uint32_t lastDraw=0;
    if(millis()-lastDraw>=100){lastDraw=millis();drawTareScreen();}
#endif
    if(status==LoadCellTareStatus::Complete)
    {
        printTareResult("pre_winding");
        Serial.println("TARE_STATIONARY_COMPLETE,winding_starts");
        beginWinding();
    }
    else if(status==LoadCellTareStatus::Failed)
    {
        printTareResult("pre_winding");
        disableMotor();
        uiState=UiState::TARE_FAILED;
        beep(150);
#if !defined(HANKWINDER_TOUCH_DIAGNOSTICS)
        drawTareScreen();
#endif
    }
}

void beginWinding()
{
    // The Wi-Fi update portal and its radio activity are intentionally not
    // allowed to coexist with timing-critical STEP generation. Entering a
    // winding run always returns wireless to its ordinary idle state.
    if(FirmwareUpdate::active())
    {
        FirmwareUpdate::end();
        Telemetry::event("WIFI_UPDATE_STOPPED_FOR_WINDING");
    }
    finalRunStepCount=0;finalRunWeightGrams=NAN;finalRunWeightValid=false;finalRunAborted=false;
    Telemetry::start();
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
    motionSegmentStopStep=UINT32_MAX;
    HallSensor::beginRun();

    RunSupervisor::beginRun();

    requestedStepRateHz =
        stepsPerSecondForMotorRPS(
            Config::LAUNCH_MOTOR_RPS
        );

    enableMotor();

    motionActive = !RunSupervisor::waitingForClearance();
    if(motionActive)requestStepGeneratorWake();
    else {requestedStepRateHz=0;currentMotorRPS=0;}

    uiState =
        UiState::WINDING;

#if !defined(HANKWINDER_TOUCH_DIAGNOSTICS)
    drawWindingScreen();
#endif

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
    updatePanelLights();
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

#if !defined(HANKWINDER_TOUCH_DIAGNOSTICS)
        drawWindingScreen();
#endif
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

    if(auxiliaryMotionActive())
    {
        currentMotorRPS=Config::WeightTensionReliefMotorRps;
        requestedStepRateHz=stepsPerSecondForMotorRPS(currentMotorRPS);
    }
    else
    {
        updateWindingSpeed();
        currentMotorRPS=RunSupervisor::limitMotorRps(currentMotorRPS);
        requestedStepRateHz=currentMotorRPS>0.0f?stepsPerSecondForMotorRPS(currentMotorRPS):0;
    }

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

#if !defined(HANKWINDER_TOUCH_DIAGNOSTICS)
        drawWindingScreen();
#endif
    }

    // A requested pause that happens to reach the
    // actual end of the winding job should finish
    // normally rather than becoming "paused forever."

    if (
        !motionActive&&!RunSupervisor::completionPending()
    )
    {
        if(weightCapabilityEnabled&&autoCountMode&&RunSupervisor::requestFinalMeasurement())return;
        finalRunStepCount=safeCurrentStepCount();
        if(autoCountMode)finalRunStepCount-=min(finalRunStepCount,HallSensor::correctionSteps());
        const RunSupervisorSnapshot finalControl=RunSupervisor::snapshot();
        finalRunWeightValid=finalControl.finalWeightMeasured&&!isnan(finalControl.filteredWeightGrams);
        finalRunWeightGrams=finalRunWeightValid?finalControl.filteredWeightGrams:NAN;
        finalRunAborted=RunSupervisor::completionIsAbort();
        HallSensor::endRun();
        Telemetry::finish(finalRunAborted?RunEnd::Abort:RunEnd::Complete);
        pauseState =
            PauseState::RUNNING;

        requestedStepRateHz = 0;

        uiState =
            UiState::COMPLETE;

        beep(200);
        delay(75);
        beep(200);

#if !defined(HANKWINDER_TOUCH_DIAGNOSTICS)
        drawCompleteScreen();
#endif

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

    // GP6 is the dedicated physical STOP input for both display targets.  It
    // is deliberately independent of the legacy Mini12864 input harness.
    pinMode(
        STOP_BUTTON,
        INPUT_PULLUP
    );

    lastStop =
        digitalRead(
            STOP_BUTTON
        );

    // The touch diagnostics target shares these pins with its touch transport,
    // so it deliberately does not initialize the legacy encoder/button path.
#if !defined(HANKWINDER_TOUCH_DIAGNOSTICS)
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

    lastA =
        digitalRead(
            ENC_A
        );

    lastClick =
        digitalRead(
            ENC_CLICK
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
#endif

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
    LoadCells::setCalibration(settings.loadCellGramsPerCount);
    HallSensor::begin();

    selectedTurns=settings.autoTurns;

    // RGB shares the touch interrupt pin in the diagnostic wiring profile.
#if !defined(HANKWINDER_TOUCH_DIAGNOSTICS)
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
#endif

    // Capture the deliberate two-button setup gesture before load-cell detection.
#if !defined(HANKWINDER_TOUCH_DIAGNOSTICS)
    const bool configurationRequested=configurationButtonsHeld();
    Serial.print("STOP+encoder configuration request: ");Serial.println(configurationRequested?1:0);

    display.clearBuffer();display.setFont(u8g2_font_6x10_tf);display.drawStr(11,31,"Checking hardware");display.sendBuffer();
#else
    const bool configurationRequested=false;
    // Draw once before sensor discovery.  Do not service or animate this
    // display while HX711 detection owns the shared-clock timing.
    TouchDiagnosticsUi::showBootSplash();
#endif
    LoadCells::begin();
    Product::resolve(LoadCells::detectAtStartup());
    if(LoadCells::startupFault()){productMode=ProductMode::LoadCellFault;weightCapabilityEnabled=false;}
    Machine::begin();

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
#if !defined(HANKWINDER_TOUCH_DIAGNOSTICS)
        drawLoadCellFaultScreen();
#endif
    }
    else
    {
#if !defined(HANKWINDER_TOUCH_DIAGNOSTICS)
        runStartupSplash();
#endif
    }

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

    if(productMode==ProductMode::Turninator)autoCountMode=true;
#if defined(HANKWINDER_TOUCH_DIAGNOSTICS)
    // Touch now uses the same controller workflow as Mini12864.  The renderer
    // only observes and commands it through Machine; it does not own motion.
    if(productMode==ProductMode::Fuhgeddabouditinator)uiState=UiState::AUTO_MODE_SELECT;
    else if(productMode==ProductMode::Turninator){autoCountMode=true;uiState=UiState::TURN_SELECT;}
#else
    if(!configurationRequested)
    {
        if(productMode==ProductMode::Fuhgeddabouditinator&&!settings.loadCellCalibrationValid)
        {
            // A connected four-cell machine without a saved, machine-specific
            // fit is not ready for weight winding. Enter the ready screen only;
            // the operator still explicitly starts the held-hub procedure.
            Commissioning::cancel();
            uiState=UiState::CONFIG_COMMISSIONING;
            Serial.println("LOAD_CELL_COMMISSIONING_REQUIRED");
        }
        else if(productMode==ProductMode::Fuhgeddabouditinator)uiState=UiState::AUTO_MODE_SELECT;
        else if(productMode==ProductMode::Turninator){selectedTurns=settings.autoTurns;uiState=UiState::TURN_SELECT;}
    }
    drawCurrentScreen();
#endif

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

#if defined(HANKWINDER_TOUCH_DIAGNOSTICS)
    // Hardware discovery and the remaining controller startup are now
    // complete, so the display may perform its brief ready transition.
    TouchDiagnosticsUi::finishBootSplash();
    TouchDiagnosticsUi::begin();
#endif
}

void startWeightDiagnosticsTare()
{
    enableMotor(); // Hold the hub stationary; never generate steps in this screen.
    diagnosticsTareStarted=millis();LoadCells::startTare();
}
bool weightDiagnosticsSettling()
{
    return uiState==UiState::CONFIG_WEIGHT_DIAGNOSTICS && millis()-diagnosticsTareStarted<Config::LoadCellTareMotorSettleMs;
}

void App::loop()
{
    TmcDriver::poll();
    HallSensor::service();
    if(uiState==UiState::CONFIG_COMMISSIONING)Commissioning::service();
    if(uiState==UiState::HALL_SEEK&&HallSensor::seekFinished())
    {
        if(HallSensor::seekFailed()){uiState=UiState::HALL_FAILED;
#if !defined(HANKWINDER_TOUCH_DIAGNOSTICS)
            drawCurrentScreen();
#endif
        }
        else startTareAfterHall();
    }
    LoadCells::service();
    static uint32_t lastQualityReport=0,reportedTimeouts=0,reportedJumps=0;
    if(millis()-lastQualityReport>=1000) {
        lastQualityReport=millis();
        if(reportedTimeouts!=LoadCells::syncTimeoutCount()||reportedJumps!=LoadCells::suspiciousCount()) {
            reportedTimeouts=LoadCells::syncTimeoutCount();reportedJumps=LoadCells::suspiciousCount();
            char message[120];snprintf(message,sizeof(message),"LC_SYNC_v1_expected_%02X_missing_%02X_timeouts_%lu_jumpmask_%02X_jumps_%lu",
                LoadCells::detectedMask(),LoadCells::missingReadyMask(),(unsigned long)reportedTimeouts,
                LoadCells::suspiciousMask(),(unsigned long)reportedJumps);
            Telemetry::event(message);
        }
    }
    if(uiState==UiState::CONFIG_WEIGHT_DIAGNOSTICS&&!weightDiagnosticsSettling())LoadCells::updateTare();
    updateTareProcess();
#if defined(HANKWINDER_TOUCH_DIAGNOSTICS)
    updateMotor();
#endif
    // Publish a coherent view only after this loop's load-cell/tare work has
    // completed, so every display sees the newest status and measurement.
    Machine::service();
#if defined(HANKWINDER_TOUCH_DIAGNOSTICS)
    // GP6 remains a real, independent emergency-stop input on the touch
    // product.  It must be sampled here rather than inside LVGL so it can
    // both stop winding and complete the final System Check button test.
    updateStopButton();
    TouchDiagnosticsUi::service();
#endif
    FirmwareUpdate::poll();
    DiagnosticsTransport::poll();
    Telemetry::diagnosticPoll(uiState==UiState::CONFIG_LOAD_CELL_DIAGNOSTICS?1:
        uiState==UiState::CONFIG_WEIGHT_DIAGNOSTICS?2:
        uiState==UiState::CONFIG_LOAD_CELL_TIMING?3:0);
 #if !defined(HANKWINDER_TOUCH_DIAGNOSTICS)
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
        if(!DiagnosticsTransport::pairingOpen()){uiState=UiState::CONFIG_CONNECTIVITY_MENU;drawCurrentScreen();}
        else if(millis()-lastPairingDraw>=1000){lastPairingDraw=millis();drawCurrentScreen();}
    }
    if(uiState==UiState::CONFIG_LOAD_CELL_DIAGNOSTICS||uiState==UiState::CONFIG_WEIGHT_DIAGNOSTICS||uiState==UiState::CONFIG_LOAD_CELL_TIMING||uiState==UiState::CONFIG_COMMISSIONING)
    {
        static uint32_t lastLoadCellDiagnosticsDraw=0;
        if(millis()-lastLoadCellDiagnosticsDraw>=Config::LoadCellDiagnosticsRefreshMs)
        {
            lastLoadCellDiagnosticsDraw=millis();
            drawCurrentScreen();
        }
    }
    if(uiState==UiState::CONFIG_FIRMWARE_UPDATE)
    {
        static uint32_t lastFirmwareUpdateDraw=0;
        if(millis()-lastFirmwareUpdateDraw>=Config::FirmwareUpdateDisplayRefreshMs)
        {
            lastFirmwareUpdateDraw=millis();
            drawCurrentScreen();
        }
    }
    updateHeartbeat();

    // Immediate abort always wins.
    updateStopButton();

    updateMotor();

    updateEncoder();

    updateButton();
#endif
}
