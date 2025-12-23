#pragma once

#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/component.h"

#include <cstdint>

#ifdef USE_ESP32

namespace esphome {
namespace bthome_ble {

class BTHomeBLE : public esp32_ble_tracker::ESPBTDeviceListener, public Component {
 public:
  void set_address(uint64_t address) { this->address_ = address; }

  void set_temperature(sensor::Sensor *temperature) { this->temperature_ = temperature; }
  void set_humidity(sensor::Sensor *humidity) { this->humidity_ = humidity; }
  void set_battery_level(sensor::Sensor *battery_level) { this->battery_level_ = battery_level; }
  void set_battery_low(binary_sensor::BinarySensor *battery_low) { this->battery_low_ = battery_low; }
  void set_firmware(text_sensor::TextSensor *firmware) { this->firmware_ = firmware; }
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
  sensor::Sensor *signal_strength_{nullptr};
  binary_sensor::BinarySensor *battery_low_{nullptr};
  text_sensor::TextSensor *firmware_{nullptr};
};

}  // namespace bthome_ble
}  // namespace esphome

#endif
