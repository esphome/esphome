#pragma once

#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"

#include <cstdint>

#ifdef USE_ESP32

namespace esphome {
namespace bthome_mithermometer {

class BTHomeMiThermometer : public esp32_ble_tracker::ESPBTDeviceListener, public Component {
 public:
  void set_address(uint64_t address) { this->address_ = address; }

  void set_temperature(sensor::Sensor *temperature) { this->temperature_ = temperature; }
  void set_humidity(sensor::Sensor *humidity) { this->humidity_ = humidity; }
  void set_battery_level(sensor::Sensor *battery_level) { this->battery_level_ = battery_level; }
  void set_battery_voltage(sensor::Sensor *battery_voltage) { this->battery_voltage_ = battery_voltage; }
  void set_signal_strength(sensor::Sensor *signal_strength) { this->signal_strength_ = signal_strength; }

  void dump_config() override;
  bool parse_device(const esp32_ble_tracker::ESPBTDevice &device) override;

 protected:
  bool handle_service_data_(const esp32_ble_tracker::ServiceData &service_data,
                            const esp32_ble_tracker::ESPBTDevice &device);

  uint64_t address_{0};
  optional<uint8_t> last_packet_id_{};

  sensor::Sensor *temperature_{nullptr};
  sensor::Sensor *humidity_{nullptr};
  sensor::Sensor *battery_level_{nullptr};
  sensor::Sensor *battery_voltage_{nullptr};
  sensor::Sensor *signal_strength_{nullptr};
};

}  // namespace bthome_mithermometer
}  // namespace esphome

#endif
