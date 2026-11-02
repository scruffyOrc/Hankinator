#include "app_api.h"
#include "run_supervisor.h"
#include "firmware_update.h"

void setPanelLights()
{
#if defined(HANKWINDER_TOUCH_DIAGNOSTICS)
    // GP11 is the XPT2046 IRQ on the touchscreen harness.  It is also the
    // normal Mini12864 NeoPixel data pin, so no NeoPixel operation may occur
    // in this build—even after a motion-state change requests a light update.
    return;
#else
    pixels.clear();

    pixels.setPixelColor(
        Config::PanelBacklightPixel,
        pixels.Color(
            180,
            180,
            180
        )
    );

    pixels.setPixelColor(
        Config::PanelEncoderFirstPixel,
        pixels.Color(
            40,
            80,
            180
        )
    );

    pixels.setPixelColor(
        Config::PanelEncoderSecondPixel,
        pixels.Color(
            40,
            80,
            180
        )
    );

    pixels.show();
#endif
}

namespace {
uint32_t lastLightUpdate=0,stopLightStarted=0;
bool stopLightActive=false;
uint8_t pulseLevel(uint32_t now)
{
    const uint32_t phase=now%Config::StatusLedPulseMs;
    const float wave=0.5f-0.5f*cosf(phase*6.2831853f/Config::StatusLedPulseMs);
    return uint8_t(35+wave*145);
}
}
void signalStoppedLights()
{
#if !defined(HANKWINDER_TOUCH_DIAGNOSTICS)
    stopLightStarted=millis();stopLightActive=true;
#endif
}

void updatePanelLights()
{
#if defined(HANKWINDER_TOUCH_DIAGNOSTICS)
    // See setPanelLights(): driving the shared GP11 output would steal the
    // touchscreen interrupt exactly when winding state changes.
    return;
#else
    const uint32_t now=millis();
    if(now-lastLightUpdate<Config::StatusLedRefreshMs||!pixels.canShow())return;
    lastLightUpdate=now;
    const uint8_t level=pulseLevel(now);
    uint32_t first=pixels.Color(0,150,0),second=first;
    const bool fault=uiState==UiState::HALL_FAILED||(uiState==UiState::WINDING&&RunSupervisor::waitingForClearance())||uiState==UiState::LOAD_CELL_FAULT||uiState==UiState::TARE_FAILED
        ||(uiState==UiState::COMPLETE&&finalRunAborted)
        ||(uiState==UiState::CONFIG_FIRMWARE_UPDATE&&FirmwareUpdate::status()==FirmwareUpdateStatus::Error);
    if(stopLightActive&&now-stopLightStarted>=Config::StatusLedStopMs)stopLightActive=false;
    if(fault)first=second=pixels.Color(level,0,0);
    else if(stopLightActive)first=second=pixels.Color(180,85,0);
    else if(uiState==UiState::TARING||uiState==UiState::HALL_SEEK)first=second=pixels.Color(0,0,level);
    else if(uiState==UiState::LOAD_YARN||uiState==UiState::COMPLETE||uiState==UiState::REPEAT_PROMPT)
        first=second=pixels.Color(0,level,0);
    else if(uiState==UiState::WINDING)
    {
        if(RunSupervisor::completionPending())first=second=pixels.Color(0,0,level);
        else if(RunSupervisor::snapshot().recovery==StallRecovery::Phase::Slowing||RunSupervisor::snapshot().recovery==StallRecovery::Phase::Proving||RunSupervisor::snapshot().recovery==StallRecovery::Phase::Locked)
            first=second=pixels.Color(180,85,0);
        else if(pauseState==PauseState::PAUSED)first=second=pixels.Color(level,level/2,0);
        else if(pauseState==PauseState::RAMPING_DOWN)first=second=pixels.Color(180,85,0);
        else if(pauseState==PauseState::RAMPING_UP)
        {
            const float target=normalRequestedMotorRPS();
            const float span=target-Config::RESUME_START_MOTOR_RPS;
            const float fraction=span>0.0f?constrain((currentMotorRPS-Config::RESUME_START_MOTOR_RPS)/span,0.0f,1.0f):1.0f;
            first=second=pixels.Color(uint8_t(180*(1-fraction)),uint8_t(85*(1-fraction)),uint8_t(180*fraction));
        }
        else
        {
            uint16_t hue=uint32_t((now%Config::StatusLedRainbowMs)*65536UL/Config::StatusLedRainbowMs);
            if(!settings.clockwise)hue=uint16_t(0-hue);
            // Keep mixed RGB components visible: gamma correction at low
            // brightness can make intermediate hues look like primaries.
            first=pixels.ColorHSV(hue,200,180);
            second=pixels.ColorHSV(uint16_t(hue+21845),200,180);
        }
    }
    else
    {
        switch(uiState)
        {
            case UiState::CONFIG_MENU:case UiState::CONFIG_CONNECTIVITY_MENU:case UiState::CONFIG_MACHINE_SETTINGS_MENU:case UiState::CONFIG_DIAGNOSTICS_MENU:case UiState::CONFIG_MOTOR_MENU:
            case UiState::CONFIG_RUN_CURRENT:case UiState::CONFIG_HOLD_CURRENT:
            case UiState::CONFIG_DIRECTION:case UiState::CONFIG_WEIGHT_MENU:
            case UiState::CONFIG_WEIGHT_VALUE:case UiState::CONFIG_PAIRING:
            case UiState::CONFIG_LOAD_CELL_DIAGNOSTICS:case UiState::CONFIG_FIRMWARE_UPDATE:
                first=second=pixels.Color(100,0,150);break;
            default:break;
        }
    }
    const uint32_t backlight=pixels.Color(180,180,180);
    if(pixels.getPixelColor(Config::PanelBacklightPixel)==backlight&&pixels.getPixelColor(Config::PanelEncoderFirstPixel)==first&&pixels.getPixelColor(Config::PanelEncoderSecondPixel)==second)return;
    pixels.setPixelColor(Config::PanelBacklightPixel,backlight);pixels.setPixelColor(Config::PanelEncoderFirstPixel,first);pixels.setPixelColor(Config::PanelEncoderSecondPixel,second);
    // The Pico NeoPixel backend uses PIO; no display redraw or delay is needed.
    pixels.show();
#endif
}

