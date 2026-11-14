#pragma once

#include <Arduino.h>
#include <atomic>

#include "ModbusMessage.h"
#include "../ModbusClient/MeterData.h"

namespace Gateway
{

class GatewayLogger
{
public:
    GatewayLogger();

    void recordTraffic(
        const char *direction,
        const char *interfaceName,
        ModbusMessage message);
    String trafficJson() const;
    void clearTraffic();
    void processConsole(const MeterData &meter);

    bool debugEnabled() const;
    void setDebugEnabled(bool enabled);
    bool trafficEnabled() const;
    void setTrafficEnabled(bool enabled);

private:
    struct TrafficEntry
    {
        uint32_t timestamp;
        char direction[4];
        char interfaceName[16];
        uint16_t length;
        uint8_t data[256];
    };

    static constexpr uint8_t TRAFFIC_LOG_SIZE = 64;

    std::atomic<bool> trafficEnabled_;

    TrafficEntry trafficLog_[TRAFFIC_LOG_SIZE];
    uint8_t trafficLogNext_;
    uint8_t trafficLogCount_;
    mutable portMUX_TYPE trafficLogMux_;
};

} // namespace Gateway
