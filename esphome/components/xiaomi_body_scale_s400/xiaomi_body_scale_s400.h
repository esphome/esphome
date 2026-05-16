#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/components/xiaomi_ble/xiaomi_ble.h"

#ifdef USE_ESP32

namespace esphome::xiaomi_body_scale_s400 {

class XiaomiBodyScaleS400 : public Component, public esp32_ble_tracker::ESPBTDeviceListener {
 public:
  void set_address(uint64_t address) { address_ = address; }
  void set_bindkey(const std::string &bindkey);

  bool parse_device(const esp32_ble_tracker::ESPBTDevice &device) override;
  void dump_config() override;

  void set_weight(sensor::Sensor *weight) { weight_ = weight; }
  void set_impedance(sensor::Sensor *impedance) { impedance_ = impedance; }
  // impedance_low  = low frequency  50 kHz — numerically larger value
  void set_impedance_low(sensor::Sensor *impedance_low) { impedance_low_ = impedance_low; }
  // impedance_high = high frequency 250 kHz — numerically smaller value
  void set_impedance_high(sensor::Sensor *impedance_high) { impedance_high_ = impedance_high; }
  void set_heart_rate(sensor::Sensor *heart_rate) { heart_rate_ = heart_rate; }
  void set_profile_id(sensor::Sensor *profile_id) { profile_id_ = profile_id; }

 protected:
  uint64_t address_;
  uint8_t bindkey_[16];
  sensor::Sensor *weight_{nullptr};
  sensor::Sensor *impedance_{nullptr};
  sensor::Sensor *impedance_low_{nullptr};   // 50 kHz  — larger value
  sensor::Sensor *impedance_high_{nullptr};  // 250 kHz — smaller value
  sensor::Sensor *heart_rate_{nullptr};
  sensor::Sensor *profile_id_{nullptr};
};

}  // namespace esphome::xiaomi_body_scale_s400

#endif
