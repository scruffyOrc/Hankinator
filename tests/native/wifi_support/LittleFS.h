#pragma once
#include <stddef.h>
struct FSInfo {size_t totalBytes=0,usedBytes=0;};
struct FakeFS {
 bool mounted=true,stagedImage=false,removeSucceeds=true;
 bool begin(){return mounted;}
 bool exists(const char*){return stagedImage;}
 bool remove(const char*){if(!removeSucceeds)return false;stagedImage=false;return true;}
 bool info(FSInfo& i){if(!mounted)return false;i.totalBytes=768*1024;i.usedBytes=0;return true;}
};
inline FakeFS LittleFS;
