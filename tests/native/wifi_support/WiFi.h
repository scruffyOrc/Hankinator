#pragma once
#include "Arduino.h"
#include <vector>
constexpr int WIFI_OFF=0,WIFI_STA=1,WIFI_AP=2,WL_CONNECTED=3,ENC_TYPE_NONE=0;
struct IPAddress {
    uint32_t value=0;
    IPAddress()=default;
    IPAddress(uint8_t a,uint8_t b,uint8_t c,uint8_t d):value((uint32_t(a)<<24)|(uint32_t(b)<<16)|(uint32_t(c)<<8)|d){}
    explicit operator uint32_t()const{return value;}
    String toString()const{char s[16];snprintf(s,sizeof(s),"%u.%u.%u.%u",unsigned(value>>24),unsigned((value>>16)&255),unsigned((value>>8)&255),unsigned(value&255));return s;}
};
struct FakeWiFi {
    struct Entry {const char* name;bool secure;};
    std::vector<Entry> entries;
    int modeValue=0,connection=0,result=-1;
    bool asyncScan=false,asyncConnect=false,apWorks=true;
    std::string attemptedSsid,attemptedPassword;
    IPAddress ip;
    void mode(int m){modeValue=m;}
    void disconnect(){connection=0;ip=IPAddress();}
    void softAPdisconnect(bool){}
    bool disconnectAP(){return true;}
    void softAPConfig(IPAddress,IPAddress,IPAddress){}
    bool softAP(const char*,const char*){return apWorks;}
    IPAddress softAPIP(){return IPAddress(192,168,4,1);}
    void beginNoBlock(const char* ssid,const char* pass=""){asyncConnect=true;attemptedSsid=ssid;attemptedPassword=pass;}
    int status(){return connection;}
    IPAddress localIP(){return ip;}
    int scanNetworks(bool async){asyncScan=async;result=-1;return -1;}
    int scanComplete(){return result;}
    String SSID(int i){return entries.at(i).name;}
    int encryptionType(int i){return entries.at(i).secure?1:ENC_TYPE_NONE;}
};
inline FakeWiFi WiFi;
