#include "run_supervisor.h"
#include "app_api.h"
#include "config.h"
#include "load_cells.h"
#include "runtime.h"
#include "telemetry.h"
#include "tmc_driver.h"

namespace {
RunSupervisorSnapshot state{};
bool armed=false,abortCompletion=false,measurementPending=false,measurementOnly=false;
float configuredTarget=NAN;
uint32_t lastServiceMs=0,lastStallEventMs=0,lastWeightSequence=0;
uint32_t stageStartStep=0,lastMeasurementStep=0;
float lastMeasurementWeight=0.0f;
uint8_t lowSgSamples=0;
float settleWindow[Config::WeightFinalSettleSamples]{};
uint8_t settleCount=0,settleWrite=0;
uint32_t settleStartedMs=0;
constexpr uint8_t SgBaselineSamples=16;
uint16_t sgWindow[SgBaselineSamples]{};
uint8_t sgCount=0,sgWrite=0;

template<typename T,size_t N> T median(const T (&values)[N],uint8_t count)
{
    T sorted[N]{};
    for(uint8_t i=0;i<count;i++)sorted[i]=values[i];
    for(uint8_t i=1;i<count;i++){const T value=sorted[i];int j=i-1;while(j>=0&&sorted[j]>value){sorted[j+1]=sorted[j];j--;}sorted[j+1]=value;}
    return count?sorted[count/2]:T{};
}

void eventWithValues(const char* name,float weight,float gramsPerTurn,uint16_t turns=0)
{
    char label[96]{};
    snprintf(label,sizeof(label),"%s_wt_cg_%ld_gpt_mg_%ld_turns_%u",name,lroundf(weight*100.0f),isnan(gramsPerTurn)?LONG_MIN:lroundf(gramsPerTurn*1000.0f),turns);
    Telemetry::event(label);
}

void emitStall(const char* name)
{
    char label[64]{};
    const long weightCentiGrams=isnan(state.filteredWeightGrams)?LONG_MIN:lroundf(state.filteredWeightGrams*100.0f);
    snprintf(label,sizeof(label),"%s_sg_%u_base_%u_wt_cg_%ld",name,state.sgResult,state.sgBaseline,weightCentiGrams);
    Telemetry::event(label);
}

void abortWeightRun(const char* event)
{
    abortCompletion=true;measurementPending=false;state.settlingFinalWeight=false;
    motionActive=false;motionSegmentStopStep=UINT32_MAX;
    Telemetry::event(event);
}

void finishAtWeight(float grams)
{
    state.filteredWeightGrams=grams;state.finalWeightMeasured=true;
    state.weightState=WeightApproachState::TargetReached;
    state.settlingFinalWeight=false;measurementPending=false;
    motionActive=false;motionSegmentStopStep=UINT32_MAX;
    eventWithValues("WEIGHT_TARGET_REACHED_SETTLED",grams,state.gramsPerTurn);
}

uint16_t estimatedRemainingTurns(float grams)
{
    if(isnan(state.gramsPerTurn)||state.gramsPerTurn<Config::WeightEstimateMinimumGramsPerTurn||grams>=configuredTarget)return 0;
    const float estimate=ceilf((configuredTarget-grams)/state.gramsPerTurn);
    const uint32_t safetySteps=targetStepCount>safeCurrentStepCount()?targetStepCount-safeCurrentStepCount():0;
    const uint16_t safetyTurns=uint16_t(safetySteps/Config::StepsPerHubRev);
    return uint16_t(min<uint32_t>(uint32_t(max(1.0f,estimate)),safetyTurns));
}

void startBatch(WeightApproachState nextState,uint16_t turns,const char* event)
{
    const uint32_t current=safeCurrentStepCount();
    const uint32_t remainingSteps=targetStepCount>current?targetStepCount-current:0;
    const uint16_t availableTurns=uint16_t(remainingSteps/Config::StepsPerHubRev);
    turns=min(turns,availableTurns);
    if(!turns){abortWeightRun("WEIGHT_SAFETY_CEILING_ABORT");return;}
    state.weightState=nextState;state.plannedStageTurns=turns;
    stageStartStep=current;
    motionSegmentStopStep=current+uint32_t(turns)*Config::StepsPerHubRev;
    measurementPending=false;state.settlingFinalWeight=false;
    motionActive=true;requestStepGeneratorWake();
    eventWithValues(event,state.filteredWeightGrams,state.gramsPerTurn,turns);
}

void beginStoppedMeasurement()
{
    if(measurementPending)return;
    if(pauseState==PauseState::RAMPING_UP)pauseState=PauseState::RUNNING;
    measurementPending=true;state.settlingFinalWeight=true;
    settleCount=0;settleWrite=0;settleStartedMs=millis();
    motionActive=false;motionSegmentStopStep=UINT32_MAX;
    Telemetry::event("WEIGHT_STOPPED_MEASUREMENT_START");
}

void updateSlope(float grams,uint32_t step)
{
    if(state.weightState==WeightApproachState::Calibration)
    {
        const float turns=step/float(Config::StepsPerHubRev);
        state.gramsPerTurn=turns>0.0f?grams/turns:NAN;
        return;
    }
    const float turns=(step-lastMeasurementStep)/float(Config::StepsPerHubRev);
    if(turns<=0.0f)return;
    const float candidate=(grams-lastMeasurementWeight)/turns;
    if(candidate<Config::WeightEstimateMinimumGramsPerTurn){Telemetry::event("WEIGHT_SLOPE_SAMPLE_REJECTED_LOW");return;}
    if(!isnan(state.gramsPerTurn)&&(candidate<state.gramsPerTurn*0.25f||candidate>state.gramsPerTurn*4.0f)){Telemetry::event("WEIGHT_SLOPE_SAMPLE_REJECTED_OUTLIER");return;}
    state.gramsPerTurn=isnan(state.gramsPerTurn)?candidate:state.gramsPerTurn*0.4f+candidate*0.6f;
}

void handleStoppedWeight(float grams)
{
    const uint32_t step=safeCurrentStepCount();
    state.filteredWeightGrams=grams;
    if(measurementOnly)
    {
        measurementOnly=false;measurementPending=false;state.settlingFinalWeight=false;state.finalWeightMeasured=true;
        Telemetry::event("FINAL_WEIGHT_SETTLED");return;
    }
    if(grams>=configuredTarget){finishAtWeight(grams);return;}
    updateSlope(grams,step);
    if(isnan(state.gramsPerTurn)||state.gramsPerTurn<Config::WeightEstimateMinimumGramsPerTurn){abortWeightRun("WEIGHT_ESTIMATE_INVALID_SLOPE_ABORT");return;}

    const WeightApproachState measuredStage=state.weightState;
    const uint16_t remaining=estimatedRemainingTurns(grams);
    eventWithValues("WEIGHT_STOPPED_MEASUREMENT",grams,state.gramsPerTurn,remaining);
    lastMeasurementWeight=grams;lastMeasurementStep=step;
    if(!remaining){abortWeightRun("WEIGHT_SAFETY_CEILING_ABORT");return;}

    if(measuredStage==WeightApproachState::Calibration)
    {
        const uint16_t bulkTurns=uint16_t(floorf(remaining*Config::WeightEstimateBulkFraction));
        if(bulkTurns>Config::WeightEstimateFinalReserveTurns)startBatch(WeightApproachState::Bulk,bulkTurns,"WEIGHT_BULK_BATCH_START");
        else startBatch(WeightApproachState::Approach,min<uint16_t>(remaining,Config::WeightEstimateMaxApproachTurns),"WEIGHT_APPROACH_BATCH_START");
        return;
    }
    if(measuredStage==WeightApproachState::Bulk)
    {
        const uint16_t refinementTurns=remaining>Config::WeightEstimateFinalReserveTurns?remaining-Config::WeightEstimateFinalReserveTurns:0;
        if(refinementTurns)startBatch(WeightApproachState::Refinement,refinementTurns,"WEIGHT_REFINEMENT_BATCH_START");
        else startBatch(WeightApproachState::Approach,min<uint16_t>(remaining,Config::WeightEstimateMaxApproachTurns),"WEIGHT_APPROACH_BATCH_START");
        return;
    }

    if(state.approachBatch>=Config::WeightEstimateMaxApproachBatches){abortWeightRun("WEIGHT_APPROACH_RETRY_LIMIT_ABORT");return;}
    state.approachBatch++;
    startBatch(WeightApproachState::Approach,min<uint16_t>(remaining,Config::WeightEstimateMaxApproachTurns),"WEIGHT_APPROACH_BATCH_START");
}

void updateStoppedMeasurement()
{
    const LoadCellSnapshot loadCells=LoadCells::snapshot();
    if(loadCells.sequence==lastWeightSequence)return;
    lastWeightSequence=loadCells.sequence;
    const float grams=LoadCells::profileWeightGrams(loadCells,safeCurrentStepCount());
    if(isnan(grams))return;
    settleWindow[settleWrite]=grams;settleWrite=uint8_t((settleWrite+1)%Config::WeightFinalSettleSamples);
    if(settleCount<Config::WeightFinalSettleSamples)settleCount++;
    float minimum=settleWindow[0],maximum=settleWindow[0];
    for(uint8_t i=1;i<settleCount;i++){minimum=min(minimum,settleWindow[i]);maximum=max(maximum,settleWindow[i]);}
    state.filteredWeightGrams=median(settleWindow,settleCount);state.weightSpreadGrams=maximum-minimum;
    if(settleCount==Config::WeightFinalSettleSamples&&state.weightSpreadGrams<=Config::WeightFinalSettleMaxRangeGrams){handleStoppedWeight(state.filteredWeightGrams);return;}
    if(millis()-settleStartedMs>=Config::WeightFinalSettleTimeoutMs)
    {
        if(measurementOnly&&settleCount){measurementOnly=false;measurementPending=false;state.settlingFinalWeight=false;state.finalWeightMeasured=true;Telemetry::event("FINAL_WEIGHT_SETTLE_TIMEOUT");}
        else abortWeightRun("WEIGHT_SETTLE_TIMEOUT_ABORT");
    }
}

void updateStall()
{
    if(!Config::EnableStallDetection||!TmcDriver::connected()||pauseState!=PauseState::RUNNING||currentMotionPhase()!=MotionPhase::CRUISE||currentMotorRPS<Config::StallDetectionMinimumRps){lowSgSamples=0;state.stallState=StallDetectionState::Monitoring;return;}
    state.sgResult=TmcDriver::sgResult();
    if(state.sgResult>=Config::StallCandidateSg&&state.sgResult!=0xFFFF){sgWindow[sgWrite]=state.sgResult;sgWrite=uint8_t((sgWrite+1)%SgBaselineSamples);if(sgCount<SgBaselineSamples)sgCount++;state.sgBaseline=median(sgWindow,sgCount);}
    const uint16_t relativeThreshold=state.sgBaseline>=200?uint16_t(state.sgBaseline*35UL/100UL):Config::StallCandidateSg;
    const bool low=state.sgResult<Config::StallCandidateSg||(state.sgBaseline>=200&&state.sgResult<relativeThreshold);
    if(!low){lowSgSamples=0;state.stallState=StallDetectionState::Monitoring;return;}
    if(lowSgSamples<255)lowSgSamples++;
    if(lowSgSamples==1){state.stallState=StallDetectionState::Candidate;emitStall("STALL_CANDIDATE");}
    if(lowSgSamples<Config::StallConfirmSamples)return;
    state.stallState=StallDetectionState::Confirmed;
    if(millis()-lastStallEventMs>=Config::StallEventCooldownMs||lastStallEventMs==0){lastStallEventMs=millis();emitStall(state.sgResult<=Config::StallSevereSg?"STALL_CONFIRMED_SEVERE":"STALL_CONFIRMED");}
    if(Config::EnableAutomaticStallAbort)abortWeightRun("STALL_AUTOMATIC_ABORT");
}
}

