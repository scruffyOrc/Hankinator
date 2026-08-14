#include <LittleFS.h>
#include "config.h"
#include "tmc_driver.h"
#include "telemetry.h"
#include "diagnostics_transport.h"
#include "runtime.h"

namespace {
struct Sample {uint32_t ms,steps;uint16_t rps100,sg;uint8_t flags,pause;uint16_t status;};
struct Header {uint32_t magic;uint16_t version,count;uint8_t end,reserved;uint16_t crc;};
constexpr uint16_t Capacity=2048;
constexpr char LogPath[]="/telemetry.bin";
Sample ram[Capacity]; uint16_t count=0; uint32_t totalCount=0,started=0,last=0; bool fsReady=false;

uint16_t crc(const uint8_t* p,size_t n){uint16_t c=0xFFFF;while(n--){c^=*p++;for(int i=0;i<8;i++)c=(c&1)?(c>>1)^0xA001:c>>1;}return c;}
void fixed2(Print& output,uint16_t v){output.print(v/100);output.print('.');if(v%100<10)output.print('0');output.print(v%100);}
void turns(Print& output,uint32_t steps){uint64_t v=uint64_t(steps)*10000ULL/Config::StepsPerHubRev;output.print(uint32_t(v/10000));output.print('.');uint16_t f=v%10000;if(f<1000)output.print('0');if(f<100)output.print('0');if(f<10)output.print('0');output.print(f);}
bool header(File& f,Header& h){return f.read(reinterpret_cast<uint8_t*>(&h),sizeof(h))==sizeof(h)&&h.magic==Config::LogMagic&&h.version==Config::LogVersion&&h.count<=Capacity;}

void dump(Print& output){
 if(!fsReady){output.println("Telemetry storage unavailable");return;}
 File f=LittleFS.open(LogPath,"r");Header h{};if(!f||!header(f,h)){output.println("No saved telemetry");return;}
 output.println("ms,rps,steps,turns,sg_result,diag,pause,drv_status_low");
 for(uint16_t i=0;i<h.count;i++){
  Sample s{};if(f.read(reinterpret_cast<uint8_t*>(&s),sizeof(s))!=sizeof(s))break;
  output.print(s.ms);output.print(',');fixed2(output,s.rps100);output.print(',');output.print(s.steps);output.print(',');turns(output,s.steps);output.print(',');output.print(s.sg);output.print(',');output.print(s.flags&1);output.print(',');output.print(s.pause);output.print(",0x");
  if(s.status<0x1000)output.print('0');if(s.status<0x100)output.print('0');if(s.status<0x10)output.print('0');output.println(s.status,HEX);
 }
 output.print("end,");output.print(h.end==uint8_t(RunEnd::Complete)?"complete":"abort");output.print(",samples,");output.println(h.count);
}
void clear(Print& output){if(fsReady&&LittleFS.exists(LogPath))LittleFS.remove(LogPath);output.println("Telemetry cleared");}
}

void Telemetry::begin(){fsReady=LittleFS.begin();Serial.println(fsReady?"Telemetry commands: LOG DUMP, LOG CLEAR, LOG HELP":"Telemetry storage mount failed");}
void Telemetry::start(){count=0;totalCount=0;started=millis();last=0;if constexpr(Config::EnableBluetoothLiveTelemetry)DiagnosticsTransport::beginTelemetryStream(YARN_WEIGHTS[selectedYarnWeight].label,SKEIN_SIZES[selectedSkeinSize].label,uint16_t(selectedTurns));}
void Telemetry::sample(float rps,uint32_t steps,PauseState pause){uint32_t now=millis();if(now-last<Config::TelemetryIntervalMs)return;last=now;Sample s{now-started,steps,uint16_t(max(0.0f,rps)*100),TmcDriver::sgResult(),uint8_t(digitalRead(Pins::Diag)?1:0),uint8_t(pause),uint16_t(TmcDriver::drvStatus())};totalCount++;if(count<Capacity)ram[count++]=s;if constexpr(Config::EnableBluetoothLiveTelemetry)DiagnosticsTransport::streamTelemetry(s.ms,s.rps100,s.steps,s.sg,s.flags&1,s.pause,s.status);}
void Telemetry::finish(RunEnd end){
 if(fsReady){File f=LittleFS.open(LogPath,"w");Header h{Config::LogMagic,Config::LogVersion,count,uint8_t(end),0,crc(reinterpret_cast<uint8_t*>(ram),count*sizeof(Sample))};if(f){f.write(reinterpret_cast<const uint8_t*>(&h),sizeof(h));f.write(reinterpret_cast<const uint8_t*>(ram),count*sizeof(Sample));f.close();}}
 if constexpr(Config::EnableBluetoothLiveTelemetry)DiagnosticsTransport::endTelemetryStream(end==RunEnd::Complete?"complete":"abort",millis()-started,totalCount,count);
 Serial.print("Telemetry saved: ");Serial.print(count);Serial.print(" samples (");Serial.print(end==RunEnd::Complete?"complete":"abort");Serial.println(')');count=0;
}
void Telemetry::executeCommand(const char* command,Print& output,bool logAccessAllowed){
 String c(command);c.trim();c.toUpperCase();
 if(c=="LOG HELP"){output.println("LOG DUMP | LOG CLEAR | LOG HELP");return;}
 if(c!="LOG DUMP"&&c!="LOG CLEAR")return;
 if(!logAccessAllowed){output.println("BUSY: log access available while idle");return;}
 if(c=="LOG DUMP")dump(output);else clear(output);
}
