#pragma once

#include <ETH.h>
#include <SPI.h>
#include <atomic>

namespace Gateway
{

class EthernetConnection
{
public:
    EthernetConnection();

    bool begin();
    void handle();
    IPAddress localIP() const;

private:
    void handleEvent(arduino_event_id_t event);
    bool configureFallbackAddress();

    SPIClass spi_;
    std::atomic<bool> started_;
    std::atomic<bool> linkUp_;
    std::atomic<bool> hasIp_;
    std::atomic<bool> fallbackConfigured_;
    std::atomic<uint32_t> linkUpTimestamp_;
};

} // namespace Gateway
