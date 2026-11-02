#include "machine_interface.h"

#include <math.h>

#include "app_api.h"
#include "commissioning.h"
#include "diagnostics_transport.h"
#include "hall_sensor.h"
#include "load_cells.h"
#include "run_supervisor.h"
#include "tmc_driver.h"

namespace {
Machine::DiagnosticsSnapshot current{};
Machine::CommissioningSnapshot commissioningCurrent{};
Machine::WeightTargetsSnapshot targetsCurrent{};
Machine::MotorSettingsSnapshot motorCurrent{};
Machine::ConnectivitySnapshot connectivityCurrent{};
Machine::SystemCheckSnapshot systemCheckCurrent{};
uint32_t systemCheckStarted=0;

bool allLoadCellsHealthy(const LoadCellSnapshot& cells)
{
    const uint8_t expected=(1u<<Config::LoadCellChannelCount)-1u;
    return cells.healthyMask==expected;
}

void failSystemCheck(const char* message)
{
    cancelAuxiliaryMotion();HallSensor::cancel();disableMotor();
    systemCheckCurrent.stage=Machine::SystemCheckStage::Failed;
    systemCheckCurrent.active=false;
    snprintf(systemCheckCurrent.message,sizeof(systemCheckCurrent.message),"%s",message);
}
Machine::WindingSnapshot windingCurrent{};

Machine::LoadCellChannelStatus mapStatus(const LoadCellStatus status)
{
    switch(status)
    {
        case LoadCellStatus::Ok:return Machine::LoadCellChannelStatus::Ok;
        case LoadCellStatus::Error:return Machine::LoadCellChannelStatus::Error;
        default:return Machine::LoadCellChannelStatus::NoData;
    }
}

Machine::TareStatus mapTareStatus(const LoadCellTareStatus status)
{
    switch(status)
    {
        case LoadCellTareStatus::Collecting:return Machine::TareStatus::Collecting;
        case LoadCellTareStatus::Complete:return Machine::TareStatus::Complete;
        case LoadCellTareStatus::Failed:return Machine::TareStatus::Failed;
        case LoadCellTareStatus::Cancelled:return Machine::TareStatus::Cancelled;
        default:return Machine::TareStatus::Idle;
    }
}

Machine::CommissioningState mapCommissioningState(const Commissioning::State state)
{
    return static_cast<Machine::CommissioningState>(state);
}

Machine::WindingStage mapWindingStage()
{
    switch(uiState)
    {
        case UiState::HALL_SEEK:return Machine::WindingStage::HallSeeking;
        case UiState::HALL_FAILED:return Machine::WindingStage::HallFailed;
        case UiState::TARING:return Machine::WindingStage::Taring;
        case UiState::TARE_FAILED:return Machine::WindingStage::TareFailed;
        case UiState::LOAD_YARN:return Machine::WindingStage::LoadYarn;
        case UiState::WINDING:return Machine::WindingStage::Running;
        case UiState::COMPLETE:return Machine::WindingStage::Complete;
        case UiState::LOAD_CELL_FAULT:return Machine::WindingStage::Fault;
        default:return Machine::WindingStage::Idle;
    }
}
}

void Machine::begin()
{
    service();
}

