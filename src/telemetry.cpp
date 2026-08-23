#include <LittleFS.h>
#include "config.h"
#include "tmc_driver.h"
#include "telemetry.h"
#include "diagnostics_transport.h"
#include "runtime.h"
#include "app_api.h"
#include "load_cells.h"
#include "product_mode.h"
#include "run_supervisor.h"

namespace {
struct Sample {uint32_t ms,steps,status;uint16_t rps100,sg;uint8_t flags,pause;int32_t loadCellRaw[Config::LoadCellChannelCount];int32_t weightCentiGrams,filteredWeightCentiGrams;uint16_t weightSpreadCentiGrams,sgBaseline;uint8_t loadCellHealthyMask,tareValid,weightValid,controlFlags;};
struct Header {uint32_t magic;uint16_t version,count;uint8_t end,requiredLoadCells,detectedLoadCells,productMode,tareValid,weightControlArmed;int32_t tareOffset[Config::LoadCellChannelCount];int32_t tareSpread[Config::LoadCellChannelCount];int32_t tareProfile[Config::LoadCellProfileBins][Config::LoadCellChannelCount];uint8_t tareProfileSamples[Config::LoadCellProfileBins];int32_t targetWeightCentiGrams;uint16_t crc;};
constexpr uint16_t Capacity=2048;
constexpr char LogPath[]="/telemetry.bin";
Sample ram[Capacity]; uint16_t count=0; uint32_t totalCount=0,started=0,last=0; bool fsReady=false;
constexpr uint8_t WeightWindowSize=10;
int32_t weightWindow[WeightWindowSize]{};uint8_t weightWindowCount=0,weightWindowWrite=0;

uint16_t crc(const uint8_t* p,size_t n){uint16_t c=0xFFFF;while(n--){c^=*p++;for(int i=0;i<8;i++)c=(c&1)?(c>>1)^0xA001:c>>1;}return c;}
const char* phaseName(uint8_t phase){static const char* names[]={"launch","start_hold","accel","cruise","decel","pause_down","paused","resume"};return phase<8?names[phase]:"unknown";}
void fixed2(Print& output,uint16_t v){output.print(v/100);output.print('.');if(v%100<10)output.print('0');output.print(v%100);}
void turns(Print& output,uint32_t steps){uint64_t v=uint64_t(steps)*10000ULL/Config::StepsPerHubRev;output.print(uint32_t(v/10000));output.print('.');uint16_t f=v%10000;if(f<1000)output.print('0');if(f<100)output.print('0');if(f<10)output.print('0');output.print(f);}
void signedFixed2(Print& output,int32_t value){if(value<0){output.print('-');value=-value;}output.print(value/100);output.print('.');if(value%100<10)output.print('0');output.print(value%100);}
void hex32(Print& output,uint32_t value){for(int shift=28;shift>=0;shift-=4)output.print((value>>shift)&0x0f,HEX);}
bool header(File& f,Header& h){return f.read(reinterpret_cast<uint8_t*>(&h),sizeof(h))==sizeof(h)&&h.magic==Config::LogMagic&&h.version==Config::LogVersion&&h.count<=Capacity;}

void dump(Print& output){
 if(!fsReady){output.println("Telemetry storage unavailable");return;}
 File f=LittleFS.open(LogPath,"r");Header h{};if(!f||!header(f,h)){output.println("No saved telemetry");return;}
 output.print("META,required_load_cells,");output.print(h.requiredLoadCells);output.print(",detected_load_cells,");output.print(h.detectedLoadCells);output.print(",product_mode,");output.println(h.productMode==uint8_t(ProductMode::Turninator)?"Turninator":h.productMode==uint8_t(ProductMode::Fuhgeddabouditinator)?"Fuhgeddabouditinator":"load_cell_fault");
 output.print("CONTROL,stall_detection,");output.print(Config::EnableStallDetection?1:0);output.print(",stall_auto_abort,");output.print(Config::EnableAutomaticStallAbort?1:0);output.print(",weight_armed,");output.print(h.weightControlArmed);output.print(",target_weight_g,");signedFixed2(output,h.targetWeightCentiGrams);output.println();
 output.print("TARE,valid,");output.print(h.tareValid);for(uint8_t channel=0;channel<Config::LoadCellChannelCount;channel++){output.print(",lc");output.print(channel+1);output.print("_offset,");output.print(h.tareOffset[channel]);output.print(",lc");output.print(channel+1);output.print("_spread,");output.print(h.tareSpread[channel]);}output.println();
 for(uint8_t bin=0;bin<Config::LoadCellProfileBins;bin++){output.print("TARE_PROFILE,bin,");output.print(bin);output.print(",samples,");output.print(h.tareProfileSamples[bin]);for(uint8_t channel=0;channel<Config::LoadCellChannelCount;channel++){output.print(",lc");output.print(channel+1);output.print(',');output.print(h.tareProfile[bin][channel]);}output.println();}
 output.println("ms,rps,steps,turns,sg_result,diag,pause,phase,drv_status,lc1_raw,lc1_healthy,lc2_raw,lc2_healthy,lc3_raw,lc3_healthy,lc4_raw,lc4_healthy,weight_g,weight_spread_g,tare_valid,weight_valid,sg_baseline,stall_state,filtered_weight_g,weight_control_state");
 for(uint16_t i=0;i<h.count;i++){
  Sample s{};if(f.read(reinterpret_cast<uint8_t*>(&s),sizeof(s))!=sizeof(s))break;
  output.print(s.ms);output.print(',');fixed2(output,s.rps100);output.print(',');output.print(s.steps);output.print(',');turns(output,s.steps);output.print(',');output.print(s.sg);output.print(',');output.print(s.flags&1);output.print(',');output.print(s.pause);output.print(',');output.print(phaseName((s.flags>>1)&7));output.print(",0x");
  hex32(output,s.status);
  for(uint8_t channel=0;channel<Config::LoadCellChannelCount;channel++){output.print(',');output.print(s.loadCellRaw[channel]);output.print(',');output.print((s.loadCellHealthyMask&(1u<<channel))?1:0);}output.print(',');signedFixed2(output,s.weightCentiGrams);output.print(',');fixed2(output,s.weightSpreadCentiGrams);output.print(',');output.print(s.tareValid?1:0);output.print(',');output.print(s.weightValid?1:0);output.print(',');output.print(s.sgBaseline);output.print(',');output.print(RunSupervisor::stallStateName(StallDetectionState(s.controlFlags&3)));output.print(',');signedFixed2(output,s.filteredWeightCentiGrams);output.print(',');output.println(RunSupervisor::weightStateName(WeightApproachState((s.controlFlags>>2)&3)));
 }
 output.print("end,");output.print(h.end==uint8_t(RunEnd::Complete)?"complete":"abort");output.print(",samples,");output.println(h.count);
}
void clear(Print& output){if(fsReady&&LittleFS.exists(LogPath))LittleFS.remove(LogPath);output.println("Telemetry cleared");}
}

