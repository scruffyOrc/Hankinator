#include <cassert>
#include <cmath>
#include "resume_profile.h"

namespace { bool closeTo(float left,float right){return std::fabs(left-right)<.001f;} }

int main()
{
    const ResumeProfile::Settings profile{.5f,.5f,3.0f,.85f,1.20f};
    assert(closeTo(ResumeProfile::speedForElapsedMs(0,3.0f,profile),.5f));
    assert(closeTo(ResumeProfile::speedForElapsedMs(700,3.0f,profile),.85f));
    assert(closeTo(ResumeProfile::speedForElapsedMs(800,3.0f,profile),1.15f));
    assert(closeTo(ResumeProfile::speedForElapsedMs(820,3.0f,profile),1.2016667f));
    assert(closeTo(ResumeProfile::speedForElapsedMs(4420,3.0f,profile),3.0f));
    assert(closeTo(ResumeProfile::speedForElapsedMs(500,0.4f,profile),.4f));
}