void Machine::service()
{
    const LoadCellSnapshot cells=LoadCells::snapshot();
    for(uint8_t channel=0;channel<Config::LoadCellChannelCount;channel++)
    {
        current.raw[channel]=cells.raw[channel];
        current.channelStatus[channel]=mapStatus(cells.status[channel]);
    }
    current.healthyMask=cells.healthyMask;
    current.sequence=cells.sequence;
    current.detectedMask=LoadCells::detectedMask();
    current.missingReadyMask=LoadCells::missingReadyMask();
    current.suspiciousMask=LoadCells::suspiciousMask();
    current.suspiciousCount=LoadCells::suspiciousCount();
    current.syncTimeoutCount=LoadCells::syncTimeoutCount();
    current.sampleDelayUs=LoadCells::sampleDelayUs();
    const float grams=LoadCells::weightGrams(cells);
    current.weightValid=!isnan(grams);
    current.weightGrams=current.weightValid?grams:0.0f;
    current.weightDiagnosticsSettling=weightDiagnosticsSettling();
    current.tareStatus=mapTareStatus(LoadCells::tareStatus());

    const Commissioning::State commissioningState=Commissioning::state();
    commissioningCurrent.state=mapCommissioningState(commissioningState);
    commissioningCurrent.currentLeg=Commissioning::currentLeg();
    commissioningCurrent.currentReferenceMass=Commissioning::currentReferenceMass();
    commissioningCurrent.samplesCollected=Commissioning::samplesCollected();
    commissioningCurrent.samplesRequired=Commissioning::samplesRequired();
    commissioningCurrent.referenceMassGrams=Commissioning::referenceMassGrams(commissioningCurrent.currentReferenceMass);
    for(uint8_t reference=0;reference<3;reference++)commissioningCurrent.referenceMassesGrams[reference]=Commissioning::referenceMassGrams(reference);
    commissioningCurrent.expectedTotalGrams=Commissioning::expectedTotalGrams();
    commissioningCurrent.firstValidationGrams=Commissioning::firstValidationGrams();
    commissioningCurrent.secondValidationGrams=Commissioning::secondValidationGrams();
    commissioningCurrent.hubGain=Commissioning::hubGain();
    commissioningCurrent.firstValidationValid=!isnan(commissioningCurrent.firstValidationGrams);
    commissioningCurrent.secondValidationValid=!isnan(commissioningCurrent.secondValidationGrams);
    commissioningCurrent.active=Commissioning::active();
    snprintf(commissioningCurrent.message,sizeof(commissioningCurrent.message),"%s",Commissioning::message());

    const RunSupervisorSnapshot supervisor=RunSupervisor::snapshot();
    windingCurrent.stage=mapWindingStage();
    windingCurrent.weightMode=!autoCountMode;
    windingCurrent.paused=pauseState==PauseState::PAUSED;
    windingCurrent.pauseTransition=pauseState==PauseState::RAMPING_DOWN||pauseState==PauseState::RAMPING_UP;
    windingCurrent.requiresClearance=RunSupervisor::waitingForClearance();
    windingCurrent.selectedTurns=uint16_t(max(0,selectedTurns));
    windingCurrent.completedTurns=safeCurrentStepCount()/float(Config::StepsPerHubRev);
    windingCurrent.targetWeightGrams=supervisor.weightArmed?supervisor.targetWeightGrams:NAN;
    windingCurrent.measuredWeightGrams=supervisor.filteredWeightGrams;
    windingCurrent.measuredWeightValid=!isnan(supervisor.filteredWeightGrams);
    windingCurrent.finalTurns=uint16_t(min<uint32_t>(finalRunStepCount/Config::StepsPerHubRev,UINT16_MAX));
    windingCurrent.finalWeightGrams=finalRunWeightGrams;
    windingCurrent.finalWeightValid=finalRunWeightValid;
    windingCurrent.finalAborted=finalRunAborted;

    motorCurrent.runCurrentMa=settings.runCurrentMa;
    motorCurrent.holdCurrentMa=settings.holdCurrentMa;
    motorCurrent.clockwise=settings.clockwise;
    connectivityCurrent.bluetoothEnabled=settings.bluetoothEnabled;
    connectivityCurrent.pairingOpen=DiagnosticsTransport::pairingOpen();
    connectivityCurrent.clientConnected=DiagnosticsTransport::connected();
    connectivityCurrent.pairingSecondsRemaining=DiagnosticsTransport::pairingSecondsRemaining();

    if(systemCheckCurrent.active)
    {
        if(systemCheckCurrent.stage==SystemCheckStage::CheckingLoadCells)
        {
            if(!allLoadCellsHealthy(cells)){
                static constexpr const char* names[]={"FL","FR","RL","RR"};
                char missing[64]="Load cells:";
                for(uint8_t channel=0;channel<Config::LoadCellChannelCount;channel++)if(!(cells.healthyMask&(1u<<channel))){strncat(missing," ",sizeof(missing)-strlen(missing)-1);strncat(missing,names[channel],sizeof(missing)-strlen(missing)-1);}
                failSystemCheck(missing);
            }
            else {systemCheckCurrent.loadCellsPassed=true;enableMotor();systemCheckStarted=millis();systemCheckCurrent.stage=SystemCheckStage::Taring;snprintf(systemCheckCurrent.message,sizeof(systemCheckCurrent.message),"Load cells OK; settling");}
        }
        else if(systemCheckCurrent.stage==SystemCheckStage::Taring)
        {
            if(millis()-systemCheckStarted>=Config::LoadCellTareMotorSettleMs)
            {
                if(LoadCells::tareStatus()!=LoadCellTareStatus::Collecting)LoadCells::startTare();
                const LoadCellTareStatus status=LoadCells::updateTare();
                if(status==LoadCellTareStatus::Complete){systemCheckCurrent.tarePassed=true;if(!settings.hallEnabled)failSystemCheck("Hall feedback disabled");else {systemCheckCurrent.stage=SystemCheckStage::SeekingClockwise;snprintf(systemCheckCurrent.message,sizeof(systemCheckCurrent.message),"Seeking Hall clockwise");HallSensor::startSeek(true);}}
                else if(status==LoadCellTareStatus::Failed)failSystemCheck("Tare failed");
            }
        }
        else if(systemCheckCurrent.stage==SystemCheckStage::SeekingClockwise&&HallSensor::seekFinished())
        {
            if(HallSensor::seekFailed()||HallSensor::state()==HallSensor::State::Absent)failSystemCheck("Hall clockwise failed");
            else {systemCheckCurrent.clockwiseHallPassed=true;systemCheckCurrent.stage=SystemCheckStage::SeekingCounterClockwise;snprintf(systemCheckCurrent.message,sizeof(systemCheckCurrent.message),"Seeking Hall counterclockwise");HallSensor::startSeek(false);}
        }
        else if(systemCheckCurrent.stage==SystemCheckStage::SeekingCounterClockwise&&HallSensor::seekFinished())
        {
            if(HallSensor::seekFailed()||HallSensor::state()==HallSensor::State::Absent)failSystemCheck("Hall counterclockwise failed");
            else {systemCheckCurrent.counterClockwiseHallPassed=true;disableMotor();systemCheckCurrent.stage=SystemCheckStage::WaitingForStopButton;snprintf(systemCheckCurrent.message,sizeof(systemCheckCurrent.message),"Press STOP button");}
        }
    }
}

