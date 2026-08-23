#include "run_supervisor.h"
#include "app_api.h"
#include "config.h"
#include "load_cells.h"
#include "runtime.h"
#include "telemetry.h"
#include "tmc_driver.h"

namespace {
RunSupervisorSnapshot state{};
bool armed=false,abortCompletion=false,probeResuming=false,finalVerificationPending=false,finalMeasurementOnly=false;
float configuredTarget=NAN,speedLimit=Config::CruiseMotorRps;
uint32_t lastServiceMs=0,lastSpeedUpdateMs=0,lastStallEventMs=0;
uint32_t nextProbeStep=0;
uint32_t probeStartStep=0,probeSeenSequence=0;
uint32_t lastWeightSequence=0;
uint8_t lowSgSamples=0;
uint8_t probeRetries=0;
bool probeCapturing=false;
float probeSum=0.0f,probeMinimum=NAN,probeMaximum=NAN;
uint16_t probeSamples=0;
float settleWindow[Config::WeightFinalSettleSamples]{};
uint8_t settleCount=0,settleWrite=0;
uint32_t settleStartedMs=0;

constexpr uint8_t SgBaselineSamples=16;
uint16_t sgWindow[SgBaselineSamples]{};uint8_t sgCount=0,sgWrite=0;
float weightWindow[Config::WeightFilterSamples]{};uint8_t weightCount=0,weightWrite=0;

template<typename T> T median(const T* values,uint8_t count)
{
    T sorted[Config::WeightFilterSamples>SgBaselineSamples?Config::WeightFilterSamples:SgBaselineSamples]{};
    for(uint8_t i=0;i<count;i++)sorted[i]=values[i];
    for(uint8_t i=1;i<count;i++){const T value=sorted[i];int j=i-1;while(j>=0&&sorted[j]>value){sorted[j+1]=sorted[j];j--;}sorted[j+1]=value;}
    return count?sorted[count/2]:T{};
}

void emit(const char* name)
{
    char label[64]{};
    const long weightCentiGrams=isnan(state.filteredWeightGrams)?LONG_MIN:lroundf(state.filteredWeightGrams*100.0f);
    snprintf(label,sizeof(label),"%s_sg_%u_base_%u_wt_cg_%ld",name,state.sgResult,state.sgBaseline,weightCentiGrams);
    Telemetry::event(label);
}

void beginFinalVerification()
{
    finalVerificationPending=true;finalMeasurementOnly=false;state.settlingFinalWeight=true;
    settleCount=0;settleWrite=0;settleStartedMs=millis();
    state.weightState=WeightApproachState::TargetReached;
    motionActive=false;
    Telemetry::event("WEIGHT_TARGET_VERIFY_STOP");
}

void updateFinalVerification(float grams)
{
    settleWindow[settleWrite]=grams;settleWrite=uint8_t((settleWrite+1)%Config::WeightFinalSettleSamples);
    if(settleCount<Config::WeightFinalSettleSamples)settleCount++;
    float minimum=settleWindow[0],maximum=settleWindow[0];
    for(uint8_t i=1;i<settleCount;i++){minimum=min(minimum,settleWindow[i]);maximum=max(maximum,settleWindow[i]);}
    state.filteredWeightGrams=median(settleWindow,settleCount);state.weightSpreadGrams=maximum-minimum;
    if(settleCount==Config::WeightFinalSettleSamples&&state.weightSpreadGrams<=Config::WeightFinalSettleMaxRangeGrams)
    {
        if(finalMeasurementOnly)
        {
            finalVerificationPending=false;finalMeasurementOnly=false;state.settlingFinalWeight=false;
            state.finalWeightMeasured=true;
            Telemetry::event("FINAL_WEIGHT_SETTLED");
        }
        else if(state.filteredWeightGrams<configuredTarget)
        {
            finalVerificationPending=false;state.settlingFinalWeight=false;
            state.weightState=WeightApproachState::Monitoring;probeCapturing=false;probeRetries=0;
            probeResuming=true;
            speedLimit=Config::WeightApproachMotorRps;lastSpeedUpdateMs=millis();
            nextProbeStep=safeCurrentStepCount()+uint32_t(Config::WeightProbeRetryHubTurns*Config::StepsPerHubRev);
            motionActive=true;
            requestStepGeneratorWake();
            Telemetry::event("WEIGHT_SETTLED_BELOW_TARGET_RESUME");
        }
        else
        {
            finalVerificationPending=false;state.settlingFinalWeight=false;
            state.finalWeightMeasured=true;
            Telemetry::event("WEIGHT_TARGET_REACHED_SETTLED");
        }
        return;
    }
    if(millis()-settleStartedMs>=Config::WeightFinalSettleTimeoutMs)
    {
        finalVerificationPending=false;finalMeasurementOnly=false;state.settlingFinalWeight=false;
        state.finalWeightMeasured=true;
        Telemetry::event("WEIGHT_SETTLE_TIMEOUT_COMPLETE");
    }
}

void updateWeight()
{
    const uint32_t step=safeCurrentStepCount();
    const LoadCellSnapshot loadCells=LoadCells::snapshot();
    if(loadCells.sequence==lastWeightSequence)return;
    lastWeightSequence=loadCells.sequence;
    const float grams=LoadCells::profileWeightGrams(loadCells,step);
    if(isnan(grams))return;
    if(finalVerificationPending){updateFinalVerification(grams);return;}
    weightWindow[weightWrite]=grams;
    weightWrite=uint8_t((weightWrite+1)%Config::WeightFilterSamples);
    if(weightCount<Config::WeightFilterSamples)weightCount++;
    state.filteredWeightGrams=median(weightWindow,weightCount);
    float minimum=weightWindow[0],maximum=weightWindow[0];
    for(uint8_t i=1;i<weightCount;i++){minimum=min(minimum,weightWindow[i]);maximum=max(maximum,weightWindow[i]);}
    state.weightSpreadGrams=maximum-minimum;

    if(state.weightState==WeightApproachState::Monitoring&&weightCount==Config::WeightFilterSamples&&currentMotionPhase()==MotionPhase::CRUISE&&safeCurrentStepCount()>=nextProbeStep)
    {
        const float margin=max(Config::WeightApproachMinimumMarginGrams,configuredTarget*Config::WeightApproachMarginFraction);
        if(state.filteredWeightGrams>=configuredTarget-margin)
        {
            state.weightState=WeightApproachState::Approach;
            probeRetries=0;
            speedLimit=max(currentMotorRPS,Config::WeightApproachMotorRps);
            lastSpeedUpdateMs=millis();
            Telemetry::event("WEIGHT_APPROACH_START");
        }
    }

    if(state.weightState!=WeightApproachState::Approach)return;
    if(speedLimit>Config::WeightApproachMotorRps+0.01f)return;

    if(!probeCapturing)
    {
        probeCapturing=true;probeStartStep=step;probeSeenSequence=loadCells.sequence;
        probeSum=0.0f;probeSamples=0;probeMinimum=NAN;probeMaximum=NAN;
        Telemetry::event("WEIGHT_PROBE_REVOLUTION_START");
        return;
    }
    if(loadCells.sequence!=probeSeenSequence)
    {
        probeSeenSequence=loadCells.sequence;probeSum+=grams;probeSamples++;
        probeMinimum=isnan(probeMinimum)?grams:min(probeMinimum,grams);
        probeMaximum=isnan(probeMaximum)?grams:max(probeMaximum,grams);
        state.filteredWeightGrams=probeSum/probeSamples;
        state.weightSpreadGrams=probeMaximum-probeMinimum;
    }
    if(step-probeStartStep<Config::StepsPerHubRev)return;
    if(probeSamples<Config::WeightProbeMinimumSamples)
    {
        probeCapturing=false;
        Telemetry::event("WEIGHT_PROBE_INSUFFICIENT_SAMPLES");
        if(++probeRetries>=Config::WeightProbeMaxConsecutiveRetries)
        {
            Telemetry::event("WEIGHT_PROBE_RETRY_LIMIT_STATIONARY_VERIFY");
            beginFinalVerification();
        }
        return;
    }

    if(probeMaximum-probeMinimum>Config::WeightProbeMaxRangeGrams)
    {
        state.filteredWeightGrams=probeSum/probeSamples;
        state.weightSpreadGrams=probeMaximum-probeMinimum;
        probeCapturing=false;
        Telemetry::event("WEIGHT_PROBE_EXCESSIVE_RANGE");
        if(++probeRetries>=Config::WeightProbeMaxConsecutiveRetries)
        {
            Telemetry::event("WEIGHT_PROBE_RETRY_LIMIT_STATIONARY_VERIFY");
            beginFinalVerification();
        }
        return;
    }

    const float revolutionWeight=probeSum/probeSamples;
    state.filteredWeightGrams=revolutionWeight;
    const float margin=max(Config::WeightApproachMinimumMarginGrams,configuredTarget*Config::WeightApproachMarginFraction);
    if(revolutionWeight<configuredTarget-margin-Config::WeightProbeResumeHysteresisGrams)
    {
        state.weightState=WeightApproachState::Monitoring;
        probeResuming=true;probeCapturing=false;probeRetries=0;
        lastSpeedUpdateMs=millis();
        nextProbeStep=step+uint32_t(Config::WeightProbeRetryHubTurns*Config::StepsPerHubRev);
        Telemetry::event("WEIGHT_PROBE_BELOW_TARGET_RESUME");
        return;
    }
    if(revolutionWeight>=configuredTarget)
    {
        probeRetries=0;
        beginFinalVerification();
        return;
    }
    probeCapturing=false;
    Telemetry::event("WEIGHT_PROBE_NEAR_TARGET_REPEAT");
    if(++probeRetries>=Config::WeightProbeMaxConsecutiveRetries)
    {
        Telemetry::event("WEIGHT_PROBE_RETRY_LIMIT_STATIONARY_VERIFY");
        beginFinalVerification();
    }
}

void updateStall()
{
    if(!Config::EnableStallDetection||!TmcDriver::connected()||pauseState!=PauseState::RUNNING||currentMotionPhase()!=MotionPhase::CRUISE||currentMotorRPS<Config::StallDetectionMinimumRps)
    {
        lowSgSamples=0;
        state.stallState=StallDetectionState::Monitoring;
        return;
    }

    state.sgResult=TmcDriver::sgResult();
    if(state.sgResult>=Config::StallCandidateSg&&state.sgResult!=0xFFFF)
    {
        sgWindow[sgWrite]=state.sgResult;sgWrite=uint8_t((sgWrite+1)%SgBaselineSamples);if(sgCount<SgBaselineSamples)sgCount++;
        state.sgBaseline=median(sgWindow,sgCount);
    }
    const uint16_t relativeThreshold=state.sgBaseline>=200?uint16_t(state.sgBaseline*35UL/100UL):Config::StallCandidateSg;
    const bool low=state.sgResult<Config::StallCandidateSg||(state.sgBaseline>=200&&state.sgResult<relativeThreshold);
    if(!low)
    {
        lowSgSamples=0;
        state.stallState=StallDetectionState::Monitoring;
        return;
    }

    if(lowSgSamples<255)lowSgSamples++;
    if(lowSgSamples==1)
    {
        state.stallState=StallDetectionState::Candidate;
        emit("STALL_CANDIDATE");
    }
    if(lowSgSamples<Config::StallConfirmSamples)return;

    state.stallState=StallDetectionState::Confirmed;
    if(millis()-lastStallEventMs>=Config::StallEventCooldownMs||lastStallEventMs==0)
    {
        lastStallEventMs=millis();
        emit(state.sgResult<=Config::StallSevereSg?"STALL_CONFIRMED_SEVERE":"STALL_CONFIRMED");
    }
    if(Config::EnableAutomaticStallAbort)
    {
        abortCompletion=true;
        motionActive=false;
    }
}
}

