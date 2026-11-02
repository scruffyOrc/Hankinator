#include "config.h"
#include "stall_recovery.h"
#include <cassert>
#include <cstdio>
using namespace StallRecovery;
Settings settings(){return {Config::StallReducedMotorRps,Config::StallSlowdownRate,Config::StallRecoveryRampRate,Config::StallRecoveryMinimumRps,
    Config::StallConfirmMs,Config::StallSevereMs,Config::StallPersistentMs,Config::StallInvalidMs,Config::StallStableMs,
    Config::StallProofTimeoutMs,Config::StepsPerHubRev,{Config::StallSpeedFloors[0],Config::StallSpeedFloors[1],Config::StallSpeedFloors[2]},
    Config::StallSevereSg,Config::StallHealthyHysteresis,Config::StallMaxHolds};}
struct Rig {
    Controller c{settings()};uint32_t now=0,step=0;
    Rig(){c.reset(3,now);}
    Action tick(uint16_t sg=300,bool valid=true,bool diag=false,bool moving=true,float speed=-1,bool eligible=true,bool fault=false,uint32_t dt=50,bool sgValid=true){
        if(speed<0)speed=c.cap;
        now+=dt;if(moving)step+=uint32_t(speed*Config::StepsPerMotorRev*dt/1000);
        return c.update({now,step,speed,sg,moving,eligible,valid,sgValid,diag,fault});
    }
    void healthy(unsigned ticks=30){while(ticks--)tick();}
    void incident(){tick(50);assert(tick(50)==Action::Slow);assert(c.uncertain);}
};
int main(){
    {Rig r;r.healthy();auto baseline=r.c.baseline;assert(baseline==300);r.tick(50);r.tick();assert(r.c.phase==Phase::Normal);assert(!r.c.uncertain);assert(r.c.baseline==baseline);}
    {Rig r;r.healthy();r.incident();r.healthy(25);assert(r.c.phase==Phase::Proving);const auto start=r.step;
        r.healthy(40);assert(r.c.phase==Phase::Proving);
        r.tick(80); // marginal is not a full healthy turn, resets proof
        // The proof needs one hub turn.  A revised recovery target may
        // legitimately complete that turn before this fixed test interval.
        auto reset=r.step;r.healthy(75);assert(r.c.phase==Phase::Proving||r.c.phase==Phase::Ramping);
        for(int i=0;i<10&&r.c.phase==Phase::Proving;i++)r.tick();
        assert(r.c.phase==Phase::Ramping);assert(r.step-reset>=Config::StepsPerHubRev);assert(r.step-start>Config::StepsPerHubRev);
        r.tick(50);assert(r.tick(50)==Action::Lock);r.healthy(150);assert(r.c.phase==Phase::Locked);assert(r.c.cap==Config::StallReducedMotorRps);}
    {Rig r;r.healthy();r.incident();for(int i=0;i<220&&r.c.phase!=Phase::Normal;i++)r.tick();assert(r.c.phase==Phase::Normal);assert(r.c.incidents==1);
        r.tick(50);assert(r.tick(50)==Action::Lock);r.healthy(150);assert(r.c.phase==Phase::Locked);}
    {Rig r;r.healthy();r.tick(0);assert(r.tick(0)==Action::Hold);assert(r.c.phase==Phase::Held);
        r.healthy();assert(r.c.phase==Phase::Held);assert(r.c.confirm(r.now,r.step,0.25f));
        r.healthy(70);assert(r.c.phase==Phase::Locked);assert(r.c.cap==Config::StallReducedMotorRps);
        r.tick(0);assert(r.tick(0)==Action::Hold);assert(r.c.confirm(r.now,r.step,0.25f));r.healthy(35);
        r.tick(0);assert(r.tick(0)==Action::Abort);assert(!r.c.confirm(r.now,r.step,0.25f));}
    // With no numerical SG_RESULT, the first Hall-confirmed recovery proves
    // one clean turn at reduced speed, then restores normal speed. A second
    // Hall recovery remains reduced; the third hold still aborts.
    {Rig r;r.healthy();assert(r.c.requireClearance()==Action::Hold);
        assert(r.c.confirm(r.now,r.step,0.25f,true));assert(r.c.phase==Phase::Proving);
        uint32_t reducedStart=0;bool restored=false;
        for(int i=0;i<160&&!restored;i++){
            // UI and I/O occasionally make one supervisor pass late. The
            // Hall-confirmed proof must still recognize a full clean turn.
            const Action action=r.tick(0,true,false,true,-1,true,false,i==80?250:50,false);
            if(r.c.cap>=Config::StallReducedMotorRps-0.001f&&!reducedStart)reducedStart=r.step;
            if(action==Action::Restore){restored=true;assert(reducedStart);assert(r.step-reducedStart>=Config::StepsPerHubRev);}
        }
        assert(restored);for(int i=0;i<40;i++)r.tick(0,true,false,true,-1,true,false,50,false);
        assert(r.c.cap>Config::StallReducedMotorRps);
        assert(r.c.requireClearance()==Action::Hold);assert(r.c.confirm(r.now,r.step,0.25f,true));
        assert(r.c.phase==Phase::Locked);for(int i=0;i<100;i++)r.tick(0,true,false,true,-1,true,false,50,false);
        assert(r.c.cap==Config::StallReducedMotorRps);assert(r.c.requireClearance()==Action::Abort);}
    {Rig r;r.healthy();r.tick(300,true,true);assert(r.tick(300,true,true)==Action::Hold);}
    {Rig r;r.healthy();for(int i=0;i<12;i++)r.tick(50);assert(r.c.phase==Phase::Held);}
    {Rig r;r.healthy();for(int i=0;i<6;i++)r.tick(0,false);assert(r.c.phase==Phase::Held);}
    {Rig r;for(int i=0;i<20;i++)r.tick(0,true,true,true,0.5f);assert(r.c.phase==Phase::Normal);}
    // A slow launch does not always have fresh StallGuard data. It must not
    // become a false "missing driver data" hold below the monitoring floor.
    {Rig r;for(int i=0;i<20;i++)r.tick(0,false,false,true,0.25f);assert(r.c.phase==Phase::Normal);}
    // Status-only operation remains safe through DIAG, but missing SG_RESULT
    // alone must not become a false driver-data hold.
    {Rig r;for(int i=0;i<20;i++)r.tick(0,true,false,true,3,true,false,50,false);assert(r.c.phase==Phase::Normal);}
    {Rig r;r.tick(0,true,true,true,3,true,false,50,false);assert(r.tick(0,true,true,true,3,true,false,50,false)==Action::Hold);}
    {Rig r;r.healthy();assert(r.tick(300,true,false,true,3,true,true)==Action::Abort);}
    {Rig r;r.healthy();r.incident();r.healthy(60);assert(r.c.phase==Phase::Proving||r.c.phase==Phase::Ramping);
        r.tick(300,true,false,false);r.healthy(70);assert(r.c.phase==Phase::Proving||r.c.phase==Phase::Ramping);}
    {Rig r;r.now=UINT32_MAX-100;r.c.reset(3,r.now);r.healthy();assert(r.c.phase==Phase::Normal);r.incident();}
    {Rig r;r.healthy();r.incident();for(int i=0;i<260&&r.c.phase!=Phase::Held;i++)r.tick(80);assert(r.c.phase==Phase::Held);}
    std::puts("12 stall recovery scenarios passed");
}
