#include "app_api.h"
#include "diagnostics_transport.h"
#include "load_cells.h"
#include "firmware_update.h"
#include "hall_sensor.h"
#include "run_supervisor.h"
#include "machine_interface.h"

namespace {
void formatOneDecimal(char* output,size_t size,float value)
{
    const long tenths=lroundf(value*10.0f);
    const unsigned long magnitude=tenths<0?static_cast<unsigned long>(-tenths):static_cast<unsigned long>(tenths);
    snprintf(output,size,"%s%lu.%lu",tenths<0?"-":"",magnitude/10,magnitude%10);
}

void formatTwoDecimals(char* output,size_t size,float value)
{
    const long hundredths=lroundf(value*100.0f);
    const unsigned long magnitude=hundredths<0?static_cast<unsigned long>(-hundredths):static_cast<unsigned long>(hundredths);
    snprintf(output,size,"%s%lu.%02lu",hundredths<0?"-":"",magnitude/100,magnitude%100);
}

const char* fuhProgramName(FuhProgram program)
{
    static const char* names[]={"Mini  20g","Half  50g","Full  100g"};
    return names[uint8_t(program)];
}

void formatFuhProgress(char* output,size_t size,int completedTurns,const RunSupervisorSnapshot& control)
{
    if(autoCountMode){snprintf(output,size,"%d / %d turns",completedTurns,selectedTurns);return;}
    if(isnan(control.filteredWeightGrams)){snprintf(output,size,"Measuring...");return;}
    char measured[12]{};formatOneDecimal(measured,sizeof(measured),control.filteredWeightGrams);
    static const char* nominal[]={"20g","50g","100g"};
    snprintf(output,size,"%s / %s",measured,nominal[uint8_t(activeFuhProgram)]);
}
}

void drawWeightDiagnosticsScreen()
{
    display.clearBuffer();display.setFont(u8g2_font_6x12_tf);
    display.drawStr(4,12,"Weight Diagnostics");
    const Machine::DiagnosticsSnapshot& diagnostics=Machine::diagnostics();
    char value[28]{};
    if(diagnostics.weightDiagnosticsSettling)snprintf(value,sizeof(value),"Motor settling...");
    else if(diagnostics.tareStatus==Machine::TareStatus::Collecting)snprintf(value,sizeof(value),"Taring...");
    else if(diagnostics.tareStatus==Machine::TareStatus::Failed)snprintf(value,sizeof(value),"Tare failed");
    else if(!diagnostics.weightValid)snprintf(value,sizeof(value),"No valid weight");
    else {char number[20];formatOneDecimal(number,sizeof(number),diagnostics.weightGrams);snprintf(value,sizeof(value),"%s g",number);}
    display.drawStr(4,31,value);
    display.setFont(u8g2_font_5x8_tf);
    if(diagnostics.missingReadyMask)snprintf(value,sizeof(value),"Not ready mask: %02X",diagnostics.missingReadyMask);
    else if(diagnostics.suspiciousMask)snprintf(value,sizeof(value),"JUMP? mask: %02X",diagnostics.suspiciousMask);
    else snprintf(value,sizeof(value),"Raw-derived / no smoothing");
    display.drawStr(0,44,value);
    display.drawStr(0,61,"Click: Tare STOP: Back");display.sendBuffer();
}

