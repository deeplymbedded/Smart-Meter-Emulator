#include "ModbusServer.h"

#include "RTUutils.h"

namespace Gateway
{

ModbusServer::ModbusServer(
    uint8_t serverId,
    int uartIndex,
    int rxPin,
    int txPin,
    uint32_t baudRate,
    uint16_t timeoutMs,
    const char *interfaceName)
    : serverId_(serverId),
      rxPin_(rxPin),
      txPin_(txPin),
      baudRate_(baudRate),
      interfaceName_(interfaceName),
      serial_(uartIndex),
      server_(timeoutMs),
      client_(nullptr)
{
}

void ModbusServer::begin(ModbusClient &client)
{
    client_ = &client;

    RTUutils::prepareHardwareSerial(serial_);
    serial_.begin(baudRate_, SERIAL_8N1, rxPin_, txPin_);

    server_.registerWorker(
        serverId_,
        Modbus::READ_HOLD_REGISTER,
        [this](ModbusMessage request)
        {
            return handleRequest(request);
        });
    server_.registerSniffer(
        [this](ModbusMessage request)
        {
            traceMessage("RX", interfaceName_, request);
        });
    server_.begin(serial_);

}

MeterData ModbusServer::meterData() const
{
    return client_->meterData();
}

} // namespace Gateway
