#pragma once

#include <stdint.h>

// Normal pause/resume uses wall time, rather than travelled steps, so its
// acceleration stays predictable even if the hub is under load. The narrow
// resonance band is traversed deliberately faster than the surrounding ramp.
namespace ResumeProfile {
struct Settings {
    float startRps;
    float baseRateRpsPerSecond;
    float resonanceCrossRateRpsPerSecond;
    float resonanceLowRps;
    float resonanceHighRps;
};

inline float speedForElapsedMs(uint32_t elapsedMs,float targetRps,const Settings& settings)
{
    if(targetRps<=settings.startRps)return targetRps;

    float remainingSeconds=elapsedMs/1000.0f;
    float speed=settings.startRps;
    const float boundaries[]={settings.resonanceLowRps,settings.resonanceHighRps,targetRps};
    for(uint8_t index=0;index<3&&speed<targetRps;++index)
    {
        const float next=boundaries[index]<targetRps?boundaries[index]:targetRps;
        if(next<=speed)continue;
        const bool inResonanceBand=speed>=settings.resonanceLowRps&&speed<settings.resonanceHighRps;
        const float rate=inResonanceBand?settings.resonanceCrossRateRpsPerSecond:settings.baseRateRpsPerSecond;
        const float secondsToBoundary=(next-speed)/rate;
        if(remainingSeconds<secondsToBoundary)return speed+remainingSeconds*rate;
        speed=next;
        remainingSeconds-=secondsToBoundary;
    }
    return targetRps;
}
}
