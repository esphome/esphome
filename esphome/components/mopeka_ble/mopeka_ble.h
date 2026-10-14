#pragma once

#include <vector>

#include "esphome/components/ble_device_base/ble_device.h"
#include "esphome/core/component.h"

namespace esphome::mopeka_ble {

class MopekaListener final : public ble_device_base::ESPBTDeviceListener {
 public:
  bool parse_device(const ble_device_base::ESPBTDevice &device) override;
  void set_show_sensors_without_sync(bool show_sensors_without_sync) {
    show_sensors_without_sync_ = show_sensors_without_sync;
  }

 protected:
  bool show_sensors_without_sync_;
};

}  // namespace esphome::mopeka_ble
