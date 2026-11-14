# Smart Meter Emulator

Smart Meter Emulator is an ESP32-based Modbus RTU gateway for a specific
GoodWe installation. It polls an existing Huawei DTSU666-HW meter, caches its
measurements, and serves a GoodWe-compatible register set to the inverter.

The target use case is operating a GoodWe GW50K-ET10 with a BAT112 battery
without the normally expected GM330, using the Huawei DTSU666-HW already
installed at the site instead. The inverter can poll the emulator as it would
poll the meter interface.

This is an independent, community project. It is not affiliated with or
endorsed by GoodWe, Huawei, or YelloByte. It is not a complete GM330
replacement and is not certified for safety-critical or production use.

## How it works

```text
Huawei DTSU666-HW
   | Modbus RTU, slave ID 11
   v
YelloByte ESP32-S3-ETH
   | polls and caches meter data
   | Modbus RTU server, slave ID 3
   v
GoodWe GW50K-ET10
```

The Huawei client reads three register blocks for phase voltage, current, and
power. A GoodWe-specific server maps the cached measurements to the register
addresses requested by the inverter.

The client/server architecture is intended to be reusable for additional meter
and inverter combinations. New device-specific implementations can provide
their own polling and register-mapping behavior.

## Hardware

- YelloByte ESP32-S3-ETH board
- The board's Ethernet `INT` solder bridge must be closed for Ethernet operation
- Huawei DTSU666-HW meter
- GoodWe GW50K-ET10 inverter; the intended installation includes a BAT112
- The required RS-485 interfaces/transceivers and suitable cabling for the
  board and both devices

The firmware configures these UART pins and Modbus settings:

| Connection | UART | ESP32 RX | ESP32 TX | Modbus ID | Serial format |
| --- | ---: | ---: | ---: | ---: | --- |
| Huawei DTSU666-HW | 2 | GPIO 4 | GPIO 5 | 11 | 9600 baud, 8N1 |
| GoodWe inverter | 1 | GPIO 6 | GPIO 7 | 3 | 9600 baud, 8N1 |

These are the firmware's UART signal pins, not a wiring diagram. Follow the
board and device documentation for the RS-485 interface, polarity, isolation,
termination, and electrical installation. Verify the meter and inverter
settings before connecting the gateway.

## Modbus compatibility scope

- The Huawei device is polled as Modbus RTU slave ID `11`.
- The emulator responds to the GoodWe as Modbus RTU slave ID `3`.
- The GoodWe server accepts holding-register reads within addresses `306` to
  `343`.
- This implementation maps the selected voltage, current, and power values.
  Addresses in the accepted range that are not explicitly mapped currently
  return zero; this is a limited implementation, not a full GM330 register
  map.
- A read outside the supported range receives a Modbus illegal-address error.
- The cache is updated only after all three Huawei register reads succeed. If
  communication later fails, the last successful sample remains cached; the
  firmware does not currently expire stale data automatically. Monitor the
  cache status and age in the web interface.

Confirm the required behavior and register interpretation against the
documentation for your exact inverter, meter, and firmware versions. Do not
rely on this project as the sole source of energy or battery control data.

## Build and upload

The project uses PlatformIO and the `yellobyte_esp32s3_eth` environment.

```powershell
pio run -e yellobyte_esp32s3_eth
pio run -e yellobyte_esp32s3_eth -t upload
pio device monitor -e yellobyte_esp32s3_eth
```

The serial monitor runs at `115200` baud. The firmware configures a 16 MB flash
layout for the board. The project currently has no automated test suite.

## Ethernet and web interface

Ethernet uses the board's W5500 interface. The device first attempts to obtain
an address through DHCP. If DHCP has not provided an address after the
configured timeout, the firmware falls back to `192.168.178.200/24`, with
gateway and DNS `192.168.178.1`.

The built-in web interface listens on HTTP port `80` and provides:

- Meter cache status and measurements
- Emulated GoodWe register values
- Debug output and optional RS-485 traffic capture
- Firmware upload (OTA)

The web interface and firmware upload currently have no authentication or
HTTPS. Keep the device on a trusted, isolated network and do not expose it
directly to the Internet. Check that the fallback IP does not conflict with
another device on your network.

## Safety and responsibility

Incorrect meter data, stale readings, or incompatible register semantics can
affect inverter and battery operation. Validate the measurements and system
behavior under qualified supervision before relying on the gateway. Electrical
work must be performed by a qualified professional and in accordance with the
applicable local requirements.

The software is provided "as is", without a guarantee of compatibility,
accuracy, availability, or fitness for a particular purpose. Use it at your
own risk.

## License

This project is licensed under the MIT License. See [LICENSE](LICENSE) for
details.
