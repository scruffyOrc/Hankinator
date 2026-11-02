#include "app_api.h"
#include "run_supervisor.h"
#include "load_cells.h"
#include "tmc_driver.h"
#include <cassert>
#include <string>
#include <vector>
bool weightCapabilityEnabled=true,autoCountMode=true,motionActive=false;
uint32_t targetStepCount=0,motionSegmentStopStep=UINT32_MAX,requestedStepRateHz=0;
float currentMotorRPS=3,selectedCruiseMotorRPS=3;
PauseState pauseState=PauseState::RUNNING;
uint32_t clockMs=0,steps=0,seq=0;int auxTicks=0,reverseCalls=0,forwardCalls=0;
bool auxDone=false,disabled=false;float measured=5;
TmcDriver::Sample reading{300,1UL<<30,true,0,true};
bool cachedDriverStatusAvailable=false;uint32_t cachedDriverStatus=0;
std::vector<std::string> events;
uint32_t millis(){return clockMs;}
uint32_t safeCurrentStepCount(){return steps;}
bool auxiliaryMotionActive(){return auxTicks>0;}
bool startAuxiliaryHubMotion(bool forward,uint32_t,float){assert(!motionActive);if(forward)forwardCalls++;else reverseCalls++;auxTicks=2;motionActive=true;return true;}
bool consumeAuxiliaryMotionComplete(){bool result=auxDone;auxDone=false;return result;}
void cancelAuxiliaryMotion(){auxTicks=0;auxDone=false;motionActive=false;}
void requestStepGeneratorWake(){}
uint32_t stepsPerSecondForMotorRPS(float speed){return uint32_t(speed*Config::StepsPerMotorRev);}
void disableMotor(){disabled=true;}
namespace LoadCells {LoadCellSnapshot snapshot(){return {seq,15};}float stoppedWeightGrams(const LoadCellSnapshot&){return measured;}bool tareValid(){return true;}}
namespace TmcDriver {Sample sample(){return reading;}bool consumeDiag(){return false;}bool hasCachedDrvStatus(){return cachedDriverStatusAvailable;}uint32_t cachedDrvStatus(){return cachedDriverStatus;}}
namespace Telemetry {void event(const char* e){events.emplace_back(e);}}
void reset(bool weight=false){
    clockMs+=1000;steps=0;seq=0;auxTicks=reverseCalls=forwardCalls=0;auxDone=disabled=false;events.clear();
    reading={300,1UL<<30,true,0,true};cachedDriverStatusAvailable=false;cachedDriverStatus=0;autoCountMode=!weight;pauseState=PauseState::RUNNING;
    motionActive=true;targetStepCount=100*Config::StepsPerHubRev;currentMotorRPS=selectedCruiseMotorRPS=3;
    if(weight)RunSupervisor::armWeightTarget(20.2f);else RunSupervisor::disarmWeightTarget();
    RunSupervisor::beginRun();
}
void tick(uint16_t sg=300){
    clockMs+=50;seq++;reading.sg=sg;reading.sequence++;
    if(auxTicks>0){if(--auxTicks==0){motionActive=false;auxDone=true;}}
    else if(motionActive){steps+=uint32_t(currentMotorRPS*Config::StepsPerMotorRev*.05f);if(steps>=min(targetStepCount,motionSegmentStopStep)){steps=min(targetStepCount,motionSegmentStopStep);motionActive=false;}}
    RunSupervisor::service();
    if(!auxiliaryMotionActive())currentMotorRPS=RunSupervisor::limitMotorRps(3);
}
void hold(){for(int i=0;i<5;i++)tick();tick(0);tick(0);assert(RunSupervisor::waitingForClearance());assert(!motionActive);}
void waitForTensionReliefStart(){for(int i=0;i<70&&reverseCalls==0;i++)tick();assert(reverseCalls==1);}
int main(){
    reset();hold();for(int i=0;i<20;i++)tick();assert(RunSupervisor::completionPending());assert(reverseCalls==0);
    assert(RunSupervisor::snapshot().clearanceReason==ClearanceReason::Load);
    // Numeric StallGuard may be unavailable even with a fresh DRV_STATUS.
    // Only a missing status reply blocks the user-confirmed resume.
    reading.valid=false;reading.statusValid=false;assert(!RunSupervisor::confirmClearance());
    assert(RunSupervisor::snapshot().clearanceReason==ClearanceReason::DriverData);reading.statusValid=true;
    assert(RunSupervisor::confirmClearance());assert(motionActive);assert(RunSupervisor::snapshot().recovery==StallRecovery::Phase::Locked);
    assert(RunSupervisor::snapshot().clearanceReason==ClearanceReason::None);
    assert(reverseCalls==0);assert(currentMotorRPS<=Config::LaunchMotorRps);
    // A user may inspect a Hall fault longer than the live-status freshness
    // window. A last-known healthy status from this boot remains adequate.
    reset();hold();reading.statusValid=false;cachedDriverStatusAvailable=true;cachedDriverStatus=1UL<<30;
    assert(RunSupervisor::confirmClearance());
    reset(true);hold();assert(reverseCalls==0);assert(RunSupervisor::confirmClearance());assert(reverseCalls==0);waitForTensionReliefStart();
    for(int i=0;i<180&&forwardCalls==0;i++)tick();
    tick();tick();assert(forwardCalls==1);assert(RunSupervisor::snapshot().plannedStageTurns==1);
    assert(isnan(RunSupervisor::snapshot().gramsPerTurn));assert(motionActive);
    reset();reading.status=0x40000002;tick();assert(!motionActive);assert(disabled);assert(RunSupervisor::completionIsAbort());assert(!RunSupervisor::completionPending());
    reset();hold();assert(RunSupervisor::snapshot().countUncertain);reset();assert(!RunSupervisor::snapshot().countUncertain);
    reset();motionActive=false;steps=targetStepCount;assert(RunSupervisor::requestFinalMeasurement());
    for(int i=0;i<180&&!RunSupervisor::snapshot().finalWeightMeasured;i++)tick();
    assert(RunSupervisor::snapshot().finalWeightMeasured);assert(!RunSupervisor::completionPending());assert(!RunSupervisor::requestFinalMeasurement());
    assert(reverseCalls==1&&forwardCalls==0);assert(steps==targetStepCount);
    reset();pauseState=PauseState::PAUSED;motionActive=false;steps=37*Config::StepsPerHubRev;
    assert(RunSupervisor::requestFinalMeasurement());
    for(int i=0;i<180&&!RunSupervisor::snapshot().finalWeightMeasured;i++)tick();
    assert(RunSupervisor::snapshot().finalWeightMeasured);assert(!RunSupervisor::completionIsAbort());
    assert(pauseState==PauseState::RUNNING);assert(reverseCalls==1&&forwardCalls==0);
    reset();reading.valid=false;reading.statusValid=false;RunSupervisor::beginRun();assert(RunSupervisor::waitingForClearance());assert(!RunSupervisor::confirmClearance());
    assert(RunSupervisor::snapshot().clearanceReason==ClearanceReason::DriverData);
    // Once a healthy DRV_STATUS has been seen, an intermittent runtime poll
    // miss must not fabricate a driver-data hold while the Hall feedback is
    // still supervising actual hub motion.
    reset();reading.valid=false;reading.statusValid=false;cachedDriverStatusAvailable=true;cachedDriverStatus=1UL<<30;
    for(int i=0;i<12;i++)tick();
    assert(!RunSupervisor::waitingForClearance()&&motionActive);
    reading.statusValid=true;reading.status=0x40000002;tick();assert(RunSupervisor::completionIsAbort());assert(disabled);
    reset();RunSupervisor::holdForHallFault("HALL_TEST");assert(RunSupervisor::waitingForClearance()&&!motionActive);
    assert(RunSupervisor::confirmClearance());RunSupervisor::holdForHallFault("HALL_TEST");
    assert(RunSupervisor::confirmClearance());RunSupervisor::holdForHallFault("HALL_TEST");
    assert(RunSupervisor::completionIsAbort()&&disabled&&!RunSupervisor::completionPending());
    reset(true);RunSupervisor::holdForHallFault("HALL_TEST");assert(reverseCalls==0);
    assert(RunSupervisor::confirmClearance());assert(reverseCalls==0);waitForTensionReliefStart();
    // The calibration probe keeps 1.5 RPS until its final two turns, then
    // reaches the shared stopped-measurement boundary at 0.5 RPS.
    reset(true);steps=6*Config::StepsPerHubRev;assert(fabsf(RunSupervisor::limitMotorRps(3.0f)-1.5f)<0.01f);
    steps=7*Config::StepsPerHubRev;assert(fabsf(RunSupervisor::limitMotorRps(3.0f)-1.0f)<0.01f);
    std::puts("8 supervisor integration scenarios passed");
}
