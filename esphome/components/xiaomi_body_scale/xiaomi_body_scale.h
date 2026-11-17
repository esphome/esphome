#pragma once

#include "esphome/core/component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/ble_device_base/ble_device.h"

namespace esphome::xiaomi_body_scale {

class XiaomiBodyScale final : public Component, public ble_device_base::ESPBTDeviceListener {
  SUB_SENSOR(weight)
  SUB_SENSOR(impedance_low)
  SUB_SENSOR(impedance_high)
  SUB_SENSOR(heart_rate)
  SUB_SENSOR(profile_id)
  SUB_BINARY_SENSOR(stabilized)

 public:
  XiaomiBodyScale(uint64_t address, const char *bindkey);

  bool parse_device(const ble_device_base::ESPBTDevice &device) override;
  void dump_config() override;

 protected:
  bool decrypt_(const uint8_t *frame, uint8_t *plaintext) const;
  void publish_s400_(uint32_t packed);
  void publish_stabilized_(bool stabilized);

  uint64_t address_;
  uint8_t bindkey_[16];
  uint16_t last_frame_count_{0x100};  // outside the 8-bit range, so the first frame always passes
};

}  // namespace esphome::xiaomi_body_scale