const Machine::DiagnosticsSnapshot& Machine::diagnostics()
{
    return current;
}

const char* Machine::channelStatusName(const LoadCellChannelStatus status)
{
    switch(status)
    {
        case LoadCellChannelStatus::Ok:return "OK";
        case LoadCellChannelStatus::Error:return "Err";
        default:return "No Data";
    }
}

bool Machine::openCommissioning()
{
    if(!weightCapabilityEnabled)return false;
    Commissioning::cancel();
    uiState=UiState::CONFIG_COMMISSIONING;
    service();
    return true;
}

bool Machine::confirmCommissioning()
{
    if(uiState!=UiState::CONFIG_COMMISSIONING)return false;
    const Commissioning::State state=Commissioning::state();
    if(state==Commissioning::State::Complete||state==Commissioning::State::Failed)return false;
    Commissioning::confirm();
    service();
    return true;
}

void Machine::cancelCommissioning()
{
    Commissioning::cancel();
    if(uiState==UiState::CONFIG_COMMISSIONING)uiState=UiState::CONFIG_MACHINE_SETTINGS_MENU;
    service();
}

const Machine::CommissioningSnapshot& Machine::commissioning()
{
    return commissioningCurrent;
}

bool Machine::hallFeedbackEnabled()
{
    return settings.hallEnabled;
}

