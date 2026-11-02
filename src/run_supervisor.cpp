#include "run_supervisor.h"
#include "app_api.h"
#include "config.h"
#include "load_cells.h"
#include "runtime.h"
#include "telemetry.h"
#include "tmc_driver.h"

namespace {
RunSupervisorSnapshot state{};
bool finalMeasurementAttempted=false;
bool armed=false,abortCompletion=false,measurementPending=false,measurementOnly=false;
enum class TensionReliefPhase:uint8_t {Idle,WaitingBeforeReverse,BackingOff,Measuring,Recovering};
TensionReliefPhase tensionReliefPhase=TensionReliefPhase::Idle;
float recoveredMeasurementGrams=NAN;
float configuredTarget=NAN;
uint32_t lastServiceMs=0,lastWeightSequence=0;
uint32_t stageStartStep=0,lastMeasurementStep=0;
float lastMeasurementWeight=0.0f;
float settleWindow[Config::WeightFinalSettleSamples]{};
uint8_t settleCount=0,settleWrite=0;
uint32_t settleStartedMs=0;
uint32_t tensionReliefWaitStartedMs=0;
StallRecovery::Controller recovery({
    Config::StallReducedMotorRps,Config::StallSlowdownRate,Config::StallRecoveryRampRate,Config::StallRecoveryMinimumRps,
    Config::StallConfirmMs,Config::StallSevereMs,Config::StallPersistentMs,Config::StallInvalidMs,Config::StallStableMs,
    Config::StallProofTimeoutMs,Config::StepsPerHubRev,
    {Config::StallSpeedFloors[0],Config::StallSpeedFloors[1],Config::StallSpeedFloors[2]},Config::StallSevereSg,Config::StallHealthyHysteresis,Config::StallMaxHolds});
uint32_t lastStallPoll=0;
uint32_t lastSgSequence=0;
bool discardWeightSegment=false;
bool sgUnavailableAnnounced=false;
bool staleStatusCacheAnnounced=false;

float limitBatchStopSpeed(float stageSpeed)
{
    if(!state.weightArmed||motionSegmentStopStep==UINT32_MAX)return stageSpeed;
    const uint32_t current=safeCurrentStepCount();
    if(current>=motionSegmentStopStep)return stageSpeed;
    const uint32_t decelSteps=uint32_t(Config::WeightBatchDecelTurns*Config::StepsPerHubRev);
    if(!decelSteps)return stageSpeed;
    const uint32_t remaining=motionSegmentStopStep-current;
    if(remaining>=decelSteps)return stageSpeed;
    // A short batch may still be in its launch profile. Do not raise that
    // deliberately gentle speed solely because the batch is about to stop.
    const float endSpeed=min(stageSpeed,Config::WeightBatchEndMotorRps);
    const float progress=remaining/float(decelSteps);
    return min(stageSpeed,endSpeed+(stageSpeed-endSpeed)*progress);
}
void syncRecovery() {
    state.recovery=recovery.phase;state.countUncertain=recovery.uncertain;
    state.stallIncidents=recovery.incidents;state.sgBaseline=recovery.baseline;
}

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
    cancelAuxiliaryMotion();tensionReliefPhase=TensionReliefPhase::Idle;
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
    // Auxiliary tension relief uses the same hardware STEP alarm as winding.
    // A PAUSED state deliberately suppresses every alarm pulse, so leave that
    // gate before beginning the small measurement-only reverse move. This
    // does not resume the job: motionActive stays false until the auxiliary
    // move is explicitly started below.
    if(pauseState==PauseState::PAUSED||pauseState==PauseState::RAMPING_UP)pauseState=PauseState::RUNNING;
    measurementPending=true;state.settlingFinalWeight=true;
    motionActive=false;motionSegmentStopStep=UINT32_MAX;
    // A complete stop before changing direction avoids loading the belt with
    // the hub's remaining inertia. This intentionally applies to every
    // stopped weight measurement, so probes and final completion behave alike.
    tensionReliefWaitStartedMs=millis();
    tensionReliefPhase=TensionReliefPhase::WaitingBeforeReverse;
    Telemetry::event("WEIGHT_TENSION_RELIEF_WAIT");
}

