#pragma once
#include <stdint.h>
#include <math.h>
#include <algorithm>

// Hardware-independent recovery policy, exercised by native tests.
namespace StallRecovery {
enum class Phase:uint8_t {Normal,Slowing,Proving,Ramping,Locked,Held,Aborted};
enum class Action:uint8_t {None,Slow,Restore,Lock,Hold,Abort};
struct Settings {
    float reducedRps,downRate,upRate,minRps;
    float resonanceCrossRate,resonanceLowRps,resonanceHighRps;
    uint32_t confirmMs,severeMs,persistentMs,invalidMs,stableMs,proofTimeoutMs,stepsPerTurn;
    uint8_t proofTurns;
    uint16_t floors[3],severeSg,hysteresis;
    // Number of user-cleared recovery restarts allowed.  Zero means unlimited.
    uint8_t maxRestarts;
};
struct Input {
    uint32_t now,steps;
    float speed;
    uint16_t sg;
    // valid proves the UART/status path is alive. sgValid separately says a
    // numeric StallGuard sample is available; DIAG remains useful without it.
    bool moving,eligible,valid,sgValid,diag,hardFault;
};
class Controller {
public:
    explicit Controller(const Settings& settings):cfg(settings){}
    void reset(float cruise,uint32_t now,uint32_t step=0) {
        phase=Phase::Normal;incidents=holds=0;uncertain=false;cap=cruise;ceiling=cruise;
        lastNow=now;lastStep=step;lastSpeed=0;lowMs=severeMs=invalidMs=stableMs=proofMs=proofSteps=proofStartStep=0;
        episode=false;proofGood=false;band=0;baseline=0;candidate=false;
        for(auto& b:bins)b=Bin{};
    }
    Action update(const Input& in) {
        const uint32_t elapsed=in.now-lastNow;
        const uint32_t dt=std::min(elapsed,uint32_t(100));
        const uint32_t delta=in.steps>=lastStep?in.steps-lastStep:0;
        const float seconds=dt/1000.0f;
        lastNow=in.now;lastStep=in.steps;
        if(phase==Phase::Aborted)return Action::None;
        if(in.hardFault){phase=Phase::Aborted;uncertain=true;return Action::Abort;}
        if(phase==Phase::Held)return Action::None;
        if(!in.moving){clearEvidence();return Action::None;}
        // StallGuard4 is explicitly observational outside the monitored
        // operating range. In particular, a slow launch may not yet provide
        // a fresh SG_RESULT, which is not evidence of a driver-data fault.
        if(!in.eligible){clearEvidence();return Action::None;}
        if(in.speed<cfg.minRps){
            // A user-cleared recovery restarts from launch speed. Raise its
            // cap toward the recovery plateau without classifying unavailable
            // SG data, so it can re-enter monitored operation instead of
            // remaining trapped at launch speed.
            if(phase==Phase::Slowing||phase==Phase::Locked||phase==Phase::Proving) {
                const float target=std::min(ceiling,cfg.reducedRps);
                cap=raiseCap(target,seconds);
            }
            clearEvidence();return Action::None;
        }
        if(!in.valid) {
            invalidMs+=dt;proofSteps=0;proofGood=false;candidate=false;
            if(invalidMs>=cfg.invalidMs)return hold();
            return Action::None;
        }
        invalidMs=0;
        if(phase==Phase::Slowing||phase==Phase::Locked||phase==Phase::Proving) {
            const float target=std::min(ceiling,cfg.reducedRps);
            cap=cap>target?std::max(target,cap-cfg.downRate*seconds):raiseCap(target,seconds);
        } else if(phase==Phase::Ramping)cap=raiseCap(ceiling,seconds);
        const bool sameSpeed=fabsf(in.speed-lastSpeed)<=0.06f;
        lastSpeed=in.speed;
        const uint8_t nextBand=in.speed<1.5f?0:(in.speed<2.5f?1:2);
        if(nextBand!=band){band=nextBand;stableMs=0;}
        stableMs=sameSpeed?stableMs+dt:0;
        const bool stable=stableMs>=cfg.stableMs;
        const Bin& b=bins[band];
        baseline=b.count>=8?median(b):0;
        const uint16_t threshold=std::max(cfg.floors[band],uint16_t(baseline*35UL/100UL));
        const bool severe=in.diag||(in.sgValid&&in.sg<=cfg.severeSg);
        severeMs=severe?severeMs+dt:0;
        if(severeMs>=cfg.severeMs)return hold();
        // Some modules continue to provide DRV_STATUS but do not return a
        // usable SG_RESULT. Keep electrical-fault and DIAG protection active,
        // but do not invent a numerical load baseline from missing data.
        if(!in.sgValid){
            lowMs=0;candidate=false;
            // Hall-confirmed recovery can still prove a clear mechanism even
            // when this TMC2209 module does not expose numerical SG_RESULT.
            // Once the recovery plateau is reached, measure four hub turns from an
            // absolute step origin. A late main-loop service pass must not
            // erase real, uninterrupted hub motion just because its interval
            // exceeded the normal 100 ms supervisory cadence.
            if(phase==Phase::Proving) {
                const float target=std::min(ceiling,cfg.reducedRps);
                // Count only travel at the settled recovery plateau, not
                // the ramp that reaches it. `proofGood` marks that the
                // absolute origin has been captured; it is cleared on a stop
                // or any fall below the reduced speed.
                const bool atReducedSpeed=cap>=target-0.001f&&in.speed>=target-0.01f;
                if(!atReducedSpeed) {proofSteps=0;proofGood=false;}
                else if(!proofGood) {proofGood=true;proofStartStep=in.steps;proofSteps=0;}
                else proofSteps=in.steps>=proofStartStep?in.steps-proofStartStep:0;
                if(proofSteps>=proofTargetSteps()){phase=Phase::Ramping;return Action::Restore;}
            }
            return Action::None;
        }
        const bool low=in.sg<threshold;
        candidate=low&&(stable||phase==Phase::Ramping);
        lowMs=low&&(stable||phase==Phase::Ramping||episode)?lowMs+dt:0;
        const bool healthy=stable&&!severe&&in.sg>=threshold+cfg.hysteresis;
        if(healthy)episode=false;
        if(lowMs>=cfg.persistentMs)return hold();
        if(lowMs>=cfg.confirmMs&&!episode) {
            episode=true;uncertain=true;incidents++;proofSteps=proofMs=0;proofGood=false;
            cap=std::min(cap,in.speed);
            // Every incident gets the same plateau-and-ramp recovery. A
            // separately configured restart limit may still stop an unsafe
            // sequence of uncleared incidents, but repeat incidents must not
            // silently strand a successfully-cleared run at reduced speed.
            phase=Phase::Slowing;
            return Action::Slow;
        }
        if(phase==Phase::Slowing||phase==Phase::Locked) {
            const float target=std::min(ceiling,cfg.reducedRps);
            if(phase==Phase::Slowing&&cap<=target+0.001f&&in.speed<=target+0.06f) {
                phase=Phase::Proving;proofSteps=proofMs=0;proofGood=false;
            }
        } else if(phase==Phase::Proving) {
            proofMs+=dt;
            // The proof plateau answers a mechanical question: can the hub
            // make four uninterrupted turns at the reduced speed? Requiring
            // every SG sample to clear the learned "healthy" threshold made
            // an otherwise smooth recovery time out whenever SG was merely
            // noisy. A persistent/severe low result still reaches hold()
            // above; an isolated low sample simply restarts this proof.
            const float target=std::min(ceiling,cfg.reducedRps);
            const bool atReducedSpeed=cap>=target-0.001f&&in.speed>=target-0.01f;
            if(!atReducedSpeed||low) {
                proofSteps=0;
                proofGood=false;
            } else if(!proofGood) {
                proofGood=true;
                proofStartStep=in.steps;
                proofSteps=0;
            } else {
                // Use an absolute STEP origin rather than service cadence;
                // foreground work may delay a poll without interrupting a
                // physically clean reduced-speed turn.
                proofSteps=in.steps>=proofStartStep?in.steps-proofStartStep:0;
            }
            if(proofSteps>=proofTargetSteps()){phase=Phase::Ramping;return Action::Restore;}
            if(proofMs>=cfg.proofTimeoutMs)return hold();
        } else if(phase==Phase::Ramping) {
            if(cap>=ceiling)phase=Phase::Normal;
        }
        // Classify first; never teach a baseline with suspicious/recovery samples.
        if(phase==Phase::Normal&&healthy&&!episode) {
            Bin& learned=bins[band];learned.values[learned.write]=in.sg;
            learned.write=(learned.write+1)%16;if(learned.count<16)learned.count++;
        }
        return Action::None;
    }
    bool confirm(uint32_t now,uint32_t step,float launchRps,bool proveFirstRecovery=false) {
        if(phase!=Phase::Held)return false;
        // Hall faults are independent confirmations of missed hub motion.
        // Every user-cleared incident starts at launch speed, reaches the
        // plateau, proves four clean hub turns, and then ramps to cruise.
        // `proveFirstRecovery` remains part of the public call for source
        // compatibility with the Hall path; recovery policy is now identical
        // for every cleared bind.
        (void)proveFirstRecovery;
        phase=Phase::Proving;
        incidents=std::max(uint8_t(1),incidents);
        cap=std::min(launchRps,cfg.reducedRps);lastNow=now;lastStep=step;lastSpeed=0;
        clearEvidence();episode=false;return true;
    }
    Action requireClearance(){return hold();}
    void setMaxRestarts(uint8_t limit){cfg.maxRestarts=limit;}
    Phase phase=Phase::Normal;
    float cap=0;
    uint8_t incidents=0,holds=0;
    bool uncertain=false,candidate=false;
    uint16_t baseline=0;
private:
    struct Bin {uint16_t values[16]{};uint8_t count=0,write=0;};
    Settings cfg;Bin bins[3]{};
    float ceiling=0,lastSpeed=0;
    uint32_t lastNow=0,lastStep=0,lowMs=0,severeMs=0,invalidMs=0,stableMs=0,proofMs=0,proofSteps=0,proofStartStep=0;
    uint8_t band=0;bool episode=false,proofGood=false;
    uint32_t proofTargetSteps() const {return cfg.stepsPerTurn*cfg.proofTurns;}
    float raiseCap(float target,float seconds) const {
        float speed=cap;
        while(seconds>0.0f&&speed<target) {
            const bool inResonance=speed>=cfg.resonanceLowRps&&speed<cfg.resonanceHighRps;
            const float rate=inResonance?cfg.resonanceCrossRate:cfg.upRate;
            float boundary=target;
            if(speed<cfg.resonanceLowRps) boundary=std::min(target,cfg.resonanceLowRps);
            else if(speed<cfg.resonanceHighRps) boundary=std::min(target,cfg.resonanceHighRps);
            const float secondsToBoundary=(boundary-speed)/rate;
            if(seconds<secondsToBoundary) return speed+seconds*rate;
            speed=boundary;
            seconds-=secondsToBoundary;
        }
        return speed;
    }
    static uint16_t median(const Bin& b){uint16_t v[16];for(uint8_t i=0;i<b.count;i++)v[i]=b.values[i];std::sort(v,v+b.count);return b.count?v[b.count/2]:0;}
    void clearEvidence(){lowMs=severeMs=invalidMs=stableMs=proofSteps=proofStartStep=0;candidate=false;proofGood=false;}
    Action hold(){
        uncertain=true;
        if(holds<UINT8_MAX)holds++;
        // `holds` is the incident that is now asking for clearance; a limit
        // of two therefore permits two successful resumes and aborts on the
        // third distinct incident. Zero deliberately disables this cap.
        phase=(cfg.maxRestarts!=0&&holds>cfg.maxRestarts)?Phase::Aborted:Phase::Held;
        return phase==Phase::Aborted?Action::Abort:Action::Hold;
    }
};
}
