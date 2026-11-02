#pragma once
#include <stdint.h>
#include <string.h>
namespace TmcUart {
inline uint8_t crc(const uint8_t* data,uint8_t count) {
    uint8_t result=0;
    for(uint8_t i=0;i<count;i++){uint8_t value=data[i];for(uint8_t b=0;b<8;b++){result=((result>>7)^(value&1))?(result<<1)^7:result<<1;value>>=1;}}
    return result;
}
class Frame {
public:
    void reset(){count=0;}
    bool push(uint8_t byte,uint8_t address,uint32_t& value) {
        if(count<8)data[count++]=byte;else {memmove(data,data+1,7);data[7]=byte;}
        if(count!=8||data[0]!=5||data[1]!=0xFF||data[2]!=address||crc(data,7)!=data[7])return false;
        value=(uint32_t(data[3])<<24)|(uint32_t(data[4])<<16)|(uint32_t(data[5])<<8)|data[6];
        return true;
    }
private:
    uint8_t data[8]{},count=0;
};
}
