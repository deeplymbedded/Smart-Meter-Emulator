#include <Arduino.h>

#include "BuildInfo.h"
#include "DebugOutput.h"
#include "GatewayLogger.h"
#include "ModbusGateway.h"
#include "ModbusServer/GoodWeInverter.h"
#include "ModbusClient/HuaweiDTSU666HW.h"
#include "Webserver/GatewayWebServer.h"

namespace
{
Gateway::GatewayLogger logger;
Gateway::HuaweiDTSU666HW huaweiClient;
Gateway::GoodWeInverter goodWeServer;
Gateway::ModbusGateway modbusGateway(huaweiClient, goodWeServer);
Gateway::GatewayWebServer webServer(modbusGateway, logger);
}

void setup()
{
    Serial.begin(115200);
    delay(500);
    GatewayDebug::printf("Software build: %s\n", Gateway::SOFTWARE_BUILD_TIMESTAMP);
    GatewayDebug::println("Starting Smart Meter Emulator.");
    GatewayDebug::println("GoodWe ID: 3, Huawei DTSU666-HW ID: 11");

    modbusGateway.setTrafficCallback(
        [](const char *direction, const char *interfaceName, ModbusMessage message)
        {
            logger.recordTraffic(direction, interfaceName, message);
        });
    if (!modbusGateway.begin())
    {
        GatewayDebug::println("ModbusGateway failed to initialize.");
    }

    webServer.begin();
    GatewayDebug::println("Gateway started.");
}

void loop()
{
    logger.processConsole(modbusGateway.meterData());
    webServer.handle();
    delay(1);
}
