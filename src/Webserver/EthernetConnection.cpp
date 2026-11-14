#include "EthernetConnection.h"

#include "../Gateway/DebugOutput.h"

namespace Gateway
{
namespace
{
constexpr char ETHERNET_HOSTNAME[] = "smart-meter-emulator";
constexpr uint32_t DHCP_FALLBACK_DELAY_MS = 10000;
const IPAddress FALLBACK_IP(192, 168, 178, 200);
const IPAddress FALLBACK_GATEWAY(192, 168, 178, 1);
const IPAddress FALLBACK_SUBNET(255, 255, 255, 0);
const IPAddress FALLBACK_DNS(192, 168, 178, 1);
}

EthernetConnection::EthernetConnection()
    : spi_(FSPI),
      started_(false),
      linkUp_(false),
      hasIp_(false),
      fallbackConfigured_(false),
      linkUpTimestamp_(0)
{
}

bool EthernetConnection::begin()
{
    Network.onEvent(
        [this](arduino_event_id_t event, arduino_event_info_t)
        {
            handleEvent(event);
        });

    spi_.begin(SCK, MISO, MOSI, W5500_SS);
    started_ = ETH.begin(
        ETH_PHY_W5500,
        0,
        W5500_SS,
        W5500_INT,
        W5500_RST,
        spi_);
    if (!started_)
    {
        GatewayDebug::println("EthernetConnection: Failed to start W5500.");
        return false;
    }

    return true;
}

void EthernetConnection::handle()
{
    if (!started_)
    {
        return;
    }

    if (!linkUp_)
    {
        fallbackConfigured_ = false;
        return;
    }

    if (hasIp_ || fallbackConfigured_ ||
        millis() - linkUpTimestamp_.load() < DHCP_FALLBACK_DELAY_MS)
    {
        return;
    }

    if (configureFallbackAddress())
    {
        fallbackConfigured_ = true;
        hasIp_ = true;
        GatewayDebug::printf(
            "Ethernet DHCP timed out; using fallback IP: %s\n",
            FALLBACK_IP.toString().c_str());
    }
}

IPAddress EthernetConnection::localIP() const
{
    return ETH.localIP();
}

void EthernetConnection::handleEvent(arduino_event_id_t event)
{
    switch (event)
    {
    case ARDUINO_EVENT_ETH_START:
        ETH.setHostname(ETHERNET_HOSTNAME);
        GatewayDebug::println("Ethernet started.");
        break;
    case ARDUINO_EVENT_ETH_CONNECTED:
        linkUp_ = true;
        hasIp_ = ETH.hasIP();
        linkUpTimestamp_ = millis();
        GatewayDebug::println("Ethernet link connected.");
        break;
    case ARDUINO_EVENT_ETH_GOT_IP:
        linkUp_ = true;
        hasIp_ = true;
        fallbackConfigured_ = false;
        GatewayDebug::printf("Ethernet-IP: %s\n", ETH.localIP().toString().c_str());
        break;
    case ARDUINO_EVENT_ETH_DISCONNECTED:
        linkUp_ = false;
        hasIp_ = false;
        GatewayDebug::println("Ethernet link disconnected.");
        break;
    case ARDUINO_EVENT_ETH_LOST_IP:
        hasIp_ = false;
        if (!fallbackConfigured_)
        {
            linkUpTimestamp_ = millis();
        }
        GatewayDebug::println("Ethernet IP address lost.");
        break;
    case ARDUINO_EVENT_ETH_STOP:
        linkUp_ = false;
        hasIp_ = false;
        started_ = false;
        GatewayDebug::println("Ethernet stopped.");
        break;
    default:
        break;
    }
}

bool EthernetConnection::configureFallbackAddress()
{
    if (!ETH.config(
            FALLBACK_IP,
            FALLBACK_GATEWAY,
            FALLBACK_SUBNET,
            FALLBACK_DNS))
    {
        GatewayDebug::println("EthernetConnection: Failed to configure fallback IP.");
        return false;
    }
    return true;
}

} // namespace Gateway
