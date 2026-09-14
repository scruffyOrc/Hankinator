#include "load_cells.h"
#include "hardware/gpio.h"
#include "pico/time.h"

namespace {
int32_t rawValues[Config::LoadCellChannelCount]{};
uint32_t lastResultMs[Config::LoadCellChannelCount]{};
uint8_t validSamples[Config::LoadCellChannelCount]{};
bool hasResult[Config::LoadCellChannelCount]{};
bool lastResultValid[Config::LoadCellChannelCount]{};
uint32_t resultSequence[Config::LoadCellChannelCount]{};
uint32_t snapshotSequence=0;
bool startupValidationFault=false;
bool startupChannelFault[Config::LoadCellChannelCount]{};

int32_t tareWindow[Config::LoadCellChannelCount][Config::LoadCellTareSamples]{};
uint8_t tareWindowCount[Config::LoadCellChannelCount]{};
uint8_t tareWindowWrite[Config::LoadCellChannelCount]{};
uint32_t tareSeenSequence[Config::LoadCellChannelCount]{};
uint32_t tareStartedMs=0;
LoadCellTareResult currentTare{};
LoadCellProfileTareResult profileTare{};
int64_t profileSum[Config::LoadCellProfileBins][Config::LoadCellChannelCount]{};
uint32_t profileStartStep=0,profileStartedMs=0,profileSeenSequence=0;
bool profileCollecting=false;

bool validRaw(int32_t value)
{
    // HX711 specifies these codes as the positive and negative saturation rails.
    return value!=8388607&&value!=-8388608;
}

void readReadyChannels(uint8_t readyMask)
{
    uint32_t values[Config::LoadCellChannelCount]{};
    for(uint8_t bit=0;bit<24;bit++)
    {
        noInterrupts();
        gpio_put(Pins::LoadCellClock,true);
        busy_wait_us_32(1);
        const uint32_t inputs=gpio_get_all();
        gpio_put(Pins::LoadCellClock,false);
        interrupts();
        for(uint8_t channel=0;channel<Config::LoadCellChannelCount;channel++)
            if(readyMask&(1u<<channel))values[channel]=(values[channel]<<1)|((inputs>>Pins::LoadCellData[channel])&1u);
        delayMicroseconds(1);
    }

    // Pulse 25 selects channel A, gain 128 for the next conversion.
    noInterrupts();gpio_put(Pins::LoadCellClock,true);busy_wait_us_32(1);gpio_put(Pins::LoadCellClock,false);interrupts();delayMicroseconds(1);

    const uint32_t now=millis();
    for(uint8_t channel=0;channel<Config::LoadCellChannelCount;channel++)
    {
        if(!(readyMask&(1u<<channel)))continue;
        const int32_t value=(values[channel]&0x00800000)?int32_t(values[channel]|0xFF000000):int32_t(values[channel]);
        rawValues[channel]=value;
        hasResult[channel]=true;
        lastResultMs[channel]=now;
        lastResultValid[channel]=validRaw(value);
        resultSequence[channel]++;
        if(lastResultValid[channel])
        {
            if(validSamples[channel]<255)validSamples[channel]++;
        }
    }
    snapshotSequence++;
}

bool evaluateTareWindow()
{
    for(uint8_t channel=0;channel<Config::LoadCellChannelCount;channel++)
        if(tareWindowCount[channel]<Config::LoadCellTareSamples)return false;

    bool stable=true;
    for(uint8_t channel=0;channel<Config::LoadCellChannelCount;channel++)
    {
        int64_t sum=0;
        int32_t minimum=INT32_MAX,maximum=INT32_MIN;
        for(uint8_t sample=0;sample<Config::LoadCellTareSamples;sample++)
        {
            const int32_t value=tareWindow[channel][sample];
            sum+=value;
            minimum=min(minimum,value);
            maximum=max(maximum,value);
        }
        currentTare.offset[channel]=int32_t(sum/Config::LoadCellTareSamples);
        currentTare.spread[channel]=maximum-minimum;
        if(currentTare.spread[channel]>Config::LoadCellTareMaxSpreadCounts[channel])stable=false;
    }
    currentTare.samples=Config::LoadCellTareSamples;
    return stable;
}
}

