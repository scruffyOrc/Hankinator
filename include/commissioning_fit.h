#pragma once

#include <stdint.h>
#include <stddef.h>
#include <math.h>

namespace CommissioningFit {
constexpr uint8_t Channels=4;

inline bool solve(const float rawDeltas[Channels][Channels],const float targets[Channels],float coefficients[Channels])
{
    float matrix[Channels][Channels+1]{};
    for(uint8_t row=0;row<Channels;row++)
    {
        for(uint8_t column=0;column<Channels;column++)matrix[row][column]=rawDeltas[row][column];
        matrix[row][Channels]=targets[row];
    }

    for(uint8_t column=0;column<Channels;column++)
    {
        uint8_t pivot=column;
        for(uint8_t row=uint8_t(column+1);row<Channels;row++)
            if(fabsf(matrix[row][column])>fabsf(matrix[pivot][column]))pivot=row;
        if(fabsf(matrix[pivot][column])<1.0f)return false;
        if(pivot!=column)
            for(uint8_t value=column;value<=Channels;value++)
            {
                const float swap=matrix[column][value];matrix[column][value]=matrix[pivot][value];matrix[pivot][value]=swap;
            }
        const float divisor=matrix[column][column];
        for(uint8_t value=column;value<=Channels;value++)matrix[column][value]/=divisor;
        for(uint8_t row=0;row<Channels;row++)
        {
            if(row==column)continue;
            const float factor=matrix[row][column];
            for(uint8_t value=column;value<=Channels;value++)matrix[row][value]-=factor*matrix[column][value];
        }
    }
    for(uint8_t row=0;row<Channels;row++)
    {
        if(!isfinite(matrix[row][Channels]))return false;
        coefficients[row]=matrix[row][Channels];
    }
    return true;
}

inline bool solve(const int32_t rawDeltas[Channels][Channels],const float targets[Channels],float coefficients[Channels])
{
    float converted[Channels][Channels]{};
    for(uint8_t row=0;row<Channels;row++)
        for(uint8_t channel=0;channel<Channels;channel++)converted[row][channel]=float(rawDeltas[row][channel]);
    return solve(converted,targets,coefficients);
}

// Retained for comparison tests: a diagonal-only fit ignores cross-cell load
// transfer and is not used by the commissioning workflow.
template<size_t Samples>
inline bool solveDirectLegSlopes(const int32_t (&rawDeltas)[Samples][Channels],const float (&targets)[Samples],float coefficients[Channels])
{
    for(uint8_t channel=0;channel<Channels;channel++)
    {
        float numerator=0,denominator=0;
        for(uint8_t sample=0;sample<Samples;sample++)
        {
            const float delta=float(rawDeltas[sample][channel]);
            numerator+=delta*targets[sample];
            denominator+=delta*delta;
        }
        if(!isfinite(numerator)||!isfinite(denominator)||denominator<1.0f)return false;
        coefficients[channel]=numerator/denominator;
        if(!isfinite(coefficients[channel]))return false;
    }
    return true;
}

// Every direct-over-leg capture includes all four HX711 readings. Fit a single
// total-weight equation across the overdetermined sample set:
//   grams = c1*d1 + c2*d2 + c3*d3 + c4*d4
// This remains a leg-based calibration while preserving the measured frame
// and foot cross-coupling needed for a hub load to sum correctly.
template<size_t Samples>
inline bool solveDirectLegTotalModel(const int32_t (&rawDeltas)[Samples][Channels],const float (&targets)[Samples],float coefficients[Channels])
{
    float normal[Channels][Channels]{};
    float rhs[Channels]{};
    for(size_t sample=0;sample<Samples;sample++)
    {
        for(uint8_t row=0;row<Channels;row++)
        {
            const float rowValue=float(rawDeltas[sample][row]);
            rhs[row]+=rowValue*targets[sample];
            for(uint8_t column=0;column<Channels;column++)
                normal[row][column]+=rowValue*float(rawDeltas[sample][column]);
        }
    }
    return solve(normal,rhs,coefficients);
}

inline float predict(const int32_t rawDelta[Channels],const float coefficients[Channels])
{
    float grams=0;
    for(uint8_t channel=0;channel<Channels;channel++)grams+=rawDelta[channel]*coefficients[channel];
    return grams;
}
}