void drawCommissioningScreen()
{
    display.clearBuffer();display.setFont(u8g2_font_5x8_tf);
    display.drawStr(22,8,"Load Cell Commission");display.drawHLine(0,11,128);
    const Machine::CommissioningSnapshot& commissioning=Machine::commissioning();
    const auto state=commissioning.state;
    char line[30]{};
    if(state==Machine::CommissioningState::Ready)
    {
        char massOne[12]{},massTwo[12]{},massThree[12]{},collars[12]{};
        formatTwoDecimals(massOne,sizeof(massOne),commissioning.referenceMassesGrams[0]);
        formatTwoDecimals(massTwo,sizeof(massTwo),commissioning.referenceMassesGrams[1]);
        formatTwoDecimals(massThree,sizeof(massThree),commissioning.referenceMassesGrams[2]);
        formatTwoDecimals(collars,sizeof(collars),commissioning.expectedTotalGrams);
        snprintf(line,sizeof(line),"M1 %sg M2 %sg",massOne,massTwo);display.drawStr(2,25,line);
        snprintf(line,sizeof(line),"M3 %sg then hub",massThree);display.drawStr(2,35,line);
        snprintf(line,sizeof(line),"Collars validate %sg",collars);display.drawStr(2,47,line);
        display.drawStr(2,61,"Click: Begin STOP: Back");
    }
    else if(state==Machine::CommissioningState::PromptLeg)
    {
        display.drawStr(2,25,commissioning.message);
        display.drawStr(2,38,"Directly over that leg");display.drawStr(2,50,"Click when positioned");
        display.drawStr(2,61,"STOP: Cancel");
    }
    else if(state==Machine::CommissioningState::PromptAllFirst)
    {
        display.drawStr(2,25,"Place 4 collars at ends");display.drawStr(2,38,"Capture orientation one");
        display.drawStr(2,50,"Click when positioned");display.drawStr(2,61,"STOP: Cancel");
    }
    else if(state==Machine::CommissioningState::PromptHubReference)
    {
        display.drawStr(2,25,"Center-hub correction");display.drawStr(2,38,commissioning.message);
        display.drawStr(2,50,"Click when positioned");display.drawStr(2,61,"STOP: Cancel");
    }
    else if(state==Machine::CommissioningState::PromptRotate)
    {
        display.drawStr(2,25,"First validation passed");display.drawStr(2,38,"Remove all weights");
        display.drawStr(2,50,"Click: rotate hub 180 deg");display.drawStr(2,61,"STOP: Cancel");
    }
    else if(state==Machine::CommissioningState::PromptAllSecond)
    {
        display.drawStr(2,25,"W1 lower left fixed");display.drawStr(2,38,"W2-4 same chassis spots");
        display.drawStr(2,50,"Click when positioned");display.drawStr(2,61,"STOP: Cancel");
    }
    else if(state==Machine::CommissioningState::Complete)
    {
        char first[12]{},second[12]{},expected[12]{};
        formatOneDecimal(first,sizeof(first),commissioning.firstValidationGrams);
        formatOneDecimal(second,sizeof(second),commissioning.secondValidationGrams);
        formatOneDecimal(expected,sizeof(expected),commissioning.expectedTotalGrams);
        snprintf(line,sizeof(line),"0 deg: %s / %s g",first,expected);display.drawStr(0,28,line);
        snprintf(line,sizeof(line),"180: %s / %s g",second,expected);display.drawStr(0,40,line);
        display.drawStr(8,51,"Calibration saved");display.drawStr(2,61,"Click: Configuration");
    }
    else if(state==Machine::CommissioningState::Failed)
    {
        display.drawStr(2,28,"COMMISSION FAILED");display.drawStr(2,42,commissioning.message);
        display.drawStr(2,61,"Click/STOP: Configuration");
    }
    else
    {
        display.drawStr(2,25,commissioning.message);
        if(state==Machine::CommissioningState::CapturingLeg||state==Machine::CommissioningState::CapturingHubReference||state==Machine::CommissioningState::CapturingAllFirst||state==Machine::CommissioningState::CapturingAllSecond)
        {
            snprintf(line,sizeof(line),"Collecting %u / %u",commissioning.samplesCollected,commissioning.samplesRequired);display.drawStr(2,39,line);
        }
        else if(state==Machine::CommissioningState::Indexing)display.drawStr(2,39,"Motor turning slowly");
        else if(state==Machine::CommissioningState::Taring)display.drawStr(2,39,"Keep hub empty and still");
        else if(state==Machine::CommissioningState::Rotating)display.drawStr(2,39,"Motor turning slowly");
        display.drawStr(2,61,"STOP: Cancel");
    }
    display.sendBuffer();
}

void drawFuhProgramScreen()
{
    display.clearBuffer();display.setFont(u8g2_font_5x8_tf);
    display.drawStr((128-display.getStrWidth("Auto HankWinder"))/2,9,"Auto HankWinder");display.drawHLine(0,12,128);
    display.setFont(u8g2_font_6x12_tf);display.drawStr(25,29,"Select winding");
    const char* label=fuhProgramName(selectedFuhProgram);
    display.setFont(u8g2_font_ncenB14_tr);const int width=display.getStrWidth(label);display.drawStr((128-width)/2,53,label);
    display.setFont(u8g2_font_5x8_tf);display.drawStr(24,63,"Turn / Click");display.sendBuffer();
}

