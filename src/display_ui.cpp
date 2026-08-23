#include "app_api.h"
#include "diagnostics_transport.h"
#include "load_cells.h"
#include "firmware_update.h"
#include "run_supervisor.h"

namespace {
void formatOneDecimal(char* output,size_t size,float value)
{
    const long tenths=lroundf(value*10.0f);
    const unsigned long magnitude=tenths<0?static_cast<unsigned long>(-tenths):static_cast<unsigned long>(tenths);
    snprintf(output,size,"%s%lu.%lu",tenths<0?"-":"",magnitude/10,magnitude%10);
}

const char* fuhProgramName(FuhProgram program)
{
    static const char* names[]={"Mini  20g","Half  50g","Full  100g","Just Turn"};
    return names[uint8_t(program)];
}

void formatFuhProgress(char* output,size_t size,int completedTurns,const RunSupervisorSnapshot& control)
{
    if(activeFuhProgram==FuhProgram::JustTurn){snprintf(output,size,"%d turns",completedTurns);return;}
    if(isnan(control.filteredWeightGrams)){snprintf(output,size,"Measuring...");return;}
    char measured[12]{};formatOneDecimal(measured,sizeof(measured),control.filteredWeightGrams);
    static const char* nominal[]={"20g","50g","100g"};
    snprintf(output,size,"%s / %s",measured,nominal[uint8_t(activeFuhProgram)]);
}
}

void drawFuhProgramScreen()
{
    display.clearBuffer();display.setFont(u8g2_font_5x8_tf);
    display.drawStr(7,9,"FUHGEDDABOUDITINATOR");display.drawHLine(0,12,128);
    display.setFont(u8g2_font_6x12_tf);display.drawStr(25,29,"Select winding");
    const char* label=fuhProgramName(selectedFuhProgram);
    display.setFont(u8g2_font_ncenB14_tr);const int width=display.getStrWidth(label);display.drawStr((128-width)/2,53,label);
    display.setFont(u8g2_font_5x8_tf);display.drawStr(24,63,"Turn / Click");display.sendBuffer();
}

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

void drawSpeedScreen()
{
    display.clearBuffer();
    display.setFont(u8g2_font_6x12_tf);
    display.drawStr(19,11,"THE HANKINATOR");
    display.drawHLine(0,15,128);
    display.drawStr(29,32,"Motor Speed");
    char speed[12]{},line[20]{};
    formatOneDecimal(speed,sizeof(speed),selectedCruiseMotorRPS);
    snprintf(line,sizeof(line),"%s RPS",speed);
    display.setFont(u8g2_font_ncenB18_tr);
    const int width=display.getStrWidth(line);
    display.drawStr((128-width)/2,58,line);
    display.sendBuffer();
}

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

    char speed[12]{};formatOneDecimal(speed,sizeof(speed),selectedCruiseMotorRPS);
    snprintf(line,sizeof(line),"%d turns / %s r/s",selectedTurns,speed);

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
    const RunSupervisorSnapshot control=RunSupervisor::snapshot();

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

        if(weightCapabilityEnabled)formatFuhProgress(buffer,sizeof(buffer),completedTurns,control);
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

        if(weightCapabilityEnabled)formatFuhProgress(buffer,sizeof(buffer),completedTurns,control);
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

        if(weightCapabilityEnabled)formatFuhProgress(buffer,sizeof(buffer),completedTurns,control);
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
    else if(weightCapabilityEnabled&&activeFuhProgram==FuhProgram::JustTurn)windingTitle="JUST TURN";
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

    if(weightCapabilityEnabled)formatFuhProgress(buffer,sizeof(buffer),completedTurns,control);
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
    if(weightCapabilityEnabled)snprintf(buffer,sizeof(buffer),"%s r/s   Click: Pause",rps);
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
    display.drawStr(24,61,"Click to continue");

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
        case UiState::YARN_WEIGHT_SELECT:
            drawYarnWeightScreen();
            break;

        case UiState::SKEIN_SELECT:
            drawSkeinScreen();
            break;

        case UiState::TURN_SELECT:
            drawTurnScreen();
            break;

        case UiState::SPEED_SELECT:
            drawSpeedScreen();
            break;

        case UiState::FUH_PROGRAM_SELECT:
            drawFuhProgramScreen();
            break;

        case UiState::READY:
            drawReadyScreen();
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

        case UiState::CONFIG_FIRMWARE_UPDATE:
            drawFirmwareUpdateScreen();
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
        display.drawStr(9,60,"Hub rotating slowly");
    }
    display.sendBuffer();
}

