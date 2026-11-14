#pragma once

#include <functional>

#include "ModbusMessage.h"

namespace Gateway
{

using TrafficCallback =
    std::function<void(const char *, const char *, ModbusMessage)>;

class ModbusTrace
{
public:
    virtual ~ModbusTrace() = default;

    void setTrafficCallback(TrafficCallback callback)
    {
        trafficCallback_ = callback;
    }

protected:
    void traceMessage(
        const char *direction,
        const char *interfaceName,
        ModbusMessage message) const
    {
        if (trafficCallback_)
        {
            trafficCallback_(direction, interfaceName, message);
        }
    }

private:
    TrafficCallback trafficCallback_;
};

} // namespace Gateway
