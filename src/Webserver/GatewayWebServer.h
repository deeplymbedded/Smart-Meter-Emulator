#pragma once

#include <WebServer.h>

#include "EthernetConnection.h"
#include "../Gateway/GatewayLogger.h"
#include "../Gateway/ModbusGateway.h"

namespace Gateway
{

class GatewayWebServer
{
public:
    GatewayWebServer(ModbusGateway &modbusGateway, GatewayLogger &logger);

    void begin();
    void handle();

private:
    void configureRoutes();
    String dataJson() const;

    WebServer server_;
    EthernetConnection ethernet_;
    ModbusGateway &modbusGateway_;
    GatewayLogger &logger_;
    bool serverReady_;
    bool otaRestartPending_;
};

} // namespace Gateway
