#include "product_mode.h"

void Product::resolve(uint8_t detectedCount)
{
    detectedLoadCellCount=detectedCount;
    if(detectedCount==0)
    {
        productMode=ProductMode::Turninator;
        weightCapabilityEnabled=false;
    }
    else if(detectedCount>=Config::RequiredLoadCells)
    {
        productMode=ProductMode::Fuhgeddabouditinator;
        weightCapabilityEnabled=true;
    }
    else
    {
        productMode=ProductMode::LoadCellFault;
        weightCapabilityEnabled=false;
    }
}

const char* Product::name()
{
    switch(productMode)
    {
        case ProductMode::Turninator:return "Turninator";
        case ProductMode::Fuhgeddabouditinator:return "Fuhgeddabouditinator";
        case ProductMode::LoadCellFault:return "load_cell_fault";
    }
    return "unknown";
}
