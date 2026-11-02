#include <TMCStepper.h>
#include "config.h"
#include "runtime.h"
#include "tmc_driver.h"
#include "tmc_uart_frame.h"
namespace {
TMC2209Stepper driver(&Serial2,Config::TmcRsense,Config::TmcAddress);
bool online=false,pending=false,gotSg=false,gotStatus=false,statusSeen=false;
uint8_t readRegister=0x41,nextRegister=0x41;
TmcUart::Frame frame;
uint32_t requestStarted=0,lastRequest=0,sgAt=0,statusAt=0;
uint32_t sgSequence=0;
uint16_t cachedSg=0xFFFF;uint32_t cachedStatus=0;
volatile bool diagSeen=false;
void onDiag(){diagSeen=true;}
}
void TmcDriver::applyCurrent(uint16_t runMa,uint16_t holdMa){if(!online)return;pending=false;gotSg=gotStatus=false;float multiplier=constrain(holdMa/float(runMa),0.0f,1.0f);driver.I_scale_analog(false);driver.rms_current(runMa,multiplier);}
void TmcDriver::begin(){
 pinMode(Pins::Diag,INPUT_PULLDOWN); Serial2.setRX(Pins::TmcRx);Serial2.setTX(Pins::TmcTx);Serial2.begin(115200);
 driver.begin(); online=driver.test_connection()==0;
 if(!online){Serial.println("TMC2209 UART unavailable; restore wiring and restart before winding");return;}
 // Preserve the MS-pin microstep setting while moving current control to UART.
 driver.pdn_disable(true);driver.mstep_reg_select(false);applyCurrent(settings.runCurrentMa,settings.holdCurrentMa);
 attachInterrupt(digitalPinToInterrupt(Pins::Diag),onDiag,RISING);
 driver.TPWMTHRS(0);driver.en_spreadCycle(false);driver.TCOOLTHRS(0xFFFFF);driver.SGTHRS(Config::StallThreshold);
 Serial.print("TMC2209 UART online; current ");Serial.print(settings.runCurrentMa);Serial.print("/");Serial.print(settings.holdCurrentMa);Serial.println(" mA run/hold; adaptive StallGuard recovery enabled");
}
bool TmcDriver::connected(){return online;}
uint16_t TmcDriver::sgResult(){return sample().valid?cachedSg:0xFFFF;}
uint32_t TmcDriver::drvStatus(){return gotStatus?cachedStatus:0;}
bool TmcDriver::hasCachedDrvStatus(){return online&&statusSeen;}
uint32_t TmcDriver::cachedDrvStatus(){return cachedStatus;}

// Runtime reads are asynchronous. TMCStepper owns initialization/configuration
// only; telemetry shares this cache and never starts competing UART reads.
void TmcDriver::poll() {
    if(!online)return;
    const uint32_t now=millis();
    uint16_t budget=64;
    while(Serial2.available()&&budget--) {
        const uint8_t byte=uint8_t(Serial2.read());
        if(!pending)continue;
        uint32_t value=0;
        if(frame.push(byte,readRegister,value)) {
            if(readRegister==0x41){gotSg=value<=510;cachedSg=gotSg?uint16_t(value):0xFFFF;sgAt=now;sgSequence++;}
            else {cachedStatus=value;gotStatus=true;statusSeen=true;statusAt=now;}
            pending=false;frame.reset();
        }
    }
    if(pending&&now-requestStarted>=Config::StallUartTimeoutMs) {
        if(readRegister==0x41)gotSg=false;else gotStatus=false;
        pending=false;frame.reset();
    }
    // Arduino-Pico SerialUART reports readiness as 0/1, not FIFO free bytes.
    // We are the only runtime writer, with one four-byte request outstanding;
    // after the request interval the previous TX has long since drained.
    if(!pending&&now-lastRequest>=Config::StallUartRequestMs&&Serial2.availableForWrite()>0) {
        readRegister=nextRegister;nextRegister=nextRegister==0x41?0x6F:0x41;
        uint8_t request[]={5,Config::TmcAddress,readRegister,0};request[3]=TmcUart::crc(request,3);
        frame.reset();pending=true;requestStarted=lastRequest=now;
        Serial2.write(request,sizeof(request));
    }
}
TmcDriver::Sample TmcDriver::sample() {
    const uint32_t now=millis();
    const bool statusValid=online&&gotStatus&&now-statusAt<=Config::StallDataFreshMs;
    return {cachedSg,cachedStatus,statusValid&&gotSg&&now-sgAt<=Config::StallDataFreshMs,sgSequence,statusValid};
}
bool TmcDriver::consumeDiag() {
    noInterrupts();const bool seen=diagSeen;diagSeen=false;interrupts();
    return seen||digitalRead(Pins::Diag)==HIGH;
}
