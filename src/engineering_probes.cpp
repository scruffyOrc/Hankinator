#include <Arduino.h>
#include "config.h"
#include "engineering_probes.h"
#include "runtime.h"
#include "telemetry.h"
#include "app_api.h"
#include "machine_interface.h"
#include "load_cells.h"

#if HANK_ENABLE_ENGINEERING_DIAGNOSTICS
namespace {
// A deliberately non-persistent characterization tool. This lets us inspect
// the arm load-path vectors before deciding whether they are suitable
// evidence for a future commissioning model. It never changes calibration.
enum class ArmProbeState:uint8_t {Idle,SettlingTare,CollectingTare,Prompt,Capturing,Complete,Failed};
ArmProbeState armProbeState=ArmProbeState::Idle;
Print* armProbeOutput=nullptr;
uint32_t armProbeStarted=0,armProbeLastSequence=0;
uint8_t armProbeReference=0,armProbePosition=0,armProbeSamples=0;
uint16_t armProbeMassCentiGrams=0;
bool armProbeSingleMass=false;
int64_t armProbeSums[Config::LoadCellChannelCount]{};
int32_t armProbeTare[Config::LoadCellChannelCount]{};

// A no-handling movement characterization.  It intentionally shares one
// stationary tare across the quiet-before / run / quiet-after phases so the
// resulting LC_DATA makes the effect of motion directly comparable.
enum class MotionProbeState:uint8_t {Idle,Taring,PreCapture,Winding,PostCapture};
MotionProbeState motionProbeState=MotionProbeState::Idle;
Print* motionProbeOutput=nullptr;
uint32_t motionProbePhaseStarted=0;
constexpr uint32_t MotionProbeCaptureMs=20000;
constexpr uint16_t MotionProbeTurns=31;
constexpr float MotionProbeSweepRps[]={2.0f,2.5f,3.0f,3.5f,4.0f};
constexpr uint8_t MotionProbeSweepCount=sizeof(MotionProbeSweepRps)/sizeof(MotionProbeSweepRps[0]);
uint8_t motionProbeSweepIndex=0;

// The preceding 31-turn characterization took about 35 seconds from the
// start of movement through normal completion.  This provides a no-STEP
// electrical/thermal control of comparable duration.
enum class MotionHoldState:uint8_t {Idle,Taring,PreCapture,Holding,PostCapture};
MotionHoldState motionHoldState=MotionHoldState::Idle;
Print* motionHoldOutput=nullptr;
uint32_t motionHoldPhaseStarted=0;
constexpr uint32_t MotionHoldDurationMs=35000;

constexpr uint8_t ArmProbeRequiredMask=uint8_t((1u<<Config::LoadCellChannelCount)-1u);

bool armProbeHealthy(const LoadCellSnapshot& snapshot)
{
    return (snapshot.healthyMask&ArmProbeRequiredMask)==ArmProbeRequiredMask;
}

const char* armProbePositionName(const uint8_t position)
{
    static const char* names[]={"front","middle","rear"};
    return position<Config::CommissioningArmPositionCount?names[position]:"?";
}

void armProbePrintPrompt()
{
    if(!armProbeOutput)return;
    const uint16_t centigrams=armProbeMassCentiGrams;
    armProbeOutput->print("ARM_PROBE_PROMPT,mass_cg,");armProbeOutput->print(centigrams);
    armProbeOutput->print(",position,");armProbeOutput->print(armProbePositionName(armProbePosition));
    armProbeOutput->println(",place mass on the low arm; then send ARM PROBE CAPTURE");
}

void armProbeStop(const char* result)
{
    LoadCells::cancelTare();
    disableMotor();
    if(armProbeOutput){armProbeOutput->print("ARM_PROBE_");armProbeOutput->println(result);}
    armProbeOutput=nullptr;armProbeState=ArmProbeState::Idle;
}

void serviceArmProbe()
{
    if(armProbeState==ArmProbeState::Idle||!armProbeOutput)return;
    if(armProbeState==ArmProbeState::SettlingTare)
    {
        if(millis()-armProbeStarted<Config::LoadCellTareMotorSettleMs)return;
        armProbeState=ArmProbeState::CollectingTare;
    }
    if(armProbeState==ArmProbeState::CollectingTare)
    {
        const LoadCellTareStatus status=LoadCells::updateTare();
        if(status==LoadCellTareStatus::Failed){armProbeState=ArmProbeState::Failed;armProbeStop("FAILED,reason,tare_unstable");return;}
        if(status!=LoadCellTareStatus::Complete)return;
        const LoadCellTareResult tare=LoadCells::tareResult();
        for(uint8_t channel=0;channel<Config::LoadCellChannelCount;channel++)armProbeTare[channel]=tare.offset[channel];
        armProbeState=ArmProbeState::Prompt;
        armProbeOutput->println("ARM_PROBE_TARE_COMPLETE,hub_held,1");
        armProbePrintPrompt();
        return;
    }
    if(armProbeState!=ArmProbeState::Capturing)return;
    const LoadCellSnapshot snapshot=LoadCells::snapshot();
    if(!armProbeHealthy(snapshot)){armProbeState=ArmProbeState::Failed;armProbeStop("FAILED,reason,load_cell_data_lost");return;}
    if(snapshot.sequence==armProbeLastSequence)return;
    armProbeLastSequence=snapshot.sequence;
    for(uint8_t channel=0;channel<Config::LoadCellChannelCount;channel++)armProbeSums[channel]+=snapshot.raw[channel];
    if(++armProbeSamples<Config::CommissioningSamples)return;

    const uint16_t centigrams=armProbeMassCentiGrams;
    armProbeOutput->print("ARM_PROBE_DATA,mass_cg,");armProbeOutput->print(centigrams);
    armProbeOutput->print(",position,");armProbeOutput->print(armProbePositionName(armProbePosition));
    armProbeOutput->print(",samples,");armProbeOutput->print(armProbeSamples);
    for(uint8_t channel=0;channel<Config::LoadCellChannelCount;channel++)
    {
        const int32_t average=int32_t(armProbeSums[channel]/armProbeSamples);
        armProbeOutput->print(",raw");armProbeOutput->print(channel+1);armProbeOutput->print(',');armProbeOutput->print(average);
        armProbeOutput->print(",tare");armProbeOutput->print(channel+1);armProbeOutput->print(',');armProbeOutput->print(armProbeTare[channel]);
        armProbeOutput->print(",delta");armProbeOutput->print(channel+1);armProbeOutput->print(',');armProbeOutput->print(average-armProbeTare[channel]);
    }
    armProbeOutput->println();

    if(++armProbePosition>=Config::CommissioningArmPositionCount)
    {
        armProbePosition=0;
        if(armProbeSingleMass)
        {
            armProbeState=ArmProbeState::Complete;
            armProbeStop("COMPLETE,no_calibration_saved");
            return;
        }
        if(++armProbeReference>=Config::CommissioningReferenceMassCount)
        {
            armProbeState=ArmProbeState::Complete;
            armProbeStop("COMPLETE,no_calibration_saved");
            return;
        }
        armProbeMassCentiGrams=Config::CommissioningReferenceMassCentiGrams[armProbeReference];
    }
    armProbeState=ArmProbeState::Prompt;
    armProbePrintPrompt();
}

bool handleArmProbeCommand(const char* command,Print& output,const bool allowed)
{
    String normalized(command);normalized.trim();normalized.toUpperCase();
    if(!normalized.startsWith("ARM PROBE"))return false;
    if(normalized=="ARM PROBE HELP"||normalized=="ARM PROBE")
    {
        output.println("ARM PROBE START [grams] | ARM PROBE CAPTURE | ARM PROBE STATUS | ARM PROBE CANCEL");
        output.println("Nine non-persistent captures: 79.43g / 111.59g / 333.53g at front / middle / rear of one low arm.");
        output.println("Add a gram value to START for one mass at front / middle / rear only; e.g. ARM PROBE START 192.28");
        return true;
    }
    if(normalized=="ARM PROBE STATUS")
    {
        output.print("ARM_PROBE_STATUS,state,");output.print(unsigned(armProbeState));
        output.print(",mass_cg,");output.print(armProbeMassCentiGrams);
        output.print(",position,");output.println(armProbePositionName(armProbePosition));
        return true;
    }
    if(normalized=="ARM PROBE CANCEL")
    {
        if(armProbeState==ArmProbeState::Idle)output.println("ARM_PROBE_IDLE");
        else armProbeStop("CANCELLED");
        return true;
    }
    if(normalized=="ARM PROBE START"||normalized.startsWith("ARM PROBE START "))
    {
        if(!allowed){output.println("BUSY: ARM PROBE is available while idle");return true;}
        if(armProbeState!=ArmProbeState::Idle){output.println("ARM_PROBE_BUSY,use ARM PROBE CANCEL first");return true;}
        if(!weightCapabilityEnabled||!armProbeHealthy(LoadCells::snapshot())){output.println("ARM_PROBE_FAILED,reason,all_four_load_cells_required");return true;}
        armProbeSingleMass=normalized.length()>15;
        uint16_t requestedMass=Config::CommissioningReferenceMassCentiGrams[0];
        if(armProbeSingleMass)
        {
            const float grams=normalized.substring(16).toFloat();
            if(grams<=0.0f||grams>1000.0f){output.println("ARM_PROBE_FAILED,reason,invalid_mass_grams");return true;}
            requestedMass=uint16_t(lroundf(grams*100.0f));
        }
        armProbeOutput=&output;armProbeReference=0;armProbePosition=0;armProbeSamples=0;armProbeMassCentiGrams=requestedMass;
        enableMotor();LoadCells::startTare();armProbeStarted=millis();armProbeState=ArmProbeState::SettlingTare;
        output.print("ARM_PROBE_TARE_STARTED,settle_ms,");output.println(Config::LoadCellTareMotorSettleMs);
        output.println("ARM_PROBE_NOTE,position one arm low before starting; this test does not move the hub or save calibration");
        return true;
    }
    if(normalized=="ARM PROBE CAPTURE")
    {
        if(armProbeState!=ArmProbeState::Prompt){output.println("ARM_PROBE_NOT_READY,wait for a prompt");return true;}
        memset(armProbeSums,0,sizeof(armProbeSums));armProbeSamples=0;armProbeLastSequence=LoadCells::snapshot().sequence;
        armProbeState=ArmProbeState::Capturing;
        output.println("ARM_PROBE_CAPTURING,samples,40");
        return true;
    }
    output.println("ARM_PROBE_UNKNOWN,send ARM PROBE HELP");return true;
}

const char* motionProbeStateName(const MotionProbeState state)
{
    switch(state)
    {
        case MotionProbeState::Taring:return "taring";
        case MotionProbeState::PreCapture:return "pre_capture";
        case MotionProbeState::Winding:return "winding";
        case MotionProbeState::PostCapture:return "post_capture";
        default:return "idle";
    }
}

float motionProbeRps()
{
    return MotionProbeSweepRps[min<uint8_t>(motionProbeSweepIndex,MotionProbeSweepCount-1)];
}

void motionProbePrintPhase(const char* phase)
{
    if(!motionProbeOutput)return;
    motionProbeOutput->print("MOTION_PROBE_");motionProbeOutput->print(phase);
    motionProbeOutput->print(",run,");motionProbeOutput->print(motionProbeSweepIndex+1);
    motionProbeOutput->print(",of,");motionProbeOutput->print(MotionProbeSweepCount);
    motionProbeOutput->print(",cruise_rps,");motionProbeOutput->println(motionProbeRps(),2);
}

bool motionProbeStartTare()
{
    if(!Machine::startWeightDiagnosticsTare())return false;
    motionProbeState=MotionProbeState::Taring;
    motionProbePrintPhase("TARE_STARTED");
    return true;
}

void motionProbeStopCapture()
{
    if(motionProbeOutput&&(motionProbeState==MotionProbeState::PreCapture||motionProbeState==MotionProbeState::PostCapture))
        Telemetry::executeCommand("STOP WEIGHT",*motionProbeOutput,true);
}

void motionProbeFinish(const char* result)
{
    motionProbeStopCapture();
    if(motionProbeOutput){motionProbeOutput->print("MOTION_PROBE_");motionProbeOutput->println(result);}
    motionProbeOutput=nullptr;
    motionProbeState=MotionProbeState::Idle;
}

void serviceMotionProbe()
{
    if(motionProbeState==MotionProbeState::Idle||!motionProbeOutput)return;

    if(motionProbeState==MotionProbeState::Taring)
    {
        const Machine::TareStatus tare=Machine::diagnostics().tareStatus;
        if(tare==Machine::TareStatus::Failed){motionProbeFinish("FAILED,reason,tare_failed");return;}
        if(tare!=Machine::TareStatus::Complete)return;
        motionProbePrintPhase("TARE_COMPLETE");
        motionProbePrintPhase("PRE_CAPTURE");
        motionProbeOutput->print("MOTION_PROBE_CAPTURE_SECONDS,");motionProbeOutput->println(MotionProbeCaptureMs/1000);
        Telemetry::executeCommand("START WEIGHT",*motionProbeOutput,true);
        motionProbePhaseStarted=millis();
        motionProbeState=MotionProbeState::PreCapture;
        return;
    }

    if(motionProbeState==MotionProbeState::PreCapture)
    {
        if(millis()-motionProbePhaseStarted<MotionProbeCaptureMs)return;
        motionProbeStopCapture();
        motionProbePrintPhase("WIND_START");
        motionProbeOutput->print("MOTION_PROBE_RUN_DETAILS,turns,");motionProbeOutput->print(MotionProbeTurns);
        motionProbeOutput->println(",hall_feedback,ignored");
        if(!Machine::startMotionCharacterization(MotionProbeTurns,motionProbeRps(),true)){motionProbeFinish("FAILED,reason,wind_start_rejected");return;}
        motionProbeState=MotionProbeState::Winding;
        return;
    }

    if(motionProbeState==MotionProbeState::Winding)
    {
        const Machine::WindingSnapshot run=Machine::winding();
        if(run.stage==Machine::WindingStage::Fault||run.stage==Machine::WindingStage::HallFailed||run.stage==Machine::WindingStage::TareFailed||
           (run.stage==Machine::WindingStage::Complete&&run.finalAborted))
        {
            motionProbeFinish("FAILED,reason,winding_aborted");
            return;
        }
        if(run.stage!=Machine::WindingStage::Complete)return;
        motionProbePrintPhase("WIND_COMPLETE");
        motionProbeOutput->print("MOTION_PROBE_FINAL_TURNS,");motionProbeOutput->println(run.finalTurns);
        Telemetry::executeCommand("START WEIGHT",*motionProbeOutput,true);
        motionProbePhaseStarted=millis();
        motionProbeState=MotionProbeState::PostCapture;
        motionProbePrintPhase("POST_CAPTURE");
        motionProbeOutput->print("MOTION_PROBE_CAPTURE_SECONDS,");motionProbeOutput->println(MotionProbeCaptureMs/1000);
        return;
    }

    if(motionProbeState==MotionProbeState::PostCapture&&millis()-motionProbePhaseStarted>=MotionProbeCaptureMs)
    {
        motionProbeStopCapture();
        Machine::exitWindingWorkflow();
        if(++motionProbeSweepIndex>=MotionProbeSweepCount){motionProbeFinish("SWEEP_COMPLETE");return;}
        if(!motionProbeStartTare()){motionProbeFinish("FAILED,reason,next_tare_rejected");return;}
    }
}

bool handleMotionProbeCommand(const char* command,Print& output,const bool allowed)
{
    String normalized(command);normalized.trim();normalized.toUpperCase();
    if(!normalized.startsWith("MOTION PROBE"))return false;
    if(normalized=="MOTION PROBE"||normalized=="MOTION PROBE HELP")
    {
        output.println("MOTION PROBE SWEEP START | MOTION PROBE STATUS | MOTION PROBE CANCEL");
        output.println("Five independent cycles at 2.00 / 2.50 / 3.00 / 3.50 / 4.00 RPS: tare, 20 s LC_DATA, 31 ramped belt-on turns, 20 s LC_DATA. Hall feedback is ignored.");
        return true;
    }
    if(normalized=="MOTION PROBE STATUS")
    {
        output.print("MOTION_PROBE_STATUS,state,");output.print(motionProbeStateName(motionProbeState));
        output.print(",run,");output.print(motionProbeSweepIndex+1);
        output.print(",of,");output.print(MotionProbeSweepCount);
        output.print(",cruise_rps,");output.print(motionProbeRps(),2);
        output.println(",hall_feedback,ignored");
        return true;
    }
    if(normalized=="MOTION PROBE CANCEL")
    {
        if(motionProbeState==MotionProbeState::Idle){output.println("MOTION_PROBE_IDLE");return true;}
        if(motionProbeState==MotionProbeState::Winding||motionProbeState==MotionProbeState::PostCapture)Machine::exitWindingWorkflow();
        else Machine::closeWeightDiagnostics();
        motionProbeFinish("CANCELLED");
        return true;
    }
    if(normalized=="MOTION PROBE SWEEP START")
    {
        if(!allowed){output.println("BUSY: MOTION PROBE is available while idle");return true;}
        if(motionProbeState!=MotionProbeState::Idle){output.println("MOTION_PROBE_BUSY,use MOTION PROBE CANCEL first");return true;}
        if(!weightCapabilityEnabled||!armProbeHealthy(LoadCells::snapshot())){output.println("MOTION_PROBE_FAILED,reason,all_four_load_cells_required");return true;}
        motionProbeOutput=&output;
        motionProbeSweepIndex=0;
        output.print("MOTION_PROBE_SWEEP_STARTED,runs,");output.print(MotionProbeSweepCount);
        output.print(",turns_per_run,");output.print(MotionProbeTurns);
        output.print(",capture_seconds,");output.print(MotionProbeCaptureMs/1000);
        output.println(",hall_feedback,ignored");
        if(!motionProbeStartTare()){motionProbeFinish("FAILED,reason,could_not_start_tare");return true;}
        return true;
    }
    output.println("MOTION_PROBE_UNKNOWN,send MOTION PROBE HELP");
    return true;
}

const char* motionHoldStateName(const MotionHoldState state)
{
    switch(state)
    {
        case MotionHoldState::Taring:return "taring";
        case MotionHoldState::PreCapture:return "pre_capture";
        case MotionHoldState::Holding:return "holding";
        case MotionHoldState::PostCapture:return "post_capture";
        default:return "idle";
    }
}

void motionHoldStopCapture()
{
    if(motionHoldOutput&&(motionHoldState==MotionHoldState::PreCapture||motionHoldState==MotionHoldState::PostCapture))
        Telemetry::executeCommand("STOP WEIGHT",*motionHoldOutput,true);
}

void motionHoldFinish(const char* result)
{
    motionHoldStopCapture();
    if(motionHoldOutput){motionHoldOutput->print("MOTION_HOLD_");motionHoldOutput->println(result);}
    motionHoldOutput=nullptr;
    motionHoldState=MotionHoldState::Idle;
}

void serviceMotionHold()
{
    if(motionHoldState==MotionHoldState::Idle||!motionHoldOutput)return;
    if(motionHoldState==MotionHoldState::Taring)
    {
        const Machine::TareStatus tare=Machine::diagnostics().tareStatus;
        if(tare==Machine::TareStatus::Failed){motionHoldFinish("FAILED,reason,tare_failed");return;}
        if(tare!=Machine::TareStatus::Complete)return;
        motionHoldOutput->print("MOTION_HOLD_PRE_CAPTURE,seconds,");motionHoldOutput->println(MotionProbeCaptureMs/1000);
        Telemetry::executeCommand("START WEIGHT",*motionHoldOutput,true);
        motionHoldPhaseStarted=millis();
        motionHoldState=MotionHoldState::PreCapture;
        return;
    }
    if(motionHoldState==MotionHoldState::PreCapture)
    {
        if(millis()-motionHoldPhaseStarted<MotionProbeCaptureMs)return;
        motionHoldStopCapture();
        // The tare path has already energized the driver.  Keep it enabled
        // without issuing STEP pulses; this deliberately never starts Hall.
        enableMotor();
        motionHoldPhaseStarted=millis();
        motionHoldState=MotionHoldState::Holding;
        motionHoldOutput->print("MOTION_HOLD_START,seconds,");motionHoldOutput->println(MotionHoldDurationMs/1000);
        return;
    }
    if(motionHoldState==MotionHoldState::Holding)
    {
        if(millis()-motionHoldPhaseStarted<MotionHoldDurationMs)return;
        motionHoldOutput->print("MOTION_HOLD_COMPLETE,seconds,");motionHoldOutput->println(MotionHoldDurationMs/1000);
        Telemetry::executeCommand("START WEIGHT",*motionHoldOutput,true);
        motionHoldPhaseStarted=millis();
        motionHoldState=MotionHoldState::PostCapture;
        motionHoldOutput->print("MOTION_HOLD_POST_CAPTURE,seconds,");motionHoldOutput->println(MotionProbeCaptureMs/1000);
        return;
    }
    if(motionHoldState==MotionHoldState::PostCapture&&millis()-motionHoldPhaseStarted>=MotionProbeCaptureMs)
    {
        motionHoldStopCapture();
        Machine::closeWeightDiagnostics();
        motionHoldFinish("COMPLETE");
    }
}

bool handleMotionHoldCommand(const char* command,Print& output,const bool allowed)
{
    String normalized(command);normalized.trim();normalized.toUpperCase();
    if(!normalized.startsWith("MOTION HOLD"))return false;
    if(normalized=="MOTION HOLD"||normalized=="MOTION HOLD HELP")
    {
        output.println("MOTION HOLD START | MOTION HOLD STATUS | MOTION HOLD CANCEL");
        output.println("Stationary tare, 20 s LC_DATA, 35 s motor hold with no STEP pulses, then 20 s LC_DATA.");
        return true;
    }
    if(normalized=="MOTION HOLD STATUS")
    {
        output.print("MOTION_HOLD_STATUS,state,");output.println(motionHoldStateName(motionHoldState));
        return true;
    }
    if(normalized=="MOTION HOLD CANCEL")
    {
        if(motionHoldState==MotionHoldState::Idle){output.println("MOTION_HOLD_IDLE");return true;}
        Machine::closeWeightDiagnostics();
        motionHoldFinish("CANCELLED");
        return true;
    }
    if(normalized=="MOTION HOLD START")
    {
        if(!allowed){output.println("BUSY: MOTION HOLD is available while idle");return true;}
        if(motionProbeState!=MotionProbeState::Idle||motionHoldState!=MotionHoldState::Idle){output.println("MOTION_HOLD_BUSY,another motion characterization is active");return true;}
        if(!weightCapabilityEnabled||!armProbeHealthy(LoadCells::snapshot())){output.println("MOTION_HOLD_FAILED,reason,all_four_load_cells_required");return true;}
        if(!Machine::startWeightDiagnosticsTare()){output.println("MOTION_HOLD_FAILED,reason,could_not_start_tare");return true;}
        motionHoldOutput=&output;
        motionHoldState=MotionHoldState::Taring;
        output.print("MOTION_HOLD_TARE_STARTED,pre_capture_seconds,");output.print(MotionProbeCaptureMs/1000);
        output.print(",hold_seconds,");output.print(MotionHoldDurationMs/1000);
        output.print(",post_capture_seconds,");output.println(MotionProbeCaptureMs/1000);
        return true;
    }
    output.println("MOTION_HOLD_UNKNOWN,send MOTION HOLD HELP");
    return true;
}
}

namespace EngineeringProbes {
bool handleCommand(const char* command,Print& output,const bool allowed)
{
    return handleMotionHoldCommand(command,output,allowed)
        || handleMotionProbeCommand(command,output,allowed)
        || handleArmProbeCommand(command,output,allowed);
}

void poll()
{
    serviceMotionHold();
    serviceMotionProbe();
    serviceArmProbe();
}
}
#else
namespace EngineeringProbes {
bool handleCommand(const char*,Print&,bool){return false;}
void poll(){}
}
#endif