void startTensionReliefReverse()
{
    if(!startAuxiliaryHubMotion(false,Config::WeightTensionReliefSteps,Config::WeightTensionReliefMotorRps))
    {
        abortWeightRun("WEIGHT_TENSION_RELIEF_START_ABORT");return;
    }
    tensionReliefPhase=TensionReliefPhase::BackingOff;
    Telemetry::event("WEIGHT_TENSION_RELIEF_REVERSE_START");
}

void startStoppedMeasurementWindow()
{
    settleCount=0;settleWrite=0;settleStartedMs=millis();lastWeightSequence=0;
    tensionReliefPhase=TensionReliefPhase::Measuring;
    Telemetry::event("WEIGHT_STOPPED_MEASUREMENT_START");
}

void recoverAfterMeasurement(float grams)
{
    // Count mode finishes with tension released for unloading.
    if(measurementOnly&&autoCountMode)
    {
        tensionReliefPhase=TensionReliefPhase::Idle;
        measurementOnly=false;measurementPending=false;state.settlingFinalWeight=false;
        state.filteredWeightGrams=grams;state.finalWeightMeasured=!isnan(grams);
        if(isnan(grams))abortCompletion=true;
        Telemetry::event("FINAL_WEIGHT_SETTLED");return;
    }
    recoveredMeasurementGrams=grams;state.settlingFinalWeight=false;
    if(!startAuxiliaryHubMotion(true,Config::WeightTensionReliefSteps,Config::WeightTensionReliefMotorRps))
    {
        abortWeightRun("WEIGHT_TENSION_RECOVERY_START_ABORT");return;
    }
    tensionReliefPhase=TensionReliefPhase::Recovering;
    Telemetry::event("WEIGHT_TENSION_RECOVERY_FORWARD_START");
}

void updateSlope(float grams,uint32_t step)
{
    if(state.weightState==WeightApproachState::Calibration)
    {
        const float turns=(step-lastMeasurementStep)/float(Config::StepsPerHubRev);
        state.gramsPerTurn=turns>0.0f?(grams-lastMeasurementWeight)/turns:NAN;
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
        measurementOnly=false;measurementPending=false;state.settlingFinalWeight=false;state.finalWeightMeasured=!isnan(grams);
        Telemetry::event("FINAL_WEIGHT_SETTLED");return;
    }
    if(grams>=configuredTarget){finishAtWeight(grams);return;}
    if(discardWeightSegment)
    {
        discardWeightSegment=false;
        // The disturbed segment cannot teach grams per commanded turn.
        lastMeasurementWeight=grams;lastMeasurementStep=step;
        state.gramsPerTurn=NAN;
        startBatch(WeightApproachState::Calibration,1,"WEIGHT_RELEARN_AFTER_STALL");
        return;
    }
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
    if(millis()-settleStartedMs<Config::WeightStoppedMechanicalSettleMs)return;
    const LoadCellSnapshot loadCells=LoadCells::snapshot();
    const bool timedOut=millis()-settleStartedMs>=Config::WeightFinalSettleTimeoutMs;
    const uint8_t requiredMask=uint8_t((1u<<Config::LoadCellChannelCount)-1u);
    if(loadCells.sequence==lastWeightSequence)
    {
        if(timedOut)
        {
            if(measurementOnly){Telemetry::event("FINAL_WEIGHT_UNAVAILABLE");recoverAfterMeasurement(NAN);}
            else abortWeightRun("LOAD_CELL_UNHEALTHY_AT_MEASUREMENT_ABORT");
        }
        return;
    }
    lastWeightSequence=loadCells.sequence;
    const float grams=LoadCells::stoppedWeightGrams(loadCells);
    if(isnan(grams))
    {
        if(timedOut)
        {
            if(measurementOnly){Telemetry::event("FINAL_WEIGHT_UNAVAILABLE");recoverAfterMeasurement(NAN);}
            else abortWeightRun("LOAD_CELL_UNHEALTHY_AT_MEASUREMENT_ABORT");
        }
        return;
    }
    settleWindow[settleWrite]=grams;settleWrite=uint8_t((settleWrite+1)%Config::WeightFinalSettleSamples);
    if(settleCount<Config::WeightFinalSettleSamples)settleCount++;
    float minimum=settleWindow[0],maximum=settleWindow[0];
    for(uint8_t i=1;i<settleCount;i++){minimum=min(minimum,settleWindow[i]);maximum=max(maximum,settleWindow[i]);}
    state.filteredWeightGrams=median(settleWindow,settleCount);state.weightSpreadGrams=maximum-minimum;
    if(settleCount==Config::WeightFinalSettleSamples&&state.weightSpreadGrams<=Config::WeightFinalSettleMaxRangeGrams){recoverAfterMeasurement(state.filteredWeightGrams);return;}
    if(timedOut)
    {
        if(measurementOnly&&settleCount&&!autoCountMode){Telemetry::event("FINAL_WEIGHT_SETTLE_TIMEOUT");recoverAfterMeasurement(state.filteredWeightGrams);}
        else
        {
            abortWeightRun((loadCells.healthyMask&requiredMask)==requiredMask?"WEIGHT_SETTLE_TIMEOUT_ABORT":"LOAD_CELL_UNHEALTHY_AT_MEASUREMENT_ABORT");
        }
    }
}

