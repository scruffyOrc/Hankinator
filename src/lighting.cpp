#include "app_api.h"

void setPanelLights()
{
    pixels.clear();

    pixels.setPixelColor(
        0,
        pixels.Color(
            180,
            180,
            180
        )
    );

    pixels.setPixelColor(
        1,
        pixels.Color(
            40,
            80,
            180
        )
    );

    pixels.setPixelColor(
        2,
        pixels.Color(
            40,
            80,
            180
        )
    );

    pixels.show();
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
    digitalWrite(
        BEEPER,
        HIGH
    );

    delay(duration);

    digitalWrite(
        BEEPER,
        LOW
    );
}
