#pragma once
#include <stdint.h>
#include <stddef.h>
#include <cmath>
#include <climits>
#include <cstdio>
#include <algorithm>
#include <cstring>
using std::min;using std::max;using std::isnan;
uint32_t millis();
uint32_t micros();
constexpr int LOW=0,HIGH=1,INPUT_PULLUP=2,CHANGE=3;
void pinMode(int,int);
int digitalRead(int);
inline int digitalPinToInterrupt(int pin){return pin;}
void attachInterrupt(int,void(*)(),int);
inline void noInterrupts(){}
inline void interrupts(){}
#define LED_BUILTIN 25