void updateStall()
{
    if(!Config::EnableStallDetection||!Config::EnableStallRecovery)return;
    const uint32_t now=millis();
    if(now-lastStallPoll<Config::StallPollMs)return;
    lastStallPoll=now;
    const TmcDriver::Sample driver=TmcDriver::sample();
    state.sgResult=driver.valid?driver.sg:0xFFFF;
    // Do not classify the same cached conversion twice. Still process stale
    // data and driver faults on every supervisory tick.
    // The UART status request can be late while other foreground work is
    // happening, even though the last completed DRV_STATUS was healthy. A
    // stale reply is not an electrical fault. Startup is checked strictly;
    // during a run retain the most recent status so Hall and DIAG can remain
    // the active feedback paths instead of generating a false driver hold.
    const bool cachedStatus=!driver.statusValid&&TmcDriver::hasCachedDrvStatus();
    const uint32_t status=driver.statusValid?driver.status:TmcDriver::cachedDrvStatus();
    const bool statusValid=driver.statusValid||cachedStatus;
    const bool hardFault=statusValid&&(status&0x3FUL);
    if(driver.valid&&driver.sequence==lastSgSequence&&!hardFault)return;
    lastSgSequence=driver.sequence;
    const bool diag=TmcDriver::consumeDiag();
    const bool production=motionActive&&!auxiliaryMotionActive()&&!measurementPending;
    const bool normalPause=pauseState==PauseState::RUNNING;
    const bool sgValid=driver.valid&&(driver.status&(1UL<<30)); // StallGuard4 requires StealthChop.
    if(production&&cachedStatus&&!staleStatusCacheAnnounced)
    {
        staleStatusCacheAnnounced=true;
        Telemetry::event("STALL_STATUS_STALE_USING_CACHE");
    }
    if(production&&driver.statusValid&&!sgValid&&!sgUnavailableAnnounced)
    {
        sgUnavailableAnnounced=true;
        Telemetry::event("STALL_SG_UNAVAILABLE_DIAG_ONLY");
    }
    const auto action=recovery.update({now,safeCurrentStepCount(),currentMotorRPS,driver.sg,
        production,production&&normalPause,statusValid,sgValid,diag,hardFault});
    state.stallState=recovery.candidate?StallDetectionState::Candidate:StallDetectionState::Monitoring;
    if(action!=StallRecovery::Action::None)state.stallState=StallDetectionState::Confirmed;
    syncRecovery();
    if(action==StallRecovery::Action::Slow||action==StallRecovery::Action::Lock) {
        discardWeightSegment=state.weightArmed;
        emitStall(action==StallRecovery::Action::Slow?"STALL_SLOWDOWN":"STALL_SLOW_LOCK");
    } else if(action==StallRecovery::Action::Restore)emitStall("STALL_HEALTHY_TURN_RAMP_UP");
    else if(action==StallRecovery::Action::Hold) {
        state.clearanceReason=sgValid?ClearanceReason::Load:(driver.valid?ClearanceReason::DriverMode:ClearanceReason::DriverData);
        discardWeightSegment=state.weightArmed;
        // Stop immediately without automatically reversing an unknown obstruction.
        motionActive=false;requestedStepRateHz=0;currentMotorRPS=0;
        pauseState=PauseState::RUNNING;
        emitStall((sgValid||diag)?"STALL_CHECK_YARN_HOLD":"STALL_DRIVER_DATA_HOLD");
    } else if(action==StallRecovery::Action::Abort) {
        abortWeightRun(hardFault?"DRIVER_ELECTRICAL_FAULT_ABORT":"STALL_RETRY_LIMIT_ABORT");
        disableMotor();
    }
}
}