void drawLoadYarnScreen()
{
    display.clearBuffer();
    display.setFont(u8g2_font_6x10_tf);
    display.drawStr(31,11,"LOAD CELLS");
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
    static const char* positions[]={"FL","FR","RL","RR"};
    const LoadCellSnapshot loadCells=LoadCells::snapshot();
    display.clearBuffer();
    display.setDrawColor(1);
    display.setFont(u8g2_font_5x8_tf);
    display.drawStr(11,8,"Load Cell Diagnostics");
    display.drawHLine(0,11,128);
    char line[27]{};
    for(uint8_t channel=0;channel<Config::LoadCellChannelCount;channel++)
    {
        snprintf(line,sizeof(line),"%s %8ld  %s",positions[channel],long(loadCells.raw[channel]),LoadCells::statusName(loadCells.status[channel]));
        display.drawStr(2,21+channel*10,line);
    }
    display.drawStr(20,63,"STOP: Back   10 Hz");
    display.sendBuffer();
}

void drawFirmwareUpdateScreen()
{
    display.clearBuffer();display.setDrawColor(1);display.setFont(u8g2_font_5x8_tf);
    display.drawStr(23,8,"Firmware Update");display.drawHLine(0,11,128);
    if(FirmwareUpdate::status()==FirmwareUpdateStatus::Ready)
    {
        char line[28]{};snprintf(line,sizeof(line),"Wi-Fi: %s",Config::FirmwareUpdateSsid);display.drawStr(2,22,line);
        snprintf(line,sizeof(line),"Pass: %s",Config::FirmwareUpdatePassword);display.drawStr(2,33,line);
        display.drawStr(2,44,"Open: 192.168.4.1");
        display.drawStr(2,61,"STOP: Cancel");
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
        display.drawStr(2,29,line);display.drawStr(2,45,"STOP: Back");
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
        static const char* turnItems[]={"Bluetooth","Pair device","Motor","Load Cell Diagnostics","Firmware Update","Exit setup"};
        static const char* fuhItems[]={"Bluetooth","Pair device","Motor","Weight Targets","Load Cell Diagnostics","Firmware Update","Exit setup"};
        const bool fuh=weightCapabilityEnabled;
        display.drawStr(2,29,fuh?fuhItems[configMenuSelection]:turnItems[configMenuSelection]);
        if(configMenuSelection==0)snprintf(value,sizeof(value),"%s",settings.bluetoothEnabled?"ON":"OFF");
        else if(configMenuSelection==1)snprintf(value,sizeof(value),"%s",settings.bluetoothEnabled?"Open 60 sec":"Bluetooth OFF");
        else if(configMenuSelection==2)snprintf(value,sizeof(value),"%u/%u mA  %s",settings.runCurrentMa,settings.holdCurrentMa,settings.clockwise?"CW":"CCW");
        else if(fuh&&configMenuSelection==3)snprintf(value,sizeof(value),"Mini / Half / Full");
        else if(configMenuSelection==(fuh?4:3))snprintf(value,sizeof(value),"FL / FR / RL / RR");
        else if(configMenuSelection==(fuh?5:4))snprintf(value,sizeof(value),"Wi-Fi browser upload");
        else snprintf(value,sizeof(value),"Return to winding");
        display.drawStr(2,43,value);
        snprintf(value,sizeof(value),"%d/%d  Turn / Click",configMenuSelection+1,fuh?7:6);
        display.setFont(u8g2_font_5x8_tf);display.drawStr(2,61,value);
    }
    else if(uiState==UiState::CONFIG_WEIGHT_MENU)
    {
        static const char* names[]={"Mini target","Half target","Full target"};
        display.drawStr(2,29,names[weightTargetMenuSelection]);
        const uint16_t centi=settings.weightTargetsCentiGrams[weightTargetMenuSelection];
        snprintf(value,sizeof(value),"%u.%02u g",centi/100,centi%100);
        display.drawStr(2,43,value);
        snprintf(value,sizeof(value),"%d/3  Turn / Click",weightTargetMenuSelection+1);
        display.setFont(u8g2_font_5x8_tf);display.drawStr(2,61,value);
    }
    else if(uiState==UiState::CONFIG_WEIGHT_VALUE)
    {
        static const char* names[]={"Mini target","Half target","Full target"};
        display.drawStr(2,29,names[weightTargetMenuSelection]);
        const uint16_t centi=settings.weightTargetsCentiGrams[weightTargetMenuSelection];
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
