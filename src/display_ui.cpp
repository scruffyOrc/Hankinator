#include "app_api.h"

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