void LoadCells::begin()
{
    pinMode(Pins::LoadCellClock,OUTPUT);
    digitalWrite(Pins::LoadCellClock,LOW);
    for(uint8_t channel=0;channel<Config::LoadCellChannelCount;channel++)pinMode(Pins::LoadCellData[channel],INPUT_PULLUP);
}

void LoadCells::service()
{
    uint8_t readyMask=0;
    for(uint8_t channel=0;channel<Config::LoadCellChannelCount;channel++)
        if(digitalRead(Pins::LoadCellData[channel])==LOW)readyMask|=1u<<channel;
    if(readyMask)readReadyChannels(readyMask);
}

uint8_t LoadCells::detectAtStartup()
{
    uint32_t seen[Config::LoadCellChannelCount]{};
    uint16_t samples[Config::LoadCellChannelCount]{};
    int32_t first[Config::LoadCellChannelCount]{};
    bool changed[Config::LoadCellChannelCount]{};
    bool responded[Config::LoadCellChannelCount]{};
    startupValidationFault=false;
    for(uint8_t channel=0;channel<Config::LoadCellChannelCount;channel++)
    {
        seen[channel]=resultSequence[channel];
        startupChannelFault[channel]=false;
    }
    const uint32_t started=millis();
    while(millis()-started<Config::LoadCellDetectionWindowMs)
    {
        service();
        for(uint8_t channel=0;channel<Config::LoadCellChannelCount;channel++)
        {
            if(seen[channel]==resultSequence[channel])continue;
            seen[channel]=resultSequence[channel];
            responded[channel]=true;
            if(!lastResultValid[channel])continue;
            if(samples[channel]==0)first[channel]=rawValues[channel];
            else if(rawValues[channel]!=first[channel])changed[channel]=true;
            samples[channel]++;
        }
        delay(1);
    }
    uint8_t count=0;
    for(uint8_t channel=0;channel<Config::LoadCellChannelCount;channel++)
    {
        const bool passed=samples[channel]>=Config::LoadCellDetectionSamples&&changed[channel]
            &&lastResultValid[channel]&&millis()-lastResultMs[channel]<=Config::LoadCellHealthyTimeoutMs;
        if(passed)count++;
        startupChannelFault[channel]=responded[channel]&&!passed;
        startupValidationFault|=startupChannelFault[channel];
    }
    return count;
}

bool LoadCells::startupFault(){return startupValidationFault;}

bool LoadCells::healthy(uint8_t channel)
{
    return status(channel)==LoadCellStatus::Ok;
}

LoadCellStatus LoadCells::status(uint8_t channel)
{
    if(channel>=Config::LoadCellChannelCount||!hasResult[channel])return LoadCellStatus::NoData;
    if(startupChannelFault[channel]||!lastResultValid[channel]||millis()-lastResultMs[channel]>Config::LoadCellHealthyTimeoutMs)return LoadCellStatus::Error;
    return LoadCellStatus::Ok;
}

const char* LoadCells::statusName(LoadCellStatus value)
{
    switch(value)
    {
        case LoadCellStatus::Ok:return "OK";
        case LoadCellStatus::NoData:return "No Data";
        case LoadCellStatus::Error:return "Err";
    }
    return "Err";
}

LoadCellSnapshot LoadCells::snapshot()
{
    LoadCellSnapshot result{};
    for(uint8_t channel=0;channel<Config::LoadCellChannelCount;channel++)
    {
        result.raw[channel]=rawValues[channel];
        result.status[channel]=status(channel);
        if(result.status[channel]==LoadCellStatus::Ok)result.healthyMask|=1u<<channel;
    }
    result.sequence=snapshotSequence;
    return result;
}

