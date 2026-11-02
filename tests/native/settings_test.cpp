#include "settings_support/app_api.h"
#include <cstdio>
PersistentSettings settings;
struct V5 {uint32_t magic;uint16_t version;uint16_t turns[8][3];uint16_t run,hold;uint8_t bt,cw;uint16_t targets[3],autoTurns;};
static_assert(sizeof(V5)==offsetof(PersistentSettings,hallEnabled),"v5 prefix must match");
int main(){
    memset(EEPROM.bytes,0xFF,sizeof(EEPROM.bytes));loadSettings();assert(settingsAreValid());assert(!settings.hallEnabled&&!settings.wifiSsid[0]);
    V5 old{};old.magic=Config::SettingsMagic;old.version=5;old.run=950;old.hold=250;old.bt=1;old.cw=0;
    for(auto& row:old.turns)for(auto& turns:row)turns=75;
    old.targets[0]=2030;old.targets[1]=5150;old.targets[2]=10200;old.autoTurns=85;
    memset(EEPROM.bytes,0xFF,sizeof(EEPROM.bytes));EEPROM.put(0,old);loadSettings();
    assert(settingsAreValid()&&settings.runCurrentMa==950&&settings.holdCurrentMa==250&&settings.clockwise==0);
    assert(settings.autoTurns==85&&settings.weightTargetsCentiGrams[2]==10200&&settings.turns[7][2]==75);
    assert(!settings.hallEnabled&&!settings.wifiPassword[0]);
    settings.hallEnabled=1;strcpy(settings.wifiSsid,"Test");strcpy(settings.wifiPassword,"testpass");saveSettings();
    const int before=EEPROM.commits;memset(&settings,0,sizeof(settings));loadSettings();
    assert(EEPROM.commits==before&&settings.hallEnabled==1&&strcmp(settings.wifiSsid,"Test")==0&&strcmp(settings.wifiPassword,"testpass")==0);
    old.version=4;EEPROM.put(0,old);loadSettings();assert(settings.autoTurns==62&&settings.runCurrentMa==950&&!settings.hallEnabled);
    memset(settings.wifiSsid,'x',sizeof(settings.wifiSsid));assert(!settingsAreValid());
    std::puts("5 EEPROM migration scenarios passed");
}
