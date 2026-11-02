#pragma once
#include "config.h"
#include "persistent_settings.h"
#include <cstring>
#include <cassert>
namespace Config {
constexpr int MIN_TURNS=MinTurns,MAX_TURNS=MaxTurns;
constexpr uint32_t SETTINGS_MAGIC=SettingsMagic;
constexpr uint16_t SETTINGS_VERSION=SettingsVersion;
constexpr size_t EEPROM_SIZE=EepromSize;
}
constexpr int YARN_WEIGHT_COUNT=SettingsLayout::YarnCount,SKEIN_SIZE_COUNT=SettingsLayout::SizeCount;
struct SkeinSize {const char* name;uint16_t factoryDefaultTurns;};
constexpr SkeinSize SKEIN_SIZES[]={{"Mini",63},{"Half",90},{"Full",110}};
extern PersistentSettings settings;
struct FakeEEPROM {
    unsigned char bytes[4096];int commits=0;
    void begin(size_t){}
    template<class T>void put(size_t offset,const T& v){assert(offset+sizeof(v)<=sizeof(bytes));memcpy(bytes+offset,&v,sizeof(v));}
    template<class T>void get(size_t offset,T& v){assert(offset+sizeof(v)<=sizeof(bytes));memcpy(&v,bytes+offset,sizeof(v));}
    void commit(){commits++;}
};
inline FakeEEPROM EEPROM;
void loadSettings();void saveSettings();void loadFactoryDefaults();bool settingsAreValid();
