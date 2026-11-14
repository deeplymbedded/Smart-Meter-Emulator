#include "GatewayLogger.h"

#include <string.h>

#include "DebugOutput.h"
#include "RTUutils.h"

namespace Gateway
{
namespace
{
constexpr size_t DEBUG_COMMAND_BUFFER_SIZE = 32;
}

GatewayLogger::GatewayLogger()
    : trafficEnabled_(false),
      trafficLog_{},
      trafficLogNext_(0),
      trafficLogCount_(0),
      trafficLogMux_(portMUX_INITIALIZER_UNLOCKED)
{
}

void GatewayLogger::recordTraffic(
    const char *direction,
    const char *interfaceName,
    ModbusMessage message)
{
    if (!trafficEnabled_.load())
    {
        return;
    }

    RTUutils::addCRC(message);
    const uint8_t *buffer = message.data();
    const size_t length = message.size();

    portENTER_CRITICAL(&trafficLogMux_);
    TrafficEntry &entry = trafficLog_[trafficLogNext_];
    entry.timestamp = millis();
    strncpy(entry.direction, direction, sizeof(entry.direction) - 1);
    entry.direction[sizeof(entry.direction) - 1] = '\0';
    strncpy(entry.interfaceName, interfaceName, sizeof(entry.interfaceName) - 1);
    entry.interfaceName[sizeof(entry.interfaceName) - 1] = '\0';
    entry.length = length > sizeof(entry.data) ? sizeof(entry.data) : length;
    memcpy(entry.data, buffer, entry.length);

    trafficLogNext_ = (trafficLogNext_ + 1) % TRAFFIC_LOG_SIZE;
    if (trafficLogCount_ < TRAFFIC_LOG_SIZE)
    {
        ++trafficLogCount_;
    }
    portEXIT_CRITICAL(&trafficLogMux_);
}

String GatewayLogger::trafficJson() const
{
    String json = "[";
    uint8_t next;
    uint8_t count;
    portENTER_CRITICAL(&trafficLogMux_);
    next = trafficLogNext_;
    count = trafficLogCount_;
    portEXIT_CRITICAL(&trafficLogMux_);

    const uint8_t firstIndex =
        (next + TRAFFIC_LOG_SIZE - count) % TRAFFIC_LOG_SIZE;
    for (uint8_t item = 0; item < count; ++item)
    {
        TrafficEntry entry;
        portENTER_CRITICAL(&trafficLogMux_);
        entry = trafficLog_[(firstIndex + item) % TRAFFIC_LOG_SIZE];
        portEXIT_CRITICAL(&trafficLogMux_);

        if (item > 0)
        {
            json += ',';
        }

        json += "{\"age_ms\":";
        json += millis() - entry.timestamp;
        json += ",\"timestamp_ms\":";
        json += entry.timestamp;
        json += ",\"interface\":\"";
        json += entry.interfaceName;
        json += "\",\"direction\":\"";
        json += entry.direction;
        json += "\",\"hex\":\"";

        for (uint16_t byteIndex = 0; byteIndex < entry.length; ++byteIndex)
        {
            if (byteIndex > 0)
            {
                json += ' ';
            }
            if (entry.data[byteIndex] < 0x10)
            {
                json += '0';
            }
            json += String(entry.data[byteIndex], HEX);
        }
        json += "\"}";
    }
    json += ']';
    return json;
}

void GatewayLogger::clearTraffic()
{
    portENTER_CRITICAL(&trafficLogMux_);
    trafficLogNext_ = 0;
    trafficLogCount_ = 0;
    portEXIT_CRITICAL(&trafficLogMux_);
}

void GatewayLogger::processConsole(const MeterData &meter)
{
    static char command[DEBUG_COMMAND_BUFFER_SIZE];
    static size_t length = 0;

    while (Serial.available())
    {
        const char character = static_cast<char>(Serial.read());
        if (character != '\r' && character != '\n')
        {
            if (length < sizeof(command) - 1)
            {
                command[length++] = character;
            }
            else
            {
                length = 0;
            }
            continue;
        }

        if (length == 0)
        {
            continue;
        }
        command[length] = '\0';

        if (strcmp(command, "traffic on") == 0 ||
            strcmp(command, "monitor on") == 0)
        {
            setTrafficEnabled(true);
            GatewayDebug::println("Traffic monitor: ON");
        }
        else if (strcmp(command, "traffic off") == 0 ||
                 strcmp(command, "monitor off") == 0)
        {
            setTrafficEnabled(false);
            GatewayDebug::println("Traffic monitor: OFF");
        }
        else if (strcmp(command, "debug on") == 0)
        {
            setDebugEnabled(true);
            GatewayDebug::println("Debug: ON");
        }
        else if (strcmp(command, "debug off") == 0)
        {
            setDebugEnabled(false);
            GatewayDebug::println("Debug: OFF");
        }
        else if (strcmp(command, "status") == 0)
        {
            GatewayDebug::printf(
                "Debug=%s Traffic=%s ModbusClient=%s\n",
                debugEnabled() ? "ON" : "OFF",
                trafficEnabled() ? "ON" : "OFF",
                meter.valid ? "VALID" : "INVALID");
        }
        else if (strcmp(command, "help") == 0)
        {
            GatewayDebug::println(
                "Commands: traffic on|off, debug on|off, status, help");
        }
        else
        {
            GatewayDebug::println("Unknown command. Type 'help'.");
        }
        length = 0;
    }
}

bool GatewayLogger::debugEnabled() const
{
    return GatewayDebug::enabled();
}

void GatewayLogger::setDebugEnabled(bool enabled)
{
    GatewayDebug::setEnabled(enabled);
}

bool GatewayLogger::trafficEnabled() const
{
    return trafficEnabled_.load();
}

void GatewayLogger::setTrafficEnabled(bool enabled)
{
    trafficEnabled_.store(enabled);
}

} // namespace Gateway
