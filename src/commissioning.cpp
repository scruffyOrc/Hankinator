#include "commissioning.h"

#include "app_api.h"
#include "commissioning_fit.h"
#include "diagnostics_transport.h"
#include "hall_sensor.h"
#include "load_cells.h"
#include "telemetry.h"

namespace {
Commissioning::State current=Commissioning::State::Ready;
uint8_t leg=0,reference=0,samples=0;
uint32_t phaseStarted=0,lastSequence=0;
bool tareStarted=false;
int64_t sums[Config::LoadCellChannelCount]{};
int32_t tareOffsets[Config::LoadCellChannelCount]{};
int32_t directLegDeltas[Config::CommissioningDirectSampleCount][Config::LoadCellChannelCount]{};
int32_t captureDeltas[Config::LoadCellChannelCount]{};
float fitted[Config::LoadCellChannelCount]{};
float firstValidation=NAN,secondValidation=NAN;
float centerHubMeasured[Config::CommissioningReferenceMassCount]{};
float calculatedHubGain=1.0f;
char statusMessage[48]="Click to commission";

const uint8_t requiredMask=uint8_t((1u<<Config::LoadCellChannelCount)-1u);

bool healthy(const LoadCellSnapshot& value)
{
    return (value.healthyMask&requiredMask)==requiredMask;
}

void setMessage(const char* text)
{
    snprintf(statusMessage,sizeof(statusMessage),"%s",text);
}

void emitDiagnostic(const char* line)
{
    Serial.print(line);
    // Commissioning is stopped, low-rate service work. Sending a line to a
    // connected Bluetooth terminal here cannot interfere with STEP timing.
    DiagnosticsTransport::diagnosticLine(line);
}

void resetCapture()
{
    memset(sums,0,sizeof(sums));samples=0;lastSequence=LoadCells::snapshot().sequence;phaseStarted=millis();
}

void startCapture(Commissioning::State next)
{
    current=next;resetCapture();setMessage("Settling load...");
}

bool captureComplete()
{
    if(millis()-phaseStarted<Config::CommissioningLoadSettleMs)return false;
    const LoadCellSnapshot value=LoadCells::snapshot();
    if(!healthy(value))
    {
        current=Commissioning::State::Failed;setMessage("Load cell data lost");return false;
    }
    if(value.sequence==lastSequence)return false;
    lastSequence=value.sequence;
    for(uint8_t channel=0;channel<Config::LoadCellChannelCount;channel++)sums[channel]+=value.raw[channel];
    if(samples<255)samples++;
    if(samples<Config::CommissioningSamples)return false;
    for(uint8_t channel=0;channel<Config::LoadCellChannelCount;channel++)
        captureDeltas[channel]=int32_t(sums[channel]/samples)-tareOffsets[channel];
    return true;
}

bool coefficientsPlausible()
{
    for(uint8_t channel=0;channel<Config::LoadCellChannelCount;channel++)
        if(!isfinite(fitted[channel])||fabsf(fitted[channel])<Config::CommissioningMinimumCoefficient||fabsf(fitted[channel])>Config::CommissioningMaximumCoefficient)return false;
    return true;
}

bool validationPasses(float measured)
{
    const float expected=Commissioning::expectedTotalGrams();
    return isfinite(measured)&&fabsf(measured-expected)<=max(Config::CommissioningValidationMinimumGrams,expected*Config::CommissioningValidationFraction);
}

bool fitCenterHubGain()
{
    // A least-squares fit through zero is appropriate because the empty-hub
    // tare removes offset. The direct-leg model supplies the shape; this is a
    // single scalar correction for its center-hub gain.
    float numerator=0.0f,denominator=0.0f;
    for(uint8_t index=0;index<Config::CommissioningReferenceMassCount;index++)
    {
        const float actual=Commissioning::referenceMassGrams(index);
        const float measured=centerHubMeasured[index];
        if(!isfinite(measured)||fabsf(measured)<Config::CommissioningValidationMinimumGrams)return false;
        numerator+=actual*measured;
        denominator+=measured*measured;
    }
    if(denominator<=0.0f)return false;
    calculatedHubGain=numerator/denominator;
    return isfinite(calculatedHubGain)&&calculatedHubGain>=Config::CommissioningMinimumHubGain&&calculatedHubGain<=Config::CommissioningMaximumHubGain;
}

void logCapture(const char* name,const int32_t delta[Config::LoadCellChannelCount],float target,float grams)
{
    char line[192]{};
    const int32_t targetCentiGrams=lroundf(target*100.0f);
    size_t used=0;
    if(isnan(grams))
        used=snprintf(line,sizeof(line),"COMMISSION_%s,target_cg,%ld,measured_cg,NA",name,long(targetCentiGrams));
    else
        used=snprintf(line,sizeof(line),"COMMISSION_%s,target_cg,%ld,measured_cg,%ld",name,long(targetCentiGrams),long(lroundf(grams*100.0f)));
    for(uint8_t channel=0;channel<Config::LoadCellChannelCount&&used<sizeof(line);channel++)
        used+=snprintf(line+used,sizeof(line)-used,",d%u,%ld",unsigned(channel+1),long(delta[channel]));
    if(used<sizeof(line)-1){line[used++]='\n';line[used]='\0';}
    else {line[sizeof(line)-2]='\n';line[sizeof(line)-1]='\0';}
    emitDiagnostic(line);
}

const char* legName(uint8_t index)
{
    static const char* names[]={"FL","FR","RL","RR"};
    return index<Config::LoadCellChannelCount?names[index]:"?";
}

void setLegPrompt()
{
    const uint16_t centigrams=Config::CommissioningReferenceMassCentiGrams[reference];
    snprintf(statusMessage,sizeof(statusMessage),"%u.%02ug over %s leg",centigrams/100,centigrams%100,legName(leg));
}

bool fitDirectLegCalibration()
{
    float targets[Config::CommissioningDirectSampleCount]{};
    for(uint8_t mass=0;mass<Config::CommissioningReferenceMassCount;mass++)
        for(uint8_t position=0;position<Config::LoadCellChannelCount;position++)
            targets[mass*Config::LoadCellChannelCount+position]=Commissioning::referenceMassGrams(mass);
    return CommissioningFit::solveDirectLegTotalModel(directLegDeltas,targets,fitted)&&coefficientsPlausible();
}
}

