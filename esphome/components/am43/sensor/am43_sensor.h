#pragma once

#include "esphome/core/component.h"
#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/am43/am43_base.h"

#ifdef USE_ESP32

#include <esp_gattc_api.h>

namespace esphome {
namespace am43 {

namespace espbt = esphome::esp32_ble_tracker;

class Am43 : public esphome::ble_client::BLEClientNode, public PollingComponent {
 public:
  void setup() override;
  void update() override;
  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  void set_battery(sensor::Sensor *battery) { battery_ = battery; }
  void set_illuminance(sensor::Sensor *illuminance) { illuminance_ = illuminance; }

 protected:
  uint16_t char_handle_;
  std::unique_ptr<Am43Encoder> encoder_;
  std::unique_ptr<Am43Decoder> decoder_;
  bool logged_in_;
  sensor::Sensor *battery_{nullptr};
  sensor::Sensor *illuminance_{nullptr};
  uint8_t current_sensor_;
  // The AM43 often gets into a state where it spams loads of battery update
  // notifications. Here we will limit to no more than every 10s.
  uint8_t last_battery_update_;
};

}  // namespace am43
}  // namespace esphome

#endif
