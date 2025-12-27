#pragma once

#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"

#include <cstdint>
#include <vector>

#ifdef USE_ESP32

namespace esphome::neewerlight_ct {

class NeewerBleClient {
 public:
  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);

  // NOTE: msg is non-const because BLECharacteristic::write_value failed to declare it as const, just std::move it in
  void send_message(esphome::ble_client::BLEClient *client, std::vector<uint8_t> &&msg);

  esp32_ble_tracker::ESPBTUUID service_uuid_ = esp32_ble_tracker::ESPBTUUID::from_raw(SERVICE_UUID);
  esp32_ble_tracker::ESPBTUUID characteristic_uuid_ = esp32_ble_tracker::ESPBTUUID::from_raw(CHARACTERISTIC_UUID);
  bool require_response_ = true;

 protected:
  esp32_ble_tracker::ClientState client_state_ = esp32_ble_tracker::ClientState::INIT;

  static constexpr const char *SERVICE_UUID = "69400001-B5A3-F393-E0A9-E50E24DCCA99";
  static constexpr const char *CHARACTERISTIC_UUID = "69400002-B5A3-F393-E0A9-E50E24DCCA99";
};

}  // namespace esphome::neewerlight_ct

#endif  // USE_ESP32