void RunSupervisor::beginRun()
{
    state=RunSupervisorSnapshot{};
    state.weightArmed=armed&&weightCapabilityEnabled&&LoadCells::profileTareValid();
    state.targetWeightGrams=configuredTarget;
    state.weightState=state.weightArmed?WeightApproachState::Monitoring:WeightApproachState::Disabled;
    abortCompletion=false;probeResuming=false;finalVerificationPending=false;finalMeasurementOnly=false;lastServiceMs=0;lastSpeedUpdateMs=millis();lastStallEventMs=0;nextProbeStep=0;
    lowSgSamples=0;probeRetries=0;sgCount=0;sgWrite=0;weightCount=0;weightWrite=0;
    probeCapturing=false;probeStartStep=0;probeSeenSequence=0;lastWeightSequence=0;probeSum=0.0f;probeSamples=0;probeMinimum=NAN;probeMaximum=NAN;
    speedLimit=selectedCruiseMotorRPS;
}

void RunSupervisor::service()
{
    const uint32_t now=millis();
    if(now-lastServiceMs<Config::ControlSupervisorIntervalMs)return;
    lastServiceMs=now;
    if(state.weightArmed)
    {
        const LoadCellSnapshot loadCells=LoadCells::snapshot();
        const uint8_t requiredMask=uint8_t((1u<<Config::LoadCellChannelCount)-1u);
        if((loadCells.healthyMask&requiredMask)!=requiredMask)
        {
            abortCompletion=true;motionActive=false;Telemetry::event("LOAD_CELL_UNHEALTHY_ABORT");return;
        }
    }
    updateStall();
    if(state.weightArmed||finalVerificationPending)updateWeight();
}