void RunSupervisor::beginRun()
{
    state=RunSupervisorSnapshot{};state.weightArmed=armed&&weightCapabilityEnabled&&!autoCountMode&&LoadCells::tareValid();state.targetWeightGrams=configuredTarget;
    finalMeasurementAttempted=false;
    abortCompletion=false;measurementPending=false;measurementOnly=false;tensionReliefPhase=TensionReliefPhase::Idle;tensionReliefWaitStartedMs=0;recoveredMeasurementGrams=NAN;lastServiceMs=0;lastWeightSequence=0;sgUnavailableAnnounced=false;staleStatusCacheAnnounced=false;
    recovery.reset(selectedCruiseMotorRPS,millis());lastStallPoll=millis();lastSgSequence=0;discardWeightSegment=false;
    TmcDriver::consumeDiag();syncRecovery();
    lastMeasurementStep=0;lastMeasurementWeight=0.0f;stageStartStep=0;
    if(state.weightArmed)
    {
        state.weightState=WeightApproachState::Calibration;state.plannedStageTurns=Config::WeightEstimateCalibrationTurns;
        motionSegmentStopStep=uint32_t(Config::WeightEstimateCalibrationTurns)*Config::StepsPerHubRev;
        eventWithValues("WEIGHT_CALIBRATION_BATCH_START",0.0f,NAN,Config::WeightEstimateCalibrationTurns);
    }
    else {state.weightState=WeightApproachState::Disabled;motionSegmentStopStep=UINT32_MAX;}
    // A TMC2209 may return a healthy DRV_STATUS while SG_RESULT is unavailable.
    // In DIAG-only mode, startup requires that status response, not numeric SG.
    if(Config::EnableStallRecovery&&!TmcDriver::sample().statusValid) {
        state.clearanceReason=ClearanceReason::DriverData;
        recovery.requireClearance();syncRecovery();
        Telemetry::event("STALL_START_REFUSED_DRIVER_DATA");
    }
}

