#include <Arduino.h>
#include <SerialBT.h>
#include "diagnostics_transport.h"
#include "runtime.h"
#include "telemetry.h"

namespace {
constexpr size_t CommandCapacity=48;

struct CommandBuffer {
    char data[CommandCapacity]{};
    size_t length=0;
};

CommandBuffer usbCommand;
CommandBuffer bluetoothCommand;
bool bluetoothConnected=false;
bool telemetryHeaderSent=false;
bool telemetryRunActive=false;
const char* telemetryYarnWeight="";
const char* telemetrySkeinSize="";
uint16_t telemetryTargetTurns=0;

void printRunStart()
{
    SerialBT.print("RUN_START,yarn_weight,");SerialBT.print(telemetryYarnWeight);
    SerialBT.print(",skein_size,");SerialBT.print(telemetrySkeinSize);
    SerialBT.print(",target_turns,");SerialBT.println(telemetryTargetTurns);
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
    SerialBT.setName("Hankinator 00:00:00:00:00:00");
    SerialBT.begin();
    // SerialBT defaults to DISPLAY_YES_NO pairing even though the Hankinator
    // has no way to confirm the number. Use Bluetooth "Just Works" pairing
    // so Windows does not wait forever for a confirmation from the device.
    gap_ssp_set_io_capability(SSP_IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
    gap_ssp_set_auto_accept(1);
    Serial.println("Bluetooth diagnostics ready: pair with Hankinator");
}

void DiagnosticsTransport::poll()
{
    consume(Serial,Serial,usbCommand);
    const bool connected=SerialBT.availableForWrite()>0;
    if(connected&&!bluetoothConnected) {
        Serial.println("Bluetooth diagnostics client connected");
        SerialBT.println("Hankinator diagnostics connected");
        SerialBT.println("LOG DUMP | LOG CLEAR | LOG HELP");
    }
    if(!connected&&bluetoothConnected) {
        Serial.println("Bluetooth diagnostics client disconnected");
        telemetryHeaderSent=false;
    }
    bluetoothConnected=connected;
    if(connected) consume(SerialBT,SerialBT,bluetoothCommand);
}

void DiagnosticsTransport::beginTelemetryStream(const char* yarnWeight,const char* skeinSize,uint16_t targetTurns)
{
    telemetryYarnWeight=yarnWeight;
    telemetrySkeinSize=skeinSize;
    telemetryTargetTurns=targetTurns;
    telemetryRunActive=true;
    telemetryHeaderSent=false;
}

void DiagnosticsTransport::streamTelemetry(uint32_t ms,uint16_t rps100,uint32_t steps,uint16_t sg,uint8_t diag,uint8_t pause,uint16_t status)
{
    if(SerialBT.availableForWrite()<=0) return;
    if(!telemetryHeaderSent) {
        printRunStart();
        SerialBT.println("ms,rps,steps,turns,sg_result,diag,pause,drv_status_low");
        telemetryHeaderSent=true;
    }

    SerialBT.print(ms);SerialBT.print(',');
    SerialBT.print(rps100/100);SerialBT.print('.');if(rps100%100<10)SerialBT.print('0');SerialBT.print(rps100%100);SerialBT.print(',');
    SerialBT.print(steps);SerialBT.print(',');
    const uint64_t turns10000=uint64_t(steps)*10000ULL/Config::StepsPerHubRev;
    SerialBT.print(uint32_t(turns10000/10000));SerialBT.print('.');
    const uint16_t fraction=turns10000%10000;
    if(fraction<1000)SerialBT.print('0');if(fraction<100)SerialBT.print('0');if(fraction<10)SerialBT.print('0');SerialBT.print(fraction);SerialBT.print(',');
    SerialBT.print(sg);SerialBT.print(',');SerialBT.print(diag);SerialBT.print(',');SerialBT.print(pause);SerialBT.print(",0x");
    if(status<0x1000)SerialBT.print('0');if(status<0x100)SerialBT.print('0');if(status<0x10)SerialBT.print('0');SerialBT.println(status,HEX);
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
