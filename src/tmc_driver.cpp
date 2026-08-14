#include <TMCStepper.h>
#include "config.h"
#include "tmc_driver.h"
namespace { TMC2209Stepper driver(&Serial2,Config::TmcRsense,Config::TmcAddress); bool online=false; }
void TmcDriver::begin(){
 pinMode(Pins::Diag,INPUT_PULLDOWN); Serial2.setRX(Pins::TmcRx);Serial2.setTX(Pins::TmcTx);Serial2.begin(115200);
 driver.begin(); online=driver.test_connection()==0;
 if(!online){Serial.println("TMC2209 UART unavailable; standalone motion remains enabled");return;}
 // Preserve the module's hardware current and MS-pin microstep settings.
 driver.pdn_disable(true);driver.mstep_reg_select(false);driver.I_scale_analog(true);
 driver.en_spreadCycle(false);driver.TCOOLTHRS(0xFFFFF);driver.SGTHRS(Config::StallThreshold);
 Serial.println("TMC2209 UART online; StallGuard telemetry enabled (observational only)");
}
bool TmcDriver::connected(){return online;}
uint16_t TmcDriver::sgResult(){return online?driver.SG_RESULT():0xFFFF;}
uint32_t TmcDriver::drvStatus(){return online?driver.DRV_STATUS():0;}