void RunSupervisor::service()
{
    updateStall();
    if(waitingForClearance()||abortCompletion)return;
    const uint32_t now=millis();if(now-lastServiceMs<Config::ControlSupervisorIntervalMs)return;lastServiceMs=now;
    // Load cells remain observational while the hub is moving. Weight control
    // only requires them after tension relief, while stopped; a missed HX711
    // conversion during motion must not terminate an otherwise healthy run.
    if(tensionReliefPhase==TensionReliefPhase::WaitingBeforeReverse)
    {
        if(now-tensionReliefWaitStartedMs>=Config::WeightTensionReliefPreReverseSettleMs)startTensionReliefReverse();
    }
    else if(tensionReliefPhase==TensionReliefPhase::BackingOff&&consumeAuxiliaryMotionComplete())startStoppedMeasurementWindow();
    else if(tensionReliefPhase==TensionReliefPhase::Recovering&&consumeAuxiliaryMotionComplete())
    {
        tensionReliefPhase=TensionReliefPhase::Idle;
        Telemetry::event("WEIGHT_TENSION_RECOVERY_COMPLETE");
        handleStoppedWeight(recoveredMeasurementGrams);
    }
    if(state.weightArmed&&!measurementPending&&!motionActive&&tensionReliefPhase==TensionReliefPhase::Idle&&state.weightState!=WeightApproachState::TargetReached)beginStoppedMeasurement();
    if(measurementPending&&tensionReliefPhase==TensionReliefPhase::Measuring)updateStoppedMeasurement();
}

float RunSupervisor::limitMotorRps(float requestedRps)
{
    if(waitingForClearance()||abortCompletion)return 0.0f;
    if(Config::EnableStallRecovery)requestedRps=min(requestedRps,recovery.cap);
    if(state.settlingFinalWeight||state.weightState==WeightApproachState::TargetReached)return 0.0f;
    if(!state.weightArmed)return requestedRps;
    float limited=requestedRps;
    if(state.weightState==WeightApproachState::Calibration)limited=min(limited,Config::WeightEstimateCalibrationMotorRps);
    else if(state.weightState==WeightApproachState::Approach)limited=min(limited,Config::WeightApproachMotorRps);
    else
    {
        const uint32_t step=safeCurrentStepCount();const float rampSteps=2.0f*Config::StepsPerHubRev;
        const float started=min(1.0f,(step-stageStartStep)/rampSteps);
        limited=min(limited,Config::WeightApproachMotorRps+(selectedCruiseMotorRPS-Config::WeightApproachMotorRps)*started);
    }
    return limitBatchStopSpeed(limited);
}

