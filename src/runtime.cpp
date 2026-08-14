#include "runtime.h"
U8G2_UC1701_MINI12864_F_4W_SW_SPI display(U8G2_R0,LCD_CLOCK,LCD_DATA,LCD_CS,LCD_DC,LCD_RESET);
Adafruit_NeoPixel pixels(NEOPIXEL_COUNT,NEOPIXEL_PIN,NEO_RGB+NEO_KHZ800);
PersistentSettings settings{}; UiState uiState=UiState::YARN_WEIGHT_SELECT;
volatile PauseState pauseState=PauseState::RUNNING;
volatile uint32_t pauseRampStartStep=0,pauseRampStopStep=0,resumeRampStartStep=0,resumeRampEndStep=0;
float pauseRampStartRPS=Config::START_MOTOR_RPS;
uint32_t pauseStartedTime=0,totalPausedTime=0; bool pauseTimeRecorded=false;
int selectedYarnWeight=0,selectedSkeinSize=0,selectedTurns=63; bool repeatYes=true;
int speedTrimPercent=100; float currentMotorRPS=Config::START_MOTOR_RPS; uint32_t windingStartTime=0;
volatile bool motionActive=false; volatile uint32_t currentStepCount=0,targetStepCount=0,requestedStepRateHz=0;
alarm_id_t stepAlarmId=0; int lastA=HIGH; bool lastClick=HIGH,lastStop=HIGH;
uint32_t lastTurnEncoderTime=0; bool heartbeatState=false; uint32_t lastHeartbeat=0;
