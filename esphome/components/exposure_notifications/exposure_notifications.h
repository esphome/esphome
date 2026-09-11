#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/ble_device_base/ble_device.h"
#include <array>

namespace esphome::exposure_notifications {

struct ExposureNotification {
  std::array<uint8_t, 6> address;
  int rssi;
  std::array<uint8_t, 16> rolling_proximity_identifier;
  std::array<uint8_t, 4> associated_encrypted_metadata;
};

class ExposureNotificationTrigger final : public Trigger<ExposureNotification>,
                                          public ble_device_base::ESPBTDeviceListener {
 public:
  bool parse_device(const ble_device_base::ESPBTDevice &device) override;
};

}  // namespace esphome::exposure_notifications
