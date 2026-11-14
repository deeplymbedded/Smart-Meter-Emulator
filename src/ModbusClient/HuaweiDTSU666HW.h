#pragma once

#include <Arduino.h>
#include <HardwareSerial.h>

#include "ModbusClient.h"
#include "ModbusClientRTU.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

namespace Gateway
{

constexpr float DTSU666HW_VOLTAGE_SCALE = 100.0f;
constexpr float DTSU666HW_CURRENT_SCALE = 10.0f;

class HuaweiDTSU666HW : public ModbusClient
{
public:
    HuaweiDTSU666HW();

    bool begin() override;
    MeterData meterData() const override;

private:
    static constexpr uint8_t MODBUS_ID = 11;
    static constexpr uint32_t BAUD_RATE = 9600;
    static constexpr uint32_t TIMEOUT_MS = 150;
    static constexpr uint32_t UPDATE_INTERVAL_MS = 50;
    static constexpr uint16_t REG_VOLTAGE_START = 32260;
    static constexpr uint16_t REG_CURRENT_START = 32272;
    static constexpr uint16_t REG_POWER_START = 32335;
    static constexpr uint16_t REG_COUNT = 6;

    static void meterTaskEntry(void *context);
    void meterTask();
    void updateMeterData();
    bool readRegisters(uint16_t startRegister, uint16_t *values);
    void handleData(ModbusMessage response, uint32_t token);
    void handleError(Modbus::Error error, uint32_t token);

    HardwareSerial serial_;
    ModbusClientRTU client_;
    SemaphoreHandle_t responseSemaphore_;
    TaskHandle_t meterTaskHandle_;

    mutable portMUX_TYPE meterMux_;
    MeterData meterData_;

    portMUX_TYPE responseMux_;
    uint16_t responseRegisters_[REG_COUNT];
    uint32_t expectedToken_;
    uint32_t completedToken_;
    uint32_t nextToken_;
    bool responseSucceeded_;
};

} // namespace Gateway
