#pragma once
#include "Arduino.h"
constexpr int U_FLASH=0;
constexpr uint8_t UPDATE_ERROR_WRITE=1,UPDATE_ERROR_ERASE=2,UPDATE_ERROR_READ=3,
                  UPDATE_ERROR_SPACE=4,UPDATE_ERROR_SIZE=5,UPDATE_ERROR_STREAM=6,
                  UPDATE_ERROR_MD5=7,UPDATE_ERROR_MAGIC_BYTE=10,UPDATE_ERROR_NO_DATA=13;
struct FakeUpdate {
    bool running=false;int starts=0;size_t written=0;
    bool begin(size_t,int){running=true;starts++;return true;}
    bool isRunning(){return running;}
    bool end(bool=false){running=false;return true;}
    uint8_t getError(){return 0;}
    size_t write(uint8_t*,size_t n){written+=n;return n;}
};
inline FakeUpdate Update;