void RunSupervisor::beginRun()
{
    state=RunSupervisorSnapshot{};state.weightArmed=armed&&weightCapabilityEnabled&&LoadCells::profileTareValid();state.targetWeightGrams=configuredTarget;
    abortCompletion=false;measurementPending=false;measurementOnly=false;lastServiceMs=0;lastStallEventMs=0;lastWeightSequence=0;lowSgSamples=0;sgCount=0;sgWrite=0;
    lastMeasurementStep=0;lastMeasurementWeight=0.0f;stageStartStep=0;
    if(state.weightArmed)
    {
        state.weightState=WeightApproachState::Calibration;state.plannedStageTurns=Config::WeightEstimateCalibrationTurns;
        motionSegmentStopStep=uint32_t(Config::WeightEstimateCalibrationTurns)*Config::StepsPerHubRev;
        eventWithValues("WEIGHT_CALIBRATION_BATCH_START",0.0f,NAN,Config::WeightEstimateCalibrationTurns);
    }
    else {state.weightState=WeightApproachState::Disabled;motionSegmentStopStep=UINT32_MAX;}
}

void RunSupervisor::service()
{
    const uint32_t now=millis();if(now-lastServiceMs<Config::ControlSupervisorIntervalMs)return;lastServiceMs=now;
    if(state.weightArmed)
    {
        const LoadCellSnapshot loadCells=LoadCells::snapshot();const uint8_t requiredMask=uint8_t((1u<<Config::LoadCellChannelCount)-1u);
        if((loadCells.healthyMask&requiredMask)!=requiredMask){abortWeightRun("LOAD_CELL_UNHEALTHY_ABORT");return;}
    }
    updateStall();
    if(state.weightArmed&&!measurementPending&&!motionActive&&state.weightState!=WeightApproachState::TargetReached)beginStoppedMeasurement();
    if(measurementPending)updateStoppedMeasurement();
}