float RunSupervisor::limitMotorRps(float requestedRps)
{
    if(state.weightState==WeightApproachState::TargetReached)return 0.0f;
    if(probeResuming)
    {
        const uint32_t now=millis();
        const float elapsed=(now-lastSpeedUpdateMs)/1000.0f;lastSpeedUpdateMs=now;
        speedLimit=min(selectedCruiseMotorRPS,speedLimit+Config::WeightApproachDecelRpsPerSecond*elapsed);
        if(speedLimit>=selectedCruiseMotorRPS-0.01f)probeResuming=false;
        return min(requestedRps,speedLimit);
    }
    if(state.weightState!=WeightApproachState::Approach)return requestedRps;
    const uint32_t now=millis();
    const float elapsed=(now-lastSpeedUpdateMs)/1000.0f;lastSpeedUpdateMs=now;
    speedLimit=max(Config::WeightApproachMotorRps,speedLimit-Config::WeightApproachDecelRpsPerSecond*elapsed);
    return min(requestedRps,speedLimit);
}

RunSupervisorSnapshot RunSupervisor::snapshot(){return state;}
bool RunSupervisor::armWeightTarget(float grams){if(grams<Config::MinimumWeightTargetGrams||grams>Config::MaximumWeightTargetGrams)return false;configuredTarget=grams;armed=true;return true;}
void RunSupervisor::disarmWeightTarget(){armed=false;configuredTarget=NAN;}
bool RunSupervisor::weightTargetArmed(){return armed;}
float RunSupervisor::weightTargetGrams(){return configuredTarget;}
bool RunSupervisor::completionIsAbort(){return abortCompletion||(state.weightArmed&&state.weightState!=WeightApproachState::TargetReached);}
bool RunSupervisor::completionPending(){return finalVerificationPending;}
bool RunSupervisor::requestFinalMeasurement()
{
    if(finalVerificationPending||state.finalWeightMeasured||!LoadCells::profileTareValid())return false;
    finalVerificationPending=true;finalMeasurementOnly=true;state.settlingFinalWeight=true;
    settleCount=0;settleWrite=0;settleStartedMs=millis();motionActive=false;
    Telemetry::event("FINAL_WEIGHT_VERIFY_STOP");
    return true;
}
const char* RunSupervisor::stallStateName(StallDetectionState value){return value==StallDetectionState::Candidate?"candidate":value==StallDetectionState::Confirmed?"confirmed":"monitoring";}
const char* RunSupervisor::weightStateName(WeightApproachState value){return value==WeightApproachState::Monitoring?"monitoring":value==WeightApproachState::Approach?"approach":value==WeightApproachState::TargetReached?"target_reached":"disabled";}
