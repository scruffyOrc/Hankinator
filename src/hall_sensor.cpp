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
bool correctionDisabled=false;
uint8_t seekIgnoredEdges=0;
bool commissioningIndexDone=false,commissioningIndexError=false,commissioningHavePreviousEdge=false;
bool manualTracking=false;
uint32_t manualPulses=0;
uint32_t manualEdgesAfterIndex=0;
bool manualFirstEdgeIsClosePair=false;
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
void loseRunTracking(const char* reason)
{
    // Once winding is under way, Hall feedback validates physical position; it
    // must not itself turn a recoverable missed marker into a stopped machine.
    // Reacquire at the current commanded position and make the count-quality
    // limitation explicit to the UI/telemetry. Startup/indexing remains
    // strict and still uses fail().
    RunSupervisor::markCountUncertain();
    Telemetry::event(reason);
    reacquire();
}
}
void HallSensor::begin() {pinMode(Pins::Hall,INPUT_PULLUP);flush();attachInterrupt(digitalPinToInterrupt(Pins::Hall),edgeInterrupt,CHANGE);}
void HallSensor::startSeek(const bool forward,const float motorRps,const uint8_t ignoreEdges) {
    running=false;seekDone=false;seekError=false;seekIgnoredEdges=ignoreEdges;totalCorrection=0;tracker.reset();flush();
    if(!settings.hallEnabled){current=State::Disabled;seekDone=true;return;}
    current=State::Seeking;seekStarted=millis();pauseState=PauseState::RUNNING;enableMotor();
    if(!startAuxiliaryHubMotion(forward,Config::HallSeekMaxSteps,motorRps)){fail("HALL_SEEK_START_FAILED");return;}
    Telemetry::event("HALL_SEEK_START");
}
void HallSensor::startCommissioningIndex(const float motorRps) {
    running=false;seekDone=false;seekError=false;commissioningIndexDone=false;commissioningIndexError=false;commissioningHavePreviousEdge=false;
    flush();
    if(!settings.hallEnabled){current=State::Disabled;commissioningIndexError=true;return;}
    current=State::CommissioningSeeking;seekStarted=millis();pauseState=PauseState::RUNNING;enableMotor();
    if(!startAuxiliaryHubMotion(true,Config::StepsPerHubRev*2,motorRps))
    {
        commissioningIndexError=true;fail("COMMISSION_INDEX_START_FAILED");return;
    }
    Telemetry::event("COMMISSION_INDEX_START");
}
void HallSensor::cancel(){running=false;manualTracking=false;manualPulses=0;manualEdgesAfterIndex=0;seekDone=false;seekError=false;seekIgnoredEdges=0;commissioningIndexDone=false;commissioningIndexError=false;current=settings.hallEnabled?State::Absent:State::Disabled;flush();}
void HallSensor::service() {
    if(!settings.hallEnabled){current=State::Disabled;running=false;flush();return;}
    if(manualTracking)
    {
        // These edges are intentionally observational.  A user rotating the
        // hub manually cannot be judged by the production Hall pattern or
        // missed-pulse rules used while timer-driven motion is active.
        if(overflow){overflow=false;flush();Telemetry::event("HAND_TURN_HALL_QUEUE_OVERFLOW");return;}
        while(tail!=head)
        {
            noInterrupts();tail=(tail+1)%QueueSize;interrupts();
            ++manualPulses;
            // Hand Turning parks on the first close-pair magnet after moving
            // in reverse, then asks the user to wind in the configured
            // powered direction. The first edge is therefore its close-pair
            // partner; manualQuarterTurns() accounts for that zero-progress
            // landmark before counting the three isolated quarter marks.
            ++manualEdgesAfterIndex;
            char message[64];snprintf(message,sizeof(message),"HAND_TURN_HALL_%lu_QUARTERS_%lu",
                (unsigned long)manualPulses,(unsigned long)manualQuarterTurns());
            Telemetry::event(message);
        }
        return;
    }
    if(current==State::Seeking) {
        serviceStepGeneratorWake();
        // A completed edge is used here instead of the active-level shortcut.
        // That matters when reversing after indexing: the first magnet crossed
        // is the registration magnet we just left and must be deliberately
        // passed before we park on the preceding close-pair magnet.
        while(tail!=head) {
            noInterrupts();const Edge e{queue[tail].step,queue[tail].production,queue[tail].auxiliary};tail=(tail+1)%QueueSize;interrupts();
            if(!e.auxiliary)continue;
            if(seekIgnoredEdges) {
                --seekIgnoredEdges;
                Telemetry::event("HALL_SEEK_EDGE_SKIPPED");
                continue;
            }
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
    if(overflow){loseRunTracking("HALL_EDGE_QUEUE_OVERFLOW_REACQUIRE");return;}
    while(tail!=head) {
        noInterrupts();const Edge e{queue[tail].step,queue[tail].production,queue[tail].auxiliary};tail=(tail+1)%QueueSize;interrupts();
        if(!e.production)continue;
        totalPulses++;
        char edgeEvent[64];snprintf(edgeEvent,sizeof(edgeEvent),"HALL_EDGE_step_%lu_pulse_%lu",(unsigned long)e.step,(unsigned long)totalPulses);
        Telemetry::event(edgeEvent);
        const uint32_t before=tracker.corrected;
        const auto result=tracker.edge(e.step);
        if(result==HallTracking::Result::Fault){loseRunTracking("HALL_PATTERN_REACQUIRE");return;}
        if(result==HallTracking::Result::Locked){current=State::Locked;Telemetry::event("HALL_INDEX_LOCKED");}
        if(result==HallTracking::Result::Correction) {
            const uint32_t amount=tracker.corrected-before;
            if(totalCorrection+amount>Config::HallMaxCorrectionSteps){
                correctionDisabled=true;
                loseRunTracking("HALL_CORRECTION_LIMIT_REACQUIRE");
                return;
            }
            totalCorrection+=amount;
            // Keep commanded steps monotonic. Extend only Count targets, never
            // weight targets or the absolute 1000 commanded-turn safety ceiling.
            if(autoCountMode&&!correctionDisabled) {
                const uint32_t ceiling=uint32_t(Config::FuhSafetyTurnCeiling)*Config::StepsPerHubRev;
                if(targetStepCount>ceiling-amount){fail("HALL_SAFETY_CEILING");return;}
                noInterrupts();targetStepCount+=amount;interrupts();
            }
            // Hall is the physical count reconciler, not the first bind
            // detector. A bounded late landmark proves that the hub reached
            // the position, so account for it and continue. StallGuard owns
            // early load/bind recovery; only impossible ordering, a missing
            // landmark, or an excessive accumulated correction holds the run.
            char correctionEvent[72];
            snprintf(correctionEvent,sizeof(correctionEvent),"HALL_COUNT_CORRECTION_steps_%lu_total_%lu",
                (unsigned long)amount,(unsigned long)totalCorrection);
            Telemetry::event(correctionEvent);
        }
    }
    if(motionActive&&pauseState!=PauseState::PAUSED) {
        const uint32_t step=safeCurrentStepCount();
        // The entry position is latched in the ISR, but accepted after the exit
        // edge proves a real pulse. Allow a bounded magnet width while pending.
        const bool pulseInProgress=candidate&&uint32_t(step-candidateStep)<=Config::StepsPerHubRev/12;
        if((!pulseInProgress&&tracker.overdue(step))||(!tracker.edges&&step-acquireStartedStep>Config::HallSeekMaxSteps)||(!tracker.locked&&step-acquireStartedStep>Config::HallAcquisitionMaxSteps)) {
            loseRunTracking("HALL_MISSING_PULSE_OR_INDEX_REACQUIRE");
            return;
        }
    }
    if(millis()-lastReport>=1000) {
        lastReport=millis();char message[96];
        snprintf(message,sizeof(message),"HALL_state_%s_edges_%lu_correction_steps_%lu",stateName(),(unsigned long)pulseCount(),(unsigned long)totalCorrection);
        Telemetry::event(message);
    }
}
void HallSensor::beginRun(const bool suppressFeedback)
{
    totalCorrection=0;totalPulses=0;wasAuxiliary=false;correctionDisabled=false;
    // Preserve the configured Hall state, but make this run observationally
    // independent of Hall feedback. Used only by explicit console probes.
    if(suppressFeedback){running=false;flush();return;}
    running=settings.hallEnabled&&current!=State::Absent;
    if(running)reacquire();
}
void HallSensor::endRun(){running=false;flush();}
void HallSensor::resumeAfterClearance()
{
    // This is deliberately not limited to State::Fault. A StallGuard hold
    // stops the commanded motion at an arbitrary point between Hall edges;
    // continuing a previously Locked tracker after the user clears that hold
    // can classify the first perfectly real edge after restart as an early or
    // late landmark. Reacquire from the actual post-stop position instead.
    if(running&&current!=State::Absent&&current!=State::Disabled)reacquire();
}
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
void HallSensor::beginManualTracking(const bool firstEdgeIsClosePair)
{
    running=false;seekDone=false;seekError=false;manualPulses=0;
    manualEdgesAfterIndex=0;
    manualFirstEdgeIsClosePair=firstEdgeIsClosePair;
    current=State::Acquiring;manualTracking=true;flush();
}
void HallSensor::endManualTracking(){manualTracking=false;flush();}
uint32_t HallSensor::manualPulseCount(){return manualPulses;}
uint32_t HallSensor::manualQuarterTurns()
{
    // Five post-park edges form one hub revolution. Hand Wind deliberately
    // starts in the configured winding direction. Its first crossing is the
    // second close-pair magnet, which adds no quarter turn; the following
    // three isolated magnets add 1/4, 1/2, and 3/4; the parked close-pair
    // magnet completes the full turn.
    const uint32_t whole=manualEdgesAfterIndex/5;
    const uint32_t remainder=manualEdgesAfterIndex%5;
    if(manualFirstEdgeIsClosePair)
        return whole*4+(remainder?remainder-1:0);
    // Retain the reverse-direction interpretation for any future caller that
    // intentionally begins with the three isolated magnets.
    return whole*4+(remainder<3?remainder:3);
}
