#pragma once

#include <stdint.h>

namespace Commissioning {
enum class State:uint8_t {Ready,Indexing,Taring,PromptLeg,CapturingLeg,PromptRemoveLegMass,RotatingToArm,
    TaringArm,PromptArm,CapturingArm,PromptRemoveArmMass,ReindexingForValidation,TaringValidation,PromptAllFirst,CapturingAllFirst,
    PromptRotate,Rotating,PromptAllSecond,CapturingAllSecond,Complete,Failed};
void begin();
void cancel();
void service();
void confirm();
State state();
uint8_t currentLeg();
uint8_t currentReferenceMass();
uint8_t currentArmPosition();
uint8_t samplesCollected();
uint8_t samplesRequired();
float referenceMassGrams(uint8_t reference);
float collarMassGrams(uint8_t collar);
float expectedTotalGrams();
float firstValidationGrams();
float secondValidationGrams();
float armCorrectionGain();
const char* message();
bool active();
}
