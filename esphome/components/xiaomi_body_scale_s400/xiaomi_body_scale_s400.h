#pragma once

#include "esphome/core/component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/ble_device_base/ble_device.h"

namespace esphome::xiaomi_body_scale_s400 {

class XiaomiBodyScaleS400 final : public Component, public ble_device_base::ESPBTDeviceListener {
 public:
  XiaomiBodyScaleS400(uint64_t address, const char *bindkey);

  bool parse_device(const ble_device_base::ESPBTDevice &device) override;
  void dump_config() override;

  void set_weight(sensor::Sensor *weight) { this->weight_ = weight; }
  void set_impedance_low(sensor::Sensor *impedance_low) { this->impedance_low_ = impedance_low; }
  void set_impedance_high(sensor::Sensor *impedance_high) { this->impedance_high_ = impedance_high; }
  void set_heart_rate(sensor::Sensor *heart_rate) { this->heart_rate_ = heart_rate; }
  void set_profile_id(sensor::Sensor *profile_id) { this->profile_id_ = profile_id; }
  void set_stabilized(binary_sensor::BinarySensor *stabilized) { this->stabilized_ = stabilized; }

 protected:
  bool decrypt_(const uint8_t *frame, uint8_t *plaintext) const;
  void publish_stabilized_(bool stabilized);

  uint64_t address_;
  uint8_t bindkey_[16];
  sensor::Sensor *weight_{nullptr};
  sensor::Sensor *impedance_low_{nullptr};
  sensor::Sensor *impedance_high_{nullptr};
  sensor::Sensor *heart_rate_{nullptr};
  sensor::Sensor *profile_id_{nullptr};
  binary_sensor::BinarySensor *stabilized_{nullptr};
  uint8_t last_frame_count_{0xFF};
};

}  // namespace esphome::xiaomi_body_scale_s400
