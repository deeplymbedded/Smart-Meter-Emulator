#include "ModbusGateway.h"

namespace Gateway
{

ModbusGateway::ModbusGateway(ModbusClient &client, ModbusServer &server)
    : client_(client),
      server_(server)
{
}

bool ModbusGateway::begin()
{
    server_.begin(client_);
    return client_.begin();
}

MeterData ModbusGateway::meterData() const
{
    return client_.meterData();
}

void ModbusGateway::setTrafficCallback(TrafficCallback callback)
{
    server_.setTrafficCallback(callback);
    client_.setTrafficCallback(callback);
}

} // namespace Gateway
