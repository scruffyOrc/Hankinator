#include "app_api.h"

#include "pico/stdlib.h"

namespace {
volatile bool stepWakeRequested=false,stepWakeAwaitingFirst=false,stepWakeFirstReady=false;
volatile uint32_t stepWakeRequestedUs=0,stepWakeFirstDelayUs=0,stepWakeFirstStep=0;
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

    if(stepWakeAwaitingFirst)
    {
        stepWakeFirstDelayUs=time_us_32()-stepWakeRequestedUs;
        stepWakeFirstStep=currentStepCount;
        stepWakeAwaitingFirst=false;
        stepWakeFirstReady=true;
    }

    if(currentStepCount>=motionSegmentStopStep)
    {
        motionActive=false;
        return -(int64_t)Config::IDLE_TIMER_US;
    }

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

void requestStepGeneratorWake(){stepWakeRequested=true;}

int serviceStepGeneratorWake()
{
    if(!stepWakeRequested||!motionActive||requestedStepRateHz<1)return 0;
    stepWakeRequested=false;
    const bool cancelled=cancel_alarm(stepAlarmId);
    stepWakeRequestedUs=time_us_32();stepWakeAwaitingFirst=true;stepWakeFirstReady=false;
    const alarm_id_t replacement=add_alarm_in_us(1,stepAlarmCallback,nullptr,true);
    if(replacement<0)
    {
        stepWakeAwaitingFirst=false;
        return -1;
    }
    stepAlarmId=replacement;
    return cancelled?1:2;
}

bool consumeFirstStepAfterWake(uint32_t& delayUs,uint32_t& step)
{
    if(!stepWakeFirstReady)return false;
    noInterrupts();delayUs=stepWakeFirstDelayUs;step=stepWakeFirstStep;stepWakeFirstReady=false;interrupts();
    return true;
}

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

float startupProfileMotorRPS()
{
    const float runStartRps=min(Config::START_MOTOR_RPS,selectedCruiseMotorRPS);
    const uint32_t launchSteps=uint32_t(Config::LAUNCH_RAMP_TURNS*Config::STEPS_PER_HUB_REV);
    const uint32_t completed=safeCurrentStepCount();
    if(completed<launchSteps)
    {
        float progress=launchSteps?completed/float(launchSteps):1.0f;
        progress=constrain(progress,0.0f,1.0f);
        const float eased=progress*progress*(3.0f-2.0f*progress);
        return min(selectedCruiseMotorRPS,Config::LAUNCH_MOTOR_RPS+(runStartRps-Config::LAUNCH_MOTOR_RPS)*eased);
    }

    const uint32_t elapsed=activeWindingElapsedMs();
    if(launchCompleteElapsedMs==UINT32_MAX)launchCompleteElapsedMs=elapsed;
    const uint32_t elapsedAfterLaunch=elapsed-launchCompleteElapsedMs;

    if (
        elapsedAfterLaunch <=
        Config::START_HOLD_MS
    )
    {
        return runStartRps;
    }

    uint32_t rampElapsed =
        elapsedAfterLaunch -
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
            runStartRps +
            (
                selectedCruiseMotorRPS -
                runStartRps
            ) *
            progress;
    }

    return
        selectedCruiseMotorRPS;
}

MotionPhase currentMotionPhase()
{
    if(pauseState==PauseState::RAMPING_DOWN)return MotionPhase::PAUSE_DOWN;
    if(pauseState==PauseState::PAUSED)return MotionPhase::PAUSED;
    if(pauseState==PauseState::RAMPING_UP)return MotionPhase::RESUME;
    if(turnsCompleted()<Config::LAUNCH_RAMP_TURNS)return MotionPhase::LAUNCH;
    if(turnsRemaining()<Config::DECEL_TURNS)return MotionPhase::DECEL;
    const uint32_t elapsed=activeWindingElapsedMs();
    const uint32_t afterLaunch=launchCompleteElapsedMs==UINT32_MAX?0:elapsed-launchCompleteElapsedMs;
    if(afterLaunch<=Config::START_HOLD_MS)return MotionPhase::START_HOLD;
    if(afterLaunch<Config::START_HOLD_MS+Config::ACCEL_RAMP_MS)return MotionPhase::ACCEL;
    return MotionPhase::CRUISE;
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
            selectedCruiseMotorRPS;
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
        selectedCruiseMotorRPS -
        (
            selectedCruiseMotorRPS -
            min(Config::END_MOTOR_RPS,selectedCruiseMotorRPS)
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

    // Stop one STEP before an automatic weight-measurement boundary so a
    // user pause cannot collide with, and be mistaken for, the batch stop.
    if(motionSegmentStopStep!=UINT32_MAX&&motionSegmentStopStep>pauseRampStartStep&&requestedStopStep>=motionSegmentStopStep)
        requestedStopStep=motionSegmentStopStep-1;

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

    requestStepGeneratorWake();

    beep(30);

    Serial.println(
        "Resuming - ramping up"
    );
}
