#include "app_api.h"
#include "hall_sensor.h"
#include "run_supervisor.h"
#include <cassert>
#include <string>
#include <vector>
HallTestSettings settings;
bool motionActive=false,autoCountMode=true,weightCapabilityEnabled=true,aux=false,motorEnabled=false;
uint32_t currentStepCount=0,targetStepCount=640000,motionSegmentStopStep=UINT32_MAX,requestedStepRateHz=0;
float currentMotorRPS=0,selectedCruiseMotorRPS=3;
PauseState pauseState=PauseState::RUNNING;
uint32_t clockUs=0;int input=HIGH,holds=0;void(*irq)()=nullptr;
std::vector<std::string> events;
uint32_t millis(){return clockUs/1000;}
uint32_t micros(){return clockUs;}
void pinMode(int,int){}
int digitalRead(int){return input;}
void attachInterrupt(int,void(*fn)(),int){irq=fn;}
uint32_t safeCurrentStepCount(){return currentStepCount;}
bool auxiliaryMotionActive(){return aux;}
uint32_t auxiliaryStepsTravelled(){return currentStepCount;}
bool startAuxiliaryHubMotion(bool,uint32_t,float){aux=true;motionActive=true;return true;}
void cancelAuxiliaryMotion(){aux=false;motionActive=false;}
int serviceStepGeneratorWake(){return 0;}
void enableMotor(){motorEnabled=true;}
void disableMotor(){motorEnabled=false;}
namespace Telemetry {void event(const char* e){events.emplace_back(e);}}
namespace RunSupervisor {void holdForHallFault(const char*){holds++;motionActive=false;}}
void edge(int level){clockUs+=2000;input=level;irq();}
void pulse(uint32_t step){currentStepCount=step;edge(LOW);edge(HIGH);HallSensor::service();}
void seek(){HallSensor::startSeek();edge(LOW);edge(HIGH);HallSensor::service();assert(HallSensor::seekFinished()&&!HallSensor::seekFailed());assert(!motionActive&&!aux);}
void run(){currentStepCount=0;HallSensor::beginRun();motionActive=true;pauseState=PauseState::RUNNING;}
int main(){
    HallSensor::begin();HallSensor::startSeek();assert(HallSensor::state()==HallSensor::State::Disabled);assert(!aux);
    settings.hallEnabled=1;HallSensor::startSeek();assert(aux);motionActive=false;aux=false;HallSensor::service();
    assert(HallSensor::state()==HallSensor::State::Absent&&!HallSensor::seekFailed());
    HallSensor::beginRun();motionActive=true;currentStepCount=30000;HallSensor::service();assert(holds==0);HallSensor::endRun();motionActive=false;
    seek();assert(motorEnabled);run();pulse(0);
    const uint32_t gaps[]={1422,1600,1600,1422,356};uint32_t step=0;
    for(int c=0;c<2;c++)for(auto gap:gaps){step+=gap;pulse(step);}
    assert(HallSensor::state()==HallSensor::State::Locked);assert(HallSensor::pulseCount()==11);
    step+=1522;pulse(step);assert(holds==0);step+=1600;pulse(step);
    assert(holds==1&&!motionActive);assert(targetStepCount==640036&&HallSensor::correctionSteps()==36);
    HallSensor::service();assert(holds==1);
    HallSensor::resumeAfterClearance();motionActive=true;currentStepCount+=Config::HallSeekMaxSteps+1;HallSensor::service();assert(holds==2);
    HallSensor::endRun();motionActive=false;
    HallSensor::startSeek();motionActive=false;aux=false;HallSensor::service();assert(HallSensor::seekFailed());
    HallSensor::cancel();input=LOW;HallSensor::startSeek();edge(HIGH);HallSensor::service();assert(!HallSensor::seekFinished());
    edge(LOW);edge(HIGH);HallSensor::service();assert(HallSensor::seekFinished());
    run();aux=true;for(int i=0;i<10;i++)pulse(0);aux=false;HallSensor::service();assert(HallSensor::pulseCount()==0);
    pauseState=PauseState::PAUSED;pulse(0);assert(HallSensor::pulseCount()==0);
    HallSensor::endRun();motionActive=false;input=HIGH;HallSensor::startSeek();edge(LOW);clockUs+=1100;HallSensor::service();
    assert(HallSensor::seekFinished()&&!HallSensor::seekFailed()&&!motionActive); // stop at stable entry, not magnet exit
    settings.hallEnabled=0;HallSensor::service();assert(HallSensor::state()==HallSensor::State::Disabled);
    std::puts("11 Hall hardware-integration scenarios passed");
}