void Commissioning::begin()
{
    const LoadCellSnapshot value=LoadCells::snapshot();
    if(!settings.hallEnabled){current=State::Failed;setMessage("Enable Hall feedback");return;}
    if(!healthy(value)){current=State::Failed;setMessage("All load cells needed");return;}
    leg=0;reference=0;samples=0;firstValidation=NAN;secondValidation=NAN;calculatedHubGain=1.0f;
    memset(directLegDeltas,0,sizeof(directLegDeltas));memset(centerHubMeasured,0,sizeof(centerHubMeasured));
    enableMotor();HallSensor::startCommissioningIndex();current=State::Indexing;setMessage("Finding double magnet");
    char line[160]{};
    size_t used=snprintf(line,sizeof(line),"COMMISSION_START");
    for(uint8_t sample=0;sample<Config::CommissioningReferenceMassCount&&used<sizeof(line);sample++)
        used+=snprintf(line+used,sizeof(line)-used,",reference_%u_cg,%u",unsigned(sample+1),unsigned(Config::CommissioningReferenceMassCentiGrams[sample]));
    snprintf(line+used,sizeof(line)-used,",collar_total_cg,%ld\n",long(lroundf(expectedTotalGrams()*100.0f)));
    emitDiagnostic(line);
}

void Commissioning::cancel()
{
    LoadCells::cancelTare();cancelAuxiliaryMotion();HallSensor::cancel();motionActive=false;requestedStepRateHz=0;disableMotor();
    current=State::Ready;leg=0;reference=0;samples=0;setMessage("Click to commission");
}

