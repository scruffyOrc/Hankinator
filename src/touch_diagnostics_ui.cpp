#include "touch_diagnostics_ui.h"

#if defined(HANKWINDER_TOUCH_DIAGNOSTICS)

#include <Arduino.h>
#include <SPI.h>
#include <EEPROM.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <lvgl.h>

#include "machine_interface.h"
#include "app_api.h"
#include "buzzytoes_logo.h"
#include "playwrite_splash_title.h"
#include "runtime.h"

namespace {
constexpr uint8_t TouchCs=13;
constexpr uint8_t TouchIrq=11;
constexpr int DisplayWidth=480;
constexpr int DisplayHeight=320;
constexpr int DrawBufferRows=40;
constexpr uint8_t CalibrationPointCount=5;
constexpr uint8_t TouchFilterSize=5;
constexpr int TouchPressPressure=350;
constexpr int TouchReleasePressure=180;
constexpr size_t TouchCalibrationOffset=512;
constexpr uint32_t TouchCalibrationMagic=0x48575450; // HWTP
constexpr uint16_t TouchCalibrationVersion=1;
constexpr uint32_t BacklightWakeDelayMs=120;

constexpr uint32_t Background=0xF3F5F9;
constexpr uint32_t Header=0x292D43;
constexpr uint32_t Ink=0x20243A;
constexpr uint32_t Muted=0x73788D;
constexpr uint32_t Diagnostics=0xA96508;
constexpr uint32_t DiagnosticsLight=0xC17A13;
constexpr uint32_t DiagnosticsDeep=0x8B5000;
constexpr uint32_t WeightLight=0x21A89B;
constexpr uint32_t Weight=0x11998E;
constexpr uint32_t WeightDeep=0x087A74;

uint32_t uiBackground(){return settings.touchDisplayDarkMode?0x090B12:Background;}
uint32_t uiInk(){return settings.touchDisplayDarkMode?0xF3F5F9:Ink;}
uint32_t uiMuted(){return settings.touchDisplayDarkMode?0xA8B0C4:Muted;}

struct Point { float x,y; };
struct CalibrationRecord {
    uint32_t magic;
    uint16_t version;
    uint16_t reserved;
    float ax,bx,cx,ay,by,cy;
    uint32_t checksum;
};
static_assert(TouchCalibrationOffset+sizeof(CalibrationRecord)<=Config::EepromSize,"Touch calibration must stay inside the reserved EEPROM region");
constexpr Point calibrationTargets[CalibrationPointCount]={{35,55},{445,55},{445,285},{35,285},{240,170}};
struct TouchSample { int16_t x,y; };

TFT_eSPI lcd;
XPT2046_Touchscreen touch(TouchCs,TouchIrq);
lv_color_t drawBuffer[DisplayWidth*DrawBufferRows];
lv_disp_draw_buf_t lvDrawBuffer;
lv_disp_drv_t lvDisplay;
lv_indev_drv_t lvTouch;

Point rawCalibration[CalibrationPointCount];
float ax=0,bx=0,cx=0,ay=0,by=0,cy=0;
uint8_t calibrationIndex=0,calibrationSamples=0;
long calibrationX=0,calibrationY=0;
int calibrationMinX=4095,calibrationMaxX=0,calibrationMinY=4095,calibrationMaxY=0;
bool calibrating=true,wasDown=false,lvglReady=false,touchPressed=false;
bool displayInitialized=false;
bool returnToDisplaySettingsAfterCalibration=false;
bool touchDisplaySleeping=false,consumeWakeTouch=false;
uint32_t releasedAt=0,lastTouchSample=0,lastFilteredTouchSample=0,lastLvTick=0,lastTouchActivity=0;
TouchSample touchSamples[TouchFilterSize]{};
uint8_t touchSampleCount=0,touchSampleNext=0,touchPressSamples=0;
TouchSample filteredTouch{};
lv_timer_t* liveTimer=nullptr;
lv_obj_t* rawLabels[Config::LoadCellChannelCount]{};
lv_obj_t* channelStatusLabels[Config::LoadCellChannelCount]{};
lv_obj_t* loadCellSummary=nullptr;
lv_obj_t* weightValue=nullptr;
lv_obj_t* weightStatus=nullptr;
lv_obj_t* presetValueLabel=nullptr;
lv_obj_t* presetKeypadValueLabel=nullptr;
lv_obj_t* commissioningPrimary=nullptr;
lv_obj_t* commissioningDetail=nullptr;
lv_obj_t* commissioningProgress=nullptr;
lv_obj_t* windingGauge=nullptr;
lv_obj_t* windingPercent=nullptr;
lv_obj_t* windingAction=nullptr;
lv_obj_t* bluetoothStateLabel=nullptr;
lv_obj_t* pairingButton=nullptr;
lv_obj_t* pairingButtonLabel=nullptr;
lv_obj_t* pairingStateLabel=nullptr;
lv_obj_t* pairingPromptLabel=nullptr;
lv_obj_t* displaySleepMinutesLabel=nullptr;
lv_obj_t* displaySleepSlider=nullptr;
lv_obj_t* systemCheckPrimary=nullptr;
lv_obj_t* systemCheckDetail=nullptr;
lv_obj_t* turnCountLabel=nullptr;
lv_obj_t* turnSlider=nullptr;
bool pendingWindingWorkflowRefresh=false;
bool pendingHomeAfterPhysicalStop=false;
bool pendingEasterEggAdvance=false;
bool pendingEasterEggLogoDraw=false;
Machine::CommissioningState displayedCommissioningState=Machine::CommissioningState::Ready;
Machine::WindingStage displayedWindingStage=Machine::WindingStage::Idle;
uint8_t displayedCommissioningSamples=0;
uint8_t editingWeightPreset=0;
bool editingRunCurrent=true;
uint16_t touchTurns=Config::DefaultAutoTurns;
uint32_t weightHoldStarted=0,weightLastHoldAdjustment=0;
uint32_t easterEggLastTap=0;
uint8_t easterEggTapCount=0;
uint8_t easterEggPhase=0,easterEggFlashFrame=0;
lv_obj_t* easterEggCaption=nullptr;
char presetKeypadValue[12]{};

uint32_t calibrationChecksum(const CalibrationRecord& record)
{
    uint32_t checksum=2166136261UL;
    const uint8_t* bytes=reinterpret_cast<const uint8_t*>(&record);
    for(size_t i=0;i<offsetof(CalibrationRecord,checksum);++i)checksum=(checksum^bytes[i])*16777619UL;
    return checksum;
}

bool loadCalibration()
{
    CalibrationRecord record{};EEPROM.get(TouchCalibrationOffset,record);
    if(record.magic!=TouchCalibrationMagic||record.version!=TouchCalibrationVersion||record.checksum!=calibrationChecksum(record))return false;
    if(!isfinite(record.ax)||!isfinite(record.bx)||!isfinite(record.cx)||!isfinite(record.ay)||!isfinite(record.by)||!isfinite(record.cy))return false;
    ax=record.ax;bx=record.bx;cx=record.cx;ay=record.ay;by=record.by;cy=record.cy;return true;
}

bool saveCalibration()
{
    CalibrationRecord record{TouchCalibrationMagic,TouchCalibrationVersion,0,ax,bx,cx,ay,by,cy,0};
    record.checksum=calibrationChecksum(record);EEPROM.put(TouchCalibrationOffset,record);return EEPROM.commit();
}

void textAt(int x,int y,const char* text,int size=2)
{
    lcd.setTextColor(TFT_WHITE,TFT_DARKGREY);lcd.setTextSize(size);lcd.setCursor(x,y);lcd.print(text);
}

void drawPlaywriteSplashTitle()
{
    const int originX=(DisplayWidth-PlaywriteSplashTitle::Width)/2;
    constexpr int originY=18;
    for(int y=0;y<PlaywriteSplashTitle::Height;++y)
    {
        for(int x=0;x<PlaywriteSplashTitle::Width;++x)
        {
            const size_t byteOffset=static_cast<size_t>(y)*PlaywriteSplashTitle::RowBytes+(x/8);
            const uint8_t bits=pgm_read_byte(&PlaywriteSplashTitle::Bits[byteOffset]);
            if(bits&(1U<<(x&7)))lcd.drawPixel(originX+x,originY+y,TFT_WHITE);
        }
    }
}

void initialiseDisplay()
{
    if(displayInitialized)return;
    // GP1 is harmless while the temporary MOSFET module is absent. With the
    // module fitted, the P-channel stage is active-low and this turns the
    // touch-panel LED rail on only after the display is being initialized.
    pinMode(Pins::TouchBacklight,OUTPUT);
    digitalWrite(Pins::TouchBacklight,LOW);
    lcd.init();lcd.setRotation(1);lcd.setTextWrap(false);
    displayInitialized=true;lastTouchActivity=millis();
}

void backlightOn(){digitalWrite(Pins::TouchBacklight,LOW);}
void backlightOff(){digitalWrite(Pins::TouchBacklight,HIGH);}

void wakeTouchDisplay()
{
    if(!touchDisplaySleeping){lastTouchActivity=millis();return;}
    lcd.writecommand(TFT_SLPOUT);delay(BacklightWakeDelayMs);lcd.writecommand(TFT_DISPON);backlightOn();
    touchDisplaySleeping=false;lastTouchActivity=millis();
    if(lvglReady)lv_obj_invalidate(lv_scr_act());
}

bool displayMaySleep()
{
    const Machine::WindingStage stage=Machine::winding().stage;
    return stage==Machine::WindingStage::Idle||stage==Machine::WindingStage::Complete
        ||stage==Machine::WindingStage::TareFailed||stage==Machine::WindingStage::HallFailed
        ||stage==Machine::WindingStage::Fault;
}

void serviceTouchDisplaySleep(const uint32_t now)
{
    if(!settings.touchDisplaySleepEnabled||!displayMaySleep())
    {
        if(touchDisplaySleeping)wakeTouchDisplay();
        else lastTouchActivity=now;
        return;
    }
    const uint32_t idleLimit=uint32_t(settings.touchDisplaySleepMinutes)*60000UL;
    if(!touchDisplaySleeping&&now-lastTouchActivity>=idleLimit)
    {
        lcd.writecommand(TFT_DISPOFF);lcd.writecommand(TFT_SLPIN);backlightOff();touchDisplaySleeping=true;
    }
}

void drawWinderMark(const uint16_t color,const uint16_t sweepDegrees=0,const bool drawSweep=true)
{
    constexpr int hubX=244,hubY=132;
    constexpr uint16_t frame=TFT_LIGHTGREY;
    // This is deliberately a side/profile-ish machine mark based on the real
    // HankWinder: a rimmed hub, four arms with peg caps, tripod legs, and the
    // offset belt/motor.  Those machine-specific asymmetries are what stop it
    // reading as a generic target or spinner.
    lcd.drawCircle(hubX,hubY,49,frame);
    lcd.drawCircle(hubX,hubY,46,frame);

    constexpr int arms[][2]={{-55,-55},{55,-55},{55,55},{-55,55}};
    for(uint8_t arm=0;arm<4;++arm)
    {
        const int endX=hubX+arms[arm][0],endY=hubY+arms[arm][1];
        lcd.drawLine(hubX,hubY,endX,endY,color);
        lcd.drawLine(hubX+2,hubY,endX+2,endY,color);
        // The distinct end caps read as Hank's yarn pegs rather than spokes.
        lcd.fillCircle(endX,endY,8,color);
        lcd.fillCircle(endX,endY,4,TFT_DARKGREY);
    }
    lcd.fillCircle(hubX,hubY,17,color);
    lcd.fillCircle(hubX,hubY,7,TFT_DARKGREY);

    // Tripod/frame below the hub.
    lcd.drawLine(hubX-20,hubY+38,158,238,frame);
    lcd.drawLine(hubX-17,hubY+38,161,238,frame);
    lcd.drawLine(hubX+22,hubY+38,326,238,frame);
    lcd.drawLine(hubX+19,hubY+38,323,238,frame);
    lcd.drawFastHLine(145,239,31,frame);lcd.drawFastHLine(311,239,31,frame);

    // Off-centre motor and belt/pulley, copied from the actual drive side.
    lcd.drawCircle(hubX-60,hubY+53,13,color);
    lcd.drawLine(hubX-48,hubY+45,hubX-30,hubY+30,color);
    lcd.drawLine(hubX-51,hubY+61,hubX-28,hubY+49,color);
    lcd.fillRoundRect(hubX-96,hubY+51,30,22,5,color);
    lcd.fillCircle(hubX-60,hubY+53,5,TFT_DARKGREY);

    if(drawSweep)lcd.drawArc(hubX,hubY,58,50,(278+sweepDegrees)%360,(340+sweepDegrees)%360,TFT_YELLOW,TFT_DARKGREY,true);
}

void redrawBootWinderSweep(const uint16_t previousDegrees,const uint16_t nextDegrees)
{
    constexpr int hubX=244,hubY=132;
    // Erase only the old sweep, then restore the small parts of the fixed
    // mark it crossed before painting the next sweep. No rectangular clear
    // means the fixed Hank illustration remains visibly stable.
    lcd.drawArc(hubX,hubY,58,50,(278+previousDegrees)%360,(340+previousDegrees)%360,TFT_DARKGREY,TFT_DARKGREY,true);
    drawWinderMark(TFT_CYAN,0,false);
    lcd.drawArc(hubX,hubY,58,50,(278+nextDegrees)%360,(340+nextDegrees)%360,TFT_YELLOW,TFT_DARKGREY,true);
}

void drawBootStatus(const char* text,const uint16_t color)
{
    // Keep the status strip below the tripod feet rather than erasing the
    // bottom of the machine mark during the later status changes.
    lcd.fillRect(0,260,DisplayWidth,40,TFT_DARKGREY);
    lcd.setTextDatum(MC_DATUM);lcd.setTextColor(color,TFT_DARKGREY);lcd.setTextSize(2);
    lcd.drawString(text,DisplayWidth/2,280);
    lcd.setTextDatum(TL_DATUM);
}

void clearTouchFilter();

// Pico's size-optimized printf does not include floating-point conversion.
// Keep user-facing mass strings deterministic and limited to ASCII glyphs the
// installed Montserrat subset always contains.
void formatGrams(char* output,size_t size,float grams,uint8_t decimals=2)
{
    const long scale=decimals==1?10:100;
    const long scaled=lroundf(grams*scale);
    const unsigned long magnitude=scaled<0?static_cast<unsigned long>(-scaled):static_cast<unsigned long>(scaled);
    if(decimals==1)snprintf(output,size,"%s%lu.%01lu",scaled<0?"-":"",magnitude/10,magnitude%10);
    else snprintf(output,size,"%s%lu.%02lu",scaled<0?"-":"",magnitude/100,magnitude%100);
}

void drawCalibration()
{
    lcd.fillScreen(TFT_DARKGREY);
    textAt(28,24,"Touch calibration",2);
    char progress[24];snprintf(progress,sizeof(progress),"Point %u of %u",unsigned(calibrationIndex+1),unsigned(CalibrationPointCount));
    textAt(28,51,progress,1);textAt(28,66,"Hold steadily on marker, then lift.",1);
    const int x=int(calibrationTargets[calibrationIndex].x),y=int(calibrationTargets[calibrationIndex].y);
    lcd.drawCircle(x,y,15,TFT_YELLOW);lcd.drawFastHLine(x-24,y,49,TFT_YELLOW);lcd.drawFastVLine(x,y-24,49,TFT_YELLOW);
}

void beginTouchCalibration()
{
    clearTouchFilter();
    calibrating=true;
    calibrationIndex=calibrationSamples=0;
    calibrationX=calibrationY=0;
    calibrationMinX=calibrationMinY=4095;
    calibrationMaxX=calibrationMaxY=0;
    releasedAt=millis();
    drawCalibration();
}

bool solveCalibrationAxis(bool horizontal,float& first,float& second,float& constant)
{
    float matrix[3][4]{};
    for(uint8_t i=0;i<CalibrationPointCount;++i) {
        const float rawX=rawCalibration[i].x,rawY=rawCalibration[i].y,target=horizontal?calibrationTargets[i].x:calibrationTargets[i].y;
        matrix[0][0]+=rawX*rawX;matrix[0][1]+=rawX*rawY;matrix[0][2]+=rawX;matrix[0][3]+=rawX*target;
        matrix[1][0]+=rawX*rawY;matrix[1][1]+=rawY*rawY;matrix[1][2]+=rawY;matrix[1][3]+=rawY*target;
        matrix[2][0]+=rawX;matrix[2][1]+=rawY;matrix[2][2]+=1;matrix[2][3]+=target;
    }
    for(uint8_t column=0;column<3;++column) {
        uint8_t pivot=column;for(uint8_t row=column+1;row<3;++row)if(fabsf(matrix[row][column])>fabsf(matrix[pivot][column]))pivot=row;
        if(fabsf(matrix[pivot][column])<0.001f)return false;
        if(pivot!=column)for(uint8_t entry=column;entry<4;++entry){const float hold=matrix[column][entry];matrix[column][entry]=matrix[pivot][entry];matrix[pivot][entry]=hold;}
        const float divisor=matrix[column][column];for(uint8_t entry=column;entry<4;++entry)matrix[column][entry]/=divisor;
        for(uint8_t row=0;row<3;++row)if(row!=column){const float factor=matrix[row][column];for(uint8_t entry=column;entry<4;++entry)matrix[row][entry]-=factor*matrix[column][entry];}
    }
    first=matrix[0][3];second=matrix[1][3];constant=matrix[2][3];return true;
}

bool solveCalibration(){return solveCalibrationAxis(true,ax,bx,cx)&&solveCalibrationAxis(false,ay,by,cy);}

void flushDisplay(lv_disp_drv_t*,const lv_area_t* area,lv_color_t* pixels)
{
    const uint32_t width=area->x2-area->x1+1,height=area->y2-area->y1+1;
    lcd.startWrite();lcd.setAddrWindow(area->x1,area->y1,width,height);lcd.pushColors(reinterpret_cast<uint16_t*>(&pixels->full),width*height,true);lcd.endWrite();lv_disp_flush_ready(&lvDisplay);
}

int16_t filteredCoordinate(bool horizontal)
{
    int16_t values[TouchFilterSize];for(uint8_t i=0;i<touchSampleCount;++i)values[i]=horizontal?touchSamples[i].x:touchSamples[i].y;
    for(uint8_t i=1;i<touchSampleCount;++i){const int16_t value=values[i];uint8_t position=i;while(position>0&&values[position-1]>value){values[position]=values[position-1];--position;}values[position]=value;}
    return values[touchSampleCount/2];
}

void clearTouchFilter(){touchSampleCount=touchSampleNext=touchPressSamples=0;touchPressed=false;}

void readTouch(lv_indev_drv_t*,lv_indev_data_t* data)
{
    const uint32_t now=millis();
    if(now-lastFilteredTouchSample>=4) {
        lastFilteredTouchSample=now;const TS_Point point=touch.getPoint();
        const bool valid=point.x>0&&point.x<4095&&point.y>0&&point.y<4095;
        if(valid&&point.z>=TouchPressPressure) {
            if(touchDisplaySleeping)
            {
                wakeTouchDisplay();consumeWakeTouch=true;clearTouchFilter();
                data->state=LV_INDEV_STATE_REL;return;
            }
            lastTouchActivity=now;
            if(consumeWakeTouch){data->state=LV_INDEV_STATE_REL;return;}
            touchSamples[touchSampleNext++]={int16_t(constrain(lroundf(ax*point.x+bx*point.y+cx),0,DisplayWidth-1)),int16_t(constrain(lroundf(ay*point.x+by*point.y+cy),0,DisplayHeight-1))};
            if(touchSampleNext==TouchFilterSize)touchSampleNext=0;if(touchSampleCount<TouchFilterSize)++touchSampleCount;
            filteredTouch={filteredCoordinate(true),filteredCoordinate(false)};if(!touchPressed&&++touchPressSamples>=2)touchPressed=true;
        } else if((!touchPressed&&(!valid||point.z<TouchPressPressure))||(touchPressed&&(!valid||point.z<=TouchReleasePressure))) {
            clearTouchFilter();consumeWakeTouch=false;
        }
    }
    if(touchPressed) {
        data->point.x=filteredTouch.x;
        data->point.y=filteredTouch.y;
        data->state=LV_INDEV_STATE_PR;
    } else data->state=LV_INDEV_STATE_REL;
}

lv_obj_t* label(lv_obj_t* parent,const char* text,const lv_font_t* font,lv_color_t color,lv_align_t align,int x,int y)
{
    lv_obj_t* item=lv_label_create(parent);lv_label_set_text(item,text);lv_obj_set_style_text_font(item,font,0);lv_obj_set_style_text_color(item,color,0);lv_obj_align(item,align,x,y);return item;
}

void styleCard(lv_obj_t* card,uint32_t color)
{
    lv_obj_set_style_radius(card,16,0);lv_obj_set_style_bg_color(card,lv_color_hex(color),0);lv_obj_set_style_border_width(card,0,0);lv_obj_set_style_shadow_width(card,10,0);lv_obj_set_style_shadow_opa(card,LV_OPA_30,0);
}

void clearScreen(const char* title,bool hasBack=false)
{
    if(liveTimer){lv_timer_del(liveTimer);liveTimer=nullptr;}
    lv_obj_t* root=lv_scr_act();lv_obj_clean(root);lv_obj_set_style_bg_color(root,lv_color_hex(uiBackground()),0);lv_obj_set_style_bg_opa(root,LV_OPA_COVER,0);
    lv_obj_t* header=lv_obj_create(root);lv_obj_set_size(header,DisplayWidth,70);lv_obj_set_pos(header,0,0);lv_obj_set_style_bg_color(header,lv_color_hex(Header),0);lv_obj_set_style_border_width(header,0,0);lv_obj_set_style_radius(header,0,0);lv_obj_clear_flag(header,LV_OBJ_FLAG_SCROLLABLE);
    // Reserve the first 80 px for the back target.  The previous title offset
    // started underneath the arrow, so the two glyphs were being drawn atop
    // one another on the live diagnostics pages.
    label(header,title,&lv_font_montserrat_20,lv_color_white(),LV_ALIGN_LEFT_MID,hasBack?88:22,0);
}

void buildHome(lv_event_t* =nullptr);
void buildConfiguration(lv_event_t* =nullptr);
void buildMachineSettings(lv_event_t* =nullptr);
void buildDisplaySettings(lv_event_t* =nullptr);
void buildDisplaySleep(lv_event_t* =nullptr);
void buildMotorSettings(lv_event_t* =nullptr);
void buildMotorCurrentWarning(lv_event_t* =nullptr);
void buildMotorCurrentEditor(lv_event_t* =nullptr);
void buildMotorDirection(lv_event_t* =nullptr);
void buildConnectivity(lv_event_t* =nullptr);
void buildSystemCheck(lv_event_t* =nullptr);
void buildDiagnosticsMenu(lv_event_t* =nullptr);
void buildCommissioning(lv_event_t* =nullptr);
void buildCommissioningOverwriteWarning(lv_event_t* =nullptr);
void buildWeightTargetsMenu(lv_event_t* =nullptr);
void buildWeightTargetEditor(uint8_t preset);
void buildWeightKeypad();
void buildWeightWindingMenu(lv_event_t* =nullptr);
void buildTurnWindingMenu(lv_event_t* =nullptr);
void buildWindingWorkflow(lv_event_t* =nullptr);
void buildWindingStopConfirm(lv_event_t* =nullptr);
void buildEasterEgg();

void queueHomeScreen(lv_event_t*)
{
    buildHome(nullptr);
}

void queueWindingWorkflowRefresh()
{
    // Never destroy/recreate the winding screen from its own LVGL timer.
    // Hall seeking, taring, load-yarn, pause, and completion all advance the
    // controller state asynchronously; service() performs the rebuild after
    // lv_timer_handler() has completely unwound.
    if(pendingWindingWorkflowRefresh)return;
    pendingWindingWorkflowRefresh=true;
}

void addBack(lv_obj_t* root,lv_event_cb_t callback=buildHome)
{
    lv_obj_t* back=lv_btn_create(root);lv_obj_set_size(back,70,38);lv_obj_set_pos(back,10,16);lv_obj_set_style_bg_opa(back,LV_OPA_TRANSP,0);lv_obj_set_style_border_width(back,0,0);lv_obj_t* caption=label(back,LV_SYMBOL_LEFT,&lv_font_montserrat_20,lv_color_white(),LV_ALIGN_CENTER,0,0);(void)caption;lv_obj_add_event_cb(back,callback,LV_EVENT_CLICKED,nullptr);
}

lv_obj_t* card(lv_obj_t* parent,const char* caption,int x,int y,int width,int height,uint32_t color,lv_event_cb_t callback=nullptr)
{
    lv_obj_t* item=lv_btn_create(parent);lv_obj_set_size(item,width,height);lv_obj_set_pos(item,x,y);styleCard(item,color);
    lv_obj_t* text=label(item,caption,&lv_font_montserrat_20,lv_color_white(),LV_ALIGN_CENTER,0,0);lv_obj_set_style_text_align(text,LV_TEXT_ALIGN_CENTER,0);
    if(callback)lv_obj_add_event_cb(item,callback,LV_EVENT_CLICKED,nullptr);else lv_obj_clear_flag(item,LV_OBJ_FLAG_CLICKABLE);
    return item;
}

void warningIcon(lv_obj_t* parent,const int y)
{
    label(parent,LV_SYMBOL_WARNING,&lv_font_montserrat_36,lv_color_hex(0xB61E28),LV_ALIGN_TOP_MID,0,y);
}

void easterEggTimer(lv_timer_t*)
{
    if(easterEggPhase==0)
    {
        // Keep this in LVGL rather than drawing directly to the panel.  Direct
        // TFT writes can be immediately erased by LVGL's next refresh pass.
        static constexpr uint32_t colors[]={0xE63946,0xFF9F1C,0x2FAF5A,0x2176D2,0x8E44AD,0xE83E8C};
        if(easterEggCaption)lv_obj_set_style_text_color(easterEggCaption,lv_color_hex(colors[easterEggFlashFrame%6]),0);
        if(++easterEggFlashFrame>=12)
        {
            easterEggPhase=1;
            pendingEasterEggAdvance=true;
            if(liveTimer)lv_timer_pause(liveTimer);
        }
        return;
    }

    ++easterEggPhase;
    pendingEasterEggAdvance=true;
    if(liveTimer)lv_timer_pause(liveTimer);
}

void versionEasterEggTap(lv_event_t*)
{
    constexpr uint32_t TapWindowMs=1800;
    const uint32_t now=millis();
    if(now-easterEggLastTap>TapWindowMs)easterEggTapCount=0;
    easterEggLastTap=now;
    if(++easterEggTapCount<5)return;

    easterEggTapCount=0;
    easterEggPhase=0;
    easterEggFlashFrame=0;
    buildEasterEgg();
}

void clearEasterEggScreen(const uint16_t background)
{
    // Unlike ordinary app screens, the Easter egg intentionally has no
    // header. Force LVGL's empty root to the panel first, then draw its
    // static artwork directly so no layout chrome survives underneath it.
    if(liveTimer){lv_timer_del(liveTimer);liveTimer=nullptr;}
    lv_obj_t* root=lv_scr_act();lv_obj_clean(root);
    lv_obj_set_style_bg_color(root,lv_color_make((background>>11)<<3,((background>>5)&0x3F)<<2,(background&0x1F)<<3),0);
    lv_obj_set_style_bg_opa(root,LV_OPA_COVER,0);
    easterEggCaption=nullptr;
}

void drawBuzzyToesLogo()
{
    const uint16_t background=lcd.color565(0xF3,0xF5,0xF9);
    lcd.drawXBitmap(0,0,BuzzyToesLogo::Bits,BuzzyToesLogo::Width,BuzzyToesLogo::Height,TFT_BLACK,background);
    // The supplied mark has no copyright note; add it in the requested lower
    // right position without relying on a potentially missing copyright glyph.
    // Keep the legal mark inside the panel's safe edge, including on modules
    // whose visible area loses a few pixels to the bezel.
    lcd.drawCircle(390,282,6,TFT_BLACK);lcd.setTextDatum(MC_DATUM);lcd.setTextColor(TFT_BLACK,background);lcd.setTextFont(2);lcd.drawCentreString("C",390,282,2);
    lcd.setTextDatum(TL_DATUM);lcd.drawString("2026",400,276,2);
}

void buildEasterEgg()
{
    if(easterEggPhase>=3){buildHome(nullptr);return;}
    if(easterEggPhase==0)
    {
        clearEasterEggScreen(TFT_DARKGREY);
        easterEggCaption=label(lv_scr_act(),"BEHOLD!!!",&lv_font_montserrat_36,lv_color_hex(0xE63946),LV_ALIGN_CENTER,0,0);
        liveTimer=lv_timer_create(easterEggTimer,250,nullptr);
    }
    else if(easterEggPhase==1)
    {
        clearEasterEggScreen(TFT_DARKGREY);
        label(lv_scr_act(),"Hank",&lv_font_montserrat_36,lv_color_hex(0x22D3EE),LV_ALIGN_CENTER,0,-32);
        label(lv_scr_act(),"Manufactured by",&lv_font_montserrat_28,lv_color_hex(0xD1D5DB),LV_ALIGN_CENTER,0,34);
        liveTimer=lv_timer_create(easterEggTimer,3000,nullptr);
    }
    else
    {
        clearEasterEggScreen(lcd.color565(0xF3,0xF5,0xF9));
        // Draw after the next LVGL refresh has consumed the newly-cleaned
        // root.  Otherwise the framework overwrites the logo a frame later.
        pendingEasterEggLogoDraw=true;
        liveTimer=lv_timer_create(easterEggTimer,6000,nullptr);
    }
}

const char* weightPresetName(const uint8_t preset)
{
    static constexpr const char* names[]={"Mini target","Half target","Full target"};
    return names[preset<3?preset:0];
}

void refreshWeightTargetValue()
{
    if(!presetValueLabel)return;
    char value[20]{};
    formatGrams(value,sizeof(value),Machine::weightTargets().grams[editingWeightPreset],1);
    strncat(value," g",sizeof(value)-strlen(value)-1);
    lv_label_set_text(presetValueLabel,value);
}

void adjustWeightTarget(const int tenths)
{
    const float current=Machine::weightTargets().grams[editingWeightPreset];
    Machine::setWeightTargetGrams(editingWeightPreset,current+tenths/10.0f);
    refreshWeightTargetValue();
}

void resetWeightTarget(lv_event_t*)
{
    Machine::resetWeightTargetToDefault(editingWeightPreset);
    refreshWeightTargetValue();
}

void adjustWeightTargetWhileHeld(lv_event_t* event)
{
    const uint32_t now=millis();
    const int direction=intptr_t(lv_obj_get_user_data(lv_event_get_target(event)));
    switch(lv_event_get_code(event))
    {
        case LV_EVENT_PRESSED:
            weightHoldStarted=weightLastHoldAdjustment=now;
            adjustWeightTarget(direction);
            break;
        case LV_EVENT_PRESSING:
            while(now-weightLastHoldAdjustment>=1000)
            {
                weightLastHoldAdjustment+=1000;
                adjustWeightTarget(direction*(now-weightHoldStarted>=3000?10:1));
            }
            break;
        case LV_EVENT_RELEASED:
        case LV_EVENT_PRESS_LOST:
            weightHoldStarted=weightLastHoldAdjustment=0;
            break;
        default:break;
    }
}

void refreshWeightKeypadValue()
{
    if(presetKeypadValueLabel)lv_label_set_text(presetKeypadValueLabel,presetKeypadValue[0]?presetKeypadValue:"0");
}

void keypadWeightKey(lv_event_t* event)
{
    const char key=char(intptr_t(lv_obj_get_user_data(lv_event_get_target(event))));
    const size_t length=strlen(presetKeypadValue);
    if(key=='.')
    {
        if(!strchr(presetKeypadValue,'.')&&length<sizeof(presetKeypadValue)-1){presetKeypadValue[length]='.';presetKeypadValue[length+1]='\0';}
    }
    else if(key>='0'&&key<='9'&&length<sizeof(presetKeypadValue)-1){presetKeypadValue[length]=key;presetKeypadValue[length+1]='\0';}
    refreshWeightKeypadValue();
}

void clearWeightKeypad(lv_event_t*)
{
    presetKeypadValue[0]='\0';
    refreshWeightKeypadValue();
}

void backspaceWeightKeypad(lv_event_t*)
{
    const size_t length=strlen(presetKeypadValue);
    if(length)presetKeypadValue[length-1]='\0';
    refreshWeightKeypadValue();
}

void acceptWeightKeypad(lv_event_t*)
{
    const float grams=strtof(presetKeypadValue,nullptr);
    Machine::setWeightTargetGrams(editingWeightPreset,grams);
    buildWeightTargetEditor(editingWeightPreset);
}

void addWeightKeypadButton(lv_obj_t* parent,const char* caption,const char key,int x,int y)
{
    lv_obj_t* button=card(parent,caption,x,y,58,44,0x4856C8,keypadWeightKey);
    lv_obj_set_user_data(button,reinterpret_cast<void*>(intptr_t(key)));
}

void buildWeightKeypad()
{
    clearScreen(weightPresetName(editingWeightPreset),true);lv_obj_t* root=lv_scr_act();
    addBack(root,[](lv_event_t*) {buildWeightTargetEditor(editingWeightPreset);});
    label(root,"Enter target",&lv_font_montserrat_16,lv_color_hex(uiMuted()),LV_ALIGN_TOP_LEFT,22,94);
    presetKeypadValueLabel=label(root,"0",&lv_font_montserrat_36,lv_color_hex(uiInk()),LV_ALIGN_TOP_LEFT,22,124);
    card(root,"CLEAR",22,214,86,48,Muted,clearWeightKeypad);
    card(root,"OK",118,214,86,48,0x2FAF5A,acceptWeightKeypad);
    constexpr int keyX=266,keyY=86,keyGap=64;
    addWeightKeypadButton(root,"1",'1',keyX,keyY);addWeightKeypadButton(root,"2",'2',keyX+keyGap,keyY);addWeightKeypadButton(root,"3",'3',keyX+2*keyGap,keyY);
    addWeightKeypadButton(root,"4",'4',keyX,keyY+52);addWeightKeypadButton(root,"5",'5',keyX+keyGap,keyY+52);addWeightKeypadButton(root,"6",'6',keyX+2*keyGap,keyY+52);
    addWeightKeypadButton(root,"7",'7',keyX,keyY+104);addWeightKeypadButton(root,"8",'8',keyX+keyGap,keyY+104);addWeightKeypadButton(root,"9",'9',keyX+2*keyGap,keyY+104);
    addWeightKeypadButton(root,".",'.',keyX,keyY+156);addWeightKeypadButton(root,"0",'0',keyX+keyGap,keyY+156);
    card(root,LV_SYMBOL_BACKSPACE,keyX+2*keyGap,keyY+156,58,44,0x4856C8,backspaceWeightKeypad);
}

void openWeightKeypad(lv_event_t*)
{
    presetKeypadValue[0]='\0';
    buildWeightKeypad();
}

void buildWeightTargetEditor(const uint8_t preset)
{
    editingWeightPreset=preset;
    clearScreen(weightPresetName(preset),true);lv_obj_t* root=lv_scr_act();
    addBack(root,[](lv_event_t*) {Machine::saveWeightTargets();buildWeightTargetsMenu(nullptr);});
    presetValueLabel=label(root,"",&lv_font_montserrat_36,lv_color_hex(uiInk()),LV_ALIGN_TOP_MID,0,112);
    lv_obj_add_flag(presetValueLabel,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(presetValueLabel,openWeightKeypad,LV_EVENT_CLICKED,nullptr);
    lv_obj_t* minus=card(root,"-",44,196,112,72,0x4856C8);lv_obj_add_flag(minus,LV_OBJ_FLAG_CLICKABLE);lv_obj_set_user_data(minus,reinterpret_cast<void*>(intptr_t(-1)));lv_obj_add_event_cb(minus,adjustWeightTargetWhileHeld,LV_EVENT_ALL,nullptr);
    card(root,"RESET",184,196,112,72,Muted,resetWeightTarget);
    lv_obj_t* plus=card(root,"+",324,196,112,72,0x4856C8);lv_obj_add_flag(plus,LV_OBJ_FLAG_CLICKABLE);lv_obj_set_user_data(plus,reinterpret_cast<void*>(intptr_t(1)));lv_obj_add_event_cb(plus,adjustWeightTargetWhileHeld,LV_EVENT_ALL,nullptr);
    label(root,"Tap the value to enter it directly.",&lv_font_montserrat_14,lv_color_hex(uiMuted()),LV_ALIGN_TOP_MID,0,284);
    refreshWeightTargetValue();
}

void showMiniWeightTarget(lv_event_t*) {buildWeightTargetEditor(0);}
void showHalfWeightTarget(lv_event_t*) {buildWeightTargetEditor(1);}
void showFullWeightTarget(lv_event_t*) {buildWeightTargetEditor(2);}

void buildWeightTargetsMenu(lv_event_t*)
{
    clearScreen("Weight targets",true);lv_obj_t* root=lv_scr_act();addBack(root,buildConfiguration);
    card(root,"MINI",20,112,136,136,WeightLight,showMiniWeightTarget);
    card(root,"HALF",172,112,136,136,Weight,showHalfWeightTarget);
    card(root,"FULL",324,112,136,136,WeightDeep,showFullWeightTarget);
}

void updateLoadCells(lv_timer_t*)
{
    const Machine::DiagnosticsSnapshot& data=Machine::diagnostics();
    for(uint8_t channel=0;channel<Config::LoadCellChannelCount;++channel) {
        char raw[24];snprintf(raw,sizeof(raw),"%ld",long(data.raw[channel]));if(rawLabels[channel])lv_label_set_text(rawLabels[channel],raw);
        if(channelStatusLabels[channel])lv_label_set_text(channelStatusLabels[channel],Machine::channelStatusName(data.channelStatus[channel]));
    }
    if(loadCellSummary) {
        char summary[88];
        if(data.missingReadyMask)snprintf(summary,sizeof(summary),"Waiting for channels: %02X",data.missingReadyMask);
        else if(data.suspiciousMask)snprintf(summary,sizeof(summary),"Raw jump observed: %02X",data.suspiciousMask);
        else snprintf(summary,sizeof(summary),"Live data / %u us sample timing",unsigned(data.sampleDelayUs));
        lv_label_set_text(loadCellSummary,summary);
    }
}

void updateWeight(lv_timer_t*)
{
    const Machine::DiagnosticsSnapshot& data=Machine::diagnostics();
    if(!weightValue||!weightStatus)return;
    char value[32];
    if(data.weightDiagnosticsSettling){lv_label_set_text(weightValue,"Please Tare");lv_label_set_text(weightStatus,"Motor settling...");}
    else if(data.tareStatus==Machine::TareStatus::Collecting){lv_label_set_text(weightValue,"Please Tare");lv_label_set_text(weightStatus,"Taring...");}
    else if(data.tareStatus==Machine::TareStatus::Failed){lv_label_set_text(weightValue,"Please Tare");lv_label_set_text(weightStatus,"Tare failed");}
    // An idle controller has no baseline since this boot. Do not render an
    // invalid numeric placeholder; invite the operator to establish one.
    else if(data.tareStatus==Machine::TareStatus::Idle||data.tareStatus==Machine::TareStatus::Cancelled){lv_label_set_text(weightValue,"Please Tare");lv_label_set_text(weightStatus,"Tap TARE to begin");}
    else if(!data.weightValid){lv_label_set_text(weightValue,"Please Tare");lv_label_set_text(weightStatus,"No valid weight");}
    else {formatGrams(value,sizeof(value),data.weightGrams,1);strncat(value," g",sizeof(value)-strlen(value)-1);lv_label_set_text(weightValue,value);lv_label_set_text(weightStatus,"Live controller reading");}
}

void updateSystemCheck(lv_timer_t*)
{
    const Machine::SystemCheckSnapshot& check=Machine::systemCheck();
    if(systemCheckPrimary){lv_label_set_text(systemCheckPrimary,check.message);lv_obj_set_style_text_color(systemCheckPrimary,lv_color_hex(check.stage==Machine::SystemCheckStage::Passed?0x2FAF5A:(check.stage==Machine::SystemCheckStage::Failed?0xB61E28:uiInk())),0);}
    if(systemCheckDetail)
    {
        char result[96]{};
        snprintf(result,sizeof(result),"Load cells %s  Tare %s\nClockwise %s  Counter %s\nSTOP button %s",check.loadCellsPassed?"OK":"...",check.tarePassed?"OK":"...",check.clockwiseHallPassed?"OK":"...",check.counterClockwiseHallPassed?"OK":"...",check.stopButtonPassed?"OK":"...");
        lv_label_set_text(systemCheckDetail,result);
    }
}

void buildSystemCheck(lv_event_t*)
{
    clearScreen("System check",true);lv_obj_t* root=lv_scr_act();
    addBack(root,[](lv_event_t*) {Machine::cancelSystemCheck();buildDiagnosticsMenu(nullptr);});
    systemCheckPrimary=label(root,"",&lv_font_montserrat_20,lv_color_hex(uiInk()),LV_ALIGN_TOP_MID,0,108);
    systemCheckDetail=label(root,"",&lv_font_montserrat_16,lv_color_hex(uiMuted()),LV_ALIGN_TOP_MID,0,158);
    const Machine::SystemCheckSnapshot& check=Machine::systemCheck();
    if(!check.active&&check.stage!=Machine::SystemCheckStage::Passed&&check.stage!=Machine::SystemCheckStage::Failed)
        card(root,"START CHECK",42,240,396,52,Diagnostics,[](lv_event_t*) {if(Machine::startSystemCheck())updateSystemCheck(nullptr);});
    updateSystemCheck(nullptr);liveTimer=lv_timer_create(updateSystemCheck,100,nullptr);
}

void buildWeightDiagnostics(lv_event_t*)
{
    clearScreen("Weight diagnostics",true);lv_obj_t* root=lv_scr_act();
    addBack(root,[](lv_event_t*) {Machine::closeWeightDiagnostics();buildDiagnosticsMenu(nullptr);});
    weightValue=label(root,"",&lv_font_montserrat_36,lv_color_hex(uiInk()),LV_ALIGN_TOP_MID,0,112);
    weightStatus=label(root,"",&lv_font_montserrat_16,lv_color_hex(uiMuted()),LV_ALIGN_TOP_MID,0,170);
    card(root,"TARE",145,232,190,52,Diagnostics,[](lv_event_t*) {Machine::startWeightDiagnosticsTare();});
    updateWeight(nullptr);liveTimer=lv_timer_create(updateWeight,100,nullptr);
}

void buildCommissioning(lv_event_t*)
{
    clearScreen("Commission load cells",true);lv_obj_t* root=lv_scr_act();addBack(root,buildMachineSettings);
    const Machine::CommissioningSnapshot& value=Machine::commissioning();displayedCommissioningState=value.state;displayedCommissioningSamples=value.samplesCollected;
    char primary[72]{},detail[96]{},progress[48]{};
    const uint32_t ink=uiInk(),muted=uiMuted();
    switch(value.state)
    {
        case Machine::CommissioningState::Ready:
        {
            char massOne[12]{},massTwo[12]{},massThree[12]{},collars[12]{};
            formatGrams(massOne,sizeof(massOne),value.referenceMassesGrams[0]);
            formatGrams(massTwo,sizeof(massTwo),value.referenceMassesGrams[1]);
            formatGrams(massThree,sizeof(massThree),value.referenceMassesGrams[2]);
            formatGrams(collars,sizeof(collars),value.expectedTotalGrams);
            snprintf(primary,sizeof(primary),"Direct-leg + hub calibration");
            snprintf(detail,sizeof(detail),"%sg / %sg / %sg",massOne,massTwo,massThree);
            snprintf(progress,sizeof(progress),"4 collars validate %sg",collars);
            card(root,"BEGIN",26,250,428,48,Diagnostics,[](lv_event_t*) {Machine::confirmCommissioning();buildCommissioning(nullptr);});break;
        }
        case Machine::CommissioningState::PromptLeg:
        {
            static const char* legs[]={"FL","FR","RL","RR"};
            char mass[12]{};formatGrams(mass,sizeof(mass),value.referenceMassGrams);
            snprintf(primary,sizeof(primary),"%sg over %s leg",mass,legs[value.currentLeg<4?value.currentLeg:0]);
            snprintf(detail,sizeof(detail),"Directly over that leg");
            snprintf(progress,sizeof(progress),"Direct capture %u of 12",unsigned(value.currentReferenceMass*4+value.currentLeg+1));
            card(root,"CAPTURE",26,250,428,48,Diagnostics,[](lv_event_t*) {Machine::confirmCommissioning();buildCommissioning(nullptr);});break;
        }
        case Machine::CommissioningState::PromptHubReference:
            snprintf(primary,sizeof(primary),"Center-hub correction");snprintf(detail,sizeof(detail),"%s",value.message);
            card(root,"CAPTURE",26,250,428,48,Diagnostics,[](lv_event_t*) {Machine::confirmCommissioning();buildCommissioning(nullptr);});break;
        case Machine::CommissioningState::PromptAllFirst:
            snprintf(primary,sizeof(primary),"Place 4 collars at arm ends");snprintf(detail,sizeof(detail),"Capture orientation one");
            card(root,"CAPTURE",26,250,428,48,Diagnostics,[](lv_event_t*) {Machine::confirmCommissioning();buildCommissioning(nullptr);});break;
        case Machine::CommissioningState::PromptRotate:
            snprintf(primary,sizeof(primary),"First validation passed");snprintf(detail,sizeof(detail),"Remove all weights, then rotate 180°");
            card(root,"ROTATE 180°",26,250,428,48,Diagnostics,[](lv_event_t*) {Machine::confirmCommissioning();buildCommissioning(nullptr);});break;
        case Machine::CommissioningState::PromptAllSecond:
            snprintf(primary,sizeof(primary),"Weight 1: lower-left fixed");snprintf(detail,sizeof(detail),"Weights 2–4: same chassis spots");
            card(root,"CAPTURE",26,250,428,48,Diagnostics,[](lv_event_t*) {Machine::confirmCommissioning();buildCommissioning(nullptr);});break;
        case Machine::CommissioningState::Complete:
        {
            char first[12]{},second[12]{};formatGrams(first,sizeof(first),value.firstValidationGrams,1);formatGrams(second,sizeof(second),value.secondValidationGrams,1);
            snprintf(primary,sizeof(primary),"Calibration saved");snprintf(detail,sizeof(detail),"Hub %.4f | 0 deg %sg | 180 deg %sg",double(value.hubGain),first,second);
            card(root,"DONE",26,250,428,48,0x2FAF5A,[](lv_event_t*) {Machine::cancelCommissioning();buildMachineSettings(nullptr);});break;
        }
        case Machine::CommissioningState::Failed:
            snprintf(primary,sizeof(primary),"Commission failed");snprintf(detail,sizeof(detail),"%s",value.message);
            card(root,"BACK TO SETTINGS",26,250,428,48,0xB61E28,[](lv_event_t*) {Machine::cancelCommissioning();buildMachineSettings(nullptr);});break;
        default:
            snprintf(primary,sizeof(primary),"%s",value.message);
            if(value.state==Machine::CommissioningState::CapturingLeg||value.state==Machine::CommissioningState::CapturingHubReference||value.state==Machine::CommissioningState::CapturingAllFirst||value.state==Machine::CommissioningState::CapturingAllSecond)
                snprintf(detail,sizeof(detail),"Collecting %u / %u",unsigned(value.samplesCollected),unsigned(value.samplesRequired));
            else if(value.state==Machine::CommissioningState::Indexing||value.state==Machine::CommissioningState::Rotating)snprintf(detail,sizeof(detail),"Hub rotating slowly");
            else if(value.state==Machine::CommissioningState::Taring)snprintf(detail,sizeof(detail),"Keep hub empty and still");
            break;
    }
    commissioningPrimary=label(root,primary,&lv_font_montserrat_20,lv_color_hex(value.state==Machine::CommissioningState::Failed?0xB61E28:ink),LV_ALIGN_TOP_MID,0,104);
    commissioningDetail=label(root,detail,&lv_font_montserrat_16,lv_color_hex(muted),LV_ALIGN_TOP_MID,0,144);
    commissioningProgress=label(root,progress,&lv_font_montserrat_14,lv_color_hex(muted),LV_ALIGN_TOP_MID,0,178);
    liveTimer=lv_timer_create([](lv_timer_t*) {
        const Machine::CommissioningSnapshot& value=Machine::commissioning();
        if(value.state!=displayedCommissioningState){buildCommissioning(nullptr);return;}
        const bool capturing=value.state==Machine::CommissioningState::CapturingLeg||value.state==Machine::CommissioningState::CapturingHubReference||value.state==Machine::CommissioningState::CapturingAllFirst||value.state==Machine::CommissioningState::CapturingAllSecond;
        if(capturing&&value.samplesCollected!=displayedCommissioningSamples&&commissioningDetail) {
            displayedCommissioningSamples=value.samplesCollected;
            char detail[48];snprintf(detail,sizeof(detail),"Collecting %u / %u",unsigned(value.samplesCollected),unsigned(value.samplesRequired));
            lv_label_set_text(commissioningDetail,detail);
        }
    },100,nullptr);
}

void openCommissioning(lv_event_t*)
{
    // A first commissioning has nothing to overwrite. Recommissioning does,
    // so make that deliberate rather than silently replacing the physical
    // machine model that makes weight winding accurate.
    if(settings.loadCellCalibrationValid){buildCommissioningOverwriteWarning(nullptr);return;}
    if(Machine::openCommissioning())buildCommissioning(nullptr);
}

void confirmCommissioningOverwrite(lv_event_t*)
{
    if(Machine::openCommissioning())buildCommissioning(nullptr);
}

void buildCommissioningOverwriteWarning(lv_event_t*)
{
    clearScreen("Commission load cells",true);lv_obj_t* root=lv_scr_act();addBack(root,buildMachineSettings);
    warningIcon(root,84);
    label(root,"Replace saved calibration?",&lv_font_montserrat_20,lv_color_hex(0xB61E28),LV_ALIGN_TOP_MID,0,124);
    label(root,"Commissioning will overwrite this",&lv_font_montserrat_14,lv_color_hex(uiInk()),LV_ALIGN_TOP_MID,0,158);
    label(root,"machine's current load-cell model.",&lv_font_montserrat_14,lv_color_hex(uiInk()),LV_ALIGN_TOP_MID,0,178);
    label(root,"Proceed only with the calibration masses.",&lv_font_montserrat_14,lv_color_hex(uiMuted()),LV_ALIGN_TOP_MID,0,204);
    card(root,"NO",38,244,184,52,0x4856C8,buildMachineSettings);
    card(root,"YES",258,244,184,52,0xB61E28,confirmCommissioningOverwrite);
}

void toggleHallFeedback(lv_event_t*)
{
    Machine::toggleHallFeedback();
    buildMachineSettings(nullptr);
}

void startTouchCalibration(lv_event_t*)
{
    // Touch calibration owns the display while it is collecting points. Once
    // saved, return to Display Settings rather than the home screen.
    returnToDisplaySettingsAfterCalibration=true;
    beginTouchCalibration();
}

void openRunCurrentWarning(lv_event_t*)
{
    editingRunCurrent=true;
    buildMotorCurrentWarning(nullptr);
}

void openHoldCurrentWarning(lv_event_t*)
{
    editingRunCurrent=false;
    buildMotorCurrentWarning(nullptr);
}

void adjustMotorCurrent(lv_event_t* event)
{
    const int delta=intptr_t(lv_obj_get_user_data(lv_event_get_target(event)));
    const Machine::MotorSettingsSnapshot& motor=Machine::motorSettings();
    if(editingRunCurrent)
        Machine::setRunCurrentMa(uint16_t(constrain(int(motor.runCurrentMa)+delta,int(Config::MinRunCurrentMa),int(Config::MaxRunCurrentMa))));
    else
        Machine::setHoldCurrentMa(uint16_t(constrain(int(motor.holdCurrentMa)+delta,int(Config::MinHoldCurrentMa),min(int(Config::MaxHoldCurrentMa),int(motor.runCurrentMa)))));
    buildMotorCurrentEditor(nullptr);
}

void resetMotorCurrent(lv_event_t*)
{
    if(editingRunCurrent)Machine::resetRunCurrentToDefault();
    else Machine::resetHoldCurrentToDefault();
    buildMotorCurrentEditor(nullptr);
}

void saveMotorCurrentAndReturn(lv_event_t*)
{
    Machine::saveMotorSettings();
    buildMotorSettings(nullptr);
}

void buildMotorCurrentWarning(lv_event_t*)
{
    clearScreen(editingRunCurrent?"Run current":"Hold current",true);
    lv_obj_t* root=lv_scr_act();addBack(root,buildMotorSettings);
    warningIcon(root,80);
    label(root,"Support-only setting",&lv_font_montserrat_20,lv_color_hex(0xB61E28),LV_ALIGN_TOP_MID,0,120);
    label(root,"Do not change this unless instructed",&lv_font_montserrat_14,lv_color_hex(uiInk()),LV_ALIGN_TOP_MID,0,150);
    label(root,"by Support. Incorrect current can cause",&lv_font_montserrat_14,lv_color_hex(uiInk()),LV_ALIGN_TOP_MID,0,170);
    label(root,"motor or machine failure and damage not",&lv_font_montserrat_14,lv_color_hex(uiInk()),LV_ALIGN_TOP_MID,0,190);
    label(root,"covered by warranty. Proceed?",&lv_font_montserrat_14,lv_color_hex(uiInk()),LV_ALIGN_TOP_MID,0,210);
    card(root,"NO",38,244,184,52,0x4856C8,buildMotorSettings);
    card(root,"YES",258,244,184,52,0xB61E28,buildMotorCurrentEditor);
}

void buildMotorCurrentEditor(lv_event_t*)
{
    const Machine::MotorSettingsSnapshot& motor=Machine::motorSettings();
    clearScreen(editingRunCurrent?"Run current":"Hold current",true);
    lv_obj_t* root=lv_scr_act();addBack(root,saveMotorCurrentAndReturn);
    char value[24]{};
    snprintf(value,sizeof(value),"%u mA",unsigned(editingRunCurrent?motor.runCurrentMa:motor.holdCurrentMa));
    label(root,value,&lv_font_montserrat_36,lv_color_hex(uiInk()),LV_ALIGN_TOP_MID,0,112);
    label(root,editingRunCurrent?"Motor drive current":"Motor holding current",&lv_font_montserrat_14,lv_color_hex(uiMuted()),LV_ALIGN_TOP_MID,0,164);
    lv_obj_t* minus=card(root,"-",44,208,112,64,0x4856C8,adjustMotorCurrent);
    lv_obj_set_user_data(minus,reinterpret_cast<void*>(intptr_t(-Config::CurrentStepMa)));
    card(root,"RESET",184,208,112,64,Muted,resetMotorCurrent);
    lv_obj_t* plus=card(root,"+",324,208,112,64,0x4856C8,adjustMotorCurrent);
    lv_obj_set_user_data(plus,reinterpret_cast<void*>(intptr_t(Config::CurrentStepMa)));
}

void toggleMotorDirection(lv_event_t*)
{
    Machine::toggleClockwise();
    buildMotorDirection(nullptr);
}

void saveMotorDirectionAndReturn(lv_event_t*)
{
    Machine::saveMotorSettings();
    buildMotorSettings(nullptr);
}

void buildMotorDirection(lv_event_t*)
{
    const bool clockwise=Machine::motorSettings().clockwise;
    clearScreen("Rotation direction",true);
    lv_obj_t* root=lv_scr_act();addBack(root,saveMotorDirectionAndReturn);
    const uint32_t color=clockwise?0x2FAF5A:0xB61E28;
    const char* direction=clockwise?"CLOCKWISE":"COUNTER\nCLOCKWISE";
    card(root,direction,105,102,270,150,color,toggleMotorDirection);
    label(root,"Tap to change direction",&lv_font_montserrat_16,lv_color_hex(uiMuted()),LV_ALIGN_TOP_MID,0,272);
}

void buildMotorSettings(lv_event_t*)
{
    clearScreen("Motor",true);lv_obj_t* root=lv_scr_act();addBack(root,buildMachineSettings);
    card(root,"RUN\nCURRENT",20,112,136,136,0x6170D8,openRunCurrentWarning);
    card(root,"HOLD\nCURRENT",172,112,136,136,0x5261C8,openHoldCurrentWarning);
    card(root,"ROTATION\nDIRECTION",324,112,136,136,0x4856C8,buildMotorDirection);
}

void updateConnectivity(lv_timer_t*)
{
    const Machine::ConnectivitySnapshot& state=Machine::connectivity();
    if(bluetoothStateLabel)lv_label_set_text(bluetoothStateLabel,state.bluetoothEnabled?"Enabled":"Disabled");
    if(pairingButtonLabel)lv_label_set_text(pairingButtonLabel,state.pairingOpen?"STOP\nPAIRING":"PAIR\nDEVICE");
    if(pairingButton)lv_obj_set_style_bg_color(pairingButton,lv_color_hex(state.bluetoothEnabled?(state.pairingOpen?0xB61E28:0xB84555):uiMuted()),0);
    if(pairingStateLabel)
    {
        char remaining[20]{};
        if(state.pairingOpen)snprintf(remaining,sizeof(remaining),"%lus",static_cast<unsigned long>(state.pairingSecondsRemaining));
        else snprintf(remaining,sizeof(remaining),"READY");
        lv_label_set_text(pairingStateLabel,remaining);
    }
    if(pairingPromptLabel)
    {
        if(state.pairingOpen)lv_label_set_text(pairingPromptLabel,"Please pair your device now.");
        else if(state.bluetoothEnabled)lv_label_set_text(pairingPromptLabel,"Opens a 60 second pairing window.");
        else lv_label_set_text(pairingPromptLabel,"Enable Bluetooth to pair a device.");
    }
}

void toggleBluetooth(lv_event_t* event)
{
    const bool enabled=lv_obj_has_state(lv_event_get_target(event),LV_STATE_CHECKED);
    Machine::setBluetoothEnabled(enabled);
    buildConnectivity(nullptr);
}

void togglePairing(lv_event_t*)
{
    Machine::togglePairingWindow();
    buildConnectivity(nullptr);
}

void buildConnectivity(lv_event_t*)
{
    clearScreen("Connectivity",true);lv_obj_t* root=lv_scr_act();addBack(root,buildConfiguration);
    const Machine::ConnectivitySnapshot& state=Machine::connectivity();
    label(root,LV_SYMBOL_BLUETOOTH,&lv_font_montserrat_20,lv_color_hex(0x2176D2),LV_ALIGN_TOP_LEFT,24,94);
    label(root,"Bluetooth",&lv_font_montserrat_20,lv_color_hex(uiInk()),LV_ALIGN_TOP_LEFT,56,94);
    bluetoothStateLabel=label(root,state.bluetoothEnabled?"Enabled":"Disabled",&lv_font_montserrat_14,lv_color_hex(uiMuted()),LV_ALIGN_TOP_LEFT,24,124);

    lv_obj_t* toggle=lv_switch_create(root);lv_obj_set_size(toggle,82,42);lv_obj_set_pos(toggle,374,96);
    lv_obj_set_style_bg_color(toggle,lv_color_hex(0xDDE2EC),LV_PART_MAIN);lv_obj_set_style_bg_color(toggle,lv_color_hex(0xB84555),LV_PART_INDICATOR);lv_obj_set_style_bg_color(toggle,lv_color_white(),LV_PART_KNOB);lv_obj_set_style_shadow_width(toggle,0,LV_PART_KNOB);
    if(state.bluetoothEnabled)lv_obj_add_state(toggle,LV_STATE_CHECKED);
    lv_obj_add_event_cb(toggle,toggleBluetooth,LV_EVENT_VALUE_CHANGED,nullptr);

    lv_obj_t* divider=lv_obj_create(root);lv_obj_set_size(divider,432,2);lv_obj_set_pos(divider,24,158);lv_obj_set_style_bg_color(divider,lv_color_hex(0xDDE2EC),0);lv_obj_set_style_border_width(divider,0,0);lv_obj_set_style_radius(divider,0,0);
    pairingButton=card(root,state.pairingOpen?"STOP\nPAIRING":"PAIR\nDEVICE",24,180,260,70,state.bluetoothEnabled?(state.pairingOpen?0xB61E28:0xB84555):Muted,state.bluetoothEnabled?togglePairing:nullptr);
    pairingButtonLabel=lv_obj_get_child(pairingButton,0);
    lv_obj_t* status=lv_obj_create(root);lv_obj_set_size(status,150,70);lv_obj_set_pos(status,306,180);styleCard(status,state.pairingOpen?0x932535:uiMuted());lv_obj_clear_flag(status,LV_OBJ_FLAG_SCROLLABLE);
    pairingStateLabel=label(status,state.pairingOpen?"60s":"READY",&lv_font_montserrat_20,lv_color_white(),LV_ALIGN_CENTER,0,0);
    pairingPromptLabel=label(root,"",&lv_font_montserrat_16,lv_color_hex(uiMuted()),LV_ALIGN_TOP_MID,0,266);
    updateConnectivity(nullptr);
    liveTimer=lv_timer_create(updateConnectivity,250,nullptr);
}

void buildMachineSettings(lv_event_t*)
{
    clearScreen("Machine settings",true);lv_obj_t* root=lv_scr_act();addBack(root,buildConfiguration);
    card(root,"COMMISSION\nLOAD CELLS",20,88,210,92,0x6170D8,openCommissioning);
    card(root,"MOTOR",250,88,210,92,0x6170D8,buildMotorSettings);
    card(root,Machine::hallFeedbackEnabled()?"HALL FEEDBACK\nON":"HALL FEEDBACK\nOFF",20,198,210,92,Machine::hallFeedbackEnabled()?0x2FAF5A:0x9BA3BB,toggleHallFeedback);
    card(root,"DISPLAY",250,198,210,92,0x5261C8,buildDisplaySettings);
}

void refreshDisplaySleepControls()
{
    if(displaySleepMinutesLabel){char caption[20]{};snprintf(caption,sizeof(caption),"%u min",unsigned(settings.touchDisplaySleepMinutes));lv_label_set_text(displaySleepMinutesLabel,caption);}
    if(displaySleepSlider)lv_slider_set_value(displaySleepSlider,settings.touchDisplaySleepMinutes,LV_ANIM_OFF);
}

void setDisplaySleepEnabled(lv_event_t* event)
{
    settings.touchDisplaySleepEnabled=lv_obj_has_state(lv_event_get_target(event),LV_STATE_CHECKED)?1:0;
    lastTouchActivity=millis();
    buildDisplaySleep(nullptr);
}

void setDisplayDarkMode(lv_event_t* event)
{
    settings.touchDisplayDarkMode=lv_obj_has_state(lv_event_get_target(event),LV_STATE_CHECKED)?1:0;
    lastTouchActivity=millis();
    // Rebuild immediately so this control is itself the proof of the selected
    // theme. Persistence still follows the normal back action.
    buildDisplaySettings(nullptr);
}

void updateDisplaySleepMinutes(lv_event_t*)
{
    if(displaySleepSlider)settings.touchDisplaySleepMinutes=uint8_t(lv_slider_get_value(displaySleepSlider));
    refreshDisplaySleepControls();lastTouchActivity=millis();
}

void adjustDisplaySleepMinutes(lv_event_t* event)
{
    const int delta=intptr_t(lv_obj_get_user_data(lv_event_get_target(event)));
    settings.touchDisplaySleepMinutes=uint8_t(constrain(int(settings.touchDisplaySleepMinutes)+delta,int(Config::MinTouchDisplaySleepMinutes),int(Config::MaxTouchDisplaySleepMinutes)));
    refreshDisplaySleepControls();lastTouchActivity=millis();
}

void saveDisplaySleepAndReturn(lv_event_t*)
{
    saveSettings();lastTouchActivity=millis();buildDisplaySettings(nullptr);
}

void buildDisplaySleep(lv_event_t*)
{
    clearScreen("Display sleep",true);lv_obj_t* root=lv_scr_act();addBack(root,saveDisplaySleepAndReturn);
    auto displayToggle=[&](const int y,const bool selected,lv_event_cb_t callback) {
        lv_obj_t* toggle=lv_switch_create(root);lv_obj_set_size(toggle,82,42);lv_obj_set_pos(toggle,374,y);
        lv_obj_set_style_bg_color(toggle,lv_color_hex(0xDDE2EC),LV_PART_MAIN);lv_obj_set_style_bg_color(toggle,lv_color_hex(0x6170D8),LV_PART_INDICATOR);lv_obj_set_style_bg_color(toggle,lv_color_white(),LV_PART_KNOB);lv_obj_set_style_shadow_width(toggle,0,LV_PART_KNOB);
        if(selected)lv_obj_add_state(toggle,LV_STATE_CHECKED);lv_obj_add_event_cb(toggle,callback,LV_EVENT_VALUE_CHANGED,nullptr);
    };
    label(root,"Blank display after inactivity",&lv_font_montserrat_16,lv_color_hex(uiInk()),LV_ALIGN_TOP_LEFT,24,92);
    displayToggle(82,settings.touchDisplaySleepEnabled,setDisplaySleepEnabled);
    displaySleepMinutesLabel=label(root,"",&lv_font_montserrat_28,lv_color_hex(uiInk()),LV_ALIGN_TOP_MID,0,142);
    lv_obj_t* minus=card(root,"-",20,206,48,48,0x6170D8,adjustDisplaySleepMinutes);lv_obj_set_user_data(minus,reinterpret_cast<void*>(intptr_t(-1)));
    displaySleepSlider=lv_slider_create(root);lv_obj_set_size(displaySleepSlider,328,16);lv_obj_set_pos(displaySleepSlider,76,222);lv_slider_set_range(displaySleepSlider,Config::MinTouchDisplaySleepMinutes,Config::MaxTouchDisplaySleepMinutes);
    lv_obj_set_style_bg_color(displaySleepSlider,lv_color_hex(0xB9C0D5),LV_PART_MAIN);lv_obj_set_style_bg_color(displaySleepSlider,lv_color_hex(0x6170D8),LV_PART_INDICATOR);lv_obj_set_style_bg_color(displaySleepSlider,lv_color_hex(0x6170D8),LV_PART_KNOB);lv_obj_add_event_cb(displaySleepSlider,updateDisplaySleepMinutes,LV_EVENT_VALUE_CHANGED,nullptr);
    lv_obj_t* plus=card(root,"+",412,206,48,48,0x6170D8,adjustDisplaySleepMinutes);lv_obj_set_user_data(plus,reinterpret_cast<void*>(intptr_t(1)));
    refreshDisplaySleepControls();
}

void buildDisplaySettings(lv_event_t*)
{
    clearScreen("Display",true);lv_obj_t* root=lv_scr_act();addBack(root,buildMachineSettings);
    label(root,"Dark mode",&lv_font_montserrat_16,lv_color_hex(uiInk()),LV_ALIGN_TOP_LEFT,24,84);
    lv_obj_t* darkModeToggle=lv_switch_create(root);lv_obj_set_size(darkModeToggle,82,42);lv_obj_set_pos(darkModeToggle,374,74);
    lv_obj_set_style_bg_color(darkModeToggle,lv_color_hex(0xDDE2EC),LV_PART_MAIN);lv_obj_set_style_bg_color(darkModeToggle,lv_color_hex(0x6170D8),LV_PART_INDICATOR);lv_obj_set_style_bg_color(darkModeToggle,lv_color_white(),LV_PART_KNOB);lv_obj_set_style_shadow_width(darkModeToggle,0,LV_PART_KNOB);
    if(settings.touchDisplayDarkMode)lv_obj_add_state(darkModeToggle,LV_STATE_CHECKED);lv_obj_add_event_cb(darkModeToggle,setDisplayDarkMode,LV_EVENT_VALUE_CHANGED,nullptr);
    card(root,"TOUCH\nCALIBRATION",20,134,205,112,0x6170D8,startTouchCalibration);
    char caption[40]{};
    if(settings.touchDisplaySleepEnabled)snprintf(caption,sizeof(caption),"DISPLAY SLEEP\n%u MIN",unsigned(settings.touchDisplaySleepMinutes));
    else snprintf(caption,sizeof(caption),"DISPLAY SLEEP\nOFF");
    card(root,caption,255,134,205,112,settings.touchDisplaySleepEnabled?0x5261C8:0x9BA3BB,buildDisplaySleep);
}

void buildDiagnosticsMenu(lv_event_t*)
{
    clearScreen("Diagnostics",true);lv_obj_t* root=lv_scr_act();addBack(root,buildConfiguration);
    card(root,"SYSTEM\nCHECK",20,112,205,136,DiagnosticsLight,buildSystemCheck);
    card(root,"WEIGHT",255,112,205,136,Diagnostics,buildWeightDiagnostics);
}

void buildConfiguration(lv_event_t*)
{
    clearScreen("Configuration",true);lv_obj_t* root=lv_scr_act();addBack(root,buildHome);
    card(root,"WEIGHT\nTARGETS",20,88,210,92,Weight,buildWeightTargetsMenu);
    card(root,"CONNECTIVITY",250,88,210,92,0xB84555,buildConnectivity);
    card(root,"MACHINE\nSETTINGS",20,198,210,92,0x4856C8,buildMachineSettings);
    card(root,"DIAGNOSTICS",250,198,210,92,Diagnostics,buildDiagnosticsMenu);
}

void updateTouchTurns()
{
    touchTurns=uint16_t(constrain(int(touchTurns),int(Config::MinTurns),350));
    if(turnCountLabel){char caption[12];snprintf(caption,sizeof(caption),"%u",unsigned(touchTurns));lv_label_set_text(turnCountLabel,caption);}
    if(turnSlider)lv_slider_set_value(turnSlider,touchTurns,LV_ANIM_OFF);
}

void adjustTouchTurns(lv_event_t* event)
{
    const int delta=intptr_t(lv_obj_get_user_data(lv_event_get_target(event)));
    touchTurns=uint16_t(constrain(int(touchTurns)+delta,int(Config::MinTurns),350));
    updateTouchTurns();
}

void updateTouchTurnsFromSlider(lv_event_t*)
{
    if(turnSlider)touchTurns=uint16_t(lv_slider_get_value(turnSlider));
    updateTouchTurns();
}

void beginCountWinding(lv_event_t*)
{
    if(Machine::prepareCountWinding(touchTurns))buildWindingWorkflow(nullptr);
}

void beginWeightWinding(lv_event_t* event)
{
    const uint8_t preset=uint8_t(intptr_t(lv_obj_get_user_data(lv_event_get_target(event))));
    if(Machine::prepareWeightWinding(preset))buildWindingWorkflow(nullptr);
}

void confirmTouchLoadYarn(lv_event_t*)
{
    if(Machine::confirmYarnLoaded())buildWindingWorkflow(nullptr);
}

void toggleTouchPause(lv_event_t*)
{
    const Machine::WindingSnapshot& run=Machine::winding();
    if(run.requiresClearance)Machine::confirmSafetyClearance();
    else Machine::togglePauseResume();
}

void exitTouchWinding(lv_event_t*)
{
    const bool weightMode=Machine::winding().weightMode;
    Machine::exitWindingWorkflow();
    if(weightMode)buildWeightWindingMenu(nullptr);
    else buildTurnWindingMenu(nullptr);
}

void requestTouchWindingBack(lv_event_t*)
{
    // Once a run has begun, navigation must never silently turn into an
    // abort. Leave the hardware running while the user decides.
    if(Machine::winding().stage==Machine::WindingStage::Running) {
        buildWindingStopConfirm(nullptr);
        return;
    }
    exitTouchWinding(nullptr);
}

void returnToTouchWinding(lv_event_t*)
{
    buildWindingWorkflow(nullptr);
}

void buildWindingStopConfirm(lv_event_t*)
{
    const Machine::WindingSnapshot& run=Machine::winding();
    clearScreen(run.weightMode?"Weight winding":"Turn winding",true);
    lv_obj_t* root=lv_scr_act();
    // Back from this confirmation means "No" and leaves the run untouched.
    addBack(root,returnToTouchWinding);
    warningIcon(root,80);
    label(root,"Stop winding?",&lv_font_montserrat_28,lv_color_hex(uiInk()),LV_ALIGN_TOP_MID,0,120);
    label(root,"The hub will coast to a stop",&lv_font_montserrat_16,lv_color_hex(uiMuted()),LV_ALIGN_TOP_MID,0,164);
    card(root,"NO",28,226,196,58,0x4856C8,returnToTouchWinding);
    card(root,"YES, STOP",256,226,196,58,0xB61E28,exitTouchWinding);
}

void buildTurnWindingMenu(lv_event_t*)
{
    clearScreen("Turn count",true);lv_obj_t* root=lv_scr_act();addBack(root,queueHomeScreen);
    turnCountLabel=label(root,"",&lv_font_montserrat_36,lv_color_hex(uiInk()),LV_ALIGN_TOP_MID,0,88);
    turnSlider=lv_slider_create(root);lv_obj_set_size(turnSlider,328,16);lv_obj_set_pos(turnSlider,76,154);lv_slider_set_range(turnSlider,Config::MinTurns,350);
    lv_obj_set_style_bg_color(turnSlider,lv_color_hex(0xB9C0D5),LV_PART_MAIN);lv_obj_set_style_bg_color(turnSlider,lv_color_hex(0x4856C8),LV_PART_INDICATOR);lv_obj_set_style_bg_color(turnSlider,lv_color_hex(0x4856C8),LV_PART_KNOB);
    lv_obj_add_event_cb(turnSlider,updateTouchTurnsFromSlider,LV_EVENT_VALUE_CHANGED,nullptr);
    lv_obj_t* minus=card(root,"-",20,194,48,48,0x4856C8,adjustTouchTurns);lv_obj_set_user_data(minus,reinterpret_cast<void*>(intptr_t(-1)));
    lv_obj_t* plus=card(root,"+",412,194,48,48,0x4856C8,adjustTouchTurns);lv_obj_set_user_data(plus,reinterpret_cast<void*>(intptr_t(1)));
    card(root,"START",20,254,440,48,0x2FAF5A,beginCountWinding);
    touchTurns=uint16_t(constrain(int(touchTurns),int(Config::MinTurns),350));updateTouchTurns();
}

void buildWeightWindingMenu(lv_event_t*)
{
    clearScreen("Weight",true);lv_obj_t* root=lv_scr_act();addBack(root,queueHomeScreen);
    constexpr uint32_t colors[]={WeightLight,Weight,WeightDeep};
    constexpr const char* names[]={"MINI\n20 g","HALF\n50 g","FULL\n100 g"};
    for(uint8_t preset=0;preset<3;preset++) {
        lv_obj_t* item=card(root,names[preset],20+152*preset,112,136,136,colors[preset],beginWeightWinding);
        lv_obj_set_user_data(item,reinterpret_cast<void*>(intptr_t(preset)));
    }
}

void updateWindingWorkflow(lv_timer_t*)
{
    const Machine::WindingSnapshot& run=Machine::winding();
    if(run.stage!=displayedWindingStage){queueWindingWorkflowRefresh();return;}
    if(run.stage!=Machine::WindingStage::Running)return;
    float fraction=0.0f;
    if(run.weightMode&&isfinite(run.targetWeightGrams)&&run.targetWeightGrams>0.0f&&run.measuredWeightValid)
        fraction=constrain(run.measuredWeightGrams/run.targetWeightGrams,0.0f,1.0f);
    else if(run.selectedTurns)fraction=constrain(run.completedTurns/run.selectedTurns,0.0f,1.0f);
    if(windingGauge)lv_arc_set_value(windingGauge,int(lroundf(fraction*100.0f)));
    if(windingPercent){char percent[12];snprintf(percent,sizeof(percent),"%u%%",unsigned(lroundf(fraction*100.0f)));lv_label_set_text(windingPercent,percent);}
    if(windingAction)lv_label_set_text(windingAction,run.requiresClearance?"CHECK\nHUB":(run.paused?"RESUME":"PAUSE"));
}

void buildWindingWorkflow(lv_event_t*)
{
    const Machine::WindingSnapshot& run=Machine::winding();
    displayedWindingStage=run.stage;
    const char* title=run.weightMode?"Weight winding":"Turn winding";
    clearScreen(title,true);lv_obj_t* root=lv_scr_act();addBack(root,requestTouchWindingBack);
    if(run.stage==Machine::WindingStage::HallSeeking) {
        label(root,"Finding hub position",&lv_font_montserrat_28,lv_color_hex(uiInk()),LV_ALIGN_TOP_MID,0,112);
        label(root,"Please wait",&lv_font_montserrat_16,lv_color_hex(uiMuted()),LV_ALIGN_TOP_MID,0,156);
    } else if(run.stage==Machine::WindingStage::Taring) {
        label(root,"Taring machine",&lv_font_montserrat_28,lv_color_hex(uiInk()),LV_ALIGN_TOP_MID,0,112);
        label(root,"Keep the machine still",&lv_font_montserrat_16,lv_color_hex(uiMuted()),LV_ALIGN_TOP_MID,0,156);
    } else if(run.stage==Machine::WindingStage::LoadYarn) {
        label(root,"Load yarn",&lv_font_montserrat_28,lv_color_hex(uiInk()),LV_ALIGN_TOP_MID,0,94);
        label(root,"Press GO when ready",&lv_font_montserrat_16,lv_color_hex(uiMuted()),LV_ALIGN_TOP_MID,0,132);
        lv_obj_t* go=card(root,"GO",170,176,140,100,0x2FAF5A,confirmTouchLoadYarn);lv_obj_set_style_radius(go,LV_RADIUS_CIRCLE,0);
    } else if(run.stage==Machine::WindingStage::Running) {
        windingGauge=lv_arc_create(root);lv_obj_set_size(windingGauge,220,220);lv_obj_align(windingGauge,LV_ALIGN_TOP_MID,0,72);lv_arc_set_range(windingGauge,0,100);lv_arc_set_bg_angles(windingGauge,135,405);lv_arc_set_angles(windingGauge,135,405);lv_obj_remove_style(windingGauge,nullptr,LV_PART_KNOB);lv_obj_clear_flag(windingGauge,LV_OBJ_FLAG_CLICKABLE);lv_obj_set_style_arc_width(windingGauge,18,LV_PART_MAIN);lv_obj_set_style_arc_width(windingGauge,18,LV_PART_INDICATOR);lv_obj_set_style_arc_color(windingGauge,lv_color_hex(0xD3D9E8),LV_PART_MAIN);lv_obj_set_style_arc_color(windingGauge,lv_color_hex(Weight),LV_PART_INDICATOR);
        lv_obj_t* pause=card(root,"PAUSE",177,119,126,126,0x4856C8,toggleTouchPause);lv_obj_set_style_radius(pause,LV_RADIUS_CIRCLE,0);windingAction=lv_obj_get_child(pause,0);
        windingPercent=label(root,"0%",&lv_font_montserrat_28,lv_color_hex(uiInk()),LV_ALIGN_TOP_MID,0,272);
    } else if(run.stage==Machine::WindingStage::Complete) {
        label(root,run.finalAborted?"Winding stopped":"Winding complete",&lv_font_montserrat_28,lv_color_hex(run.finalAborted?0xB61E28:0x2FAF5A),LV_ALIGN_TOP_MID,0,98);
        char turns[32];snprintf(turns,sizeof(turns),"%u turns",unsigned(run.finalTurns));label(root,turns,&lv_font_montserrat_20,lv_color_hex(uiInk()),LV_ALIGN_TOP_MID,0,146);
        if(run.finalWeightValid){char grams[32];formatGrams(grams,sizeof(grams),run.finalWeightGrams,1);strncat(grams," g",sizeof(grams)-strlen(grams)-1);label(root,grams,&lv_font_montserrat_20,lv_color_hex(uiInk()),LV_ALIGN_TOP_MID,0,178);}
        card(root,"DONE",20,244,440,48,0x2FAF5A,exitTouchWinding);
    } else if(run.stage==Machine::WindingStage::HallFailed||run.stage==Machine::WindingStage::TareFailed||run.stage==Machine::WindingStage::Fault) {
        label(root,"Unable to start winding",&lv_font_montserrat_20,lv_color_hex(0xB61E28),LV_ALIGN_TOP_MID,0,110);
        label(root,"Check hub and load sensors",&lv_font_montserrat_16,lv_color_hex(uiMuted()),LV_ALIGN_TOP_MID,0,152);
        card(root,"DONE",20,244,440,48,0x4856C8,exitTouchWinding);
    }
    liveTimer=lv_timer_create(updateWindingWorkflow,100,nullptr);
    updateWindingWorkflow(nullptr);
}

void buildHome(lv_event_t*)
{
    clearScreen("Hank Winder");lv_obj_t* root=lv_scr_act();
    // Five rapid taps here reveal the idle-only credit sequence. The touch
    // target is deliberately invisible; the visible version is metadata,
    // not a button competing with the machine title.
    lv_obj_t* version=lv_btn_create(root);lv_obj_set_size(version,82,42);lv_obj_set_pos(version,184,14);
    lv_obj_set_style_bg_opa(version,LV_OPA_TRANSP,0);lv_obj_set_style_border_width(version,0,0);lv_obj_set_style_shadow_width(version,0,0);lv_obj_set_style_radius(version,0,0);
    char versionText[18];snprintf(versionText,sizeof(versionText),"v%s",Config::FirmwareVersion);
    label(root,versionText,&lv_font_montserrat_14,lv_color_hex(0xC6CCDE),LV_ALIGN_TOP_LEFT,190,26);
    lv_obj_add_event_cb(version,versionEasterEggTap,LV_EVENT_CLICKED,nullptr);
    if(weightCapabilityEnabled)card(root,"WEIGHT",34,102,190,190,Weight,buildWeightWindingMenu);
    card(root,"TURN\nCOUNT",weightCapabilityEnabled?256:145,102,190,190,0x4856C8,buildTurnWindingMenu);
    lv_obj_t* setup=lv_btn_create(root);lv_obj_set_size(setup,54,46);lv_obj_set_pos(setup,414,12);lv_obj_set_style_bg_opa(setup,LV_OPA_TRANSP,0);lv_obj_set_style_border_width(setup,0,0);
    label(setup,LV_SYMBOL_SETTINGS,&lv_font_montserrat_20,lv_color_white(),LV_ALIGN_CENTER,0,0);lv_obj_add_event_cb(setup,buildConfiguration,LV_EVENT_CLICKED,nullptr);
}

void initLvgl()
{
    lv_init();lv_disp_draw_buf_init(&lvDrawBuffer,drawBuffer,nullptr,DisplayWidth*DrawBufferRows);
    lv_disp_drv_init(&lvDisplay);lvDisplay.hor_res=DisplayWidth;lvDisplay.ver_res=DisplayHeight;lvDisplay.flush_cb=flushDisplay;lvDisplay.draw_buf=&lvDrawBuffer;
    lv_disp_t* display=lv_disp_drv_register(&lvDisplay);lv_indev_drv_init(&lvTouch);lvTouch.type=LV_INDEV_TYPE_POINTER;lvTouch.read_cb=readTouch;lv_indev_drv_register(&lvTouch);
    lv_theme_t* theme=lv_theme_default_init(display,lv_color_hex(Diagnostics),lv_color_hex(DiagnosticsLight),false,&lv_font_montserrat_16);lv_disp_set_theme(display,theme);lvglReady=true;lastLvTick=millis();buildHome();
}

void serviceCalibration(uint32_t now)
{
    if(now-lastTouchSample<20)return;lastTouchSample=now;const TS_Point point=touch.getPoint();
    const bool down=point.z>=TouchPressPressure&&point.x>0&&point.x<4095&&point.y>0&&point.y<4095;
    if(down) {
        if(now-releasedAt>=150&&calibrationSamples<20){calibrationX+=point.x;calibrationY+=point.y;++calibrationSamples;calibrationMinX=min(calibrationMinX,int(point.x));calibrationMaxX=max(calibrationMaxX,int(point.x));calibrationMinY=min(calibrationMinY,int(point.y));calibrationMaxY=max(calibrationMaxY,int(point.y));}
        wasDown=true;
    } else if(wasDown) {
        wasDown=false;releasedAt=now;const bool steady=calibrationSamples>=15&&(calibrationMaxX-calibrationMinX)<=100&&(calibrationMaxY-calibrationMinY)<=100;
            if(steady){rawCalibration[calibrationIndex]={float(calibrationX)/calibrationSamples,float(calibrationY)/calibrationSamples};++calibrationIndex;if(calibrationIndex==CalibrationPointCount){if(solveCalibration()&&saveCalibration()){calibrating=false;if(!lvglReady)initLvgl();else if(returnToDisplaySettingsAfterCalibration){returnToDisplaySettingsAfterCalibration=false;buildDisplaySettings(nullptr);}else buildHome(nullptr);}else {calibrationIndex=0;drawCalibration();}}else drawCalibration();}
        calibrationSamples=0;calibrationX=calibrationY=0;calibrationMinX=calibrationMinY=4095;calibrationMaxX=calibrationMaxY=0;
    }
}
}

void TouchDiagnosticsUi::begin()
{
    pinMode(TouchCs,OUTPUT);digitalWrite(TouchCs,HIGH);pinMode(TouchIrq,INPUT_PULLUP);
    SPI1.setRX(12);SPI1.setTX(15);SPI1.setSCK(14);touch.begin(SPI1);touch.setRotation(0);
    initialiseDisplay();
    if(loadCalibration()) {calibrating=false;initLvgl();Serial.println("Touch diagnostics: saved calibration loaded; live controller data active.");return;}
    beginTouchCalibration();
    Serial.println("Touch diagnostics: calibrate five points; live controller data follows.");
}

void TouchDiagnosticsUi::showBootSplash()
{
    initialiseDisplay();
    lcd.fillScreen(TFT_DARKGREY);
    drawPlaywriteSplashTitle();
    drawWinderMark(TFT_CYAN);
    drawBootStatus("Checking hardware",TFT_LIGHTGREY);
}

void TouchDiagnosticsUi::finishBootSplash()
{
    if(!displayInitialized)return;
    // This is purposefully a small-area animation after, never during,
    // hardware detection. It avoids the visible full-screen redraw caused by
    // the earlier experimental boot implementation.
    // Twelve small steps make one full sweep over the same 1.5-second
    // hand-off interval. Keeping the fixed status centered avoids a second,
    // distracting animation competing with the sweep.
    constexpr uint8_t SweepFrames=12;
    constexpr uint16_t SweepFrameDelayMs=125;
    drawBootStatus("Hardware ready",TFT_GREEN);
    uint16_t previousDegrees=0;
    for(uint8_t frame=1;frame<=SweepFrames;++frame)
    {
        const uint16_t nextDegrees=frame*(360/SweepFrames);
        redrawBootWinderSweep(previousDegrees,nextDegrees);
        previousDegrees=nextDegrees;
        delay(SweepFrameDelayMs);
    }
}

void TouchDiagnosticsUi::service()
{
    const uint32_t now=millis();
    if(calibrating){serviceCalibration(now);return;}
    serviceTouchDisplaySleep(now);
    const uint32_t elapsed=now-lastLvTick;lastLvTick=now;lv_tick_inc(elapsed);lv_timer_handler();
    if(pendingHomeAfterPhysicalStop)
    {
        pendingHomeAfterPhysicalStop=false;
        pendingWindingWorkflowRefresh=false;
        buildHome(nullptr);
        return;
    }
    if(pendingEasterEggAdvance)
    {
        pendingEasterEggAdvance=false;
        buildEasterEgg();
        return;
    }
    if(pendingEasterEggLogoDraw)
    {
        pendingEasterEggLogoDraw=false;
        drawBuzzyToesLogo();
    }
    if(pendingWindingWorkflowRefresh)
    {
        pendingWindingWorkflowRefresh=false;
        buildWindingWorkflow(nullptr);
    }
}

void TouchDiagnosticsUi::requestEmergencyStopView()
{
    // Do not manipulate LVGL from the GPIO edge path.  The next normal UI
    // service pass owns object deletion and returns the operator to home.
    pendingHomeAfterPhysicalStop=true;
}

#else

void TouchDiagnosticsUi::showBootSplash(){}
void TouchDiagnosticsUi::finishBootSplash(){}
void TouchDiagnosticsUi::begin(){}
void TouchDiagnosticsUi::service(){}
void TouchDiagnosticsUi::requestEmergencyStopView(){}

#endif
