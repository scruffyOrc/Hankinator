#include "tmc_uart_frame.h"
#include <cassert>
#include <cstdio>
int main(){
    const uint8_t request[]={5,0,0};assert(TmcUart::crc(request,3)==0x48);
    uint8_t response[]={5,0xFF,0x41,0,0,1,44,0};response[7]=TmcUart::crc(response,7);
    TmcUart::Frame parser;uint32_t value=999;
    for(auto b:request)assert(!parser.push(b,0x41,value));
    for(int i=0;i<7;i++)assert(!parser.push(response[i],0x41,value));
    assert(parser.push(response[7],0x41,value)&&value==300);
    parser.reset();response[7]^=1;for(auto b:response)assert(!parser.push(b,0x41,value));
    response[7]^=1;parser.reset();for(auto b:response)assert(!parser.push(b,0x6F,value));
    // A legitimate zero load result is distinct from no valid response.
    response[5]=response[6]=0;response[7]=TmcUart::crc(response,7);parser.reset();
    for(int i=0;i<7;i++)assert(!parser.push(response[i],0x41,value));
    assert(parser.push(response[7],0x41,value)&&value==0);
    std::puts("5 UART framing scenarios passed");
}
