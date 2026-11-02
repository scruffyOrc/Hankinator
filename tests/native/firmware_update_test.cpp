#include "firmware_update.h"
#include "wifi_support/app_api.h"
#include "config.h"
#include "WiFi.h"
#include "WebServer.h"
#include "Updater.h"
#include "LittleFS.h"
#include <cassert>
TestSettings settings;
bool motionActive=false;
uint32_t now=0;int saves=0;
uint32_t millis(){return now;}
bool auxiliaryMotionActive(){return false;}
void saveSettings(){saves++;}
void dhcp(){WiFi.connection=WL_CONNECTED;WiFi.ip=IPAddress(192,168,1,22);FirmwareUpdate::poll();}
int main(){
    motionActive=true;assert(!FirmwareUpdate::begin());motionActive=false;
    assert(FirmwareUpdate::begin());assert(!FirmwareUpdate::stationMode());assert(std::string(FirmwareUpdate::ipAddress())=="192.168.4.1");
    FirmwareUpdate::scan();assert(WiFi.asyncScan&&FirmwareUpdate::scanning());
    WiFi.entries={{"Home",true},{"Home",true},{"Guest",false},{"",true}};WiFi.result=4;FirmwareUpdate::poll();
    assert(FirmwareUpdate::networkCount()==2);assert(FirmwareUpdate::selectNetwork(0));
    assert(!FirmwareUpdate::acceptPasswordCharacter());assert(FirmwareUpdate::status()==FirmwareUpdateStatus::Scanning);
    // ASCII 'a' is choice 67; enter eight characters, then select Connect.
    for(int i=0;i<67;i++)FirmwareUpdate::editPassword(1);
    for(int i=0;i<8;i++)assert(!FirmwareUpdate::acceptPasswordCharacter());
    assert(FirmwareUpdate::passwordLength()==8);
    for(int i=0;i<67;i++)FirmwareUpdate::editPassword(-1);
    assert(FirmwareUpdate::acceptPasswordCharacter());now+=Config::WifiStationStartDelayMs;FirmwareUpdate::poll();assert(WiFi.asyncConnect&&saves==0);
    WiFi.connection=WL_CONNECTED;FirmwareUpdate::poll();assert(saves==0); // Wait for DHCP, not just association.
    dhcp();assert(saves==1);assert(std::string(settings.wifiSsid)=="Home");assert(std::string(settings.wifiPassword)=="aaaaaaaa");
    assert(FirmwareUpdate::stationMode());assert(std::string(FirmwareUpdate::ipAddress())=="192.168.1.22");
    auto* server=WebServer::instance;server->get();assert(server->challenged);
    server->data.status=UPLOAD_FILE_START;server->uploadHandler();assert(Update.starts==0);
    LittleFS.stagedImage=true;server->authenticated=true;server->uploadHandler();assert(Update.starts==1);assert(!LittleFS.stagedImage);
    assert(FirmwareUpdate::uploading());FirmwareUpdate::scan();FirmwareUpdate::end();assert(FirmwareUpdate::uploading());
    server->data.status=UPLOAD_FILE_ABORTED;server->uploadHandler();FirmwareUpdate::end();
    assert(!FirmwareUpdate::active());assert(WiFi.modeValue==WIFI_OFF);
    FirmwareUpdate::begin();assert(FirmwareUpdate::status()==FirmwareUpdateStatus::Starting);
    now+=Config::WifiStationStartDelayMs;FirmwareUpdate::poll();assert(FirmwareUpdate::status()==FirmwareUpdateStatus::Connecting);assert(WiFi.attemptedSsid=="Home");
    now+=Config::WifiConnectTimeoutMs;FirmwareUpdate::poll();assert(FirmwareUpdate::status()==FirmwareUpdateStatus::Starting);
    now+=Config::WifiStationStartDelayMs;FirmwareUpdate::poll();assert(FirmwareUpdate::status()==FirmwareUpdateStatus::Connecting);
    now+=Config::WifiConnectTimeoutMs;FirmwareUpdate::poll();assert(!FirmwareUpdate::stationMode());assert(saves==1);assert(std::string(settings.wifiSsid)=="Home");
    FirmwareUpdate::scan();WiFi.result=4;FirmwareUpdate::poll();assert(!FirmwareUpdate::selectNetwork(1));
    now+=Config::WifiConnectTimeoutMs;FirmwareUpdate::poll();assert(std::string(settings.wifiSsid)=="Home"); // Failed new network preserves old.
    FirmwareUpdate::scan();now+=Config::WifiScanTimeoutMs;FirmwareUpdate::poll();assert(!FirmwareUpdate::scanning());
    FirmwareUpdate::end();
    assert(FirmwareUpdate::beginBluetoothProvisioning());
    WiFi.entries={{"Provisioned",true}};WiFi.result=1;FirmwareUpdate::poll();
    assert(FirmwareUpdate::bluetoothProvisioningActive());assert(FirmwareUpdate::networkCount()==1);
    assert(FirmwareUpdate::selectBluetoothNetwork(0));
    assert(!FirmwareUpdate::submitBluetoothPassword("short"));
    assert(FirmwareUpdate::submitBluetoothPassword("provisioned-password"));
    now+=Config::WifiStationStartDelayMs;FirmwareUpdate::poll();
    WiFi.connection=WL_CONNECTED;WiFi.ip=IPAddress(192,168,1,23);FirmwareUpdate::poll();
    assert(std::string(settings.wifiSsid)=="Provisioned");assert(std::string(settings.wifiPassword)=="provisioned-password");
    assert(FirmwareUpdate::status()==FirmwareUpdateStatus::Ready);assert(!WebServer::instance->listening);
    FirmwareUpdate::end();assert(!FirmwareUpdate::active()&&WiFi.modeValue==WIFI_OFF);
    assert(FirmwareUpdate::begin());FirmwareUpdate::forgetNetwork();assert(!settings.wifiSsid[0]&&!settings.wifiPassword[0]);
    FirmwareUpdate::end();std::puts("12 Wi-Fi updater integration scenarios passed");
}
