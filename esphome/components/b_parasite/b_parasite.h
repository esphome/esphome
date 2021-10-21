#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"

#ifdef USE_ESP32

namespace esphome {
namespace b_parasite {

class BParasite : public Component, public esp32_ble_tracker::ESPBTDeviceListener {
 public:
  void set_address(uint64_t address) { address_ = address; };
  void set_bindkey(const std::string &bindkey);

  bool parse_device(const esp32_ble_tracker::ESPBTDevice &device) override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_battery_voltage(sensor::Sensor *battery_voltage) { battery_voltage_ = battery_voltage; }
  void set_temperature(sensor::Sensor *temperature) { temperature_ = temperature; }
  void set_humidity(sensor::Sensor *humidity) { humidity_ = humidity; }
  void set_soil_moisture(sensor::Sensor *soil_moisture) { soil_moisture_ = soil_moisture; }
  void set_illuminance(sensor::Sensor *illuminance) { illuminance_ = illuminance; }

 protected:
  // The received advertisement packet contains an unsigned 4 bits wrap-around counter
  // for deduplicating messages.
  int8_t last_processed_counter_ = -1;
  uint64_t address_;
  sensor::Sensor *battery_voltage_{nullptr};
  sensor::Sensor *temperature_{nullptr};
  sensor::Sensor *humidity_{nullptr};
  sensor::Sensor *soil_moisture_{nullptr};
  sensor::Sensor *illuminance_{nullptr};
};

}  // namespace b_parasite
}  // namespace esphome

#endif  // USE_ESP32
