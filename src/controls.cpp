#include "app_api.h"
#include "diagnostics_transport.h"
#include "tmc_driver.h"
#include "firmware_update.h"
#include "load_cells.h"
#include "run_supervisor.h"

namespace {
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
    uiState=weightCapabilityEnabled?UiState::FUH_PROGRAM_SELECT:UiState::YARN_WEIGHT_SELECT;
    if(!weightCapabilityEnabled)loadSelectedTurnCount();
}

void startFuhProgram()
{
    activeFuhProgram=selectedFuhProgram;selectedTurns=Config::FuhSafetyTurnCeiling;selectedCruiseMotorRPS=Config::FuhCruiseMotorRps;
    for(uint8_t i=0;i<Config::CruiseSpeedOptionCount;i++)if(Config::CruiseSpeedOptions[i]==Config::FuhCruiseMotorRps)selectedCruiseSpeedIndex=i;
    if(activeFuhProgram==FuhProgram::JustTurn)RunSupervisor::disarmWeightTarget();
    else RunSupervisor::armWeightTarget(settings.weightTargetsCentiGrams[uint8_t(activeFuhProgram)]/100.0f);
    requestWindingStart();
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
            const int maximum=weightCapabilityEnabled?6:5;
            if(configMenuSelection>maximum)configMenuSelection=0;
            if(configMenuSelection<0)configMenuSelection=maximum;
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
            settings.runCurrentMa=constrain(value,int(Config::MinRunCurrentMa),int(Config::MaxRunCurrentMa));
            if(settings.holdCurrentMa>settings.runCurrentMa)settings.holdCurrentMa=settings.runCurrentMa;
            TmcDriver::applyCurrent(settings.runCurrentMa,settings.holdCurrentMa);drawCurrentScreen();
        }
        else if(uiState==UiState::CONFIG_HOLD_CURRENT)
        {
            int value=int(settings.holdCurrentMa)+(clockwise?Config::CurrentStepMa:-Config::CurrentStepMa);
            settings.holdCurrentMa=constrain(value,int(Config::MinHoldCurrentMa),min(int(Config::MaxHoldCurrentMa),int(settings.runCurrentMa)));
            TmcDriver::applyCurrent(settings.runCurrentMa,settings.holdCurrentMa);drawCurrentScreen();
        }
        else if(uiState==UiState::CONFIG_DIRECTION)
        {
            settings.clockwise=clockwise?1:0;
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
            int32_t value=int32_t(settings.weightTargetsCentiGrams[weightTargetMenuSelection])+(clockwise?Config::WeightTargetStepCentiGrams:-int(Config::WeightTargetStepCentiGrams));
            settings.weightTargetsCentiGrams[weightTargetMenuSelection]=constrain(value,int32_t(Config::MinWeightTargetCentiGrams),int32_t(Config::MaxWeightTargetCentiGrams));
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
                PauseState::RUNNING&&!weightCapabilityEnabled
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

        else if(uiState==UiState::FUH_PROGRAM_SELECT)
        {
            int selection=int(selectedFuhProgram)+(clockwise?1:-1);
            if(selection>3)selection=0;if(selection<0)selection=3;
            selectedFuhProgram=FuhProgram(selection);drawCurrentScreen();
        }

        else if(uiState==UiState::SPEED_SELECT)
        {
            selectedCruiseSpeedIndex+=clockwise?1:-1;
            if(selectedCruiseSpeedIndex<0)selectedCruiseSpeedIndex=Config::CruiseSpeedOptionCount-1;
            if(selectedCruiseSpeedIndex>=Config::CruiseSpeedOptionCount)selectedCruiseSpeedIndex=0;
            selectedCruiseMotorRPS=Config::CruiseSpeedOptions[selectedCruiseSpeedIndex];
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
            case UiState::CONFIG_MENU:
                if(configMenuSelection==0){settings.bluetoothEnabled=!settings.bluetoothEnabled;DiagnosticsTransport::setEnabled(settings.bluetoothEnabled);saveSettings();}
                else if(configMenuSelection==1){if(settings.bluetoothEnabled){DiagnosticsTransport::openPairingWindow();uiState=UiState::CONFIG_PAIRING;}else beep(120);}
                else if(configMenuSelection==2){motorMenuSelection=0;uiState=UiState::CONFIG_MOTOR_MENU;}
                else if(weightCapabilityEnabled&&configMenuSelection==3){weightTargetMenuSelection=0;uiState=UiState::CONFIG_WEIGHT_MENU;}
                else if(configMenuSelection==(weightCapabilityEnabled?4:3))uiState=UiState::CONFIG_LOAD_CELL_DIAGNOSTICS;
                else if(configMenuSelection==(weightCapabilityEnabled?5:4)){disableMotor();DiagnosticsTransport::closePairingWindow();DiagnosticsTransport::setEnabled(false);FirmwareUpdate::begin();uiState=UiState::CONFIG_FIRMWARE_UPDATE;}
                else finishConfiguration();
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
                saveSettings();uiState=UiState::CONFIG_WEIGHT_MENU;
                break;

            case UiState::CONFIG_PAIRING:
                DiagnosticsTransport::closePairingWindow();
                uiState=UiState::CONFIG_MENU;
                break;

            case UiState::CONFIG_LOAD_CELL_DIAGNOSTICS:
                break;

            case UiState::CONFIG_FIRMWARE_UPDATE:
                break;

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
                    UiState::SPEED_SELECT;

                break;

            case UiState::FUH_PROGRAM_SELECT:

                startFuhProgram();

                break;

            case UiState::SPEED_SELECT:

                uiState =
                    UiState::READY;

                break;

            case UiState::READY:

                requestWindingStart();

                break;

            case UiState::TARE_FAILED:

                requestWindingStart();

                break;

            case UiState::TARING:

                break;

            case UiState::LOAD_YARN:

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

                if(weightCapabilityEnabled)
                {
                    if(repeatYes)startFuhProgram();
                    else uiState=UiState::FUH_PROGRAM_SELECT;
                    break;
                }

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
        if(uiState==UiState::WINDING&&weightCapabilityEnabled&&activeFuhProgram==FuhProgram::JustTurn)
        {
            motionActive=false;requestedStepRateHz=0;beep(100);lastStop=stop;return;
        }
        if(uiState==UiState::TARING||uiState==UiState::TARE_FAILED)
        {
            LoadCells::cancelTare();
            motionActive=false;
            requestedStepRateHz=0;
            disableMotor();
            uiState=weightCapabilityEnabled?UiState::FUH_PROGRAM_SELECT:UiState::READY;
            drawCurrentScreen();delay(120);lastStop=stop;return;
        }
        if(uiState==UiState::CONFIG_MENU)
        {
            finishConfiguration();
            drawCurrentScreen();delay(120);lastStop=stop;return;
        }
        if(uiState==UiState::CONFIG_MOTOR_MENU)
        {
            uiState=UiState::CONFIG_MENU;
            drawCurrentScreen();delay(120);lastStop=stop;return;
        }
        if(uiState==UiState::CONFIG_RUN_CURRENT||uiState==UiState::CONFIG_HOLD_CURRENT||uiState==UiState::CONFIG_DIRECTION)
        {
            uiState=UiState::CONFIG_MOTOR_MENU;
            drawCurrentScreen();delay(120);lastStop=stop;return;
        }
        if(uiState==UiState::CONFIG_WEIGHT_MENU)
        {
            uiState=UiState::CONFIG_MENU;drawCurrentScreen();delay(120);lastStop=stop;return;
        }
        if(uiState==UiState::CONFIG_WEIGHT_VALUE)
        {
            uiState=UiState::CONFIG_WEIGHT_MENU;drawCurrentScreen();delay(120);lastStop=stop;return;
        }
        if(uiState==UiState::CONFIG_PAIRING)
        {
            DiagnosticsTransport::closePairingWindow();
            uiState=UiState::CONFIG_MENU;
            drawCurrentScreen();delay(120);lastStop=stop;return;
        }
        if(uiState==UiState::CONFIG_LOAD_CELL_DIAGNOSTICS)
        {
            uiState=UiState::CONFIG_MENU;
            drawCurrentScreen();delay(120);lastStop=stop;return;
        }
        if(uiState==UiState::CONFIG_FIRMWARE_UPDATE)
        {
            if(FirmwareUpdate::uploading()||FirmwareUpdate::status()==FirmwareUpdateStatus::Success){beep(120);lastStop=stop;return;}
            FirmwareUpdate::end();
            if(settings.bluetoothEnabled)DiagnosticsTransport::setEnabled(true);
            uiState=UiState::CONFIG_MENU;
            drawCurrentScreen();delay(120);lastStop=stop;return;
        }
        // RESET remains a genuine immediate abort.
        // Unlike Pause, it intentionally does NOT ramp.
        resetToYarnWeightSelection();

        delay(120);
    }

    lastStop =
        stop;
}
