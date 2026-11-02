#pragma once
#include "Arduino.h"
struct TMC2209Stepper {
    TMC2209Stepper(FakeUart*,float,uint8_t){}
    void begin(){} int test_connection(){return 0;}
    void I_scale_analog(bool){} void rms_current(uint16_t,float){}
    void pdn_disable(bool){} void mstep_reg_select(bool){}
    void TPWMTHRS(uint32_t){} void en_spreadCycle(bool){}
    void TCOOLTHRS(uint32_t){} void SGTHRS(uint8_t){}
};