float RunSupervisor::limitMotorRps(float requestedRps)
{
    if(state.settlingFinalWeight||state.weightState==WeightApproachState::TargetReached)return 0.0f;
    if(!state.weightArmed)return requestedRps;
    if(state.weightState==WeightApproachState::Approach)return min(requestedRps,Config::WeightApproachMotorRps);
    const uint32_t step=safeCurrentStepCount();const float rampSteps=2.0f*Config::StepsPerHubRev;
    const float started=min(1.0f,(step-stageStartStep)/rampSteps);
    float limited=min(requestedRps,Config::WeightApproachMotorRps+(selectedCruiseMotorRPS-Config::WeightApproachMotorRps)*started);
    if(motionSegmentStopStep!=UINT32_MAX&&motionSegmentStopStep>step)
    {
        const float ending=min(1.0f,(motionSegmentStopStep-step)/rampSteps);
        limited=min(limited,Config::WeightApproachMotorRps+(selectedCruiseMotorRPS-Config::WeightApproachMotorRps)*ending);
    }
    return limited;
}

RunSupervisorSnapshot RunSupervisor::snapshot(){return state;}
bool RunSupervisor::armWeightTarget(float grams){if(grams<Config::MinimumWeightTargetGrams||grams>Config::MaximumWeightTargetGrams)return false;configuredTarget=grams;armed=true;return true;}
void RunSupervisor::disarmWeightTarget(){armed=false;configuredTarget=NAN;}
bool RunSupervisor::weightTargetArmed(){return armed;}
float RunSupervisor::weightTargetGrams(){return configuredTarget;}
bool RunSupervisor::completionIsAbort(){return abortCompletion||(state.weightArmed&&state.weightState!=WeightApproachState::TargetReached);}
bool RunSupervisor::completionPending(){return measurementPending;}
bool RunSupervisor::requestFinalMeasurement()
{
    if(measurementPending||state.finalWeightMeasured||!LoadCells::profileTareValid())return false;
    measurementPending=true;measurementOnly=true;state.settlingFinalWeight=true;settleCount=0;settleWrite=0;settleStartedMs=millis();motionActive=false;motionSegmentStopStep=UINT32_MAX;
    Telemetry::event("FINAL_WEIGHT_VERIFY_STOP");return true;
}
const char* RunSupervisor::stallStateName(StallDetectionState value){return value==StallDetectionState::Candidate?"candidate":value==StallDetectionState::Confirmed?"confirmed":"monitoring";}
const char* RunSupervisor::weightStateName(WeightApproachState value)
{
    switch(value){case WeightApproachState::Calibration:return "calibration";case WeightApproachState::Bulk:return "bulk";case WeightApproachState::Refinement:return "refinement";case WeightApproachState::Approach:return "approach";case WeightApproachState::TargetReached:return "target_reached";default:return "disabled";}
}
