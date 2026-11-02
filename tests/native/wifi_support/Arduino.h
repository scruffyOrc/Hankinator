#pragma once
#include <stdint.h>
#include <stddef.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <algorithm>
#define PROGMEM
#define LED_BUILTIN 25
uint32_t millis();
class String {
public:
    std::string value;
    String(const char* s=""):value(s){}
    String(std::string s):value(s){}
    String(unsigned n):value(std::to_string(n)){}
    size_t length()const{return value.size();}
    const char* c_str()const{return value.c_str();}
    void toCharArray(char* out,size_t n)const{snprintf(out,n,"%s",c_str());}
    bool endsWith(const char* s)const{size_t n=strlen(s);return value.size()>=n&&value.compare(value.size()-n,n,s)==0;}
    void replace(const char* from,const char* to){size_t pos=value.find(from);if(pos!=std::string::npos)value.replace(pos,strlen(from),to);}
    bool operator==(const char* s)const{return value==s;}
    String operator+(unsigned n)const{return value+std::to_string(n);}
    String operator+(const char* s)const{return value+s;}
};
struct FakeSerial {template<class T>void print(T){} template<class T>void println(T){}};
inline FakeSerial Serial;
struct FakePico {uint32_t hwrand32(){return 0x12345678;}void restart(){restarted=true;}bool restarted=false;};
inline FakePico rp2040;
