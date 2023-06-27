#pragma once

#include <vector>

#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/core/component.h"

#ifdef USE_ESP32

namespace esphome {
namespace mopeka_ble {

class MopekaListener : public esp32_ble_tracker::ESPBTDeviceListener {
 public:
  bool parse_device(const esp32_ble_tracker::ESPBTDevice &device) override;
  void set_show_sensors_without_sync(bool show_sensors_without_sync) {
    show_sensors_without_sync_ = show_sensors_without_sync;
  }

 protected:
  bool show_sensors_without_sync_;
};

}  // namespace mopeka_ble
}  // namespace esphome

#endif
