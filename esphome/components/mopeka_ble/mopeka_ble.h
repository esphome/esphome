#pragma once

#include "esphome/core/component.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"

#include <vector>

#ifdef USE_ESP32

namespace esphome {
namespace mopeka_ble {

class MopekaListener : public esp32_ble_tracker::ESPBTDeviceListener {
 public:
  bool parse_device(const esp32_ble_tracker::ESPBTDevice &device) override;

 protected:
  bool parse_sync_button_(const std::vector<uint8_t> &message);
};

}  // namespace mopeka_ble
}  // namespace esphome

#endif
