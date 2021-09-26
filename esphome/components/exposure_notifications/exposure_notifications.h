#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include <array>

#ifdef USE_ESP32

namespace esphome {
namespace exposure_notifications {

struct ExposureNotification {
  std::array<uint8_t, 6> address;
  int rssi;
  std::array<uint8_t, 16> rolling_proximity_identifier;
  std::array<uint8_t, 4> associated_encrypted_metadata;
};

class ExposureNotificationTrigger : public Trigger<ExposureNotification>,
                                    public esp32_ble_tracker::ESPBTDeviceListener {
 public:
  bool parse_device(const esp32_ble_tracker::ESPBTDevice &device) override;
};

}  // namespace exposure_notifications
}  // namespace esphome

#endif