RunSupervisorSnapshot RunSupervisor::snapshot(){return state;}
bool RunSupervisor::waitingForClearance(){return recovery.phase==StallRecovery::Phase::Held;}
void RunSupervisor::holdForHallFault(const char* reason)
{
    if(abortCompletion||waitingForClearance())return;
    state.clearanceReason=ClearanceReason::Hall;
    discardWeightSegment=state.weightArmed;
    const auto action=recovery.requireClearance();syncRecovery();
    motionActive=false;requestedStepRateHz=0;currentMotorRPS=0;pauseState=PauseState::RUNNING;
    if(action==StallRecovery::Action::Abort){abortWeightRun("HALL_RETRY_LIMIT_ABORT");disableMotor();}
    Telemetry::event(reason);
}
bool RunSupervisor::confirmClearance()
{
    if(!waitingForClearance())return false;
    const auto driver=TmcDriver::sample();
    // A status reply can expire while the user examines a Hall fault. That
    // expiry is not a new driver fault. Startup remains strict, but a user
    // clear may use the last valid status from this powered-on session.
    const bool cachedStatus=!driver.statusValid&&TmcDriver::hasCachedDrvStatus();
    const uint32_t status=driver.statusValid?driver.status:TmcDriver::cachedDrvStatus();
    // A numeric SG_RESULT is optional in DIAG-only operation. Resume only
    // needs a usable status reply, the expected StealthChop mode, and no
    // electrical driver flags.
    if((!driver.statusValid&&!cachedStatus)||!(status&(1UL<<30))||(status&0x3FUL)) {
        if(!driver.statusValid&&!cachedStatus)state.clearanceReason=ClearanceReason::DriverData;
        else if(!(status&(1UL<<30)))state.clearanceReason=ClearanceReason::DriverMode;
        Telemetry::event("STALL_CLEAR_REFUSED_DRIVER");return false;
    }
    if(cachedStatus)Telemetry::event("STALL_CLEAR_USING_CACHED_DRIVER_STATUS");
    const bool hallRecovery=state.clearanceReason==ClearanceReason::Hall;
    if(!recovery.confirm(millis(),safeCurrentStepCount(),Config::LaunchMotorRps,hallRecovery))return false;
    state.clearanceReason=ClearanceReason::None;
    syncRecovery();pauseState=PauseState::RUNNING;
    if(hallRecovery)
    {
        Telemetry::event(recovery.phase==StallRecovery::Phase::Proving?
            "HALL_RECOVERY_PROVING_ONE_TURN":"HALL_RECOVERY_LOCKED_REMAINDER");
    }
    Telemetry::event("STALL_USER_CLEAR_RESUME_SLOW");
    if(state.weightArmed) {
        discardWeightSegment=true;beginStoppedMeasurement();
    } else if(safeCurrentStepCount()<targetStepCount) {
        currentMotorRPS=Config::LaunchMotorRps;
        requestedStepRateHz=stepsPerSecondForMotorRPS(currentMotorRPS);
        motionActive=true;requestStepGeneratorWake();
    }
    return true;
}
bool RunSupervisor::armWeightTarget(float grams){if(grams<Config::MinimumWeightTargetGrams||grams>Config::MaximumWeightTargetGrams)return false;configuredTarget=grams;armed=true;return true;}
void RunSupervisor::disarmWeightTarget(){armed=false;configuredTarget=NAN;}
bool RunSupervisor::weightTargetArmed(){return armed;}
float RunSupervisor::weightTargetGrams(){return configuredTarget;}
bool RunSupervisor::completionIsAbort(){return abortCompletion||(state.weightArmed&&state.weightState!=WeightApproachState::TargetReached);}
bool RunSupervisor::completionPending()
{
    // The hardware alarm stops exactly at a batch boundary. The main loop can
    // observe that stop before the 100 ms supervisor tick starts measurement;
    // keep ownership here so the app cannot mistake it for end-of-run.
    return waitingForClearance()||measurementPending||(state.weightArmed&&!abortCompletion&&state.weightState!=WeightApproachState::TargetReached&&!motionActive);
}
bool RunSupervisor::requestFinalMeasurement()
{
    if(finalMeasurementAttempted||abortCompletion||measurementPending||state.finalWeightMeasured||!LoadCells::tareValid())return false;
    finalMeasurementAttempted=true;
    measurementOnly=true;motionActive=false;motionSegmentStopStep=UINT32_MAX;
    Telemetry::event("FINAL_WEIGHT_VERIFY_STOP");beginStoppedMeasurement();return measurementPending;
}
const char* RunSupervisor::recoveryName(StallRecovery::Phase phase)
{
    switch(phase){
        case StallRecovery::Phase::Slowing:return "slowing";
        case StallRecovery::Phase::Proving:return "healthy_turn";
        case StallRecovery::Phase::Ramping:return "recovering";
        case StallRecovery::Phase::Locked:return "slow_locked";
        case StallRecovery::Phase::Held:return "check_yarn";
        case StallRecovery::Phase::Aborted:return "aborted";
        default:return "normal";
    }
}
const char* RunSupervisor::stallStateName(StallDetectionState value){return value==StallDetectionState::Candidate?"candidate":value==StallDetectionState::Confirmed?"confirmed":"monitoring";}
const char* RunSupervisor::weightStateName(WeightApproachState value)
{
    switch(value){case WeightApproachState::Calibration:return "calibration";case WeightApproachState::Bulk:return "bulk";case WeightApproachState::Refinement:return "refinement";case WeightApproachState::Approach:return "approach";case WeightApproachState::TargetReached:return "target_reached";default:return "disabled";}
}
