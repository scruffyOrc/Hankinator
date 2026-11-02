#include "commissioning_fit.h"
#include <cassert>
#include <cmath>
#include <cstdio>

int main()
{
    // Four independent fixture placements generated from a known system model.
    constexpr float expected[]={0.0011f,-0.0022f,0.0033f,-0.0044f};
    const float deltas[4][4]={{100000,2000,1000,-1000},{500,80000,2000,-1000},{1000,4000,60000,2000},{-3000,1000,4000,50000}};
    float targets[4]{};
    for(uint8_t row=0;row<4;row++)for(uint8_t channel=0;channel<4;channel++)targets[row]+=deltas[row][channel]*expected[channel];
    float result[4]{};assert(CommissioningFit::solve(deltas,targets,result));
    for(uint8_t channel=0;channel<4;channel++)assert(fabsf(result[channel]-expected[channel])<0.000001f);
    const int32_t validation[]={30000,20000,25000,15000};
    float wanted=0;for(uint8_t channel=0;channel<4;channel++)wanted+=validation[channel]*expected[channel];
    assert(fabsf(CommissioningFit::predict(validation,result)-wanted)<0.0001f);
    const float singular[4][4]={{1,0,0,0},{2,0,0,0},{3,0,0,0},{4,0,0,0}};
    assert(!CommissioningFit::solve(singular,targets,result));

    constexpr float directExpected[]={0.0011f,-0.0022f,0.0033f,-0.0044f};
    constexpr float directTargets[]={79.43f,111.59f,333.53f};
    int32_t directDeltas[3][4]{};
    for(uint8_t sample=0;sample<3;sample++)
        for(uint8_t channel=0;channel<4;channel++)
            directDeltas[sample][channel]=lroundf(directTargets[sample]/directExpected[channel]);
    assert(CommissioningFit::solveDirectLegSlopes(directDeltas,directTargets,result));
    for(uint8_t channel=0;channel<4;channel++)assert(fabsf(result[channel]-directExpected[channel])<0.000001f);

    // Twelve direct-leg readings include intentional cross-cell responses.
    // The total model must recover the weight coefficients from every raw
    // channel, not only the channel directly under each reference mass.
    constexpr float totalExpected[]={-0.0023f,0.0024f,0.00235f,-0.0027f};
    constexpr float fixtureMasses[]={79.43f,111.59f,333.53f};
    int32_t totalDeltas[12][4]{};
    float totalTargets[12]{};
    constexpr int32_t coupling[4][4]={{42000,-3500,1800,-1200},{-2800,40000,-2100,1600},{1700,-2600,39000,-2400},{-1400,1900,-2200,41000}};
    for(uint8_t sample=0;sample<12;sample++)
    {
        const uint8_t leg=sample%4;
        for(uint8_t channel=0;channel<4;channel++)
            totalDeltas[sample][channel]=lroundf(fixtureMasses[sample/4]*coupling[leg][channel]/100.0f);
    }
    // Generate exact target data from a known total-weight equation so the
    // least-squares fit is independently exercised with cross-coupling.
    for(uint8_t sample=0;sample<12;sample++)
    {
        float model=0;
        for(uint8_t channel=0;channel<4;channel++)model+=totalDeltas[sample][channel]*totalExpected[channel];
        totalTargets[sample]=model;
    }
    assert(CommissioningFit::solveDirectLegTotalModel(totalDeltas,totalTargets,result));
    for(uint8_t channel=0;channel<4;channel++)assert(fabsf(result[channel]-totalExpected[channel])<0.000001f);
    std::puts("Commissioning fit solve, direct slopes, total model, prediction, and singular rejection passed");
}
