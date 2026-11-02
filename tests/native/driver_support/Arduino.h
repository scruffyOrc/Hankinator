#pragma once
#include "../wifi_support/Arduino.h"
#include <deque>
#include <vector>
constexpr int INPUT_PULLDOWN=0,RISING=1,HIGH=1;
inline void pinMode(int,int){}
inline int digitalPinToInterrupt(int pin){return pin;}
inline void attachInterrupt(int,void(*)(),int){}
inline void noInterrupts(){}
inline void interrupts(){}
inline int digitalRead(int){return 0;}
template<class T>T constrain(T v,T lo,T hi){return std::min(hi,std::max(lo,v));}
struct FakeUart {
    bool writable=true;
    std::deque<uint8_t> rx;
    std::vector<std::vector<uint8_t>> requests;
    void setRX(int){} void setTX(int){} void begin(int){}
    int availableForWrite(){return writable?1:0;} // Actual Arduino-Pico contract.
    int available(){return int(rx.size());}
    int read(){int v=rx.front();rx.pop_front();return v;}
    size_t write(const uint8_t* data,size_t n){requests.emplace_back(data,data+n);return n;}
};
inline FakeUart Serial2;