void LoadCells::startTare()
{
    profileCollecting=false;
    memset(tareWindow,0,sizeof(tareWindow));
    memset(tareWindowCount,0,sizeof(tareWindowCount));
    memset(tareWindowWrite,0,sizeof(tareWindowWrite));
    for(uint8_t channel=0;channel<Config::LoadCellChannelCount;channel++)tareSeenSequence[channel]=resultSequence[channel];
    currentTare=LoadCellTareResult{};
    currentTare.status=LoadCellTareStatus::Collecting;
    tareStartedMs=millis();
}

void LoadCells::startProfileTare(uint32_t startStep)
{
    memset(&profileTare,0,sizeof(profileTare));
    memset(profileSum,0,sizeof(profileSum));
    profileTare.status=LoadCellTareStatus::Collecting;
    currentTare=LoadCellTareResult{};
    currentTare.status=LoadCellTareStatus::Collecting;
    profileStartStep=startStep;profileStartedMs=millis();profileSeenSequence=snapshotSequence;profileCollecting=true;
}

LoadCellTareStatus LoadCells::updateProfileTare(uint32_t currentStep)
{
    if(!profileCollecting)return profileTare.status;
    const uint32_t elapsedSteps=currentStep-profileStartStep;
    const LoadCellSnapshot value=snapshot();
    const uint8_t requiredMask=uint8_t((1u<<Config::LoadCellChannelCount)-1u);
    if(value.sequence!=profileSeenSequence&&(value.healthyMask&requiredMask)==requiredMask&&elapsedSteps<Config::StepsPerHubRev)
    {
        profileSeenSequence=value.sequence;
        const uint8_t bin=min<uint32_t>(Config::LoadCellProfileBins-1,uint64_t(elapsedSteps)*Config::LoadCellProfileBins/Config::StepsPerHubRev);
        for(uint8_t channel=0;channel<Config::LoadCellChannelCount;channel++)profileSum[bin][channel]+=value.raw[channel];
        if(profileTare.samples[bin]<255)profileTare.samples[bin]++;
    }

    if(elapsedSteps>=Config::StepsPerHubRev)
    {
        for(uint8_t bin=0;bin<Config::LoadCellProfileBins;bin++)if(!profileTare.samples[bin])
        {
            profileTare.status=LoadCellTareStatus::Failed;currentTare.status=LoadCellTareStatus::Failed;profileCollecting=false;return profileTare.status;
        }
        int64_t channelSum[Config::LoadCellChannelCount]{};
        for(uint8_t bin=0;bin<Config::LoadCellProfileBins;bin++)for(uint8_t channel=0;channel<Config::LoadCellChannelCount;channel++)
        {
            profileTare.raw[bin][channel]=int32_t(profileSum[bin][channel]/profileTare.samples[bin]);
            channelSum[channel]+=profileTare.raw[bin][channel];
        }
        bool plausible=true;
        for(uint8_t channel=0;channel<Config::LoadCellChannelCount;channel++)
        {
            currentTare.offset[channel]=int32_t(channelSum[channel]/Config::LoadCellProfileBins);
            int32_t minimum=INT32_MAX,maximum=INT32_MIN;
            for(uint8_t bin=0;bin<Config::LoadCellProfileBins;bin++){minimum=min(minimum,profileTare.raw[bin][channel]);maximum=max(maximum,profileTare.raw[bin][channel]);}
            currentTare.spread[channel]=maximum-minimum;
            if(currentTare.spread[channel]>Config::LoadCellProfileMaxRangeCounts[channel])plausible=false;
        }
        if(!plausible)
        {
            profileTare.status=LoadCellTareStatus::Failed;currentTare.status=LoadCellTareStatus::Failed;profileCollecting=false;return profileTare.status;
        }
        currentTare.samples=Config::LoadCellProfileBins;currentTare.status=LoadCellTareStatus::Complete;
        profileTare.status=LoadCellTareStatus::Complete;profileCollecting=false;return profileTare.status;
    }
    if(millis()-profileStartedMs>=Config::LoadCellProfileTareTimeoutMs)
    {
        profileTare.status=LoadCellTareStatus::Failed;currentTare.status=LoadCellTareStatus::Failed;profileCollecting=false;
    }
    return profileTare.status;
}

