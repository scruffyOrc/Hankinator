#include <Arduino.h>
#include <SerialBT.h>
#include <BluetoothLock.h>
#include "diagnostics_transport.h"
#include "runtime.h"
#include "telemetry.h"
#include "product_mode.h"
#include "hall_sensor.h"
#include "app_api.h"
#include "firmware_update.h"
#include "machine_interface.h"

namespace {
constexpr size_t CommandCapacity=96;

struct CommandBuffer {
    char data[CommandCapacity]{};
    size_t length=0;
};

CommandBuffer usbCommand;
CommandBuffer bluetoothCommand;
bool bluetoothConnected=false;
bool bluetoothRunning=false;
uint32_t pairingDeadline=0;
bool telemetryHeaderSent=false;
bool telemetryRunActive=false;
uint32_t lastTelemetryStreamMs=0;
const char* telemetryYarnWeight="";
const char* telemetrySkeinSize="";
uint16_t telemetryTargetTurns=0;

enum class WifiSetupStage:uint8_t {Idle,Scanning,Selecting,Password,Connecting};
WifiSetupStage wifiSetupStage=WifiSetupStage::Idle;
Print* wifiSetupOutput=nullptr;
bool wifiSetupOverBluetooth=false;
Print* wifiUpdateOutput=nullptr;

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

enum class BluetoothEvent:uint8_t {
    StackState,
    ConnectionComplete,
    AuthenticationComplete,
    PairingComplete,
    LinkKeyRequest,
    LinkKeyCreated,
    ConfirmationRequest,
    RfcommIncoming,
    RfcommOpened,
    RfcommClosed,
    Disconnected
};

struct BluetoothEventRecord {BluetoothEvent event;uint8_t status;};
constexpr uint8_t BluetoothEventCapacity=16;
BluetoothEventRecord bluetoothEvents[BluetoothEventCapacity]{};
volatile uint8_t bluetoothEventWrite=0,bluetoothEventRead=0;
btstack_packet_callback_registration_t bluetoothEventRegistration{};
bool bluetoothEventHandlerRegistered=false;

void queueBluetoothEvent(BluetoothEvent event,uint8_t status=0)
{
    const uint8_t write=bluetoothEventWrite;
    const uint8_t next=uint8_t((write+1)%BluetoothEventCapacity);
    if(next==bluetoothEventRead)return;
    bluetoothEvents[write]={event,status};
    bluetoothEventWrite=next;
}

void bluetoothEventHandler(uint8_t packetType,uint16_t,uint8_t* packet,uint16_t)
{
    if(packetType!=HCI_EVENT_PACKET)return;
    switch(hci_event_packet_get_type(packet))
    {
        case BTSTACK_EVENT_STATE:queueBluetoothEvent(BluetoothEvent::StackState,btstack_event_state_get_state(packet));break;
        case HCI_EVENT_CONNECTION_COMPLETE:queueBluetoothEvent(BluetoothEvent::ConnectionComplete,hci_event_connection_complete_get_status(packet));break;
        case HCI_EVENT_AUTHENTICATION_COMPLETE:queueBluetoothEvent(BluetoothEvent::AuthenticationComplete,hci_event_authentication_complete_get_status(packet));break;
        case HCI_EVENT_SIMPLE_PAIRING_COMPLETE:queueBluetoothEvent(BluetoothEvent::PairingComplete,hci_event_simple_pairing_complete_get_status(packet));break;
        case HCI_EVENT_LINK_KEY_REQUEST:queueBluetoothEvent(BluetoothEvent::LinkKeyRequest);break;
        case HCI_EVENT_LINK_KEY_NOTIFICATION:queueBluetoothEvent(BluetoothEvent::LinkKeyCreated);break;
        case HCI_EVENT_USER_CONFIRMATION_REQUEST:queueBluetoothEvent(BluetoothEvent::ConfirmationRequest);break;
        case RFCOMM_EVENT_INCOMING_CONNECTION:queueBluetoothEvent(BluetoothEvent::RfcommIncoming);break;
        case RFCOMM_EVENT_CHANNEL_OPENED:queueBluetoothEvent(BluetoothEvent::RfcommOpened,rfcomm_event_channel_opened_get_status(packet));break;
        case RFCOMM_EVENT_CHANNEL_CLOSED:queueBluetoothEvent(BluetoothEvent::RfcommClosed);break;
        case HCI_EVENT_DISCONNECTION_COMPLETE:queueBluetoothEvent(BluetoothEvent::Disconnected,hci_event_disconnection_complete_get_reason(packet));break;
        default:break;
    }
}

const char* bluetoothEventName(BluetoothEvent event)
{
    switch(event)
    {
        case BluetoothEvent::StackState:return "stack_state";
        case BluetoothEvent::ConnectionComplete:return "acl_connection";
        case BluetoothEvent::AuthenticationComplete:return "authentication";
        case BluetoothEvent::PairingComplete:return "pairing";
        case BluetoothEvent::LinkKeyRequest:return "link_key_request";
        case BluetoothEvent::LinkKeyCreated:return "link_key_created";
        case BluetoothEvent::ConfirmationRequest:return "confirmation_request";
        case BluetoothEvent::RfcommIncoming:return "rfcomm_incoming";
        case BluetoothEvent::RfcommOpened:return "rfcomm_opened";
        case BluetoothEvent::RfcommClosed:return "rfcomm_closed";
        case BluetoothEvent::Disconnected:return "disconnected";
    }
    return "unknown";
}

void printBluetoothEvents()
{
    while(bluetoothEventRead!=bluetoothEventWrite)
    {
        const BluetoothEventRecord record=bluetoothEvents[bluetoothEventRead];
        bluetoothEventRead=uint8_t((bluetoothEventRead+1)%BluetoothEventCapacity);
        Serial.print("BT_EVENT,");Serial.print(bluetoothEventName(record.event));Serial.print(",status,0x");
        if(record.status<0x10)Serial.print('0');Serial.println(record.status,HEX);
    }
}

void printRunStart()
{
    SerialBT.print("RUN_START,yarn_weight,");SerialBT.print(telemetryYarnWeight);
    SerialBT.print(",skein_size,");SerialBT.print(telemetrySkeinSize);
    SerialBT.print(",target_turns,");SerialBT.print(telemetryTargetTurns);
    SerialBT.print(",selected_cruise_rps,");SerialBT.print(selectedCruiseMotorRPS,2);
    SerialBT.print(",run_current_ma,");SerialBT.print(settings.runCurrentMa);
    SerialBT.print(",hold_current_ma,");SerialBT.print(settings.holdCurrentMa);
    SerialBT.print(",rotation_direction,");SerialBT.print(settings.clockwise?"clockwise":"counter_clockwise");
    SerialBT.print(",required_load_cells,");SerialBT.print(Config::RequiredLoadCells);
    SerialBT.print(",detected_load_cells,");SerialBT.print(detectedLoadCellCount);
    SerialBT.print(",product_mode,");SerialBT.print(Product::name());
    SerialBT.print(",launch_rps,");SerialBT.print(Config::LaunchMotorRps,2);
    SerialBT.print(",launch_turns,");SerialBT.print(Config::LaunchRampTurns,2);
    SerialBT.print(",stall_detection,");SerialBT.print(Config::EnableStallDetection?1:0);
    SerialBT.print(",stall_recovery,");SerialBT.print(Config::EnableStallRecovery?1:0);
    SerialBT.print(",weight_control_armed,");SerialBT.print(RunSupervisor::weightTargetArmed()?1:0);
    SerialBT.print(",weight_target_g,");SerialBT.print(RunSupervisor::weightTargetGrams(),2);
    const LoadCellTareResult tare=LoadCells::tareResult();
    SerialBT.print(",tare_valid,");SerialBT.print(LoadCells::tareValid()?1:0);
    SerialBT.print(",profile_tare_valid,");SerialBT.print(LoadCells::profileTareValid()?1:0);
    SerialBT.print(",profile_bins,");SerialBT.print(Config::LoadCellProfileBins);
    for(uint8_t channel=0;channel<Config::LoadCellChannelCount;channel++)
    {
        SerialBT.print(",lc");SerialBT.print(channel+1);SerialBT.print("_tare,");SerialBT.print(tare.offset[channel]);
        SerialBT.print(",lc");SerialBT.print(channel+1);SerialBT.print("_tare_spread,");SerialBT.print(tare.spread[channel]);
    }
    SerialBT.println();
}

const char* phaseName(uint8_t phase)
{
    static const char* names[]={"launch","start_hold","accel","cruise","decel","pause_down","paused","resume"};
    return phase<8?names[phase]:"unknown";
}

void printHex32(Print& output,uint32_t value)
{
    for(int shift=28;shift>=0;shift-=4)output.print((value>>shift)&0x0f,HEX);
}

void startBluetooth()
{
    if(bluetoothRunning)return;
    SerialBT.setName("Hankinator 00:00:00:00:00:00");
    SerialBT.begin();
    {
        BluetoothLock lock;
        if(!bluetoothEventHandlerRegistered)
        {
            bluetoothEventRegistration.callback=bluetoothEventHandler;
            hci_add_event_handler(&bluetoothEventRegistration);
            bluetoothEventHandlerRegistered=true;
        }
        gap_ssp_set_io_capability(SSP_IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
        gap_ssp_set_auto_accept(1);
        gap_connectable_control(1);
        gap_discoverable_control(0);
    }
    bluetoothRunning=true;
    Serial.println("Bluetooth diagnostics enabled; connectable, pairing closed");
}

void endWifiSetup()
{
    FirmwareUpdate::end();wifiSetupStage=WifiSetupStage::Idle;
    wifiSetupOutput=nullptr;wifiSetupOverBluetooth=false;
}

bool parseWifiSelection(const char* text,int& selection)
{
    String value(text);value.trim();
    char* end=nullptr;
    const long parsed=strtol(value.c_str(),&end,10);
    while(end&&*end==' ')end++;
    if(!value.length()||!end||*end||parsed<0||parsed>Config::WifiMaxNetworks)return false;
    selection=int(parsed);return true;
}

void printWifiNetworks(Print& output)
{
    output.println("WIFI_NETWORKS");
    output.println("0,AP mode,remove saved Wi-Fi credentials");
    for(uint8_t index=0;index<FirmwareUpdate::networkCount();index++)
    {
        output.print(index+1);output.print(',');output.print(FirmwareUpdate::ssid(index));
        output.println(FirmwareUpdate::secured(index)?",password required":",open");
    }
    output.print("WIFI_SELECT,enter 0-");output.println(FirmwareUpdate::networkCount());
}

void serviceWifiSetup()
{
    if(wifiSetupStage==WifiSetupStage::Idle||!wifiSetupOutput)return;
    if(wifiSetupOverBluetooth&&!bluetoothConnected){endWifiSetup();return;}
    if(wifiSetupStage==WifiSetupStage::Scanning&&!FirmwareUpdate::scanning())
    {
        if(!FirmwareUpdate::networkCount())
        {
            wifiSetupOutput->print("WIFI_SETUP_FAILED,");wifiSetupOutput->println(FirmwareUpdate::networkMessage());
            endWifiSetup();return;
        }
        printWifiNetworks(*wifiSetupOutput);wifiSetupStage=WifiSetupStage::Selecting;
    }
    else if(wifiSetupStage==WifiSetupStage::Connecting)
    {
        if(FirmwareUpdate::status()==FirmwareUpdateStatus::Ready)
        {
            wifiSetupOutput->print("WIFI_CONNECTED,ssid,");wifiSetupOutput->print(FirmwareUpdate::networkName());
            wifiSetupOutput->print(",ip,");wifiSetupOutput->println(FirmwareUpdate::ipAddress());
            wifiSetupOutput->println("WIFI_SAVED,Wi-Fi disabled until Wi-Fi Update is opened");
            endWifiSetup();
        }
        else if(FirmwareUpdate::status()==FirmwareUpdateStatus::Error)
        {
            wifiSetupOutput->print("WIFI_SETUP_FAILED,");wifiSetupOutput->println(FirmwareUpdate::networkMessage());
            endWifiSetup();
        }
    }
}

void printWifiUpdateReady(Print& output)
{
    output.print("WIFI_UPDATE_READY,mode,");
    if(FirmwareUpdate::stationMode())
    {
        output.print("LAN,ssid,");output.print(FirmwareUpdate::networkName());
        output.print(",ip,");output.print(FirmwareUpdate::ipAddress());
        output.print(",login,admin/");output.println(FirmwareUpdate::accessCode());
    }
    else
    {
        output.print("AP,ssid,");output.print(FirmwareUpdate::networkName());
        output.print(",password,");output.print(Config::FirmwareUpdatePassword);
        output.print(",ip,");output.println(FirmwareUpdate::ipAddress());
    }
    output.println("WIFI_UPDATE_PORTAL_RUNNING,open the address above to upload firmware");
}

void serviceWifiUpdate()
{
    if(!wifiUpdateOutput)return;
    if(FirmwareUpdate::status()==FirmwareUpdateStatus::Ready)
    {
        printWifiUpdateReady(*wifiUpdateOutput);wifiUpdateOutput=nullptr;
    }
    else if(FirmwareUpdate::status()==FirmwareUpdateStatus::Error)
    {
        wifiUpdateOutput->print("WIFI_UPDATE_FAILED,");wifiUpdateOutput->println(FirmwareUpdate::networkMessage());
        wifiUpdateOutput=nullptr;
    }
}

bool handleWifiSetupCommand(const char* command,Print& output,bool overBluetooth,bool allowed)
{
    String normalized(command);normalized.trim();normalized.toUpperCase();
    if(normalized=="WIFI UPDATE STOP"||normalized=="WIFI UPDATE CLOSE")
    {
        if(FirmwareUpdate::uploading()||FirmwareUpdate::status()==FirmwareUpdateStatus::Success)
        {
            output.println("WIFI_UPDATE_BUSY,update cannot be stopped now");return true;
        }
        FirmwareUpdate::end();wifiUpdateOutput=nullptr;
        output.println("WIFI_UPDATE_STOPPED,Wi-Fi and upload portal are off");return true;
    }
    if(normalized=="WIFI UPDATE")
    {
        if(!allowed){output.println("BUSY: Wi-Fi Update is available while idle");return true;}
        if(wifiSetupStage!=WifiSetupStage::Idle){output.println("WIFI_SETUP_ACTIVE,type WIFI CANCEL before starting Wi-Fi Update");return true;}
        if(FirmwareUpdate::uploading()){output.println("WIFI_UPDATE_BUSY,upload already in progress");return true;}
        if(FirmwareUpdate::status()==FirmwareUpdateStatus::Error)FirmwareUpdate::end();
        if(!FirmwareUpdate::active()&&!FirmwareUpdate::begin())
        {
            output.println("WIFI_UPDATE_FAILED,unable to start");return true;
        }
        wifiUpdateOutput=&output;
        if(FirmwareUpdate::status()==FirmwareUpdateStatus::Connecting)output.println("WIFI_UPDATE_CONNECTING,waiting for DHCP");
        else if(FirmwareUpdate::status()==FirmwareUpdateStatus::Starting)output.println("WIFI_UPDATE_STARTING");
        return true;
    }
    if(normalized=="WIFI SETUP")
    {
        if(!allowed){output.println("BUSY: Wi-Fi setup is available while idle");return true;}
        if(wifiSetupStage!=WifiSetupStage::Idle){output.println("WIFI_SETUP_ACTIVE,type WIFI CANCEL to stop");return true;}
        if(!FirmwareUpdate::beginBluetoothProvisioning()){output.println("WIFI_SETUP_FAILED,unable to start scan");return true;}
        wifiSetupStage=WifiSetupStage::Scanning;wifiSetupOutput=&output;wifiSetupOverBluetooth=overBluetooth;
        output.println("WIFI_SCANNING,please wait");return true;
    }
    if(wifiSetupStage==WifiSetupStage::Idle)return false;
    if(normalized=="WIFI CANCEL")
    {
        output.println("WIFI_SETUP_CANCELLED");endWifiSetup();return true;
    }
    if(wifiSetupStage==WifiSetupStage::Selecting)
    {
        int selection=-1;
        String selectionText(command);
        if(normalized.startsWith("WIFI SELECT "))selectionText=String(command).substring(12);
        if(!parseWifiSelection(selectionText.c_str(),selection)||selection>FirmwareUpdate::networkCount())
        {
            output.print("WIFI_SELECT,enter 0-");output.println(FirmwareUpdate::networkCount());return true;
        }
        if(selection==0)
        {
            FirmwareUpdate::clearSavedNetwork();
            output.print("WIFI_AP_MODE,ssid,");output.print(Config::FirmwareUpdateSsid);
            output.print(",password,");output.print(Config::FirmwareUpdatePassword);
            output.println(",ip,192.168.4.1");
            output.println("WIFI_SAVED,AP mode will start when Wi-Fi Update is opened");
            endWifiSetup();return true;
        }
        const uint8_t index=uint8_t(selection-1);
        const bool requiresPassword=FirmwareUpdate::selectBluetoothNetwork(index);
        if(requiresPassword)
        {
            wifiSetupStage=WifiSetupStage::Password;
            output.print("WIFI_PASSWORD,ssid,");output.print(FirmwareUpdate::ssid(index));
            output.println(",enter password on the next line; WIFI CANCEL to stop");
        }
        else
        {
            wifiSetupStage=WifiSetupStage::Connecting;
            output.print("WIFI_CONNECTING,ssid,");output.println(FirmwareUpdate::ssid(index));
        }
        return true;
    }
    if(wifiSetupStage==WifiSetupStage::Password)
    {
        const char* password=command;
        if(normalized.startsWith("WIFI PASSWORD "))password=command+14;
        if(!FirmwareUpdate::submitBluetoothPassword(password))
        {
            output.println("WIFI_PASSWORD_INVALID,use 8-63 characters");return true;
        }
        wifiSetupStage=WifiSetupStage::Connecting;
        output.println("WIFI_CONNECTING");return true;
    }
    output.println("WIFI_CONNECTING,please wait");return true;
}

void consume(Stream& input,Print& output,CommandBuffer& command)
{
    while(input.available()) {
        const char c=char(input.read());
        // Pause an explicit weight capture while a command is being typed so
        // terminal-local echo cannot be embedded inside LC_DATA rows.
        Telemetry::noteConsoleInput();
        if(c=='\r'||c=='\n') {
            if(command.length) {
                command.data[command.length]='\0';
                const bool logAccessAllowed=uiState!=UiState::WINDING&&uiState!=UiState::HALL_SEEK&&!motionActive&&!auxiliaryMotionActive();
                String normalized(command.data);normalized.trim();normalized.toUpperCase();
                if(handleMotionHoldCommand(command.data,output,logAccessAllowed))
                {
                    // The hold probe owns its tare and capture lifecycle.
                }
                else if(handleMotionProbeCommand(command.data,output,logAccessAllowed))
                {
                    // The motion probe owns its tare, capture, and run lifecycle.
                }
                else if(handleArmProbeCommand(command.data,output,logAccessAllowed))
                {
                    // The arm probe owns its own tare and capture lifecycle.
                }
                else if(normalized=="TARE")
                {
                    if(!logAccessAllowed)output.println("BUSY: TARE is available while idle");
                    else if(!weightCapabilityEnabled)output.println("ERROR: load-cell capability unavailable");
                    else if(Machine::diagnostics().tareStatus==Machine::TareStatus::Collecting)output.println("TARE already in progress");
                    else if(Machine::startWeightDiagnosticsTare())output.println("TARE started; keep the machine still");
                    else output.println("ERROR: could not start tare");
                }
                else if(!handleWifiSetupCommand(command.data,output,&output==&SerialBT,logAccessAllowed))Telemetry::executeCommand(command.data,output,logAccessAllowed);
                command.length=0;
            }
            continue;
        }
        if(command.length<CommandCapacity-1) command.data[command.length++]=c;
        else command.length=0;
    }
}
}

void DiagnosticsTransport::begin()
{
    if(settings.bluetoothEnabled)startBluetooth();
    else Serial.println("Bluetooth diagnostics disabled in configuration");
}

void DiagnosticsTransport::poll()
{
    serviceMotionHold();
    serviceMotionProbe();
    serviceArmProbe();
    consume(Serial,Serial,usbCommand);
    printBluetoothEvents();
    if(pairingDeadline&&int32_t(millis()-pairingDeadline)>=0)closePairingWindow();
    const bool connected=bluetoothRunning&&SerialBT.availableForWrite()>0;
    if(connected&&!bluetoothConnected) {
        Serial.println("Bluetooth diagnostics client connected");
        SerialBT.println("HankWinder diagnostics connected");
        SerialBT.println("LOG DUMP | LOG CLEAR | LOG HELP | LOAD CELLS | TARE | ARM PROBE | MOTION PROBE SWEEP START | MOTION HOLD | START WEIGHT | STOP WEIGHT | MARK <label> | WIFI SETUP | WIFI UPDATE");
    }
    if(!connected&&bluetoothConnected) {
        Serial.println("Bluetooth diagnostics client disconnected");
        telemetryHeaderSent=false;
    }
    bluetoothConnected=connected;
    if(connected) consume(SerialBT,SerialBT,bluetoothCommand);
    serviceWifiSetup();
    serviceWifiUpdate();
}

void DiagnosticsTransport::setEnabled(bool enabled)
{
    if(enabled)
    {
        startBluetooth();
        BluetoothLock lock;gap_connectable_control(1);
        Serial.println("Bluetooth diagnostics enabled without stack restart");
        return;
    }
    if(!bluetoothRunning)return;
    closePairingWindow();
    {BluetoothLock lock;gap_connectable_control(0);}
    bluetoothConnected=false;
    Serial.println("Bluetooth diagnostics connection disabled; stack remains running");
}

bool DiagnosticsTransport::enabled(){return bluetoothRunning;}
bool DiagnosticsTransport::connected(){return bluetoothConnected;}
void DiagnosticsTransport::openPairingWindow(){if(!bluetoothRunning)return;{BluetoothLock lock;gap_connectable_control(1);gap_discoverable_control(1);}pairingDeadline=millis()+Config::PairingWindowMs;Serial.println("Bluetooth pairing open for 60 seconds; connectable=1 discoverable=1");}
void DiagnosticsTransport::closePairingWindow(){if(bluetoothRunning){BluetoothLock lock;gap_discoverable_control(0);}pairingDeadline=0;}
void DiagnosticsTransport::clearPairedDevices()
{
    if(!bluetoothRunning)return;
    BluetoothLock lock;
    gap_delete_all_link_keys();
    Serial.println("Bluetooth paired devices cleared");
}
bool DiagnosticsTransport::pairingOpen(){return pairingDeadline!=0;}
uint32_t DiagnosticsTransport::pairingSecondsRemaining(){if(!pairingDeadline)return 0;int32_t remaining=int32_t(pairingDeadline-millis());return remaining>0?uint32_t(remaining+999)/1000:0;}

void DiagnosticsTransport::beginTelemetryStream(const char* yarnWeight,const char* skeinSize,uint16_t targetTurns)
{
    telemetryYarnWeight=yarnWeight;
    telemetrySkeinSize=skeinSize;
    telemetryTargetTurns=targetTurns;
    telemetryRunActive=true;
    telemetryHeaderSent=false;
    lastTelemetryStreamMs=0;
}

void DiagnosticsTransport::streamTelemetry(uint32_t ms,uint16_t rps100,uint32_t steps,uint16_t sg,uint8_t diag,uint8_t pause,uint8_t phase,uint32_t status,const LoadCellSnapshot& loadCells,int32_t weightCentiGrams,uint16_t weightSpreadCentiGrams,bool tareValid,bool weightValid,const RunSupervisorSnapshot& control)
{
    if(SerialBT.availableForWrite()<=0) return;
    // SerialBT::availableForWrite() is only a connection indication in the
    // Arduino-Pico Bluetooth implementation, not a useful free-buffer count.
    // Throttle the long human-readable CSV row before starting its many Print
    // calls so a connected PuTTY session cannot starve touch polling.
    if(telemetryHeaderSent&&ms-lastTelemetryStreamMs<Config::BluetoothLiveTelemetryIntervalMs)return;
    if(!telemetryHeaderSent) {
        printRunStart();
        SerialBT.println("ms,rps,steps,turns,sg_result,diag,pause,phase,drv_status,lc1_raw,lc1_healthy,lc2_raw,lc2_healthy,lc3_raw,lc3_healthy,lc4_raw,lc4_healthy,weight_g,weight_spread_g,tare_valid,weight_valid,sg_baseline,stall_state,filtered_weight_g,weight_control_state,stall_recovery,count_uncertain,hall_state,hall_pulses,hall_correction_steps");
        telemetryHeaderSent=true;
    }
    lastTelemetryStreamMs=ms;

    SerialBT.print(ms);SerialBT.print(',');
    SerialBT.print(rps100/100);SerialBT.print('.');if(rps100%100<10)SerialBT.print('0');SerialBT.print(rps100%100);SerialBT.print(',');
    SerialBT.print(steps);SerialBT.print(',');
    const uint64_t turns10000=uint64_t(steps)*10000ULL/Config::StepsPerHubRev;
    SerialBT.print(uint32_t(turns10000/10000));SerialBT.print('.');
    const uint16_t fraction=turns10000%10000;
    if(fraction<1000)SerialBT.print('0');if(fraction<100)SerialBT.print('0');if(fraction<10)SerialBT.print('0');SerialBT.print(fraction);SerialBT.print(',');
    SerialBT.print(sg);SerialBT.print(',');SerialBT.print(diag);SerialBT.print(',');SerialBT.print(pause);SerialBT.print(',');SerialBT.print(phaseName(phase));SerialBT.print(",0x");
    printHex32(SerialBT,status);
    for(uint8_t channel=0;channel<Config::LoadCellChannelCount;channel++)
    {
        SerialBT.print(',');SerialBT.print(loadCells.raw[channel]);SerialBT.print(',');SerialBT.print((loadCells.healthyMask&(1u<<channel))?1:0);
    }
    SerialBT.print(',');SerialBT.print(weightCentiGrams/100.0f,2);
    SerialBT.print(',');SerialBT.print(weightSpreadCentiGrams/100.0f,2);
    SerialBT.print(',');SerialBT.print(tareValid?1:0);
    SerialBT.print(',');SerialBT.print(weightValid?1:0);
    SerialBT.print(',');SerialBT.print(control.sgBaseline);
    SerialBT.print(',');SerialBT.print(RunSupervisor::stallStateName(control.stallState));
    SerialBT.print(',');SerialBT.print(control.filteredWeightGrams,2);
    SerialBT.print(',');SerialBT.print(RunSupervisor::weightStateName(control.weightState));
    SerialBT.print(',');SerialBT.print(RunSupervisor::recoveryName(control.recovery));
    SerialBT.print(',');SerialBT.print(control.countUncertain?1:0);
    SerialBT.print(',');SerialBT.print(HallSensor::stateName());
    SerialBT.print(',');SerialBT.print(HallSensor::pulseCount());
    SerialBT.print(',');SerialBT.print(HallSensor::correctionSteps());
    SerialBT.println();
}

void DiagnosticsTransport::endTelemetryStream(const char* result,uint32_t durationMs,uint32_t totalSamples,uint16_t persistedSamples)
{
    if(telemetryRunActive&&SerialBT.availableForWrite()>0) {
        if(!telemetryHeaderSent) printRunStart();
        SerialBT.print("RUN_END,result,");SerialBT.print(result);
        SerialBT.print(",duration_ms,");SerialBT.print(durationMs);
        SerialBT.print(",total_samples,");SerialBT.print(totalSamples);
        SerialBT.print(",persisted_samples,");SerialBT.println(persistedSamples);
    }
    telemetryRunActive=false;
    telemetryHeaderSent=false;
}

void DiagnosticsTransport::streamEvent(uint32_t ms,const char* label)
{
    if(!telemetryRunActive||!bluetoothConnected)return;
    SerialBT.print("EVENT,ms,");SerialBT.print(ms);SerialBT.print(",label,");SerialBT.println(label);
}

void DiagnosticsTransport::diagnosticLine(const char* line)
{
    const size_t length=strlen(line);
    // Arduino-Pico SerialBT returns a connection flag (0/1), NOT free bytes.
    // Its write is blocking. This path is only used by stopped diagnostic UI.
    if(bluetoothConnected&&SerialBT.availableForWrite()>0)
        SerialBT.write(reinterpret_cast<const uint8_t*>(line),length);
}