void Telemetry::begin(){fsReady=LittleFS.begin();Serial.println(fsReady?"Telemetry commands: LOG DUMP, LOG CLEAR, LOG HELP, MARK <label>":"Telemetry storage mount failed");}
void Telemetry::start()
{
 count=0;totalCount=0;started=millis();last=0;weightWindowCount=0;weightWindowWrite=0;
 if constexpr(Config::EnableBluetoothLiveTelemetry)
 {
  static const char* programs[]={"Mini 20g","Half 50g","Full 100g","Just Turn"};
  if(weightCapabilityEnabled)DiagnosticsTransport::beginTelemetryStream("Fuhgeddabouditinator",programs[uint8_t(activeFuhProgram)],uint16_t(selectedTurns));
  else DiagnosticsTransport::beginTelemetryStream(YARN_WEIGHTS[selectedYarnWeight].label,SKEIN_SIZES[selectedSkeinSize].label,uint16_t(selectedTurns));
 }
}
void Telemetry::sample(float rps,uint32_t steps,PauseState pause){uint32_t now=millis();if(now-last<Config::TelemetryIntervalMs)return;last=now;const uint8_t phase=uint8_t(currentMotionPhase());const LoadCellSnapshot loadCells=LoadCells::snapshot();const RunSupervisorSnapshot control=RunSupervisor::snapshot();Sample s{};s.ms=now-started;s.steps=steps;s.rps100=uint16_t(max(0.0f,rps)*100);s.sg=TmcDriver::sgResult();s.flags=uint8_t((digitalRead(Pins::Diag)?1:0)|(phase<<1));s.pause=uint8_t(pause);s.status=TmcDriver::drvStatus();memcpy(s.loadCellRaw,loadCells.raw,sizeof(s.loadCellRaw));s.loadCellHealthyMask=loadCells.healthyMask;s.tareValid=LoadCells::profileTareValid()?1:0;const float grams=LoadCells::profileWeightGrams(loadCells,steps);s.weightValid=isnan(grams)?0:1;s.weightCentiGrams=s.weightValid?int32_t(lroundf(grams*100.0f)):0;if(s.weightValid){weightWindow[weightWindowWrite]=s.weightCentiGrams;weightWindowWrite=uint8_t((weightWindowWrite+1)%WeightWindowSize);if(weightWindowCount<WeightWindowSize)weightWindowCount++;}int32_t minimum=0,maximum=0;if(weightWindowCount){minimum=INT32_MAX;maximum=INT32_MIN;for(uint8_t i=0;i<weightWindowCount;i++){minimum=min(minimum,weightWindow[i]);maximum=max(maximum,weightWindow[i]);}}s.weightSpreadCentiGrams=uint16_t(constrain(maximum-minimum,0L,65535L));s.sgBaseline=control.sgBaseline;s.filteredWeightCentiGrams=isnan(control.filteredWeightGrams)?0:int32_t(lroundf(control.filteredWeightGrams*100.0f));s.controlFlags=uint8_t(uint8_t(control.stallState)|(uint8_t(control.weightState)<<2));totalCount++;if(count<Capacity)ram[count++]=s;if constexpr(Config::EnableBluetoothLiveTelemetry)DiagnosticsTransport::streamTelemetry(s.ms,s.rps100,s.steps,s.sg,s.flags&1,s.pause,phase,s.status,loadCells,s.weightCentiGrams,s.weightSpreadCentiGrams,s.tareValid!=0,s.weightValid!=0,control);}
void Telemetry::finish(RunEnd end){
 if(fsReady){File f=LittleFS.open(LogPath,"w");const LoadCellTareResult tare=LoadCells::tareResult();const LoadCellProfileTareResult profile=LoadCells::profileTareResult();Header h{};h.magic=Config::LogMagic;h.version=Config::LogVersion;h.count=count;h.end=uint8_t(end);h.requiredLoadCells=Config::RequiredLoadCells;h.detectedLoadCells=detectedLoadCellCount;h.productMode=uint8_t(productMode);h.tareValid=LoadCells::profileTareValid()?1:0;h.weightControlArmed=RunSupervisor::weightTargetArmed()?1:0;h.targetWeightCentiGrams=RunSupervisor::weightTargetArmed()?int32_t(lroundf(RunSupervisor::weightTargetGrams()*100.0f)):0;memcpy(h.tareOffset,tare.offset,sizeof(h.tareOffset));memcpy(h.tareSpread,tare.spread,sizeof(h.tareSpread));memcpy(h.tareProfile,profile.raw,sizeof(h.tareProfile));memcpy(h.tareProfileSamples,profile.samples,sizeof(h.tareProfileSamples));h.crc=crc(reinterpret_cast<uint8_t*>(ram),count*sizeof(Sample));if(f){f.write(reinterpret_cast<const uint8_t*>(&h),sizeof(h));f.write(reinterpret_cast<const uint8_t*>(ram),count*sizeof(Sample));f.close();}}
 if constexpr(Config::EnableBluetoothLiveTelemetry)DiagnosticsTransport::endTelemetryStream(end==RunEnd::Complete?"complete":"abort",millis()-started,totalCount,count);
 Serial.print("Telemetry saved: ");Serial.print(count);Serial.print(" samples (");Serial.print(end==RunEnd::Complete?"complete":"abort");Serial.println(')');count=0;
}
void Telemetry::event(const char* label){Serial.print("EVENT,");Serial.print(millis()-started);Serial.print(',');Serial.println(label);DiagnosticsTransport::streamEvent(millis()-started,label);}
void Telemetry::executeCommand(const char* command,Print& output,bool logAccessAllowed){
 String c(command);c.trim();c.toUpperCase();
 if(c=="LOG HELP"){output.println("LOG DUMP | LOG CLEAR | LOG HELP | LOAD CELLS | MARK <label> | WEIGHT TARGET <grams> | WEIGHT OFF | CONTROL STATUS");return;}
 if(c=="CONTROL STATUS"){
  const RunSupervisorSnapshot control=RunSupervisor::snapshot();
  output.print("CONTROL,stall,");output.print(RunSupervisor::stallStateName(control.stallState));output.print(",sg,");output.print(control.sgResult);output.print(",sg_baseline,");output.print(control.sgBaseline);
  output.print(",weight_armed,");output.print(RunSupervisor::weightTargetArmed()?1:0);output.print(",weight_state,");output.print(RunSupervisor::weightStateName(control.weightState));output.print(",target_g,");output.print(RunSupervisor::weightTargetGrams(),2);output.print(",filtered_g,");output.print(control.filteredWeightGrams,2);output.print(",spread_g,");output.println(control.weightSpreadGrams,2);return;
 }
 if(c=="WEIGHT OFF"){
  if(!logAccessAllowed){output.println("BUSY: change weight target while idle");return;}
  RunSupervisor::disarmWeightTarget();output.println("WEIGHT_CONTROL,off");return;
 }
 if(c.startsWith("WEIGHT TARGET")){
  if(!logAccessAllowed){output.println("BUSY: change weight target while idle");return;}
  if(!weightCapabilityEnabled){output.println("ERROR: weight capability unavailable");return;}
  String value=String(command).substring(strlen("WEIGHT TARGET"));value.trim();
  const float grams=value.toFloat();
  if(!value.length()||!RunSupervisor::armWeightTarget(grams)){output.print("ERROR: target must be ");output.print(Config::MinimumWeightTargetGrams,0);output.print('-');output.print(Config::MaximumWeightTargetGrams,0);output.println(" grams");return;}
  output.print("WEIGHT_CONTROL,target_g,");output.print(grams,2);output.println(",selected turns remain safety ceiling");return;
 }
 if(c=="LOAD CELLS"){
  const LoadCellSnapshot loadCells=LoadCells::snapshot();
  output.print("LOAD_CELLS,required,");output.print(Config::RequiredLoadCells);output.print(",detected,");output.print(detectedLoadCellCount);output.print(",product_mode,");output.println(Product::name());
  output.println("channel,raw,healthy");
  const LoadCellTareResult tare=LoadCells::tareResult();
  for(uint8_t channel=0;channel<Config::LoadCellChannelCount;channel++){output.print(channel+1);output.print(',');output.print(loadCells.raw[channel]);output.print(',');output.print((loadCells.healthyMask&(1u<<channel))?1:0);output.print(",tare,");output.print(tare.offset[channel]);output.print(",spread,");output.println(tare.spread[channel]);}
  output.print("weight_g,");output.print(LoadCells::profileWeightGrams(loadCells,safeCurrentStepCount()),2);output.print(",tare_status,");output.print(LoadCells::tareStatusName(tare.status));output.print(",profile_tare,");output.println(LoadCells::profileTareValid()?1:0);return;
 }
 if(c=="MARK"||c.startsWith("MARK ")){
  if(logAccessAllowed){output.println("IDLE: MARK is available while winding");return;}
  String label=String(command).substring(4);label.trim();if(!label.length()){output.println("MARK requires a label");return;}
  char clean[33]{};size_t n=0;for(size_t i=0;i<label.length()&&n<32;i++){char ch=label[i];if(ch==','||ch=='\r'||ch=='\n')ch='_';if(isPrintable(ch))clean[n++]=ch;}
  DiagnosticsTransport::streamEvent(millis()-started,clean);output.print("MARKED,");output.println(clean);return;
 }
 if(c!="LOG DUMP"&&c!="LOG CLEAR")return;
 if(!logAccessAllowed){output.println("BUSY: log access available while idle");return;}
 if(c=="LOG DUMP")dump(output);else clear(output);
}
