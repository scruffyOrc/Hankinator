#include <Arduino.h>
#include <SerialBT.h>
#include "config.h"
#include "diagnostics_console.h"
#include "engineering_probes.h"
#include "runtime.h"
#include "telemetry.h"
#include "firmware_update.h"
#include "machine_interface.h"
#include "app_api.h"

namespace {
constexpr size_t CommandCapacity=96;
struct CommandBuffer {char data[CommandCapacity]{};size_t length=0;};
CommandBuffer usbCommand;
CommandBuffer bluetoothCommand;

enum class WifiSetupStage:uint8_t {Idle,Scanning,Selecting,Password,Connecting};
WifiSetupStage wifiSetupStage=WifiSetupStage::Idle;
Print* wifiSetupOutput=nullptr;
bool wifiSetupOverBluetooth=false;
Print* wifiUpdateOutput=nullptr;
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

void serviceWifiSetup(const bool bluetoothConnected)
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
                if(EngineeringProbes::handleCommand(command.data,output,logAccessAllowed))
                {
                    // Engineering probes own their tare, capture, and motion lifecycle.
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

void DiagnosticsConsole::poll(const bool bluetoothConnected)
{
    consume(Serial,Serial,usbCommand);
    if(bluetoothConnected)consume(SerialBT,SerialBT,bluetoothCommand);
    serviceWifiSetup(bluetoothConnected);
    serviceWifiUpdate();
}