void Machine::setHallFeedbackEnabled(const bool enabled)
{
    if(settings.hallEnabled==enabled)return;
    settings.hallEnabled=enabled;
    // A prior seek/acquisition belongs to the old setting. Do not let it
    // survive a user toggle and influence a later commissioning run.
    HallSensor::cancel();
    saveSettings();
}

void Machine::toggleHallFeedback()
{
    setHallFeedbackEnabled(!settings.hallEnabled);
}

const Machine::WeightTargetsSnapshot& Machine::weightTargets()
{
    for(uint8_t preset=0;preset<3;preset++)targetsCurrent.grams[preset]=settings.weightTargetsCentiGrams[preset]/100.0f;
    return targetsCurrent;
}

bool Machine::setWeightTargetGrams(const uint8_t preset,const float grams)
{
    if(preset>=3||!isfinite(grams))return false;
    const float minimum=Config::MinWeightTargetCentiGrams/100.0f;
    const float maximum=Config::MaxWeightTargetCentiGrams/100.0f;
    if(grams<minimum||grams>maximum)return false;
    const int32_t centiGrams=lroundf(grams*100.0f);
    const uint16_t normalized=uint16_t(constrain(((centiGrams+5)/10)*10,int32_t(Config::MinWeightTargetCentiGrams),int32_t(Config::MaxWeightTargetCentiGrams)));
    settings.weightTargetsCentiGrams[preset]=normalized;
    targetsCurrent.grams[preset]=normalized/100.0f;
    return true;
}

bool Machine::resetWeightTargetToDefault(const uint8_t preset)
{
    if(preset>=3)return false;
    settings.weightTargetsCentiGrams[preset]=Config::DefaultWeightTargetsCentiGrams[preset];
    targetsCurrent.grams[preset]=settings.weightTargetsCentiGrams[preset]/100.0f;
    return true;
}

void Machine::saveWeightTargets()
{
    saveSettings();
}

const Machine::MotorSettingsSnapshot& Machine::motorSettings()
{
    motorCurrent.runCurrentMa=settings.runCurrentMa;
    motorCurrent.holdCurrentMa=settings.holdCurrentMa;
    motorCurrent.clockwise=settings.clockwise;
    return motorCurrent;
}

bool Machine::setRunCurrentMa(const uint16_t milliamps)
{
    const uint16_t normalized=uint16_t(constrain(milliamps,Config::MinRunCurrentMa,Config::MaxRunCurrentMa));
    const uint16_t normalizedHold=min(settings.holdCurrentMa,normalized);
    const bool changed=settings.runCurrentMa!=normalized||settings.holdCurrentMa!=normalizedHold;
    settings.runCurrentMa=normalized;
    settings.holdCurrentMa=normalizedHold;
    TmcDriver::applyCurrent(settings.runCurrentMa,settings.holdCurrentMa);
    service();
    return changed;
}

bool Machine::setHoldCurrentMa(const uint16_t milliamps)
{
    const uint16_t maximum=min(Config::MaxHoldCurrentMa,settings.runCurrentMa);
    const uint16_t normalized=uint16_t(constrain(milliamps,Config::MinHoldCurrentMa,maximum));
    const bool changed=settings.holdCurrentMa!=normalized;
    settings.holdCurrentMa=normalized;
    TmcDriver::applyCurrent(settings.runCurrentMa,settings.holdCurrentMa);
    service();
    return changed;
}

bool Machine::resetRunCurrentToDefault()
{
    return setRunCurrentMa(Config::DefaultRunCurrentMa);
}

bool Machine::resetHoldCurrentToDefault()
{
    return setHoldCurrentMa(Config::DefaultHoldCurrentMa);
}

void Machine::setClockwise(const bool clockwise)
{
    settings.clockwise=clockwise;
    service();
}

void Machine::toggleClockwise()
{
    setClockwise(!settings.clockwise);
}

void Machine::saveMotorSettings()
{
    saveSettings();
}

