#pragma once

#include <HardwareSerial.h>

#include "../ModbusClient/ModbusClient.h"
#include "../Gateway/ModbusTrace.h"
#include "ModbusServerRTU.h"

namespace Gateway
{

class ModbusServer : public ModbusTrace
{
public:
    ModbusServer(
        uint8_t serverId,
        int uartIndex,
        int rxPin,
        int txPin,
        uint32_t baudRate,
        uint16_t timeoutMs,
        const char *interfaceName);

    void begin(ModbusClient &client);

protected:
    virtual ModbusMessage handleRequest(const ModbusMessage &request) = 0;
    MeterData meterData() const;

private:
    uint8_t serverId_;
    int rxPin_;
    int txPin_;
    uint32_t baudRate_;
    const char *interfaceName_;
    HardwareSerial serial_;
    ModbusServerRTU server_;
    ModbusClient *client_;
};

} // namespace Gateway
