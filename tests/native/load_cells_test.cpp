#include <load_cells.h>
#include <cassert>
#include <cstdlib>
static uint32_t now=0,rises=0,frames=0,nextReady=0;
static unsigned connected=15,blocked=0,bit=0;
static int32_t extra=0;
static uint32_t highWaitTotal=0;
void busy_wait_us_32(uint32_t us){highWaitTotal+=us;}
uint32_t millis(){return now;}
uint32_t micros(){return now*1000;}
void delay(uint32_t ms){now+=ms;}
void delayMicroseconds(uint32_t){}
void pinMode(int,int){}
void digitalWrite(int,int){}
int digitalRead(int pin){
    for(unsigned ch=0;ch<4;++ch)if(pin==Pins::LoadCellData[ch])
        return (connected&(1u<<ch))&&!(blocked&(1u<<ch))&&now>=nextReady?LOW:HIGH;
    return HIGH;
}
void gpio_put(int,bool high){
    if(high){++rises;++bit;}
    else if(bit==25){bit=0;++frames;nextReady=now+100;}
}
uint32_t gpio_get_all(){
    assert(bit>=1&&bit<=24);
    uint32_t result=0;
    for(unsigned ch=0;ch<4;++ch){
        int32_t value=(ch%2?-100000:100000)+int32_t(frames%3)+extra;
        result|=((uint32_t(value)>>(24-bit))&1u)<<Pins::LoadCellData[ch];
    }
    return result;
}
int main(int argc,char** argv){
    connected=argc>1?unsigned(std::atoi(argv[1])):15;
    LoadCells::begin();
    unsigned count=LoadCells::detectAtStartup(),expected=0;
    for(unsigned ch=0;ch<4;++ch)expected+=(connected>>ch)&1;
    assert(count==expected);assert(!LoadCells::startupFault());
    assert(LoadCells::detectedMask()==connected);
    assert(rises==frames*25);
    if(!connected){assert(rises==0);puts("No-cell startup passed");return 0;}
    auto before=LoadCells::snapshot();
    blocked=connected&(~connected+1); // Withhold exactly one member.
    auto clockBefore=rises;
    now+=Config::LoadCellSyncTimeoutMs+1;LoadCells::service();
    assert(rises==clockBefore);assert(LoadCells::missingReadyMask()==blocked);
    assert(LoadCells::syncTimeoutCount()==1);
    assert(LoadCells::snapshot().sequence==before.sequence);
    assert((LoadCells::snapshot().healthyMask&connected)==0);
    blocked=0;LoadCells::service();
    assert(rises==clockBefore+25);assert(LoadCells::missingReadyMask()==0);
    assert((LoadCells::snapshot().healthyMask&connected)==connected);
    auto framesBeforeReady=frames;
    now+=1;LoadCells::service(); // DOUT remains high while the next conversion runs.
    assert(frames==framesBeforeReady);
    extra=20000;now+=99;LoadCells::service();
    assert(LoadCells::suspiciousMask()==connected);
    assert(LoadCells::suspiciousCount()==expected);
    assert(LoadCells::snapshot().raw[0]>110000); // Excursion not filtered.
    if(connected==15){
        LoadCells::startTare();
        for(unsigned n=0;n<20;++n){now+=1;LoadCells::service();now+=99;LoadCells::service();LoadCells::updateTare();}
        assert(LoadCells::tareValid());assert(std::abs(LoadCells::weightGrams())<0.02f);
        LoadCells::startTare();LoadCells::cancelTare();
        assert(LoadCells::tareStatus()==LoadCellTareStatus::Cancelled);
    }
    assert(LoadCells::sampleDelayUs()==5);
    LoadCells::beginTimingTest();
    assert(LoadCells::selectedTimingDelay()==5);
    // Exercise all choices starting at 1, without changing the active delay yet.
    LoadCells::selectTimingDelay(false);
    LoadCells::selectTimingDelay(false);
    LoadCells::selectTimingDelay(false);
    const uint8_t choices[]={1,2,3,5,8,10};
    for(auto choice:choices){
        assert(LoadCells::selectedTimingDelay()==choice);
        LoadCells::applyTimingDelay();
        assert(LoadCells::sampleDelayUs()==choice);
        assert(LoadCells::timingJumps()==0&&LoadCells::timingTimeouts()==0);
        assert(LoadCells::timingElapsedMs()==0);
        auto waited=highWaitTotal;now+=1;LoadCells::service();now+=99;LoadCells::service();
        assert(highWaitTotal-waited==50u*choice); // 25 symmetric high/low phases.
        assert(LoadCells::timingElapsedMs()==100);
        LoadCells::selectTimingDelay(true);
        assert(LoadCells::sampleDelayUs()==choice); // Selection alone cannot apply.
    }
    assert(LoadCells::selectedTimingDelay()==1);
    LoadCells::selectTimingDelay(false);assert(LoadCells::selectedTimingDelay()==10);
    puts("Actual HX711 module: discovery, timeout, recovery, signed raw, jumps and timing tests passed");
}
