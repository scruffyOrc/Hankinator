#include "hall_tracker.h"
#include <cassert>
#include <cstdio>
using HallTracking::Result;
const uint32_t gaps[]={1422,1600,1600,1422,356};
uint32_t acquire(HallTracking::Tracker& t,uint32_t start=0) {
    t.reset(start);uint32_t step=start;t.edge(step);
    for(int cycle=0;cycle<2;cycle++)for(auto gap:gaps){step+=gap;t.edge(step);}
    assert(t.locked);return step;
}
int main() {
    HallTracking::Tracker t(6400,20,64);
    uint32_t step=acquire(t);
    for(int cycle=0;cycle<100;cycle++)for(auto gap:gaps){step+=gap;assert(t.edge(step)==Result::None);}
    assert(t.corrected==0&&!t.fault);
    step=acquire(t);step+=1422+100;assert(t.edge(step)==Result::None);
    step+=1600;assert(t.edge(step)==Result::Correction);assert(t.corrected==36);
    step=acquire(t);step+=1422+1600;assert(t.edge(step)==Result::Fault); // missed pulse
    step=acquire(t);step+=100;assert(t.edge(step)==Result::Fault); // extra pulse
    step=acquire(t);assert(!t.overdue(step+1422));assert(t.overdue(step+1800));
    step=acquire(t,UINT32_MAX-10000);step+=1422;assert(t.edge(step)==Result::None); // wrap
    for(int initial=0;initial<5;initial++) {
        t.reset();step=0;t.edge(step);
        for(int i=0;i<15&&!t.locked;i++){step+=gaps[(initial+i)%5];t.edge(step);}
        assert(t.locked&&!t.fault); // arbitrary starting arm, either direction (symmetric layout)
    }
    t.reset();step=0;t.edge(step);for(int i=0;i<20;i++){step+=1600;t.edge(step);}assert(!t.locked);
    step=acquire(t);step+=1422+30;assert(t.edge(step)==Result::None);
    step+=1600-30;assert(t.edge(step)==Result::None);assert(!t.corrected); // jitter
    std::puts("9 Hall decoder scenarios passed");
}
