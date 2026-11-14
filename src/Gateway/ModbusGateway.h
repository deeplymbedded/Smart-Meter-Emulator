#pragma once

#include "../ModbusClient/ModbusClient.h"
#include "../ModbusServer/ModbusServer.h"

namespace Gateway
{

class ModbusGateway
{
public:
    ModbusGateway(ModbusClient &client, ModbusServer &server);

    bool begin();
    MeterData meterData() const;
    void setTrafficCallback(TrafficCallback callback);

private:
    ModbusClient &client_;
    ModbusServer &server_;
};

} // namespace Gateway
