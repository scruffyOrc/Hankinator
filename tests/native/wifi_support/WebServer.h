#pragma once
#include "Arduino.h"
#include <functional>
constexpr int HTTP_GET=0,HTTP_POST=1,UPLOAD_FILE_START=0,UPLOAD_FILE_WRITE=1,UPLOAD_FILE_END=2,UPLOAD_FILE_ABORTED=3;
struct HTTPUpload {int status=0;String filename="firmware.bin";uint8_t buf[16]{};size_t currentSize=16;};
class WebServer {
public:
    inline static WebServer* instance=nullptr;
    explicit WebServer(int){instance=this;}
    std::function<void()> get,post,uploadHandler;
    HTTPUpload data;bool authenticated=false,challenged=false,listening=false;
    void on(const char*,int method,std::function<void()> handler){if(method==HTTP_GET)get=handler;}
    void on(const char*,int,std::function<void()> handler,std::function<void()> uploader){post=handler;uploadHandler=uploader;}
    void onNotFound(std::function<void()>){ }
    bool authenticate(const char*,const char*){return authenticated;}
    void requestAuthentication(){challenged=true;}
    void sendHeader(const char*,const char*,bool=false){}
    template<class T>void send(int,const char*,T){}
    void begin(){listening=true;}
    void stop(){listening=false;}
    void handleClient(){}
    HTTPUpload& upload(){return data;}
};
