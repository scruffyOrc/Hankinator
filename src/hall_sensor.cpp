#include "hall_sensor.h"
#include "hall_tracker.h"
#include "app_api.h"
#include "telemetry.h"
#include "run_supervisor.h"

namespace {
struct Edge {uint32_t step;bool production;bool auxiliary;};
constexpr uint8_t QueueSize=16;
volatile Edge queue[QueueSize];
volatile uint8_t head=0,tail=0;
volatile bool overflow=false;
volatile uint32_t lastChangeUs=0;
volatile bool released=false;
volatile bool candidate=false,candidateProduction=false;
volatile bool candidateAuxiliary=false;
volatile uint32_t candidateStep=0;
HallTracking::Tracker tracker(Config::StepsPerHubRev,Config::HallPairGapDegrees,Config::HallPositionToleranceSteps);
HallSensor::State current=HallSensor::State::Disabled;
bool running=false,seekDone=false,seekError=false,seenThisBoot=false,wasAuxiliary=false;
bool commissioningIndexDone=false,commissioningIndexError=false,commissioningHavePreviousEdge=false;
uint32_t seekStarted=0,acquireStartedStep=0,totalCorrection=0,totalPulses=0,lastReport=0,commissioningPreviousEdge=0;
void edgeInterrupt() {
    if(!settings.hallEnabled)return;
    const uint32_t now=micros();const bool high=digitalRead(Pins::Hall)!=LOW;
    if(!high) {
        candidate=released&&now-lastChangeUs>=Config::HallDebounceUs;
        candidateAuxiliary=auxiliaryMotionActive();
        candidateStep=candidateAuxiliary?auxiliaryStepsTravelled():currentStepCount;
        candidateProduction=motionActive&&!auxiliaryMotionActive()&&pauseState!=PauseState::PAUSED;
        released=false;lastChangeUs=now;return;
    }
    const bool valid=candidate&&now-lastChangeUs>=Config::HallDebounceUs;
    candidate=false;released=true;lastChangeUs=now;
    if(!valid)return;
    const uint8_t next=(head+1)%QueueSize;
    if(next==tail){overflow=true;return;}
    queue[head].step=candidateStep;
    queue[head].production=candidateProduction;
    queue[head].auxiliary=candidateAuxiliary;
    head=next;
}
void flush() {noInterrupts();tail=head;overflow=false;candidate=false;candidateAuxiliary=false;released=digitalRead(Pins::Hall)!=LOW;lastChangeUs=micros();interrupts();}
void fail(const char* reason) {
    current=HallSensor::State::Fault;
    if(running)RunSupervisor::holdForHallFault(reason);
    else {cancelAuxiliaryMotion();requestedStepRateHz=0;disableMotor();seekError=true;seekDone=true;Telemetry::event(reason);}
}
void reacquire() {tracker.reset(safeCurrentStepCount());acquireStartedStep=safeCurrentStepCount();current=HallSensor::State::Acquiring;flush();}
}
void HallSensor::begin() {pinMode(Pins::Hall,INPUT_PULLUP);flush();attachInterrupt(digitalPinToInterrupt(Pins::Hall),edgeInterrupt,CHANGE);}
void HallSensor::startSeek(const bool forward) {
    running=false;seekDone=false;seekError=false;totalCorrection=0;tracker.reset();flush();
    if(!settings.hallEnabled){current=State::Disabled;seekDone=true;return;}
    current=State::Seeking;seekStarted=millis();pauseState=PauseState::RUNNING;enableMotor();
    if(!startAuxiliaryHubMotion(forward,Config::HallSeekMaxSteps,Config::HallSeekMotorRps)){fail("HALL_SEEK_START_FAILED");return;}
    Telemetry::event("HALL_SEEK_START");
}
void HallSensor::startCommissioningIndex() {
    running=false;seekDone=false;seekError=false;commissioningIndexDone=false;commissioningIndexError=false;commissioningHavePreviousEdge=false;
    flush();
    if(!settings.hallEnabled){current=State::Disabled;commissioningIndexError=true;return;}
    current=State::CommissioningSeeking;seekStarted=millis();pauseState=PauseState::RUNNING;enableMotor();
    if(!startAuxiliaryHubMotion(true,Config::StepsPerHubRev*2,Config::CommissioningIndexMotorRps))
    {
        commissioningIndexError=true;fail("COMMISSION_INDEX_START_FAILED");return;
    }
    Telemetry::event("COMMISSION_INDEX_START");
}
void HallSensor::cancel(){running=false;seekDone=false;seekError=false;commissioningIndexDone=false;commissioningIndexError=false;current=settings.hallEnabled?State::Absent:State::Disabled;flush();}
void HallSensor::service() {
    if(!settings.hallEnabled){current=State::Disabled;running=false;flush();return;}
    if(current==State::Seeking) {
        serviceStepGeneratorWake();
        // Stop on the first stable active edge, without waiting to cross the
        // entire magnet. A pulse completed between loop visits is also valid.
        if(head!=tail||(candidate&&uint32_t(micros()-lastChangeUs)>=Config::HallDebounceUs)) {
            cancelAuxiliaryMotion();requestedStepRateHz=0;currentMotorRPS=0;
            seenThisBoot=true;seekDone=true;current=State::Acquiring;flush();
            Telemetry::event("HALL_FIRST_MAGNET_FOUND");return;
        }
        if(!motionActive||millis()-seekStarted>=Config::HallSeekTimeoutMs) {
            cancelAuxiliaryMotion();requestedStepRateHz=0;currentMotorRPS=0;
            // No response ever seen: optional hardware fallback. Once established,
            // missing feedback or a stuck-active input is a fault, never fallback.
            if(seenThisBoot||activeLevel()){fail("HALL_SEEK_NO_RESPONSE");return;}
            current=State::Absent;seekDone=true;Telemetry::event("HALL_ABSENT_FALLBACK");
        }
        return;
    }
    if(current==State::CommissioningSeeking)
    {
        serviceStepGeneratorWake();
        if(overflow){commissioningIndexError=true;fail("COMMISSION_INDEX_EDGE_OVERFLOW");return;}
        while(tail!=head)
        {
            noInterrupts();const Edge e{queue[tail].step,queue[tail].production,queue[tail].auxiliary};tail=(tail+1)%QueueSize;interrupts();
            if(!e.auxiliary)continue;
            if(commissioningHavePreviousEdge&&e.step-commissioningPreviousEdge<Config::StepsPerHubRev/8)
            {
                cancelAuxiliaryMotion();requestedStepRateHz=0;currentMotorRPS=0;
                commissioningIndexDone=true;current=State::CommissioningIndexed;flush();
                Telemetry::event("COMMISSION_INDEX_DOUBLE_MAGNET_LOCKED");return;
            }
            commissioningPreviousEdge=e.step;commissioningHavePreviousEdge=true;
        }
        if(!motionActive||millis()-seekStarted>=Config::HallSeekTimeoutMs*3)
        {
            commissioningIndexError=true;fail("COMMISSION_INDEX_DOUBLE_MAGNET_NOT_FOUND");
        }
        return;
    }
    if(!running||current==State::Absent||current==State::Disabled||current==State::Fault||current==State::CommissioningIndexed){flush();return;}
    const bool auxiliary=auxiliaryMotionActive();
    if(auxiliary){wasAuxiliary=true;flush();return;}
    if(wasAuxiliary){wasAuxiliary=false;reacquire();} // Do not count tension relief twice.
    if(overflow){fail("HALL_EDGE_QUEUE_OVERFLOW");return;}
    while(tail!=head) {
        noInterrupts();const Edge e{queue[tail].step,queue[tail].production,queue[tail].auxiliary};tail=(tail+1)%QueueSize;interrupts();
        if(!e.production)continue;
        totalPulses++;
        char edgeEvent[64];snprintf(edgeEvent,sizeof(edgeEvent),"HALL_EDGE_step_%lu_pulse_%lu",(unsigned long)e.step,(unsigned long)totalPulses);
        Telemetry::event(edgeEvent);
        const uint32_t before=tracker.corrected;
        const auto result=tracker.edge(e.step);
        if(result==HallTracking::Result::Fault){fail("HALL_PATTERN_OR_SLIP_FAULT");return;}
        if(result==HallTracking::Result::Locked){current=State::Locked;Telemetry::event("HALL_INDEX_LOCKED");}
        if(result==HallTracking::Result::Correction) {
            const uint32_t amount=tracker.corrected-before;
            if(totalCorrection+amount>Config::HallMaxCorrectionSteps){fail("HALL_CORRECTION_LIMIT");return;}
            totalCorrection+=amount;
            // Keep commanded steps monotonic. Extend only Count targets, never
            // weight targets or the absolute 1000 commanded-turn safety ceiling.
            if(autoCountMode) {
                const uint32_t ceiling=uint32_t(Config::FuhSafetyTurnCeiling)*Config::StepsPerHubRev;
                if(targetStepCount>ceiling-amount){fail("HALL_SAFETY_CEILING");return;}
                noInterrupts();targetStepCount+=amount;interrupts();
            }
            RunSupervisor::holdForHallFault("HALL_CONFIRMED_DEFICIT_CLEAR_REQUIRED");
            current=State::Fault;return;
        }
    }
    if(motionActive&&pauseState!=PauseState::PAUSED) {
        const uint32_t step=safeCurrentStepCount();
        // The entry position is latched in the ISR, but accepted after the exit
        // edge proves a real pulse. Allow a bounded magnet width while pending.
        const bool pulseInProgress=candidate&&uint32_t(step-candidateStep)<=Config::StepsPerHubRev/12;
        if((!pulseInProgress&&tracker.overdue(step))||(!tracker.edges&&step-acquireStartedStep>Config::HallSeekMaxSteps)||(!tracker.locked&&step-acquireStartedStep>Config::HallAcquisitionMaxSteps)) {fail("HALL_MISSING_PULSE_OR_INDEX");return;}
    }
    if(millis()-lastReport>=1000) {
        lastReport=millis();char message[96];
        snprintf(message,sizeof(message),"HALL_state_%s_edges_%lu_correction_steps_%lu",stateName(),(unsigned long)pulseCount(),(unsigned long)totalCorrection);
        Telemetry::event(message);
    }
}
void HallSensor::beginRun(){totalCorrection=0;totalPulses=0;wasAuxiliary=false;running=settings.hallEnabled&&current!=State::Absent; if(running)reacquire();}
void HallSensor::endRun(){running=false;flush();}
void HallSensor::resumeAfterClearance(){if(running&&current==State::Fault)reacquire();}
bool HallSensor::seekFinished(){return seekDone;}
bool HallSensor::seekFailed(){return seekError;}
HallSensor::State HallSensor::state(){return current;}
const char* HallSensor::stateName(State value){static const char* names[]={"off","seeking","absent","acquiring","locked","fault","commissioning seek","commissioning indexed"};return uint8_t(value)<8?names[uint8_t(value)]:"unknown";}
const char* HallSensor::stateName(){return stateName(current);}
uint32_t HallSensor::pulseCount(){return totalPulses;}
uint32_t HallSensor::correctionSteps(){return totalCorrection;}
bool HallSensor::activeLevel(){return digitalRead(Pins::Hall)==LOW;}
bool HallSensor::commissioningIndexFinished(){return commissioningIndexDone;}
bool HallSensor::commissioningIndexFailed(){return commissioningIndexError||current==State::Fault;}
