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

namespace {
WebServer server(80);
FirmwareUpdateStatus currentStatus=FirmwareUpdateStatus::Off;
uint8_t currentProgress=0,currentError=0;
uint32_t receivedBytes=0;
uint32_t rebootAt=0;
bool serverConfigured=false;

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
    server.on("/",HTTP_GET,[](){String page(updatePage);page.replace("%VERSION%",Config::FirmwareVersion);server.sendHeader("Connection","close");server.send(200,"text/html",page);});
    server.on("/update",HTTP_POST,[](){
        server.sendHeader("Connection","close");
        if(currentStatus==FirmwareUpdateStatus::Success)
        {
            server.send(200,"text/plain","Update received. HankWinder is restarting...");
            rebootAt=millis()+Config::FirmwareUpdateRebootDelayMs;
        }
        else server.send(500,"text/plain",String("Update failed (error ")+currentError+"). USB firmware remains unchanged.");
    },[](){
        HTTPUpload& upload=server.upload();
        if(upload.status==UPLOAD_FILE_START)
        {
            currentProgress=0;currentError=0;receivedBytes=0;currentStatus=FirmwareUpdateStatus::Uploading;
            WiFiUDP::stopAll();
            FSInfo info{};
            if(!upload.filename.endsWith(".bin"))
            {
                currentError=254;currentStatus=FirmwareUpdateStatus::Error;
            }
            else if(!LittleFS.info(info)||!Update.begin(info.totalBytes-info.usedBytes,U_FLASH))
            {
                currentError=Update.getError();currentStatus=FirmwareUpdateStatus::Error;
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
    if(active())return currentStatus!=FirmwareUpdateStatus::Error;
    currentStatus=FirmwareUpdateStatus::Starting;currentProgress=0;currentError=0;receivedBytes=0;rebootAt=0;
    configureServer();
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(IPAddress(192,168,4,1),IPAddress(192,168,4,1),IPAddress(255,255,255,0));
    if(!WiFi.softAP(Config::FirmwareUpdateSsid,Config::FirmwareUpdatePassword))
    {
        currentStatus=FirmwareUpdateStatus::Error;currentError=255;return false;
    }
    server.begin();currentStatus=FirmwareUpdateStatus::Ready;
    Serial.print("Firmware updater ready: http://");Serial.println(WiFi.softAPIP());
    return true;
}

void FirmwareUpdate::end()
{
    if(!active()||uploading()||currentStatus==FirmwareUpdateStatus::Success)return;
    if(Update.isRunning())Update.end();
    server.stop();WiFi.softAPdisconnect(true);WiFi.mode(WIFI_OFF);
    currentStatus=FirmwareUpdateStatus::Off;currentProgress=0;currentError=0;rebootAt=0;
}

void FirmwareUpdate::poll()
{
    if(!active())return;
    server.handleClient();
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
        case FirmwareUpdateStatus::Ready:return "Open 192.168.4.1";
        case FirmwareUpdateStatus::Uploading:return "Uploading";
        case FirmwareUpdateStatus::Success:return "Restarting";
        case FirmwareUpdateStatus::Error:return "Update error";
    }
    return "Update error";
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

#endif