void drawWindingScreen()
{
    if(RunSupervisor::waitingForClearance()) {
        display.clearBuffer();display.setFont(u8g2_font_6x12_tf);
        const auto reason=RunSupervisor::snapshot().clearanceReason;
        const bool dataFault=reason==ClearanceReason::DriverData;
        const bool modeFault=reason==ClearanceReason::DriverMode;
        display.drawStr(4,13,dataFault?"DRIVER DATA MISSING":modeFault?"DRIVER MODE ERROR":reason==ClearanceReason::Hall?"CHECK HUB / SENSOR":"CHECK YARN / BIND");
        display.drawStr(4,30,dataFault?"Check UART / power":modeFault?"Check driver setup":"Clear before resume");
        display.setFont(u8g2_font_5x8_tf);
        display.drawStr(4,45,dataFault||modeFault?"Not a detected bind":autoCountMode?"Turn count uncertain":"Weight will be checked");
        display.drawStr(4,61,"Click: Resume STOP: End");
        display.sendBuffer();return;
    }
    display.clearBuffer();

    uint32_t target =
        targetStepCount;

    uint32_t current =
        safeCurrentStepCount();
    if(autoCountMode){const uint32_t correction=HallSensor::correctionSteps();current-=min(current,correction);target-=min(target,correction);}

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
    const RunSupervisorSnapshot control=RunSupervisor::snapshot();

    // A paused count run can be intentionally finished with STOP. Its final
    // stopped-weight measurement owns the screen until the normal COMPLETE
    // screen takes over, rather than looking like an ordinary resumable pause.
    if(control.settlingFinalWeight&&autoCountMode)
    {
        display.setFont(u8g2_font_ncenB14_tr);
        const char* text="MEASURING";
        int width=display.getStrWidth(text);
        display.drawStr((128-width)/2,25,text);
        display.setFont(u8g2_font_6x12_tf);
        snprintf(buffer,sizeof(buffer),"Final count: %d",completedTurns);
        width=display.getStrWidth(buffer);display.drawStr((128-width)/2,43,buffer);
        display.setFont(u8g2_font_5x8_tf);
        display.drawStr(13,61,"Capturing final weight");
        display.sendBuffer();
        return;
    }

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

        if(weightCapabilityEnabled||autoCountMode)formatFuhProgress(buffer,sizeof(buffer),completedTurns,control);
        else snprintf(buffer,sizeof(buffer),"%d / %d turns",completedTurns,selectedTurns);

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

        if(weightCapabilityEnabled||autoCountMode)formatFuhProgress(buffer,sizeof(buffer),completedTurns,control);
        else snprintf(buffer,sizeof(buffer),"%d / %d turns",completedTurns,selectedTurns);

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

        if(weightCapabilityEnabled||autoCountMode)formatFuhProgress(buffer,sizeof(buffer),completedTurns,control);
        else snprintf(buffer,sizeof(buffer),"%d / %d turns",completedTurns,selectedTurns);

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

    const bool weightControl=control.weightArmed;
    const char* windingTitle="WINDING";
    if(control.settlingFinalWeight)windingTitle="MEASURING";
    else if(control.recovery==StallRecovery::Phase::Slowing||control.recovery==StallRecovery::Phase::Proving)windingTitle="LOAD: SLOWING";
    else if(control.recovery==StallRecovery::Phase::Locked)windingTitle="REDUCED SPEED";
    else if(control.recovery==StallRecovery::Phase::Ramping)windingTitle="RECOVERING";
    else if(control.weightState==WeightApproachState::Calibration)windingTitle="LEARNING";
    else if(control.weightState==WeightApproachState::Bulk)windingTitle="BULK WIND";
    else if(control.weightState==WeightApproachState::Refinement)windingTitle="REFINING";
    else if(control.weightState==WeightApproachState::Approach)windingTitle="APPROACH";
    display.drawStr((128-display.getStrWidth(windingTitle))/2,11,windingTitle);

    display.drawHLine(
        0,
        15,
        128
    );

    if(weightCapabilityEnabled||autoCountMode)formatFuhProgress(buffer,sizeof(buffer),completedTurns,control);
    else if(weightControl&&!isnan(control.filteredWeightGrams))
    {
        char measured[12]{},target[12]{};
        formatOneDecimal(measured,sizeof(measured),control.filteredWeightGrams);
        formatOneDecimal(target,sizeof(target),control.targetWeightGrams);
        snprintf(buffer,sizeof(buffer),"%s/%sg",measured,target);
    }
    else snprintf(buffer,sizeof(buffer),"%d / %d",completedTurns,selectedTurns);

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

    char rps[12]{};formatOneDecimal(rps,sizeof(rps),currentMotorRPS);
    if(weightCapabilityEnabled||autoCountMode)snprintf(buffer,sizeof(buffer),"%s r/s   Click: Pause",rps);
    else if(weightControl)snprintf(buffer,sizeof(buffer),"%s r/s  %d/%d turns",rps,completedTurns,selectedTurns);
    else snprintf(buffer,sizeof(buffer),"%s r/s  %d%%",rps,speedTrimPercent);

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

void drawCompleteScreen()
{
    display.clearBuffer();

    display.setFont(
        u8g2_font_6x12_tf
    );

    const char* title=finalRunAborted?"ABORTED":"COMPLETE";
    display.drawStr((128-display.getStrWidth(title))/2,12,title);

    display.drawHLine(
        0,
        16,
        128
    );

    const uint32_t turns100=uint64_t(finalRunStepCount)*100ULL/Config::StepsPerHubRev;
    char line[28]{};
    snprintf(line,sizeof(line),"Turns: %lu.%02lu",(unsigned long)(turns100/100),(unsigned long)(turns100%100));
    int width=display.getStrWidth(line);display.drawStr((128-width)/2,31,line);
    if(finalRunWeightValid)
    {
        char weight[12]{};formatOneDecimal(weight,sizeof(weight),finalRunWeightGrams);
        snprintf(line,sizeof(line),"Weight: %s g",weight);
        width=display.getStrWidth(line);display.drawStr((128-width)/2,45,line);
    }
    else if(finalRunAborted)
    {
        display.drawStr(18,45,"Target not reached");
    }
    display.setFont(u8g2_font_5x8_tf);
    if(autoCountMode&&RunSupervisor::snapshot().countUncertain)display.drawStr(6,54,"Turn count uncertain");
    display.drawStr(weightCapabilityEnabled?1:24,63,weightCapabilityEnabled?"Remove hank, then click":"Click to continue");

    display.sendBuffer();
}

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

void drawCurrentScreen()
{
    switch (uiState)
    {
        case UiState::AUTO_MODE_SELECT:
            display.clearBuffer();display.setFont(u8g2_font_6x12_tf);
            display.drawStr(19,12,"Auto HankWinder");
            display.drawHLine(0,16,128);
            display.drawStr(16,34,autoCountMode?"> Count based":"  Count based");
            display.drawStr(16,48,autoCountMode?"  Weight based":"> Weight based");
            display.setFont(u8g2_font_5x8_tf);display.drawStr(24,63,"Turn / Click");display.sendBuffer();
            break;

        case UiState::TURN_SELECT:
            {
                display.clearBuffer();display.setFont(u8g2_font_6x12_tf);
                const char* title=weightCapabilityEnabled?"Auto HankWinder":"HankWinder";
                display.drawStr((128-display.getStrWidth(title))/2,12,title);
                display.drawHLine(0,16,128);display.drawStr(28,29,"Turn count");
                char count[12]{};snprintf(count,sizeof(count),"%d",selectedTurns);
                display.setFont(u8g2_font_ncenB14_tr);display.drawStr((128-display.getStrWidth(count))/2,49,count);
                display.setFont(u8g2_font_5x8_tf);display.drawStr(4,63,"Click: Start STOP: Back");display.sendBuffer();
            }
            break;

        case UiState::FUH_PROGRAM_SELECT:
            drawFuhProgramScreen();
            break;

        case UiState::TARING:
        case UiState::TARE_FAILED:
            drawTareScreen();
            break;

        case UiState::LOAD_YARN:
            drawLoadYarnScreen();
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

        case UiState::CONFIG_MENU:
        case UiState::CONFIG_CONNECTIVITY_MENU:
        case UiState::CONFIG_MACHINE_SETTINGS_MENU:
        case UiState::CONFIG_DIAGNOSTICS_MENU:
        case UiState::CONFIG_MOTOR_MENU:
        case UiState::CONFIG_RUN_CURRENT:
        case UiState::CONFIG_HOLD_CURRENT:
        case UiState::CONFIG_DIRECTION:
        case UiState::CONFIG_WEIGHT_MENU:
        case UiState::CONFIG_WEIGHT_VALUE:
        case UiState::CONFIG_PAIRING:
            drawConfigScreen();
            break;

        case UiState::CONFIG_LOAD_CELL_DIAGNOSTICS:
            drawLoadCellDiagnosticsScreen();
            break;
        case UiState::CONFIG_LOAD_CELL_TIMING:
        {
            display.clearBuffer();display.setFont(u8g2_font_5x8_tf);
            char line[40];
            display.drawStr(0,8,"Load Cell Timing (us)");
            snprintf(line,sizeof(line),"Set:%u Active:%u %lus",LoadCells::selectedTimingDelay(),LoadCells::sampleDelayUs(),(unsigned long)(LoadCells::timingElapsedMs()/1000));
            display.drawStr(0,16,line);
            const auto cells=LoadCells::snapshot();
            const char* labels[]={"FL","FR","RL","RR"};
            for(uint8_t ch=0;ch<Config::LoadCellChannelCount;++ch){
                snprintf(line,sizeof(line),"%s %ld %s",labels[ch],(long)cells.raw[ch],LoadCells::statusName(cells.status[ch]));
                display.drawStr(0,24+8*ch,line);
            }
            snprintf(line,sizeof(line),"J:%lu TO:%lu",(unsigned long)LoadCells::timingJumps(),(unsigned long)LoadCells::timingTimeouts());
            display.drawStr(0,56,line);
            display.drawStr(0,64,"Click:Apply STOP:Back");display.sendBuffer();
            break;
        }
        case UiState::CONFIG_WEIGHT_DIAGNOSTICS:
            drawWeightDiagnosticsScreen();
            break;
        case UiState::CONFIG_COMMISSIONING:
            drawCommissioningScreen();
            break;

        case UiState::CONFIG_FIRMWARE_UPDATE:
            drawFirmwareUpdateScreen();
            break;

        case UiState::HALL_SEEK:
        case UiState::HALL_FAILED:
            display.clearBuffer();display.setFont(u8g2_font_6x10_tf);
            display.drawStr(4,14,uiState==UiState::HALL_SEEK?"Finding first magnet":"HALL SENSOR ERROR");
            display.drawStr(4,31,uiState==UiState::HALL_SEEK?"Keep hub clear":"Check sensor / hub");
            display.setFont(u8g2_font_5x8_tf);
            display.drawStr(4,48,uiState==UiState::HALL_SEEK?"Slow rotation before tare":"Click: Retry");
            display.drawStr(4,61,"STOP: Cancel");display.sendBuffer();
            break;

        case UiState::LOAD_CELL_FAULT:
            drawLoadCellFaultScreen();
            break;
    }
}

void drawTareScreen()
{
    display.clearBuffer();
    display.setFont(u8g2_font_6x10_tf);
    display.drawStr(31,11,"LOAD CELLS");
    display.drawHLine(0,15,128);
    if(uiState==UiState::TARE_FAILED||LoadCells::tareStatus()==LoadCellTareStatus::Failed)
    {
        display.setFont(u8g2_font_helvB10_tf);
        display.drawStr(9,34,"TARE UNSTABLE");
        display.setFont(u8g2_font_5x8_tf);
        display.drawStr(12,49,"Click: Retry");
        display.drawStr(12,60,"STOP: Cancel");
    }
    else
    {
        display.setFont(u8g2_font_helvB10_tf);
        display.drawStr(36,34,"TARING");
        display.setFont(u8g2_font_5x8_tf);
        display.drawStr(19,49,"Do not touch");
        display.drawStr(9,60,"Motor holding still");
    }
    display.sendBuffer();
}

void drawLoadYarnScreen()
{
    display.clearBuffer();
    display.setFont(u8g2_font_6x10_tf);
    if(HallSensor::state()==HallSensor::State::Absent&&settings.hallEnabled)display.drawStr(10,11,"HALL NOT DETECTED");
    else display.drawStr(25,11,weightCapabilityEnabled?"LOAD CELLS":"READY TO WIND");
    display.drawHLine(0,15,128);
    display.setFont(u8g2_font_helvB10_tf);
    display.drawStr(24,34,"LOAD YARN");
    display.setFont(u8g2_font_5x8_tf);
    display.drawStr(13,49,"Click when ready");
    display.drawStr(22,60,"STOP: Cancel");
    display.sendBuffer();
}

void drawLoadCellFaultScreen()
{
    display.clearBuffer();
    display.setDrawColor(1);
    display.setFont(u8g2_font_6x10_tf);
    display.drawStr(17,12,"LOAD CELL ERROR");
    display.drawHLine(0,16,128);
    char line[24]{};
    snprintf(line,sizeof(line),"Detected: %u of %u",detectedLoadCellCount,Config::RequiredLoadCells);
    display.drawStr(14,35,line);
    display.setFont(u8g2_font_5x8_tf);
    display.drawStr(8,50,"Check wiring, then");
    display.drawStr(28,60,"restart machine");
    display.sendBuffer();
}

void drawLoadCellDiagnosticsScreen()
{
    const Machine::SystemCheckSnapshot& check=Machine::systemCheck();
    display.clearBuffer();
    display.setDrawColor(1);
    display.setFont(u8g2_font_5x8_tf);
    display.drawStr(28,8,"System Check");
    display.drawHLine(0,11,128);
    display.setFont(u8g2_font_6x10_tf);
    if(check.stage==Machine::SystemCheckStage::Passed)
    {
        display.drawStr(19,31,"ALL CHECKS PASS");
        display.setFont(u8g2_font_5x8_tf);
        display.drawStr(5,44,"Load cells / tare / Hall / STOP");
    }
    else if(check.stage==Machine::SystemCheckStage::Failed)
    {
        display.drawStr(42,28,"FAILED");
        display.setFont(u8g2_font_5x8_tf);
        display.drawStr(2,43,check.message);
    }
    else
    {
        display.drawStr(8,30,check.message);
        display.setFont(u8g2_font_5x8_tf);
        if(check.loadCellsPassed)display.drawStr(6,43,"Load cells: OK");
        if(check.tarePassed)display.drawStr(6,52,"Tare: OK");
        if(check.clockwiseHallPassed)display.drawStr(70,43,"CW Hall: OK");
        if(check.counterClockwiseHallPassed)display.drawStr(70,52,"CCW Hall: OK");
        if(check.stage==Machine::SystemCheckStage::WaitingForStopButton)display.drawStr(6,61,"Press physical STOP");
    }
    display.setFont(u8g2_font_5x8_tf);
    if(check.stage!=Machine::SystemCheckStage::WaitingForStopButton)display.drawStr(25,63,"STOP: Cancel / Back");
    display.sendBuffer();
}

void drawFirmwareUpdateScreen()
{
    display.clearBuffer();display.setDrawColor(1);display.setFont(u8g2_font_5x8_tf);
    display.drawStr(31,8,"Wi-Fi Update");display.drawHLine(0,11,128);
    if(FirmwareUpdate::status()==FirmwareUpdateStatus::Ready)
    {
        char line[28]{};snprintf(line,sizeof(line),"%s: %.20s",FirmwareUpdate::stationMode()?"LAN":"AP",FirmwareUpdate::networkName());display.drawStr(2,21,line);
        snprintf(line,sizeof(line),"http://%s",FirmwareUpdate::ipAddress());display.drawStr(2,32,line);
        if(FirmwareUpdate::stationMode())snprintf(line,sizeof(line),"Login admin / %s",FirmwareUpdate::accessCode());
        else snprintf(line,sizeof(line),"AP pass: %s",Config::FirmwareUpdatePassword);
        display.drawStr(2,43,line);
        display.drawStr(2,52,FirmwareUpdate::networkMessage());
        display.drawStr(2,63,"STOP: Close Wi-Fi");
    }
    else if(FirmwareUpdate::status()==FirmwareUpdateStatus::Uploading)
    {
        char line[24]{};snprintf(line,sizeof(line),"Received: %lu KB",(unsigned long)(FirmwareUpdate::bytesReceived()/1024));
        display.drawStr(2,31,line);
        display.drawStr(2,61,"Keep power connected");
    }
    else if(FirmwareUpdate::status()==FirmwareUpdateStatus::Success)
    {
        display.drawStr(28,31,"Update received");display.drawStr(35,45,"Restarting...");
    }
    else
    {
        char line[24]{};snprintf(line,sizeof(line),"%s (%u)",FirmwareUpdate::statusText(),FirmwareUpdate::errorCode());
        display.drawStr(2,29,line);display.drawStr(2,45,FirmwareUpdate::networkMessage());display.drawStr(2,61,"STOP: Close Wi-Fi");
    }
    display.sendBuffer();
}

void drawConfigScreen()
{
    display.clearBuffer();
    display.setDrawColor(1);
    display.setFont(u8g2_font_6x10_tf);
    display.drawStr(2,10,"Configuration");
    display.drawHLine(0,13,128);

    display.setFont(u8g2_font_6x10_tf);
    char value[24]{};
    if(uiState==UiState::CONFIG_MENU)
    {
        static const char* turnItems[]={"Connectivity","Machine Specific Settings","Diagnostics","Exit setup"};
        static const char* fuhItems[]={"Weight Targets","Connectivity","Machine Specific Settings","Diagnostics","Exit setup"};
        const bool fuh=weightCapabilityEnabled;
        const bool machineSettings=configMenuSelection==(fuh?2:1);
        if(machineSettings)display.setFont(u8g2_font_5x8_tf);
        display.drawStr(2,29,fuh?fuhItems[configMenuSelection]:turnItems[configMenuSelection]);
        if(machineSettings)display.setFont(u8g2_font_6x10_tf);
        if(fuh&&configMenuSelection==0)snprintf(value,sizeof(value),"Mini / Half / Full");
        else if(configMenuSelection==(fuh?1:0))snprintf(value,sizeof(value),"Bluetooth / Wi-Fi");
        else if(configMenuSelection==(fuh?2:1))snprintf(value,sizeof(value),"Motor / Hall / calibrate");
        else if(configMenuSelection==(fuh?3:2))snprintf(value,sizeof(value),"Service and live readings");
        else snprintf(value,sizeof(value),"Return to winding");
        display.drawStr(2,43,value);
        snprintf(value,sizeof(value),"%d/%d  Turn / Click",configMenuSelection+1,fuh?5:4);
        display.setFont(u8g2_font_5x8_tf);display.drawStr(2,61,value);
    }
    else if(uiState==UiState::CONFIG_CONNECTIVITY_MENU)
    {
        static const char* items[]={"Bluetooth","Pair device","Wi-Fi Update"};
        display.drawStr(2,29,items[configSubMenuSelection]);
        if(configSubMenuSelection==0)snprintf(value,sizeof(value),"%s",settings.bluetoothEnabled?"ON":"OFF");
        else if(configSubMenuSelection==1)snprintf(value,sizeof(value),"%s",settings.bluetoothEnabled?"Open 60 sec":"Bluetooth OFF");
        else snprintf(value,sizeof(value),"Wi-Fi browser upload");
        display.drawStr(2,43,value);
        snprintf(value,sizeof(value),"%d/3  Turn / Click",configSubMenuSelection+1);
        display.setFont(u8g2_font_5x8_tf);display.drawStr(2,61,value);
    }
    else if(uiState==UiState::CONFIG_MACHINE_SETTINGS_MENU)
    {
        static const char* turnItems[]={"Motor","Hall feedback"};
        static const char* fuhItems[]={"Motor","Hall feedback","Commission load cells"};
        const bool fuh=weightCapabilityEnabled;
        display.drawStr(2,29,fuh?fuhItems[configSubMenuSelection]:turnItems[configSubMenuSelection]);
        if(configSubMenuSelection==0)snprintf(value,sizeof(value),"%u/%u mA  %s",settings.runCurrentMa,settings.holdCurrentMa,settings.clockwise?"CW":"CCW");
        else if(configSubMenuSelection==1)snprintf(value,sizeof(value),"%s  GP10",settings.hallEnabled?"ON":"OFF");
        else snprintf(value,sizeof(value),"Index / tare / validate");
        display.drawStr(2,43,value);
        snprintf(value,sizeof(value),"%d/%d  Turn / Click",configSubMenuSelection+1,fuh?3:2);
        display.setFont(u8g2_font_5x8_tf);display.drawStr(2,61,value);
    }
    else if(uiState==UiState::CONFIG_DIAGNOSTICS_MENU)
    {
        static const char* turnItems[]={"Weight Diagnostics","Load Cell Timing"};
        static const char* fuhItems[]={"System Check","Weight Diagnostics","Load Cell Timing"};
        const bool fuh=weightCapabilityEnabled;
        display.drawStr(2,29,fuh?fuhItems[configSubMenuSelection]:turnItems[configSubMenuSelection]);
        if(fuh&&configSubMenuSelection==0)snprintf(value,sizeof(value),"Cells / tare / Hall motion");
        else if(configSubMenuSelection==(fuh?1:0))snprintf(value,sizeof(value),"Tare / live grams");
        else snprintf(value,sizeof(value),"Sample delay / raw");
        display.drawStr(2,43,value);
        snprintf(value,sizeof(value),"%d/%d  Turn / Click",configSubMenuSelection+1,fuh?3:2);
        display.setFont(u8g2_font_5x8_tf);display.drawStr(2,61,value);
    }
    else if(uiState==UiState::CONFIG_WEIGHT_MENU)
    {
        static const char* names[]={"Mini target","Half target","Full target"};
        display.drawStr(2,29,names[weightTargetMenuSelection]);
        const uint16_t centi=uint16_t(lroundf(Machine::weightTargets().grams[weightTargetMenuSelection]*100.0f));
        snprintf(value,sizeof(value),"%u.%02u g",centi/100,centi%100);
        display.drawStr(2,43,value);
        snprintf(value,sizeof(value),"%d/3  Turn / Click",weightTargetMenuSelection+1);
        display.setFont(u8g2_font_5x8_tf);display.drawStr(2,61,value);
    }
    else if(uiState==UiState::CONFIG_WEIGHT_VALUE)
    {
        static const char* names[]={"Mini target","Half target","Full target"};
        display.drawStr(2,29,names[weightTargetMenuSelection]);
        const uint16_t centi=uint16_t(lroundf(Machine::weightTargets().grams[weightTargetMenuSelection]*100.0f));
        snprintf(value,sizeof(value),"%u.%02u g",centi/100,centi%100);
        display.setFont(u8g2_font_helvB14_tf);display.drawStr(2,49,value);
        display.setFont(u8g2_font_5x8_tf);display.drawStr(2,61,"Turn / Click save");
    }
    else if(uiState==UiState::CONFIG_MOTOR_MENU)
    {
        static const char* items[]={"Run Current","Hold Current","Rotation Direction"};
        display.drawStr(2,29,items[motorMenuSelection]);
        if(motorMenuSelection==0)snprintf(value,sizeof(value),"%u mA",settings.runCurrentMa);
        else if(motorMenuSelection==1)snprintf(value,sizeof(value),"%u mA",settings.holdCurrentMa);
        else snprintf(value,sizeof(value),"%s",settings.clockwise?"Clockwise":"Counter clockwise");
        display.drawStr(2,43,value);
        snprintf(value,sizeof(value),"%d/3  Turn / Click",motorMenuSelection+1);
        display.setFont(u8g2_font_5x8_tf);display.drawStr(2,61,value);
    }
    else if(uiState==UiState::CONFIG_PAIRING)
    {
        display.drawStr(2,29,DiagnosticsTransport::connected()?"Paired client connected":"Pairing is open");
        snprintf(value,sizeof(value),"%lu seconds",(unsigned long)DiagnosticsTransport::pairingSecondsRemaining());
        display.drawStr(2,43,value);display.setFont(u8g2_font_5x8_tf);display.drawStr(2,61,"Click to close");
    }
    else if(uiState==UiState::CONFIG_DIRECTION)
    {
        display.drawStr(2,29,"Rotation Direction");
        display.setFont(settings.clockwise?u8g2_font_helvB14_tf:u8g2_font_6x10_tf);
        display.drawStr(2,49,settings.clockwise?"Clockwise":"Counter clockwise");
        display.setFont(u8g2_font_5x8_tf);display.drawStr(2,61,"Turn / Click save");
    }
    else
    {
        const bool run=uiState==UiState::CONFIG_RUN_CURRENT;
        display.drawStr(2,29,run?"Run current":"Hold current");
        snprintf(value,sizeof(value),"%u mA",run?settings.runCurrentMa:settings.holdCurrentMa);
        display.setFont(u8g2_font_helvB14_tf);display.drawStr(2,49,value);
        display.setFont(u8g2_font_5x8_tf);display.drawStr(2,61,"Turn / Click save");
    }
    display.sendBuffer();
}
