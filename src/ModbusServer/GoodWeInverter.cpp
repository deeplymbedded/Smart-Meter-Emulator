#include "GoodWeInverter.h"

#include <math.h>
#include <string.h>

namespace Gateway
{
namespace
{
constexpr uint8_t GOODWE_ID = 3;
constexpr int GOODWE_UART_INDEX = 1;
constexpr int GOODWE_RX_PIN = 6;
constexpr int GOODWE_TX_PIN = 7;
constexpr uint32_t GOODWE_BAUD_RATE = 9600;
constexpr uint16_t GOODWE_SERVER_TIMEOUT_MS = 150;

constexpr uint16_t GW_UA = 306;
constexpr uint16_t GW_UB = 307;
constexpr uint16_t GW_UC = 308;
constexpr uint16_t GW_IA_HIGH = 309;
constexpr uint16_t GW_IA_LOW = 310;
constexpr uint16_t GW_IB_HIGH = 311;
constexpr uint16_t GW_IB_LOW = 312;
constexpr uint16_t GW_IC_HIGH = 313;
constexpr uint16_t GW_IC_LOW = 314;
constexpr uint16_t GW_PA_HIGH = 315;
constexpr uint16_t GW_PA_LOW = 316;
constexpr uint16_t GW_PB_HIGH = 317;
constexpr uint16_t GW_PB_LOW = 318;
constexpr uint16_t GW_PC_HIGH = 319;
constexpr uint16_t GW_PC_LOW = 320;
constexpr uint16_t GW_PT_HIGH = 321;
constexpr uint16_t GW_PT_LOW = 322;

void writeSignedPowerPair(
    uint16_t *registers,
    uint16_t signRegister,
    uint16_t valueRegister,
    int32_t value)
{
    registers[signRegister - GoodWeInverter::REG_START] =
        value < 0 ? 0xFFFFu : 0x0000u;
    registers[valueRegister - GoodWeInverter::REG_START] =
        static_cast<uint16_t>(static_cast<int16_t>(value));
}
} // namespace

GoodWeInverter::GoodWeInverter()
    : ModbusServer(
          GOODWE_ID,
          GOODWE_UART_INDEX,
          GOODWE_RX_PIN,
          GOODWE_TX_PIN,
          GOODWE_BAUD_RATE,
          GOODWE_SERVER_TIMEOUT_MS,
          "GoodWe")
{
}

void GoodWeInverter::buildRegisters(
    const MeterData &meter,
    uint16_t *registers)
{
    memset(registers, 0, sizeof(uint16_t) * REG_COUNT);

    registers[GW_UA - REG_START] =
        static_cast<uint16_t>(lroundf(meter.ua / VOLTAGE_SCALE));
    registers[GW_UB - REG_START] =
        static_cast<uint16_t>(lroundf(meter.ub / VOLTAGE_SCALE));
    registers[GW_UC - REG_START] =
        static_cast<uint16_t>(lroundf(meter.uc / VOLTAGE_SCALE));

    registers[GW_IA_HIGH - REG_START] = 0;
    registers[GW_IA_LOW - REG_START] =
        static_cast<uint16_t>(-static_cast<int32_t>(meter.ia));
    registers[GW_IB_HIGH - REG_START] = 0;
    registers[GW_IB_LOW - REG_START] =
        static_cast<uint16_t>(-static_cast<int32_t>(meter.ib));
    registers[GW_IC_HIGH - REG_START] = 0;
    registers[GW_IC_LOW - REG_START] =
        static_cast<uint16_t>(-static_cast<int32_t>(meter.ic));

    const int32_t pa = -meter.pa;
    const int32_t pb = -meter.pb;
    const int32_t pc = -meter.pc;
    const int32_t pTotal = pa + pb + pc;

    writeSignedPowerPair(registers, GW_PA_HIGH, GW_PA_LOW, pa);
    writeSignedPowerPair(registers, GW_PB_HIGH, GW_PB_LOW, pb);
    writeSignedPowerPair(registers, GW_PC_HIGH, GW_PC_LOW, pc);
    writeSignedPowerPair(registers, GW_PT_HIGH, GW_PT_LOW, pTotal);
}

ModbusMessage GoodWeInverter::handleRequest(const ModbusMessage &request)
{
    uint16_t startRegister = 0;
    uint16_t quantity = 0;
    request.get(2, startRegister, quantity);

    ModbusMessage response;
    if (quantity == 0)
    {
        response.setError(
            request.getServerID(),
            request.getFunctionCode(),
            Modbus::ILLEGAL_DATA_VALUE);
        return response;
    }

    const uint32_t requestedEnd =
        static_cast<uint32_t>(startRegister) + quantity - 1;
    if (startRegister < REG_START || requestedEnd > REG_END)
    {
        response.setError(
            request.getServerID(),
            request.getFunctionCode(),
            Modbus::ILLEGAL_DATA_ADDRESS);
        return response;
    }

    const MeterData snapshot = meterData();
    if (!snapshot.valid)
    {
        response.setError(
            request.getServerID(),
            request.getFunctionCode(),
            Modbus::SERVER_DEVICE_FAILURE);
        return response;
    }

    uint16_t registers[REG_COUNT];
    buildRegisters(snapshot, registers);

    response.add(
        request.getServerID(),
        request.getFunctionCode(),
        static_cast<uint8_t>(quantity * sizeof(uint16_t)));
    const uint16_t offset = startRegister - REG_START;
    for (uint16_t index = 0; index < quantity; ++index)
    {
        response.add(registers[offset + index]);
    }

    traceMessage("TX", "GoodWe", response);
    return response;
}

} // namespace Gateway
