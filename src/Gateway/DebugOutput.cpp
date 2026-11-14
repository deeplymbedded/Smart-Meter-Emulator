#include "DebugOutput.h"

#include <atomic>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

namespace GatewayDebug
{
namespace
{
constexpr size_t LOG_LINE_SIZE = 192;
constexpr uint8_t LOG_LINE_COUNT = 32;

char logLines[LOG_LINE_COUNT][LOG_LINE_SIZE] = {};
uint8_t logNext = 0;
uint8_t logCount = 0;
portMUX_TYPE logMux = portMUX_INITIALIZER_UNLOCKED;
std::atomic<bool> captureEnabled{true};

void record(const char *message)
{
    if (!captureEnabled.load())
    {
        return;
    }

    portENTER_CRITICAL(&logMux);
    strncpy(logLines[logNext], message, LOG_LINE_SIZE - 1);
    logLines[logNext][LOG_LINE_SIZE - 1] = '\0';
    logNext = (logNext + 1) % LOG_LINE_COUNT;
    if (logCount < LOG_LINE_COUNT)
    {
        ++logCount;
    }
    portEXIT_CRITICAL(&logMux);
}

void appendJsonString(String &json, const char *text)
{
    json += '"';
    while (*text != '\0')
    {
        const char character = *text++;
        if (character == '"' || character == '\\')
        {
            json += '\\';
            json += character;
        }
        else if (character == '\n')
        {
            json += "\\n";
        }
        else if (character == '\r')
        {
            json += "\\r";
        }
        else if (static_cast<uint8_t>(character) < 0x20)
        {
            json += ' ';
        }
        else
        {
            json += character;
        }
    }
    json += '"';
}
}

void println(const char *message)
{
    Serial.println(message);
    record(message);
}

void printf(const char *format, ...)
{
    char message[LOG_LINE_SIZE];
    va_list arguments;
    va_start(arguments, format);
    vsnprintf(message, sizeof(message), format, arguments);
    va_end(arguments);

    Serial.print(message);
    record(message);
}

bool enabled()
{
    return captureEnabled.load();
}

void setEnabled(bool enabled)
{
    captureEnabled.store(enabled);
}

String logJson()
{
    uint8_t next;
    uint8_t count;
    portENTER_CRITICAL(&logMux);
    next = logNext;
    count = logCount;
    portEXIT_CRITICAL(&logMux);

    String json = "{\"enabled\":";
    json += enabled() ? "true" : "false";
    json += ",\"entries\":[";

    const uint8_t firstIndex = (next + LOG_LINE_COUNT - count) % LOG_LINE_COUNT;
    for (uint8_t item = 0; item < count; ++item)
    {
        char line[LOG_LINE_SIZE];
        portENTER_CRITICAL(&logMux);
        memcpy(line, logLines[(firstIndex + item) % LOG_LINE_COUNT], sizeof(line));
        portEXIT_CRITICAL(&logMux);
        line[LOG_LINE_SIZE - 1] = '\0';

        if (item > 0)
        {
            json += ',';
        }
        appendJsonString(json, line);
    }

    json += "]}";
    return json;
}

} // namespace GatewayDebug
