#pragma once
#include <stdint.h>
#include <algorithm>

// Hardware-independent five-magnet decoder. All distances are commanded STEP
// pulses captured at Hall edges, never elapsed time (acceleration is allowed).
namespace HallTracking {
enum class Result:uint8_t {None,Locked,Correction,Fault};
class Tracker {
public:
    Tracker(uint32_t revolution,uint16_t pairDegrees,uint32_t tolerance)
        :rev(revolution),shortGap(2*((revolution*pairDegrees/360+1)/2)),tol(tolerance){}
    void reset(uint32_t step=0) {
        first=true;locked=false;fault=false;edges=0;corrected=0;previousLag=0;
        write=count=cycles=phase=0;last=origin=step;expected=0;
    }
    Result edge(uint32_t step) {
        if(fault)return Result::Fault;
        ++edges;
        if(first){first=false;last=step;return Result::None;}
        const uint32_t gap=step-last;last=step;
        if(!locked) {
            gaps[write]=gap;write=(write+1)%5;if(count<5)++count;
            if(count==5&&gap<rev/8) {
                uint32_t sum=0;bool valid=true;
                for(uint8_t i=0;i<5;i++) {
                    const uint32_t observed=gaps[(write+i)%5],nominal=distance(i);
                    sum+=observed;
                    if(difference(observed,nominal)>tol)valid=false;
                }
                if(difference(sum,rev)>tol)valid=false;
                cycles=valid?uint8_t(cycles+1):0;
                if(cycles>=2){locked=true;origin=step;expected=0;phase=0;return Result::Locked;}
            }
            return Result::None;
        }
        const uint32_t nominal=distance(phase);
        // A missing/extra pulse or large slip is ambiguous: do not invent travel.
        const uint32_t ambiguity=std::max(tol+1,shortGap/2);
        if(difference(gap,nominal)>ambiguity){fault=true;return Result::Fault;}
        expected+=nominal;phase=(phase+1)%5;
        const int32_t lag=int32_t(step-origin)-int32_t(expected)-int32_t(corrected);
        if(lag < -int32_t(tol)||lag>int32_t(ambiguity)){fault=true;return Result::Fault;}
        // Require the same deficit at two consecutive distinct landmarks.
        if(lag>int32_t(tol)&&previousLag>int32_t(tol)) {
            const uint32_t amount=uint32_t(std::min(lag,previousLag))-tol;
            corrected+=amount;previousLag=0;return Result::Correction;
        }
        previousLag=lag;return Result::None;
    }
    bool overdue(uint32_t step)const {
        if(first)return false;
        const uint32_t maximum=locked?distance(phase)+std::max(tol+1,shortGap/2):rev/3;
        return step-last>maximum;
    }
    bool locked=false,fault=false;
    uint32_t edges=0,corrected=0;
private:
    uint32_t rev,shortGap,tol,last=0,origin=0,expected=0,gaps[5]{};
    int32_t previousLag=0;
    uint8_t write=0,count=0,cycles=0,phase=0;
    bool first=true;
    uint32_t distance(uint8_t i)const {return i==4?shortGap:(i==0||i==3?(rev/4-shortGap/2):rev/4);}
    static uint32_t difference(uint32_t a,uint32_t b){return a>b?a-b:b-a;}
};
}
