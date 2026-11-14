#include "HuaweiDTSU666HW.h"

#include <string.h>

#include "../Gateway/DebugOutput.h"
#include "RTUutils.h"

namespace Gateway
{
namespace
{
constexpr int DTSU666HW_RX_PIN = 4;
constexpr int DTSU666HW_TX_PIN = 5;
constexpr uint32_t METER_TASK_STACK_SIZE = 4096;
constexpr UBaseType_t METER_TASK_PRIORITY = 2;
}

HuaweiDTSU666HW::HuaweiDTSU666HW()
    : serial_(2),
      client_(-1, 4),
      responseSemaphore_(nullptr),
      meterTaskHandle_(nullptr),
      meterMux_(portMUX_INITIALIZER_UNLOCKED),
      meterData_{},
      responseMux_(portMUX_INITIALIZER_UNLOCKED),
      responseRegisters_{},
      expectedToken_(0),
      completedToken_(0),
      nextToken_(0),
      responseSucceeded_(false)
{
}

bool HuaweiDTSU666HW::begin()
{
    responseSemaphore_ = xSemaphoreCreateBinary();
    if (responseSemaphore_ == nullptr)
    {
        GatewayDebug::println("DTSU666-HW: Failed to create response semaphore.");
        return false;
    }

    RTUutils::prepareHardwareSerial(serial_);
    serial_.begin(BAUD_RATE, SERIAL_8N1, DTSU666HW_RX_PIN, DTSU666HW_TX_PIN);

    client_.onDataHandler(
        [this](ModbusMessage response, uint32_t token)
        {
            handleData(response, token);
        });
    client_.onErrorHandler(
        [this](Modbus::Error error, uint32_t token)
        {
            handleError(error, token);
        });
    client_.setTimeout(TIMEOUT_MS);
    client_.begin(serial_);

    const BaseType_t taskCreated = xTaskCreatePinnedToCore(
        meterTaskEntry,
        "MeterPoll",
        METER_TASK_STACK_SIZE,
        this,
        METER_TASK_PRIORITY,
        &meterTaskHandle_,
        tskNO_AFFINITY);
    if (taskCreated != pdPASS)
    {
        meterTaskHandle_ = nullptr;
        GatewayDebug::println("DTSU666-HW: Failed to start meter task.");
        return false;
    }

    return true;
}

MeterData HuaweiDTSU666HW::meterData() const
{
    MeterData snapshot;
    portENTER_CRITICAL(&meterMux_);
    snapshot = meterData_;
    portEXIT_CRITICAL(&meterMux_);
    return snapshot;
}

void HuaweiDTSU666HW::meterTaskEntry(void *context)
{
    static_cast<HuaweiDTSU666HW *>(context)->meterTask();
}

void HuaweiDTSU666HW::meterTask()
{
    while (true)
    {
        updateMeterData();
        vTaskDelay(pdMS_TO_TICKS(UPDATE_INTERVAL_MS));
    }
}

void HuaweiDTSU666HW::updateMeterData()
{
    uint16_t voltage[REG_COUNT];
    uint16_t current[REG_COUNT];
    uint16_t power[REG_COUNT];

    if (!readRegisters(REG_VOLTAGE_START, voltage) ||
        !readRegisters(REG_CURRENT_START, current) ||
        !readRegisters(REG_POWER_START, power))
    {
        return;
    }

    MeterData updated;
    updated.ua = (static_cast<uint32_t>(voltage[0]) << 16) | voltage[1];
    updated.ub = (static_cast<uint32_t>(voltage[2]) << 16) | voltage[3];
    updated.uc = (static_cast<uint32_t>(voltage[4]) << 16) | voltage[5];
    updated.ia = (static_cast<uint32_t>(current[0]) << 16) | current[1];
    updated.ib = (static_cast<uint32_t>(current[2]) << 16) | current[3];
    updated.ic = (static_cast<uint32_t>(current[4]) << 16) | current[5];
    updated.pa = static_cast<int32_t>(
        (static_cast<uint32_t>(power[0]) << 16) | power[1]);
    updated.pb = static_cast<int32_t>(
        (static_cast<uint32_t>(power[2]) << 16) | power[3]);
    updated.pc = static_cast<int32_t>(
        (static_cast<uint32_t>(power[4]) << 16) | power[5]);
    updated.valid = true;
    updated.timestamp = millis();

    portENTER_CRITICAL(&meterMux_);
    meterData_ = updated;
    portEXIT_CRITICAL(&meterMux_);

}

bool HuaweiDTSU666HW::readRegisters(
    uint16_t startRegister,
    uint16_t *values)
{
    xSemaphoreTake(responseSemaphore_, 0);

    portENTER_CRITICAL(&responseMux_);
    expectedToken_ = ++nextToken_;
    responseSucceeded_ = false;
    const uint32_t token = expectedToken_;
    portEXIT_CRITICAL(&responseMux_);

    ModbusMessage request(
        MODBUS_ID,
        Modbus::READ_HOLD_REGISTER,
        startRegister,
        REG_COUNT);
    traceMessage("TX", "DTSU666-HW", request);

    const Modbus::Error queueResult = client_.addRequest(
        token,
        MODBUS_ID,
        Modbus::READ_HOLD_REGISTER,
        startRegister,
        REG_COUNT);
    if (queueResult != Modbus::SUCCESS)
    {
        GatewayDebug::printf(
            "DTSU666-HW: Failed to queue request for register %u (0x%02X).\n",
            startRegister,
            static_cast<unsigned int>(queueResult));
        return false;
    }

    const TickType_t waitTicks = pdMS_TO_TICKS(TIMEOUT_MS + 50);
    const TickType_t waitStarted = xTaskGetTickCount();
    while (true)
    {
        const TickType_t elapsed = xTaskGetTickCount() - waitStarted;
        if (elapsed >= waitTicks ||
            xSemaphoreTake(responseSemaphore_, waitTicks - elapsed) != pdTRUE)
        {
            GatewayDebug::printf(
                "DTSU666-HW: Timed out reading register %u.\n",
                startRegister);
            return false;
        }

        portENTER_CRITICAL(&responseMux_);
        const bool completed = completedToken_ == token;
        const bool success = completed && responseSucceeded_;
        if (success)
        {
            memcpy(values, responseRegisters_, sizeof(responseRegisters_));
        }
        portEXIT_CRITICAL(&responseMux_);
        if (completed)
        {
            return success;
        }
    }
}

void HuaweiDTSU666HW::handleData(
    ModbusMessage response,
    uint32_t token)
{
    traceMessage("RX", "DTSU666-HW", response);

    bool success = false;
    uint16_t values[REG_COUNT] = {};
    if (response.size() == 3 + REG_COUNT * sizeof(uint16_t) &&
        response[2] == REG_COUNT * sizeof(uint16_t))
    {
        uint16_t offset = 3;
        for (uint16_t &value : values)
        {
            offset = response.get(offset, value);
        }
        success = true;
    }

    portENTER_CRITICAL(&responseMux_);
    const bool isExpected = token == expectedToken_;
    if (isExpected)
    {
        completedToken_ = token;
        responseSucceeded_ = success;
        if (success)
        {
            memcpy(responseRegisters_, values, sizeof(responseRegisters_));
        }
    }
    portEXIT_CRITICAL(&responseMux_);
    if (isExpected)
    {
        xSemaphoreGive(responseSemaphore_);
    }
}

void HuaweiDTSU666HW::handleError(
    Modbus::Error error,
    uint32_t token)
{
    GatewayDebug::printf(
        "DTSU666-HW: Modbus error 0x%02X for request %lu.\n",
        static_cast<unsigned int>(error),
        static_cast<unsigned long>(token));

    portENTER_CRITICAL(&responseMux_);
    const bool isExpected = token == expectedToken_;
    if (isExpected)
    {
        completedToken_ = token;
        responseSucceeded_ = false;
    }
    portEXIT_CRITICAL(&responseMux_);
    if (isExpected)
    {
        xSemaphoreGive(responseSemaphore_);
    }
}

} // namespace Gateway
