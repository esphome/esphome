#pragma once
#include "esphome/core/defines.h"
#if defined(USE_ZEPHYR) && defined(USE_NRF52)
#include "esphome/components/ota/ota_backend.h"

namespace esphome::adafruit_ble {

// The update itself is served by the Adafruit bootloader; this component only
// hosts the BLE trigger service defined in ota_adafruit_ble.cpp.
class OTAComponent : public ota::OTAComponent {
 public:
  void dump_config() override;
};

}  // namespace esphome::adafruit_ble
#endif
