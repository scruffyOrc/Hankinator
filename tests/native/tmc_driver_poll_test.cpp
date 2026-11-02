#include <tmc_driver.h>
#include "tmc_uart_frame.h"
#include "config.h"
#include <cassert>
uint32_t now=0;
uint32_t millis(){return now;}
void reply(uint32_t value,bool good=true) {
    const auto& request=Serial2.requests.back();
    assert(request.size()==4&&request[3]==TmcUart::crc(request.data(),3));
    for(auto b:request)Serial2.rx.push_back(b); // single-wire TX echo
    uint8_t frame[]={5,255,request[2],uint8_t(value>>24),uint8_t(value>>16),uint8_t(value>>8),uint8_t(value),0};
    frame[7]=TmcUart::crc(frame,7);if(!good)frame[7]^=1;
    for(auto b:frame)Serial2.rx.push_back(b);
    ++now;TmcDriver::poll();
}
int main() {
    TmcDriver::begin();assert(TmcDriver::connected());assert(!TmcDriver::sample().valid);
    now=25;TmcDriver::poll();assert(Serial2.requests.size()==1&&Serial2.requests.back()[2]==0x41);
    reply(200);assert(!TmcDriver::sample().valid); // status still needed
    now=50;TmcDriver::poll();assert(Serial2.requests.size()==2&&Serial2.requests.back()[2]==0x6f);
    reply(1UL<<30);assert(TmcDriver::sample().valid&&TmcDriver::sample().sg==200);
    Serial2.writable=false;now=75;TmcDriver::poll();assert(Serial2.requests.size()==2);
    Serial2.writable=true;now=76;TmcDriver::poll();assert(Serial2.requests.size()==3);
    reply(42,false);now=85;TmcDriver::poll();assert(!TmcDriver::sample().valid);
    now=101;TmcDriver::poll();reply(1UL<<30);
    now=126;TmcDriver::poll();reply(0);assert(TmcDriver::sample().valid&&TmcDriver::sample().sg==0);
    now+=Config::StallDataFreshMs+1;assert(!TmcDriver::sample().valid);
    std::puts("6 actual UART polling regression scenarios passed");
}
