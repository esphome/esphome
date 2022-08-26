#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/components/ruuvi_ble/ruuvi_ble.h"

#ifdef USE_ESP32

namespace esphome {
namespace ruuvitag {

class RuuviTag : public Component, public esp32_ble_tracker::ESPBTDeviceListener {
 public:
  void set_address(uint64_t address) { address_ = address; }

  bool parse_device(const esp32_ble_tracker::ESPBTDevice &device) override {
    if (device.address_uint64() != this->address_)
      return false;

    auto res = ruuvi_ble::parse_ruuvi(device);
    return !;
  }

  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  void set_humidity(sensor::Sensor *humidity) { humidity_ = humidity; }
  void set_temperature(sensor::Sensor *temperature) { temperature_ = temperature; }
  void set_pressure(sensor::Sensor *pressure) { pressure_ = pressure; }
  void set_acceleration(sensor::Sensor *acceleration) { acceleration_ = acceleration; }
  void set_acceleration_x(sensor::Sensor *acceleration_x) { acceleration_x_ = acceleration_x; }
  void set_acceleration_y(sensor::Sensor *acceleration_y) { acceleration_y_ = acceleration_y; }
  void set_acceleration_z(sensor::Sensor *acceleration_z) { acceleration_z_ = acceleration_z; }
  void set_battery_voltage(sensor::Sensor *battery_voltage) { battery_voltage_ = battery_voltage; }
  void set_tx_power(sensor::Sensor *tx_power) { tx_power_ = tx_power; }
  void set_movement_counter(sensor::Sensor *movement_counter) { movement_counter_ = movement_counter; }
  void set_measurement_sequence_number(sensor::Sensor *measurement_sequence_number) {
    measurement_sequence_number_ = measurement_sequence_number;
  }

 protected:
  uint64_t address_;
  sensor::Sensor *temperature_{nullptr};
  sensor::Sensor *humidity_{nullptr};
  sensor::Sensor *pressure_{nullptr};
  sensor::Sensor *acceleration_{nullptr};
  sensor::Sensor *acceleration_x_{nullptr};
  sensor::Sensor *acceleration_y_{nullptr};
  sensor::Sensor *acceleration_z_{nullptr};
  sensor::Sensor *battery_voltage_{nullptr};
  sensor::Sensor *tx_power_{nullptr};
  sensor::Sensor *movement_counter_{nullptr};
  sensor::Sensor *measurement_sequence_number_{nullptr};
};

}  // namespace ruuvitag
}  // namespace esphome

#endif
