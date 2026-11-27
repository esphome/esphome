#pragma once

#include "esphome/core/component.h"
#include "esphome/components/ble_device_base/ble_device.h"

namespace esphome::radon_eye_ble {

class RadonEyeListener final : public ble_device_base::ESPBTDeviceListener {
 public:
  bool parse_device(const ble_device_base::ESPBTDevice &device) override;
};

}  // namespace esphome::radon_eye_ble
