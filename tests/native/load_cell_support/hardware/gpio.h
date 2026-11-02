#pragma once
#include <stdint.h>
constexpr int GPIO_DRIVE_STRENGTH_2MA=2,GPIO_SLEW_RATE_SLOW=0;
inline void gpio_set_drive_strength(int,int){}
inline void gpio_set_slew_rate(int,int){}
void gpio_put(int,bool);
uint32_t gpio_get_all();
