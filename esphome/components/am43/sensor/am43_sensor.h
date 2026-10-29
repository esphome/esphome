#pragma once

#include "esphome/core/component.h"
#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/am43/am43_base.h"

#ifdef USE_ESP32

#include <esp_gattc_api.h>

namespace esphome::am43 {

namespace espbt = esphome::esp32_ble_tracker;

class Am43 final : public esphome::ble_client::BLEClientNode, public PollingComponent {
  SUB_SENSOR(battery)
  SUB_SENSOR(illuminance)

 public:
  void update() override;
  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;
  void dump_config() override;

 protected:
  /// Writes a request to the control characteristic; logs on failure.
  void write_packet_(const Am43Packet &packet);
  // The AM43 often gets into a state where it spams loads of battery update
  // notifications. Here we will limit to no more than every 10s.
  uint32_t last_battery_update_{0};
  uint16_t char_handle_{0};
  uint8_t current_sensor_{0};
};

}  // namespace esphome::am43

#endif