const Machine::ConnectivitySnapshot& Machine::connectivity()
{
    connectivityCurrent.bluetoothEnabled=settings.bluetoothEnabled;
    connectivityCurrent.pairingOpen=DiagnosticsTransport::pairingOpen();
    connectivityCurrent.clientConnected=DiagnosticsTransport::connected();
    connectivityCurrent.pairingSecondsRemaining=DiagnosticsTransport::pairingSecondsRemaining();
    return connectivityCurrent;
}

void Machine::setBluetoothEnabled(const bool enabled)
{
    if(settings.bluetoothEnabled==enabled)return;
    settings.bluetoothEnabled=enabled;
    if(!enabled)DiagnosticsTransport::closePairingWindow();
    DiagnosticsTransport::setEnabled(enabled);
    saveSettings();
    service();
}

void Machine::toggleBluetoothEnabled()
{
    setBluetoothEnabled(!settings.bluetoothEnabled);
}

bool Machine::openPairingWindow()
{
    if(!settings.bluetoothEnabled)return false;
    DiagnosticsTransport::openPairingWindow();
    service();
    return DiagnosticsTransport::pairingOpen();
}

void Machine::closePairingWindow()
{
    DiagnosticsTransport::closePairingWindow();
    service();
}

void Machine::togglePairingWindow()
{
    if(DiagnosticsTransport::pairingOpen())closePairingWindow();
    else openPairingWindow();
}

bool Machine::startSystemCheck()
{
    if(motionActive||auxiliaryMotionActive())return false;
    memset(&systemCheckCurrent,0,sizeof(systemCheckCurrent));
    // The System Check's final stage consumes the dedicated GP6 STOP edge.
    // Touch navigation does not otherwise set the legacy UiState, so make
    // this explicit for both display implementations.
    uiState=UiState::CONFIG_LOAD_CELL_DIAGNOSTICS;
    systemCheckCurrent.active=true;systemCheckCurrent.stage=SystemCheckStage::CheckingLoadCells;
    snprintf(systemCheckCurrent.message,sizeof(systemCheckCurrent.message),"Checking load cells");
    service();
    return true;
}

bool Machine::confirmSystemCheckStopButton()
{
    if(!systemCheckCurrent.active||systemCheckCurrent.stage!=SystemCheckStage::WaitingForStopButton)return false;
    systemCheckCurrent.stopButtonPassed=true;
    systemCheckCurrent.stage=SystemCheckStage::Passed;
    systemCheckCurrent.active=false;
    snprintf(systemCheckCurrent.message,sizeof(systemCheckCurrent.message),"All checks passed");
    service();
    return true;
}

void Machine::cancelSystemCheck()
{
    if(systemCheckCurrent.active)
    {
        cancelAuxiliaryMotion();HallSensor::cancel();disableMotor();
        systemCheckCurrent.active=false;systemCheckCurrent.stage=SystemCheckStage::Idle;
        snprintf(systemCheckCurrent.message,sizeof(systemCheckCurrent.message),"Check cancelled");
    }
    if(uiState==UiState::CONFIG_LOAD_CELL_DIAGNOSTICS)uiState=UiState::CONFIG_DIAGNOSTICS_MENU;
}

const Machine::SystemCheckSnapshot& Machine::systemCheck(){return systemCheckCurrent;}

bool Machine::prepareCountWinding(const uint16_t turns)
{
    if(productMode==ProductMode::LoadCellFault||turns<Config::MinTurns)return false;
    autoCountMode=true;
    selectedTurns=min<uint16_t>(turns,Config::FuhSafetyTurnCeiling);
    settings.autoTurns=selectedTurns;
    saveSettings();
    selectedCruiseMotorRPS=Config::FuhCruiseMotorRps;
    for(uint8_t i=0;i<Config::CruiseSpeedOptionCount;i++)
        if(Config::CruiseSpeedOptions[i]==Config::FuhCruiseMotorRps)selectedCruiseSpeedIndex=i;
    RunSupervisor::disarmWeightTarget();
    // Yarn is loaded before the Hall seek/tare sequence. This removes the
    // former stop between a successful empty-machine tare and winding.
    uiState=UiState::LOAD_YARN;
    service();
    return true;
}

