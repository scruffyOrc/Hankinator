#include "app_api.h"
#include "diagnostics_transport.h"
#include "tmc_driver.h"
#include "firmware_update.h"
#include "load_cells.h"
#include "run_supervisor.h"
#include "telemetry.h"
#include "hall_sensor.h"
#include "machine_interface.h"
#if defined(HANKWINDER_TOUCH_DIAGNOSTICS)
#include "touch_diagnostics_ui.h"
#endif

namespace {
int connectivityTopIndex(){return weightCapabilityEnabled?1:0;}
int machineSettingsTopIndex(){return weightCapabilityEnabled?2:1;}
int diagnosticsTopIndex(){return weightCapabilityEnabled?3:2;}
void returnToConfigurationMenu(int selection)
{
    configMenuSelection=selection;uiState=UiState::CONFIG_MENU;drawCurrentScreen();
}

void finishConfiguration()
{
    if(FirmwareUpdate::active())FirmwareUpdate::end();
    saveSettings();
    DiagnosticsTransport::closePairingWindow();
    if(productMode==ProductMode::LoadCellFault)
    {
        uiState=UiState::LOAD_CELL_FAULT;
        drawLoadCellFaultScreen();
        return;
    }
    runStartupSplash();
    uiState=weightCapabilityEnabled?UiState::AUTO_MODE_SELECT:UiState::TURN_SELECT;
    if(!weightCapabilityEnabled){autoCountMode=true;selectedTurns=settings.autoTurns;}
}

}

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

        if(uiState==UiState::CONFIG_MENU)
        {
            configMenuSelection+=clockwise?1:-1;
            const int maximum=weightCapabilityEnabled?4:3;
            if(configMenuSelection>maximum)configMenuSelection=0;
            if(configMenuSelection<0)configMenuSelection=maximum;
            drawCurrentScreen();
        }
        else if(uiState==UiState::CONFIG_CONNECTIVITY_MENU||uiState==UiState::CONFIG_MACHINE_SETTINGS_MENU||uiState==UiState::CONFIG_DIAGNOSTICS_MENU)
        {
            const int maximum=uiState==UiState::CONFIG_CONNECTIVITY_MENU?2:
                (uiState==UiState::CONFIG_MACHINE_SETTINGS_MENU?(weightCapabilityEnabled?2:1):(weightCapabilityEnabled?2:1));
            configSubMenuSelection+=clockwise?1:-1;
            if(configSubMenuSelection>maximum)configSubMenuSelection=0;
            if(configSubMenuSelection<0)configSubMenuSelection=maximum;
            drawCurrentScreen();
        }
        else if(uiState==UiState::CONFIG_LOAD_CELL_TIMING)
        {
            LoadCells::selectTimingDelay(clockwise);drawCurrentScreen();
        }
        else if(uiState==UiState::CONFIG_COMMISSIONING)
        {
            // Reference and collar masses are fixed, measured fixtures.  Do
            // not let ordinary menu rotation silently alter calibration data.
            drawCurrentScreen();
        }
        else if(uiState==UiState::CONFIG_MOTOR_MENU)
        {
            motorMenuSelection+=clockwise?1:-1;
            if(motorMenuSelection>2)motorMenuSelection=0;
            if(motorMenuSelection<0)motorMenuSelection=2;
            drawCurrentScreen();
        }
        else if(uiState==UiState::CONFIG_RUN_CURRENT)
        {
            int value=int(settings.runCurrentMa)+(clockwise?Config::CurrentStepMa:-Config::CurrentStepMa);
            Machine::setRunCurrentMa(uint16_t(constrain(value,int(Config::MinRunCurrentMa),int(Config::MaxRunCurrentMa))));drawCurrentScreen();
        }
        else if(uiState==UiState::CONFIG_HOLD_CURRENT)
        {
            int value=int(settings.holdCurrentMa)+(clockwise?Config::CurrentStepMa:-Config::CurrentStepMa);
            Machine::setHoldCurrentMa(uint16_t(constrain(value,int(Config::MinHoldCurrentMa),min(int(Config::MaxHoldCurrentMa),int(settings.runCurrentMa)))));drawCurrentScreen();
        }
        else if(uiState==UiState::CONFIG_DIRECTION)
        {
            Machine::setClockwise(clockwise);
            drawCurrentScreen();
        }
        else if(uiState==UiState::CONFIG_WEIGHT_MENU)
        {
            weightTargetMenuSelection+=clockwise?1:-1;
            if(weightTargetMenuSelection>2)weightTargetMenuSelection=0;
            if(weightTargetMenuSelection<0)weightTargetMenuSelection=2;
            drawCurrentScreen();
        }
        else if(uiState==UiState::CONFIG_WEIGHT_VALUE)
        {
            const float current=Machine::weightTargets().grams[weightTargetMenuSelection];
            Machine::setWeightTargetGrams(weightTargetMenuSelection,current+(clockwise?0.1f:-0.1f));
            drawCurrentScreen();
        }
        else if (
            uiState ==
            UiState::WINDING
        )
        {
            // Don't alter speed trim while pause
            // or resume behavior is underway.
            if (
                pauseState ==
                PauseState::RUNNING&&!weightCapabilityEnabled&&!autoCountMode
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

            if(autoCountMode)selectedTurns=min(selectedTurns,int(Config::FuhSafetyTurnCeiling));
            drawCurrentScreen();
        }

        else if(uiState==UiState::AUTO_MODE_SELECT)
        {
            autoCountMode=!autoCountMode;drawCurrentScreen();
        }
        else if(uiState==UiState::FUH_PROGRAM_SELECT)
        {
            int selection=int(selectedFuhProgram)+(clockwise?1:-1);
            if(selection>2)selection=0;if(selection<0)selection=2;
            selectedFuhProgram=FuhProgram(selection);drawCurrentScreen();
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
        if(uiState==UiState::WINDING&&RunSupervisor::waitingForClearance()) {
            Machine::confirmSafetyClearance();
            drawWindingScreen();lastClick=click;return;
        }
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
                Machine::togglePauseResume();
            }
            else if (
                pauseState ==
                PauseState::PAUSED
            )
            {
                Machine::togglePauseResume();
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
            case UiState::CONFIG_MENU:
                if(weightCapabilityEnabled&&configMenuSelection==0){weightTargetMenuSelection=0;uiState=UiState::CONFIG_WEIGHT_MENU;}
                else if(configMenuSelection==connectivityTopIndex()){configSubMenuSelection=0;uiState=UiState::CONFIG_CONNECTIVITY_MENU;}
                else if(configMenuSelection==machineSettingsTopIndex()){configSubMenuSelection=0;uiState=UiState::CONFIG_MACHINE_SETTINGS_MENU;}
                else if(configMenuSelection==diagnosticsTopIndex()){configSubMenuSelection=0;uiState=UiState::CONFIG_DIAGNOSTICS_MENU;}
                else finishConfiguration();
                break;

            case UiState::CONFIG_CONNECTIVITY_MENU:
                if(configSubMenuSelection==0){Machine::toggleBluetoothEnabled();}
                else if(configSubMenuSelection==1){if(Machine::openPairingWindow())uiState=UiState::CONFIG_PAIRING;else beep(120);}
                else {disableMotor();DiagnosticsTransport::closePairingWindow();DiagnosticsTransport::setEnabled(false);FirmwareUpdate::begin();uiState=UiState::CONFIG_FIRMWARE_UPDATE;}
                break;

            case UiState::CONFIG_MACHINE_SETTINGS_MENU:
                if(configSubMenuSelection==0){motorMenuSelection=0;uiState=UiState::CONFIG_MOTOR_MENU;}
                else if(configSubMenuSelection==1){Machine::toggleHallFeedback();}
                else if(weightCapabilityEnabled){Machine::openCommissioning();}
                break;

            case UiState::CONFIG_DIAGNOSTICS_MENU:
                if(weightCapabilityEnabled&&configSubMenuSelection==0)
                {
                    if(Machine::startSystemCheck())uiState=UiState::CONFIG_LOAD_CELL_DIAGNOSTICS;
                    else beep(120);
                }
                else if(configSubMenuSelection==(weightCapabilityEnabled?1:0)){Machine::startWeightDiagnosticsTare();}
                else {disableMotor();LoadCells::beginTimingTest();uiState=UiState::CONFIG_LOAD_CELL_TIMING;}
                break;

            case UiState::CONFIG_MOTOR_MENU:
                if(motorMenuSelection==0)uiState=UiState::CONFIG_RUN_CURRENT;
                else if(motorMenuSelection==1)uiState=UiState::CONFIG_HOLD_CURRENT;
                else uiState=UiState::CONFIG_DIRECTION;
                break;

            case UiState::CONFIG_RUN_CURRENT:
            case UiState::CONFIG_HOLD_CURRENT:
            case UiState::CONFIG_DIRECTION:
                saveSettings();
                uiState=UiState::CONFIG_MOTOR_MENU;
                break;

            case UiState::CONFIG_WEIGHT_MENU:
                uiState=UiState::CONFIG_WEIGHT_VALUE;
                break;

            case UiState::CONFIG_WEIGHT_VALUE:
                Machine::saveWeightTargets();uiState=UiState::CONFIG_WEIGHT_MENU;
                break;

            case UiState::CONFIG_PAIRING:
                Machine::closePairingWindow();
                uiState=UiState::CONFIG_CONNECTIVITY_MENU;
                break;

            case UiState::CONFIG_LOAD_CELL_DIAGNOSTICS:
                break;
            case UiState::CONFIG_WEIGHT_DIAGNOSTICS:
                Machine::startWeightDiagnosticsTare();
                break;
            case UiState::CONFIG_LOAD_CELL_TIMING:
                LoadCells::applyTimingDelay();
                break;
            case UiState::CONFIG_COMMISSIONING:
                if(Machine::commissioning().state==Machine::CommissioningState::Complete||Machine::commissioning().state==Machine::CommissioningState::Failed)
                {
                    Machine::cancelCommissioning();
                }
                else Machine::confirmCommissioning();
                break;

            case UiState::CONFIG_FIRMWARE_UPDATE:
                // Wi-Fi credentials are intentionally provisioned over the
                // Bluetooth terminal (WIFI SETUP), not through the encoder.
                break;

            case UiState::HALL_FAILED:
                Machine::retryWindingPreparation();
                break;

            case UiState::HALL_SEEK:
                break;

            case UiState::AUTO_MODE_SELECT:
                if(autoCountMode){selectedTurns=settings.autoTurns;lastTurnEncoderTime=0;uiState=UiState::TURN_SELECT;}
                else uiState=UiState::FUH_PROGRAM_SELECT;
                break;

            case UiState::TURN_SELECT:
                autoCountMode=true;
                Machine::prepareCountWinding(uint16_t(selectedTurns));
                break;

            case UiState::FUH_PROGRAM_SELECT:
                Machine::prepareWeightWinding(uint8_t(selectedFuhProgram));
                break;

            case UiState::TARE_FAILED:
                Machine::retryWindingPreparation();
                break;

            case UiState::TARING:

                break;

            case UiState::LOAD_YARN:
                Machine::confirmYarnLoaded();
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
                if(repeatYes){if(autoCountMode)Machine::prepareCountWinding(uint16_t(settings.autoTurns));else Machine::prepareWeightWinding(uint8_t(selectedFuhProgram));}
                else uiState=autoCountMode?UiState::TURN_SELECT:UiState::FUH_PROGRAM_SELECT;
                break;
        }

        drawCurrentScreen();

        delay(120);
    }

    lastClick =
        click;
}

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
        // One concise edge marker makes it possible to distinguish a GPIO
        // input problem from a higher-level STOP/state-machine problem while
        // validating the physical button over USB or Bluetooth diagnostics.
        Telemetry::event("PHYSICAL_STOP_EDGE");
        // The last System Check stage deliberately verifies the same physical
        // GP6 input that is used for an emergency stop.  Consume that press
        // here so it confirms the check instead of navigating away from it.
        if(uiState==UiState::CONFIG_LOAD_CELL_DIAGNOSTICS&&Machine::confirmSystemCheckStopButton())
        {
            lastStop=stop;
            return;
        }
