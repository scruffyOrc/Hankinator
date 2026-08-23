#include <Arduino.h>
#include <SerialBT.h>
#include <BluetoothLock.h>
#include "diagnostics_transport.h"
#include "runtime.h"
#include "telemetry.h"
#include "product_mode.h"

namespace {
constexpr size_t CommandCapacity=48;

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
const char* telemetryYarnWeight="";
const char* telemetrySkeinSize="";
uint16_t telemetryTargetTurns=0;

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
    SerialBT.print(",stall_auto_abort,");SerialBT.print(Config::EnableAutomaticStallAbort?1:0);
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

void consume(Stream& input,Print& output,CommandBuffer& command)
{
    while(input.available()) {
        const char c=char(input.read());
        if(c=='\r'||c=='\n') {
            if(command.length) {
                command.data[command.length]='\0';
                const bool logAccessAllowed=uiState!=UiState::WINDING;
                Telemetry::executeCommand(command.data,output,logAccessAllowed);
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
    consume(Serial,Serial,usbCommand);
    printBluetoothEvents();
    if(pairingDeadline&&int32_t(millis()-pairingDeadline)>=0)closePairingWindow();
    const bool connected=bluetoothRunning&&SerialBT.availableForWrite()>0;
    if(connected&&!bluetoothConnected) {
        Serial.println("Bluetooth diagnostics client connected");
        SerialBT.println("Hankinator diagnostics connected");
        SerialBT.println("LOG DUMP | LOG CLEAR | LOG HELP | LOAD CELLS | MARK <label>");
    }
    if(!connected&&bluetoothConnected) {
        Serial.println("Bluetooth diagnostics client disconnected");
        telemetryHeaderSent=false;
    }
    bluetoothConnected=connected;
    if(connected) consume(SerialBT,SerialBT,bluetoothCommand);
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
bool DiagnosticsTransport::pairingOpen(){return pairingDeadline!=0;}
uint32_t DiagnosticsTransport::pairingSecondsRemaining(){if(!pairingDeadline)return 0;int32_t remaining=int32_t(pairingDeadline-millis());return remaining>0?uint32_t(remaining+999)/1000:0;}

void DiagnosticsTransport::beginTelemetryStream(const char* yarnWeight,const char* skeinSize,uint16_t targetTurns)
{
    telemetryYarnWeight=yarnWeight;
    telemetrySkeinSize=skeinSize;
    telemetryTargetTurns=targetTurns;
    telemetryRunActive=true;
    telemetryHeaderSent=false;
}

void DiagnosticsTransport::streamTelemetry(uint32_t ms,uint16_t rps100,uint32_t steps,uint16_t sg,uint8_t diag,uint8_t pause,uint8_t phase,uint32_t status,const LoadCellSnapshot& loadCells,int32_t weightCentiGrams,uint16_t weightSpreadCentiGrams,bool tareValid,bool weightValid,const RunSupervisorSnapshot& control)
{
    if(SerialBT.availableForWrite()<=0) return;
    if(!telemetryHeaderSent) {
        printRunStart();
        SerialBT.println("ms,rps,steps,turns,sg_result,diag,pause,phase,drv_status,lc1_raw,lc1_healthy,lc2_raw,lc2_healthy,lc3_raw,lc3_healthy,lc4_raw,lc4_healthy,weight_g,weight_spread_g,tare_valid,weight_valid,sg_baseline,stall_state,filtered_weight_g,weight_control_state");
        telemetryHeaderSent=true;
    }

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
