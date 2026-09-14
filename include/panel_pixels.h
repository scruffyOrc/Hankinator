#pragma once
#include <Adafruit_NeoPixel.h>

// Use the library's color buffer and PIO program, but leave timer IRQs enabled.
class PanelPixels : public Adafruit_NeoPixel {
public:
    using Adafruit_NeoPixel::Adafruit_NeoPixel;
    void begin() {
        if(ready)return;
        if(!pio_claim_free_sm_and_add_program_for_gpio_range(&ws2812_program,&panelPio,&panelSm,&offset,pin,1,true))return;
        ws2812_program_init(panelPio,panelSm,offset,pin,800000,8);
        ready=true;
    }
    void show() {
        if(!ready||!canShow())return;
        for(uint16_t i=0;i<numBytes;i++)pio_sm_put_blocking(panelPio,panelSm,uint32_t(getPixels()[i])<<24);
        endTime=micros();
    }
private:
    PIO panelPio=nullptr;
    uint panelSm=0,offset=0;
    bool ready=false;
};
