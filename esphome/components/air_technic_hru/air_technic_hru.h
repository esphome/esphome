#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/modbus/modbus.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace air_technic_hru {

static const char *const TAG = "air_technic_hru";

class AirTechnicHru : public sensor::Sensor, public PollingComponent, public modbus::ModbusDevice {
 private:
  static constexpr uint8_t MODBUS_CMD_READ_REGISTERS = 4;

 public:
  void update() override {
    ESP_LOGW(TAG, "Sending poll");
    this->send(MODBUS_CMD_READ_REGISTERS, 0, 20);
  }

  void on_modbus_data(const std::vector<uint8_t> &data) override {
    for (const auto d : data) {
      ESP_LOGW(TAG, "0x%02X", d);
    }
  }
};

}  // namespace air_technic_hru
}  // namespace esphome