void Commissioning::service()
{
    switch(current)
    {
        case State::Indexing:
            if(HallSensor::commissioningIndexFailed()){current=State::Failed;setMessage("Double magnet not found");}
            else if(HallSensor::commissioningIndexFinished())
            {
                phaseStarted=millis();tareStarted=false;current=State::Taring;setMessage("Hub indexed; settling");
            }
            break;

        case State::Taring:
            if(!tareStarted)
            {
                if(millis()-phaseStarted<Config::CommissioningMotorSettleMs)break;
                LoadCells::startTare();tareStarted=true;setMessage("Taring empty hub");
            }
            else if(LoadCells::updateTare()==LoadCellTareStatus::Complete)
            {
                const LoadCellTareResult tare=LoadCells::tareResult();
                for(uint8_t channel=0;channel<Config::LoadCellChannelCount;channel++)tareOffsets[channel]=tare.offset[channel];
                reference=0;leg=0;current=State::PromptLeg;setLegPrompt();
                emitDiagnostic("COMMISSION_TARE_COMPLETE\n");
            }
            else if(LoadCells::tareStatus()==LoadCellTareStatus::Failed)
            {
                current=State::Failed;setMessage("Empty tare unstable");
            }
            break;

        case State::CapturingLeg:
        {
            if(!captureComplete())break;
            if(abs(captureDeltas[leg])<Config::CommissioningMinimumFirstChangeCounts)
            {
                current=State::Failed;setMessage("Weight not seen on leg");break;
            }
            const uint8_t sample=reference*Config::LoadCellChannelCount+leg;
            for(uint8_t channel=0;channel<Config::LoadCellChannelCount;channel++)
                directLegDeltas[sample][channel]=captureDeltas[channel];
            char event[16]{};snprintf(event,sizeof(event),"LEG_%s",legName(leg));
            logCapture(event,captureDeltas,referenceMassGrams(reference),NAN);
            if(++leg<Config::LoadCellChannelCount)
            {
                current=State::PromptLeg;setLegPrompt();
            }
            else if(++reference<Config::CommissioningReferenceMassCount)
            {
                leg=0;current=State::PromptLeg;setLegPrompt();
            }
            else
            {
                if(!fitDirectLegCalibration())
                {
                    current=State::Failed;setMessage("Calibration fit invalid");break;
                }
                reference=0;current=State::PromptHubReference;setMessage("Place 79.43g at hub center");
            }
            break;
        }

        case State::CapturingHubReference:
        {
            if(!captureComplete())break;
            const float measured=CommissioningFit::predict(captureDeltas,fitted);
            if(!isfinite(measured)||fabsf(measured)<Config::CommissioningValidationMinimumGrams)
            {
                current=State::Failed;setMessage("Hub weight not detected");break;
            }
            centerHubMeasured[reference]=measured;
            char event[24]{};snprintf(event,sizeof(event),"HUB_REF_%u",unsigned(reference+1));
            logCapture(event,captureDeltas,referenceMassGrams(reference),measured);
            if(++reference<Config::CommissioningReferenceMassCount)
            {
                char prompt[48]{};const uint16_t cg=Config::CommissioningReferenceMassCentiGrams[reference];
                snprintf(prompt,sizeof(prompt),"Place %u.%02ug at hub center",cg/100,cg%100);setMessage(prompt);current=State::PromptHubReference;
            }
            else if(!fitCenterHubGain())
            {
                current=State::Failed;setMessage("Hub gain fit invalid");
            }
            else
            {
                char line[96]{};snprintf(line,sizeof(line),"COMMISSION_HUB_GAIN,gain,%.7f\n",double(calculatedHubGain));emitDiagnostic(line);
                current=State::PromptAllFirst;setMessage("Place all collars; click");
            }
            break;
        }

        case State::CapturingAllFirst:
            if(!captureComplete())break;
            firstValidation=CommissioningFit::predict(captureDeltas,fitted)*calculatedHubGain;logCapture("VALIDATE_0",captureDeltas,expectedTotalGrams(),firstValidation);
            if(!validationPasses(firstValidation)){current=State::Failed;setMessage("First validation failed");break;}
            current=State::PromptRotate;setMessage("Remove weights; click rotate");
            break;

        case State::Rotating:
            if(!auxiliaryMotionActive()&&consumeAuxiliaryMotionComplete())
            {
                current=State::PromptAllSecond;setMessage("All weights fixed positions");
                emitDiagnostic("COMMISSION_ROTATE_180_COMPLETE\n");
            }
            break;

        case State::CapturingAllSecond:
        {
            if(!captureComplete())break;
            secondValidation=CommissioningFit::predict(captureDeltas,fitted)*calculatedHubGain;logCapture("VALIDATE_180",captureDeltas,expectedTotalGrams(),secondValidation);
            if(!validationPasses(secondValidation)){current=State::Failed;setMessage("180 validation failed");break;}
            for(uint8_t channel=0;channel<Config::LoadCellChannelCount;channel++)settings.loadCellGramsPerCount[channel]=fitted[channel];
            settings.loadCellCalibrationValid=1;settings.loadCellHubGain=calculatedHubGain;LoadCells::setCalibration(fitted);saveSettings();
            current=State::Complete;setMessage("Calibration saved");disableMotor();HallSensor::cancel();
            char line[96]{};
            snprintf(line,sizeof(line),"COMMISSION_COMPLETE,hub_gain,%.7f,first_cg,%ld,second_cg,%ld\n",double(calculatedHubGain),long(lroundf(firstValidation*100.0f)),long(lroundf(secondValidation*100.0f)));
            emitDiagnostic(line);
            break;
        }

        default:break;
    }
}

