#include "app_api.h"

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