LoadCellTareStatus LoadCells::updateTare()
{
    if(currentTare.status!=LoadCellTareStatus::Collecting)return currentTare.status;

    for(uint8_t channel=0;channel<Config::LoadCellChannelCount;channel++)
    {
        if(resultSequence[channel]==tareSeenSequence[channel])continue;
        tareSeenSequence[channel]=resultSequence[channel];
        if(!lastResultValid[channel])continue;
        tareWindow[channel][tareWindowWrite[channel]]=rawValues[channel];
        tareWindowWrite[channel]=uint8_t((tareWindowWrite[channel]+1)%Config::LoadCellTareSamples);
        if(tareWindowCount[channel]<Config::LoadCellTareSamples)tareWindowCount[channel]++;
    }

    if(evaluateTareWindow())
    {
        currentTare.status=LoadCellTareStatus::Complete;
        return currentTare.status;
    }

    if(millis()-tareStartedMs>=Config::LoadCellTareTimeoutMs)
        currentTare.status=LoadCellTareStatus::Failed;
    return currentTare.status;
}

void LoadCells::cancelTare()
{
    if(currentTare.status==LoadCellTareStatus::Collecting)currentTare.status=LoadCellTareStatus::Cancelled;
    if(profileCollecting){profileTare.status=LoadCellTareStatus::Cancelled;profileCollecting=false;}
}

LoadCellTareStatus LoadCells::tareStatus(){return currentTare.status;}

const char* LoadCells::tareStatusName(LoadCellTareStatus value)
{
    switch(value)
    {
        case LoadCellTareStatus::Idle:return "idle";
        case LoadCellTareStatus::Collecting:return "collecting";
        case LoadCellTareStatus::Complete:return "complete";
        case LoadCellTareStatus::Failed:return "unstable";
        case LoadCellTareStatus::Cancelled:return "cancelled";
    }
    return "unknown";
}

LoadCellTareResult LoadCells::tareResult(){return currentTare;}
bool LoadCells::tareValid(){return currentTare.status==LoadCellTareStatus::Complete;}
bool LoadCells::profileTareValid(){return profileTare.status==LoadCellTareStatus::Complete;}
LoadCellProfileTareResult LoadCells::profileTareResult(){return profileTare;}

float LoadCells::weightGrams(const LoadCellSnapshot& value)
{
    if(!tareValid())return NAN;
    const uint8_t requiredMask=uint8_t((1u<<Config::LoadCellChannelCount)-1u);
    if((value.healthyMask&requiredMask)!=requiredMask)return NAN;
    float grams=0.0f;
    for(uint8_t channel=0;channel<Config::LoadCellChannelCount;channel++)
        grams+=(value.raw[channel]-currentTare.offset[channel])*Config::LoadCellGramsPerCount[channel];
    return grams;
}

float LoadCells::weightGrams(){return weightGrams(snapshot());}

float LoadCells::stoppedWeightGrams(const LoadCellSnapshot& value)
{
    return weightGrams(value);
}

float LoadCells::profileWeightGrams(const LoadCellSnapshot& value,uint32_t hubStep)
{
    if(!profileTareValid())return NAN;
    const uint8_t requiredMask=uint8_t((1u<<Config::LoadCellChannelCount)-1u);
    if((value.healthyMask&requiredMask)!=requiredMask)return NAN;
    const uint8_t bin=min<uint32_t>(Config::LoadCellProfileBins-1,uint64_t(hubStep%Config::StepsPerHubRev)*Config::LoadCellProfileBins/Config::StepsPerHubRev);
    float grams=0.0f;
    for(uint8_t channel=0;channel<Config::LoadCellChannelCount;channel++)grams+=(value.raw[channel]-profileTare.raw[bin][channel])*Config::LoadCellGramsPerCount[channel];
    return grams;
}
