#pragma once

#include "ModbusServer.h"

namespace Gateway
{

class GoodWeInverter : public ModbusServer
{
public:
    static constexpr uint16_t REG_START = 306;
    static constexpr uint16_t REG_END = 343;
    static constexpr uint16_t REG_COUNT = REG_END - REG_START + 1;
    static constexpr float VOLTAGE_SCALE = 10.0f;

    GoodWeInverter();

    static void buildRegisters(const MeterData &meter, uint16_t *registers);

protected:
    ModbusMessage handleRequest(const ModbusMessage &request) override;
};

} // namespace Gateway
