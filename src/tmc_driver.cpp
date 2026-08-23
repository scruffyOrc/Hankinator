#include <TMCStepper.h>
#include "config.h"
#include "runtime.h"
#include "tmc_driver.h"
namespace { TMC2209Stepper driver(&Serial2,Config::TmcRsense,Config::TmcAddress); bool online=false; }
void TmcDriver::applyCurrent(uint16_t runMa,uint16_t holdMa){if(!online)return;float multiplier=constrain(holdMa/float(runMa),0.0f,1.0f);driver.I_scale_analog(false);driver.rms_current(runMa,multiplier);}
void TmcDriver::begin(){
 pinMode(Pins::Diag,INPUT_PULLDOWN); Serial2.setRX(Pins::TmcRx);Serial2.setTX(Pins::TmcTx);Serial2.begin(115200);
 driver.begin(); online=driver.test_connection()==0;
 if(!online){Serial.println("TMC2209 UART unavailable; standalone motion remains enabled");return;}
 // Preserve the MS-pin microstep setting while moving current control to UART.
 driver.pdn_disable(true);driver.mstep_reg_select(false);applyCurrent(settings.runCurrentMa,settings.holdCurrentMa);
 driver.en_spreadCycle(false);driver.TCOOLTHRS(0xFFFFF);driver.SGTHRS(Config::StallThreshold);
 Serial.print("TMC2209 UART online; current ");Serial.print(settings.runCurrentMa);Serial.print("/");Serial.print(settings.holdCurrentMa);Serial.println(" mA run/hold; StallGuard observational only");
}
bool TmcDriver::connected(){return online;}
uint16_t TmcDriver::sgResult(){return online?driver.SG_RESULT():0xFFFF;}
uint32_t TmcDriver::drvStatus(){return online?driver.DRV_STATUS():0;}
