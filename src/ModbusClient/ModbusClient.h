#pragma once

#include "MeterData.h"
#include "../Gateway/ModbusTrace.h"

namespace Gateway
{

class ModbusClient : public ModbusTrace
{
public:
    virtual bool begin() = 0;
    virtual MeterData meterData() const = 0;
};

} // namespace Gateway