#if defined(HANKWINDER_TOUCH_DIAGNOSTICS)
        // The dedicated physical STOP is never a navigation control in the
        // touch product.  While a winding workflow is preparing or active it
        // immediately cancels STEP generation and disables the TMC2209; the
        // UI is then returned to home on its own normal LVGL pass.
        if(uiState==UiState::HALL_SEEK||uiState==UiState::HALL_FAILED||
           uiState==UiState::TARING||uiState==UiState::TARE_FAILED||
           uiState==UiState::LOAD_YARN||uiState==UiState::WINDING)
        {
            resetToYarnWeightSelection();
            TouchDiagnosticsUi::requestEmergencyStopView();
        }
        lastStop=stop;
        return;
#endif
        if(uiState==UiState::HALL_SEEK||uiState==UiState::HALL_FAILED)
        {
            cancelAuxiliaryMotion();HallSensor::cancel();resetToYarnWeightSelection();lastStop=stop;return;
        }
        if(uiState==UiState::TARING||uiState==UiState::TARE_FAILED)
        {
            LoadCells::cancelTare();
            motionActive=false;
            requestedStepRateHz=0;
            disableMotor();
            uiState=weightCapabilityEnabled?(autoCountMode?UiState::TURN_SELECT:UiState::FUH_PROGRAM_SELECT):UiState::TURN_SELECT;
            drawCurrentScreen();delay(120);lastStop=stop;return;
        }
        if(uiState==UiState::CONFIG_MENU)
        {
            finishConfiguration();
            drawCurrentScreen();delay(120);lastStop=stop;return;
        }
        if(uiState==UiState::CONFIG_CONNECTIVITY_MENU)
        {
            returnToConfigurationMenu(connectivityTopIndex());delay(120);lastStop=stop;return;
        }
        if(uiState==UiState::CONFIG_MACHINE_SETTINGS_MENU)
        {
            returnToConfigurationMenu(machineSettingsTopIndex());delay(120);lastStop=stop;return;
        }
        if(uiState==UiState::CONFIG_DIAGNOSTICS_MENU)
        {
            returnToConfigurationMenu(diagnosticsTopIndex());delay(120);lastStop=stop;return;
        }
        if(uiState==UiState::CONFIG_MOTOR_MENU)
        {
            uiState=UiState::CONFIG_MACHINE_SETTINGS_MENU;
            drawCurrentScreen();delay(120);lastStop=stop;return;
        }
        if(uiState==UiState::CONFIG_RUN_CURRENT||uiState==UiState::CONFIG_HOLD_CURRENT||uiState==UiState::CONFIG_DIRECTION)
        {
            uiState=UiState::CONFIG_MOTOR_MENU;
            drawCurrentScreen();delay(120);lastStop=stop;return;
        }
        if(uiState==UiState::CONFIG_WEIGHT_MENU)
        {
            returnToConfigurationMenu(0);delay(120);lastStop=stop;return;
        }
        if(uiState==UiState::CONFIG_WEIGHT_VALUE)
        {
            uiState=UiState::CONFIG_WEIGHT_MENU;drawCurrentScreen();delay(120);lastStop=stop;return;
        }
        if(uiState==UiState::CONFIG_PAIRING)
        {
            Machine::closePairingWindow();
            uiState=UiState::CONFIG_CONNECTIVITY_MENU;
            drawCurrentScreen();delay(120);lastStop=stop;return;
        }
        if(uiState==UiState::CONFIG_LOAD_CELL_DIAGNOSTICS||uiState==UiState::CONFIG_WEIGHT_DIAGNOSTICS||uiState==UiState::CONFIG_LOAD_CELL_TIMING||uiState==UiState::CONFIG_COMMISSIONING)
        {
            if(uiState==UiState::CONFIG_LOAD_CELL_DIAGNOSTICS)Machine::cancelSystemCheck();
            if(uiState==UiState::CONFIG_WEIGHT_DIAGNOSTICS)Machine::closeWeightDiagnostics();
            if(uiState==UiState::CONFIG_COMMISSIONING)Machine::cancelCommissioning();
            uiState=uiState==UiState::CONFIG_COMMISSIONING?UiState::CONFIG_MACHINE_SETTINGS_MENU:UiState::CONFIG_DIAGNOSTICS_MENU;
            drawCurrentScreen();delay(120);lastStop=stop;return;
        }
        if(uiState==UiState::CONFIG_FIRMWARE_UPDATE)
        {
            if(FirmwareUpdate::uploading()||FirmwareUpdate::status()==FirmwareUpdateStatus::Success){beep(120);lastStop=stop;return;}
            FirmwareUpdate::end();
            if(settings.bluetoothEnabled)DiagnosticsTransport::setEnabled(true);
            uiState=UiState::CONFIG_CONNECTIVITY_MENU;
            drawCurrentScreen();delay(120);lastStop=stop;return;
        }
        if(weightCapabilityEnabled&&(uiState==UiState::TURN_SELECT||uiState==UiState::FUH_PROGRAM_SELECT))
        {
            uiState=UiState::AUTO_MODE_SELECT;
            drawCurrentScreen();lastStop=stop;return;
        }
        if(uiState==UiState::WINDING&&autoCountMode&&pauseState==PauseState::PAUSED)
        {
            // A deliberate stop from rest is a completed partial count run,
            // not an abort. The supervisor releases tension, waits for a
            // quiet load-cell window, and then hands the final count/weight
            // to the normal COMPLETE screen.
            if(weightCapabilityEnabled&&RunSupervisor::requestFinalMeasurement())
            {
                Telemetry::event("COUNT_STOP_PAUSED_FINAL_MEASURE");
                drawWindingScreen();lastStop=stop;return;
            }
            // Measurement is already in progress or has just completed.
            // Ignore a second STOP rather than turning a successful finish
            // into an abort.
            if(RunSupervisor::snapshot().settlingFinalWeight||RunSupervisor::snapshot().finalWeightMeasured)
            {
                lastStop=stop;return;
            }
        }
        if(uiState==UiState::WINDING&&autoCountMode)
        {
            // A STOP while moving is a true emergency coast stop. No ramp,
            // reverse, or commanded deceleration is requested; disabling the
            // driver lets a free hub coast and a bound hub stop immediately.
            Telemetry::event("COUNT_STOP_RUNNING_COAST_ABORT");
        }
        if(uiState==UiState::WINDING&&!autoCountMode&&pauseState==PauseState::RUNNING)
        {
            // Weight-mode STOP while running is the same immediate coast
            // abort as count mode.  Keep the paused path unchanged.
            Telemetry::event("WEIGHT_STOP_RUNNING_COAST_ABORT");
        }
        // RESET remains a genuine immediate abort.
        // Unlike Pause, it intentionally does NOT ramp.
        resetToYarnWeightSelection();

        delay(120);
    }

    lastStop =
        stop;
}