void drawStartupSplash()
{
    display.clearBuffer();

    display.setFont(
        u8g2_font_ncenB24_tr
    );

    const char* text =
        "Behold!";

    int width =
        display.getStrWidth(
            text
        );

    display.drawStr(
        (128 - width) / 2,
        43,
        text
    );

    display.sendBuffer();
}

void runStartupSplash()
{
    drawStartupSplash();

    uint32_t startTime =
        millis();

    while (
        millis() - startTime <
        Config::SPLASH_TIME_MS
    )
    {
        uint32_t elapsed =
            millis() -
            startTime;

        uint16_t baseHue =
            (uint16_t)(
                (
                    elapsed *
                    65536UL
                ) /
                Config::RAINBOW_CYCLE_MS
            );

        for (
            uint8_t i = 0;
            i < NEOPIXEL_COUNT;
            i++
        )
        {
            uint16_t hue =
                baseHue +
                (uint16_t)(
                    i *
                    (
                        65536UL /
                        NEOPIXEL_COUNT
                    )
                );

            uint32_t color =
                pixels.gamma32(
                    pixels.ColorHSV(
                        hue,
                        255,
                        180
                    )
                );

            pixels.setPixelColor(
                i,
                color
            );
        }

        pixels.show();

        delay(20);
    }

    setPanelLights();
}

void beep(
    uint16_t duration
)
{
#if defined(HANKWINDER_TOUCH_DIAGNOSTICS)
    // GP12 is SPI1 MISO for the XPT2046 touch controller in this build.  Do
    // not even write the inactive beeper level here: Back, Pause, and Resume
    // all request a beep and a GPIO write can disrupt the touch SPI pin mux.
    (void)duration;
    return;
#else
    if(!Config::EnableBeeper)
    {
        // Hold the output at its inactive level even if a prior tone was
        // interrupted.  This affects sound only; all UI and fault actions
        // that request a beep still execute normally.
        digitalWrite(BEEPER,LOW);
        return;
    }

    digitalWrite(
        BEEPER,
        HIGH
    );

    delay(duration);

    digitalWrite(
        BEEPER,
        LOW
    );
#endif
}