void Commissioning::confirm()
{
    switch(current)
    {
        case State::Ready:begin();break;
        case State::PromptLeg:startCapture(State::CapturingLeg);break;
        case State::PromptHubReference:startCapture(State::CapturingHubReference);break;
        case State::PromptAllFirst:startCapture(State::CapturingAllFirst);break;
        case State::PromptRotate:
            if(startAuxiliaryHubMotion(true,Config::StepsPerHubRev/2,Config::CommissioningIndexMotorRps)){current=State::Rotating;setMessage("Rotating 180 deg");}
            else {current=State::Failed;setMessage("Hub rotation failed");}
            break;
        case State::PromptAllSecond:startCapture(State::CapturingAllSecond);break;
        default:break;
    }
}

Commissioning::State Commissioning::state(){return current;}
uint8_t Commissioning::currentLeg(){return leg;}
uint8_t Commissioning::currentReferenceMass(){return reference;}
uint8_t Commissioning::samplesCollected(){return samples;}
uint8_t Commissioning::samplesRequired(){return Config::CommissioningSamples;}
float Commissioning::referenceMassGrams(uint8_t sample){return sample<Config::CommissioningReferenceMassCount?Config::CommissioningReferenceMassCentiGrams[sample]/100.0f:NAN;}
float Commissioning::collarMassGrams(uint8_t collar){return collar<Config::LoadCellChannelCount?settings.commissioningCollarCentiGrams[collar]/100.0f:NAN;}
float Commissioning::expectedTotalGrams(){float total=0;for(uint8_t collar=0;collar<Config::LoadCellChannelCount;collar++)total+=collarMassGrams(collar);return total;}
float Commissioning::firstValidationGrams(){return firstValidation;}
float Commissioning::secondValidationGrams(){return secondValidation;}
float Commissioning::hubGain(){return calculatedHubGain;}
const char* Commissioning::message(){return statusMessage;}
bool Commissioning::active(){return current!=State::Ready&&current!=State::Complete&&current!=State::Failed;}
