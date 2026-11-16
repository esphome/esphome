#pragma once
#include "esphome/core/defines.h"
#if defined(USE_ZEPHYR) && defined(USE_NRF52)
#include "esphome/components/ota/ota_backend.h"

namespace esphome::adafruit_ble {

// The update itself is served by the Adafruit bootloader; this component only
// requests entry, optionally hosting an application BLE trigger service.
class OTAComponent : public ota::OTAComponent {
 public:
#ifdef USE_ADAFRUIT_BLE_TRIGGER
  OTAComponent();
#endif
  void dump_config() override;
  // Called by a YAML action or the Bluetooth trigger; always deferred to the main loop.
  void request_dfu_mode();
};

}  // namespace esphome::adafruit_ble
#endif