bool Machine::prepareWeightWinding(const uint8_t preset)
{
    if(preset>=3||!weightCapabilityEnabled||!settings.loadCellCalibrationValid||productMode!=ProductMode::Fuhgeddabouditinator)return false;
    autoCountMode=false;
    selectedFuhProgram=FuhProgram(preset);
    activeFuhProgram=selectedFuhProgram;
    selectedTurns=Config::FuhSafetyTurnCeiling;
    selectedCruiseMotorRPS=Config::FuhCruiseMotorRps;
    for(uint8_t i=0;i<Config::CruiseSpeedOptionCount;i++)
        if(Config::CruiseSpeedOptions[i]==Config::FuhCruiseMotorRps)selectedCruiseSpeedIndex=i;
    if(!RunSupervisor::armWeightTarget(weightTargets().grams[preset]))return false;
    // Keep both winding modes on the same appliance workflow: load first,
    // then GO performs the hardware preparation and starts the run.
    uiState=UiState::LOAD_YARN;
    service();
    return true;
}

bool Machine::confirmYarnLoaded()
{
    if(uiState!=UiState::LOAD_YARN)return false;
    requestWindingStart();
    service();
    return true;
}

bool Machine::togglePauseResume()
{
    if(uiState!=UiState::WINDING||RunSupervisor::waitingForClearance())return false;
    if(pauseState==PauseState::RUNNING)requestPause();
    else if(pauseState==PauseState::PAUSED)requestResume();
    else return false;
    service();
    return true;
}

bool Machine::confirmSafetyClearance()
{
    if(uiState!=UiState::WINDING||!RunSupervisor::waitingForClearance())return false;
    const bool accepted=RunSupervisor::confirmClearance();
    if(accepted)HallSensor::resumeAfterClearance();
    service();
    return accepted;
}

bool Machine::retryWindingPreparation()
{
    if(uiState!=UiState::HALL_FAILED&&uiState!=UiState::TARE_FAILED)return false;
    requestWindingStart();
    service();
    return true;
}

bool Machine::exitWindingWorkflow()
{
    switch(uiState)
    {
        case UiState::COMPLETE:
            // Completion has already recorded telemetry and finalized the
            // run. Do not call the abort reset path after a successful run.
            disableMotor();
            uiState=weightCapabilityEnabled&&!autoCountMode?UiState::FUH_PROGRAM_SELECT:UiState::TURN_SELECT;
            break;
        case UiState::REPEAT_PROMPT:
        case UiState::AUTO_MODE_SELECT:
        case UiState::TURN_SELECT:
        case UiState::FUH_PROGRAM_SELECT:
            // The Mini12864 flow can advance from COMPLETE to a selector
            // between UI frames. Treat that as an already-completed exit,
            // not as a reason to strand the touch display on its DONE page.
            disableMotor();
            break;
        case UiState::HALL_SEEK:
        case UiState::HALL_FAILED:
        case UiState::TARING:
        case UiState::TARE_FAILED:
        case UiState::LOAD_YARN:
        case UiState::WINDING:
            // This deliberately follows the physical STOP behavior: cancel
            // preparation or coast-stop an active hub before returning.
            resetToYarnWeightSelection();
            break;
        default:
            return false;
    }
    service();
    return true;
}

const Machine::WindingSnapshot& Machine::winding()
{
    return windingCurrent;
}

bool Machine::startWeightDiagnosticsTare()
{
    if(!weightCapabilityEnabled)return false;
    uiState=UiState::CONFIG_WEIGHT_DIAGNOSTICS;
    ::startWeightDiagnosticsTare();
    service();
    return true;
}

void Machine::closeWeightDiagnostics()
{
    LoadCells::cancelTare();
    disableMotor();
    if(uiState==UiState::CONFIG_WEIGHT_DIAGNOSTICS)uiState=UiState::CONFIG_DIAGNOSTICS_MENU;
    service();
}
