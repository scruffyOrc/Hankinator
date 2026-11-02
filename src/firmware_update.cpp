#include "firmware_update.h"

#ifndef HANKINATOR_ENABLE_WIFI_UPDATER
#define HANKINATOR_ENABLE_WIFI_UPDATER 1
#endif

#if HANKINATOR_ENABLE_WIFI_UPDATER

#include <WiFi.h>
#include <WiFiUdp.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <Updater.h>
#include "config.h"
#include "app_api.h"

namespace {
WebServer server(80);
FirmwareUpdateStatus currentStatus=FirmwareUpdateStatus::Off;
uint8_t currentProgress=0,currentError=0;
uint32_t receivedBytes=0;
uint32_t rebootAt=0;
bool serverConfigured=false;
bool station=false,saveOnConnect=false,scanPending=false,portalRequested=false,bluetoothProvisioning=false,stationStartPending=false;
uint8_t stationConnectAttempts=0;
uint32_t networkStarted=0;
char address[16]{},connectedName[33]{},pendingSsid[33]{},pendingPassword[64]{},loginCode[9]{};
const char* networkNote="";
struct Network {char name[33];bool secure;};
Network networks[Config::WifiMaxNetworks]{};
uint8_t networksFound=0;
int characterChoice=0; // Connect, Delete, then printable ASCII.
constexpr uint8_t UpdateStorageMountError=250,UpdateStorageRemoveError=251;

const char* updateErrorText(uint8_t error)
{
    switch(error)
    {
        case UPDATE_ERROR_WRITE:return "flash staging write failed";
        case UPDATE_ERROR_ERASE:return "flash erase failed";
        case UPDATE_ERROR_READ:return "flash read failed";
        case UPDATE_ERROR_SPACE:return "not enough update storage";
        case UPDATE_ERROR_SIZE:return "update staging size is zero";
        case UPDATE_ERROR_STREAM:return "upload stream timed out";
        case UPDATE_ERROR_MD5:return "upload integrity check failed";
        case UPDATE_ERROR_MAGIC_BYTE:return "invalid firmware image";
        case UPDATE_ERROR_NO_DATA:return "no firmware data received";
        case UpdateStorageMountError:return "update storage could not be mounted";
        case UpdateStorageRemoveError:return "old staged update could not be cleared";
        default:return "unknown updater error";
    }
}

bool prepareUpdateStorage(uint64_t& available)
{
    // PicoOTA stages the image in LittleFS. A previous interrupted upload can
    // leave firmware.bin behind, so delete that disposable staging file before
    // computing free capacity. Do not touch the user's telemetry log.
    if(!LittleFS.begin()){currentError=UpdateStorageMountError;return false;}
    if(LittleFS.exists("firmware.bin")&&!LittleFS.remove("firmware.bin")){currentError=UpdateStorageRemoveError;return false;}
    FSInfo info{};
    if(!LittleFS.info(info)||info.totalBytes<=info.usedBytes){currentError=UPDATE_ERROR_SIZE;return false;}
    available=info.totalBytes-info.usedBytes;
    return true;
}

bool authorized() {
    if(!station||server.authenticate("admin",loginCode))return true;
    server.requestAuthentication();return false;
}
bool canConfigure(){return FirmwareUpdate::active()&&!FirmwareUpdate::uploading()&&currentStatus!=FirmwareUpdateStatus::Success;}
void stopNetwork()
{
    server.stop();
    // Arduino-Pico's softAPdisconnect() routes a standalone AP through the
    // station-only disconnect path. Explicitly tear down the AP interface and
    // its DHCP server before turning the radio off.
    WiFi.disconnect();WiFi.disconnectAP();WiFi.mode(WIFI_OFF);
    scanPending=false;stationStartPending=false;
}

void scheduleStationStart(const char* note)
{
    WiFi.disconnect();WiFi.mode(WIFI_OFF);
    stationStartPending=true;networkStarted=millis();currentStatus=FirmwareUpdateStatus::Starting;networkNote=note;
}

void beginStationAttempt()
{
    WiFi.mode(WIFI_STA);
    if(pendingPassword[0])WiFi.beginNoBlock(pendingSsid,pendingPassword);
    else WiFi.beginNoBlock(pendingSsid);
    stationStartPending=false;stationConnectAttempts++;
    networkStarted=millis();currentStatus=FirmwareUpdateStatus::Connecting;
    networkNote=stationConnectAttempts>1?"Retrying Wi-Fi...":"Connecting...";
}

void connectNetwork(bool save,bool startPortal) {
    stopNetwork();station=true;saveOnConnect=save;address[0]=0;stationConnectAttempts=0;
    portalRequested=startPortal;
    snprintf(connectedName,sizeof(connectedName),"%s",pendingSsid);
    // The CYW43439 can reject an immediate STA restart after an AP/STA
    // teardown. Let the radio settle before the first DHCP attempt.
    scheduleStationStart("Starting Wi-Fi...");
}

const char updatePage[] PROGMEM=R"HTML(
<!doctype html><html><head><meta name="viewport" content="width=device-width,initial-scale=1">
<title>HankWinder Firmware Update</title><style>
body{font:16px system-ui;max-width:34rem;margin:3rem auto;padding:0 1rem;background:#f7f5f0;color:#25231f}
main{background:white;padding:1.5rem;border-radius:12px;box-shadow:0 2px 14px #0002}button,input{font:inherit;margin:.5rem 0}
button{padding:.6rem 1rem}progress{width:100%;height:1.2rem}small{color:#666}</style></head><body><main>
<h2>HankWinder Firmware Update</h2><p>Current version: <b>%VERSION%</b></p>
<form id="f"><input id="file" type="file" accept=".bin,application/octet-stream" required><br><button>Install Update</button></form>
<progress id="p" max="100" value="0"></progress><p id="s">Choose a firmware.bin file.</p>
<small>Keep the machine powered. It will restart automatically after a successful update.</small>
<script>f.onsubmit=e=>{e.preventDefault();let x=new XMLHttpRequest(),d=new FormData();d.append('update',file.files[0]);
x.upload.onprogress=e=>{if(e.lengthComputable){let n=Math.round(e.loaded*100/e.total);p.value=n;s.textContent='Uploading '+n+'%';}};
x.onload=()=>{s.textContent=x.responseText};x.onerror=()=>s.textContent='Upload failed';x.open('POST','/update');x.send(d);};</script>
</main></body></html>)HTML";

void configureServer()
{
    if(serverConfigured)return;
    server.on("/",HTTP_GET,[](){if(!authorized())return;String page(updatePage);page.replace("%VERSION%",Config::FirmwareVersion);server.sendHeader("Connection","close");server.send(200,"text/html",page);});
    server.on("/update",HTTP_POST,[](){
        if(!authorized())return;
        server.sendHeader("Connection","close");
        if(currentStatus==FirmwareUpdateStatus::Success)
        {
            server.send(200,"text/plain","Update received. HankWinder is restarting...");
            rebootAt=millis()+Config::FirmwareUpdateRebootDelayMs;
        }
        else server.send(500,"text/plain",String("Update failed: ")+updateErrorText(currentError)+" (error "+currentError+"). USB firmware remains unchanged.");
    },[](){
        if(station&&!server.authenticate("admin",loginCode))return;
        HTTPUpload& upload=server.upload();
        if(upload.status==UPLOAD_FILE_START)
        {
            currentProgress=0;currentError=0;receivedBytes=0;currentStatus=FirmwareUpdateStatus::Uploading;
            WiFiUDP::stopAll();
            uint64_t updateCapacity=0;
            if(!upload.filename.endsWith(".bin"))
            {
                currentError=254;currentStatus=FirmwareUpdateStatus::Error;
            }
            else if(!prepareUpdateStorage(updateCapacity)||updateCapacity>SIZE_MAX||!Update.begin(size_t(updateCapacity),U_FLASH))
            {
                if(!currentError)currentError=Update.getError();
                currentStatus=FirmwareUpdateStatus::Error;
            }
        }
        else if(upload.status==UPLOAD_FILE_WRITE&&currentStatus==FirmwareUpdateStatus::Uploading)
        {
            if(Update.write(upload.buf,upload.currentSize)!=upload.currentSize)
            {
                currentError=Update.getError();currentStatus=FirmwareUpdateStatus::Error;
            }
            else receivedBytes+=upload.currentSize;
        }
        else if(upload.status==UPLOAD_FILE_END&&currentStatus==FirmwareUpdateStatus::Uploading)
        {
            if(Update.end(true)){currentProgress=100;currentStatus=FirmwareUpdateStatus::Success;}
            else {currentError=Update.getError();currentStatus=FirmwareUpdateStatus::Error;}
        }
        else if(upload.status==UPLOAD_FILE_ABORTED)
        {
            Update.end();currentError=Update.getError();currentStatus=FirmwareUpdateStatus::Error;
        }
    });
    server.onNotFound([](){server.sendHeader("Location","/",true);server.send(302,"text/plain","");});
    serverConfigured=true;
}
}

bool FirmwareUpdate::begin()
{
    if(motionActive||auxiliaryMotionActive())return false;
    if(active())return currentStatus!=FirmwareUpdateStatus::Error;
    currentStatus=FirmwareUpdateStatus::Starting;currentProgress=0;currentError=0;receivedBytes=0;rebootAt=0;networkNote="";portalRequested=true;bluetoothProvisioning=false;
    configureServer();
    snprintf(loginCode,sizeof(loginCode),"%08lx",(unsigned long)rp2040.hwrand32());
    if(settings.wifiSsid[0]) {
        snprintf(pendingSsid,sizeof(pendingSsid),"%s",settings.wifiSsid);
        snprintf(pendingPassword,sizeof(pendingPassword),"%s",settings.wifiPassword);
        connectNetwork(false,true);return true;
    }
    useAccessPoint();return currentStatus==FirmwareUpdateStatus::Ready;
}

void FirmwareUpdate::useAccessPoint()
{
    if(!canConfigure())return;
    stopNetwork();station=false;saveOnConnect=false;portalRequested=true;bluetoothProvisioning=false;address[0]=0;
    memset(pendingPassword,0,sizeof(pendingPassword));
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(IPAddress(192,168,4,1),IPAddress(192,168,4,1),IPAddress(255,255,255,0));
    if(!WiFi.softAP(Config::FirmwareUpdateSsid,Config::FirmwareUpdatePassword))
    {
        currentStatus=FirmwareUpdateStatus::Error;currentError=255;return;
    }
    server.begin();currentStatus=FirmwareUpdateStatus::Ready;
    snprintf(address,sizeof(address),"%s",WiFi.softAPIP().toString().c_str());
    snprintf(connectedName,sizeof(connectedName),"%s",Config::FirmwareUpdateSsid);
    Serial.print("Firmware updater ready: http://");Serial.println(WiFi.softAPIP());
}

void FirmwareUpdate::end()
{
    if(!active()||uploading()||currentStatus==FirmwareUpdateStatus::Success)return;
    if(Update.isRunning())Update.end();
    stopNetwork();memset(pendingPassword,0,sizeof(pendingPassword));saveOnConnect=false;portalRequested=false;bluetoothProvisioning=false;
    currentStatus=FirmwareUpdateStatus::Off;currentProgress=0;currentError=0;rebootAt=0;
}

void FirmwareUpdate::poll()
{
    if(!active())return;
    if(scanPending) {
        const int result=WiFi.scanComplete();
        if(result>=0) {
            networksFound=0;
            for(int i=0;i<result&&networksFound<Config::WifiMaxNetworks;i++) {
                const String name=WiFi.SSID(i);if(!name.length()||name.length()>32)continue;
                bool duplicate=false;for(uint8_t j=0;j<networksFound;j++)if(name==networks[j].name)duplicate=true;
                if(duplicate)continue;
                name.toCharArray(networks[networksFound].name,33);
                networks[networksFound++].secure=WiFi.encryptionType(i)!=ENC_TYPE_NONE;
            }
            scanPending=false;networkNote=networksFound?"Select network":"No networks found";
        } else if(millis()-networkStarted>=Config::WifiScanTimeoutMs) {
            scanPending=false;networkNote="Scan failed; retry";
        }
    }
    if(stationStartPending&&millis()-networkStarted>=Config::WifiStationStartDelayMs)beginStationAttempt();
    if(currentStatus==FirmwareUpdateStatus::Connecting) {
        if(WiFi.status()==WL_CONNECTED&&uint32_t(WiFi.localIP())!=0) {
            snprintf(address,sizeof(address),"%s",WiFi.localIP().toString().c_str());
            if(saveOnConnect) {
                snprintf(settings.wifiSsid,sizeof(settings.wifiSsid),"%s",pendingSsid);
                snprintf(settings.wifiPassword,sizeof(settings.wifiPassword),"%s",pendingPassword);
                saveSettings();
            }
            saveOnConnect=false;memset(pendingPassword,0,sizeof(pendingPassword));
            networkNote="Connected";
            if(portalRequested)server.begin();
            currentStatus=FirmwareUpdateStatus::Ready;
            Serial.print("Firmware updater DHCP IP: ");Serial.println(address);
        } else if(millis()-networkStarted>=Config::WifiConnectTimeoutMs) {
            if(stationConnectAttempts<Config::WifiStationConnectAttempts)scheduleStationStart("Retrying Wi-Fi...");
            else if(portalRequested){useAccessPoint();networkNote="Wi-Fi failed; AP ready";}
            else {stopNetwork();station=false;currentError=1;currentStatus=FirmwareUpdateStatus::Error;networkNote="Wi-Fi connect failed";}
        }
    }
    if(portalRequested&&station&&currentStatus==FirmwareUpdateStatus::Ready&&WiFi.status()!=WL_CONNECTED) {
        useAccessPoint();networkNote="Wi-Fi lost; AP ready";
    }
    if(portalRequested&&(currentStatus==FirmwareUpdateStatus::Ready||uploading()||currentStatus==FirmwareUpdateStatus::Success||currentStatus==FirmwareUpdateStatus::Error))server.handleClient();
    if(rebootAt&&int32_t(millis()-rebootAt)>=0)rp2040.restart();
}

bool FirmwareUpdate::active(){return currentStatus!=FirmwareUpdateStatus::Off;}
bool FirmwareUpdate::uploading(){return currentStatus==FirmwareUpdateStatus::Uploading;}
FirmwareUpdateStatus FirmwareUpdate::status(){return currentStatus;}
uint8_t FirmwareUpdate::progressPercent(){return currentProgress;}
uint32_t FirmwareUpdate::bytesReceived(){return receivedBytes;}
uint8_t FirmwareUpdate::errorCode(){return currentError;}
const char* FirmwareUpdate::statusText()
{
    switch(currentStatus)
    {
        case FirmwareUpdateStatus::Off:return "Off";
        case FirmwareUpdateStatus::Starting:return "Starting Wi-Fi";
        case FirmwareUpdateStatus::Ready:return "Ready";
        case FirmwareUpdateStatus::Connecting:return "Connecting Wi-Fi";
        case FirmwareUpdateStatus::Scanning:return "Choose network";
        case FirmwareUpdateStatus::Uploading:return "Uploading";
        case FirmwareUpdateStatus::Success:return "Restarting";
        case FirmwareUpdateStatus::Error:return "Update error";
    }
    return "Update error";
}

const char* FirmwareUpdate::ipAddress(){return address;}
const char* FirmwareUpdate::networkName(){return connectedName;}
const char* FirmwareUpdate::accessCode(){return loginCode;}
const char* FirmwareUpdate::networkMessage(){return networkNote;}
bool FirmwareUpdate::stationMode(){return station;}
void FirmwareUpdate::scan() {
    if(!canConfigure())return;
    stopNetwork();saveOnConnect=false;station=false;networksFound=0;memset(pendingPassword,0,sizeof(pendingPassword));
    WiFi.mode(WIFI_STA);currentStatus=FirmwareUpdateStatus::Scanning;
    networkNote="Scanning...";networkStarted=millis();scanPending=true;WiFi.scanNetworks(true);
}
bool FirmwareUpdate::scanning(){return scanPending;}
uint8_t FirmwareUpdate::networkCount(){return networksFound;}
const char* FirmwareUpdate::ssid(uint8_t index){return index<networksFound?networks[index].name:"";}
bool FirmwareUpdate::secured(uint8_t index){return index<networksFound&&networks[index].secure;}
bool FirmwareUpdate::selectNetwork(uint8_t index) {
    if(!canConfigure()||scanPending||index>=networksFound)return false;
    snprintf(pendingSsid,sizeof(pendingSsid),"%s",networks[index].name);
    memset(pendingPassword,0,sizeof(pendingPassword));characterChoice=0;
    if(networks[index].secure)return true;
    connectNetwork(true,true);return false;
}
void FirmwareUpdate::editPassword(int delta){characterChoice=(characterChoice+delta+97)%97;}
uint8_t FirmwareUpdate::passwordLength(){return strlen(pendingPassword);}
const char* FirmwareUpdate::passwordToken() {
    static char token[2]{};
    if(characterChoice==0)return "CONNECT";
    if(characterChoice==1)return "DELETE";
    if(characterChoice==2)return "SPACE";
    token[0]=char(32+characterChoice-2);return token;
}
bool FirmwareUpdate::acceptPasswordCharacter() {
    const size_t length=strlen(pendingPassword);
    if(characterChoice==0) {
        if(length<8){networkNote="Password: 8-63 chars";return false;}
        connectNetwork(true,true);return true;
    }
    if(characterChoice==1){if(length)pendingPassword[length-1]=0;}
    else if(length<63){pendingPassword[length]=char(32+characterChoice-2);pendingPassword[length+1]=0;}
    networkNote="Turn: choose Click: add";return false;
}
void FirmwareUpdate::forgetNetwork() {
    if(!canConfigure())return;
    clearSavedNetwork();
    useAccessPoint();networkNote="Saved Wi-Fi removed";
}

bool FirmwareUpdate::beginBluetoothProvisioning()
{
    if(motionActive||auxiliaryMotionActive()||active())return false;
    currentStatus=FirmwareUpdateStatus::Starting;currentProgress=0;currentError=0;receivedBytes=0;rebootAt=0;
    portalRequested=false;bluetoothProvisioning=true;networkNote="Scanning...";
    scan();return scanning();
}

bool FirmwareUpdate::bluetoothProvisioningActive(){return bluetoothProvisioning;}

bool FirmwareUpdate::selectBluetoothNetwork(uint8_t index)
{
    if(!bluetoothProvisioning||scanPending||index>=networksFound)return false;
    snprintf(pendingSsid,sizeof(pendingSsid),"%s",networks[index].name);
    memset(pendingPassword,0,sizeof(pendingPassword));
    if(networks[index].secure)return true;
    connectNetwork(true,false);return false;
}

bool FirmwareUpdate::submitBluetoothPassword(const char* password)
{
    if(!bluetoothProvisioning||!pendingSsid[0]||!password)return false;
    const size_t length=strlen(password);
    if(length<8||length>=sizeof(pendingPassword))return false;
    snprintf(pendingPassword,sizeof(pendingPassword),"%s",password);
    connectNetwork(true,false);return true;
}

void FirmwareUpdate::clearSavedNetwork()
{
    memset(settings.wifiSsid,0,sizeof(settings.wifiSsid));memset(settings.wifiPassword,0,sizeof(settings.wifiPassword));saveSettings();
}

#else

bool FirmwareUpdate::begin()
{
    Serial.println("Firmware updater disabled in Bluetooth diagnostic build");
    return false;
}

void FirmwareUpdate::end() {}
void FirmwareUpdate::poll() {}
bool FirmwareUpdate::active(){return false;}
bool FirmwareUpdate::uploading(){return false;}
FirmwareUpdateStatus FirmwareUpdate::status(){return FirmwareUpdateStatus::Error;}
uint8_t FirmwareUpdate::progressPercent(){return 0;}
uint32_t FirmwareUpdate::bytesReceived(){return 0;}
uint8_t FirmwareUpdate::errorCode(){return 253;}
const char* FirmwareUpdate::statusText(){return "Disabled for BT test";}
const char* FirmwareUpdate::ipAddress(){return "";}
const char* FirmwareUpdate::networkName(){return "";}
const char* FirmwareUpdate::accessCode(){return "";}
const char* FirmwareUpdate::networkMessage(){return "Wi-Fi disabled";}
bool FirmwareUpdate::stationMode(){return false;}
bool FirmwareUpdate::beginBluetoothProvisioning(){return false;}
bool FirmwareUpdate::bluetoothProvisioningActive(){return false;}
bool FirmwareUpdate::selectBluetoothNetwork(uint8_t){return false;}
bool FirmwareUpdate::submitBluetoothPassword(const char*){return false;}
void FirmwareUpdate::clearSavedNetwork(){}
void FirmwareUpdate::useAccessPoint(){}
void FirmwareUpdate::scan(){}
bool FirmwareUpdate::scanning(){return false;}
uint8_t FirmwareUpdate::networkCount(){return 0;}
const char* FirmwareUpdate::ssid(uint8_t){return "";}
bool FirmwareUpdate::secured(uint8_t){return false;}
bool FirmwareUpdate::selectNetwork(uint8_t){return false;}
void FirmwareUpdate::editPassword(int){}
bool FirmwareUpdate::acceptPasswordCharacter(){return false;}
const char* FirmwareUpdate::passwordToken(){return "";}
uint8_t FirmwareUpdate::passwordLength(){return 0;}
void FirmwareUpdate::forgetNetwork(){}

#endif
