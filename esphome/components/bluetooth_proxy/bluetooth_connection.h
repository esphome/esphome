#pragma once

#ifdef USE_ESP32

#include "esphome/components/esp32_ble_client/ble_client_base.h"

namespace esphome {
namespace bluetooth_proxy {

class BluetoothProxy;

class BluetoothConnection : public esp32_ble_client::BLEClientBase {
 public:
  void dump_config() override;
  bool gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;
  void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) override;
  esp32_ble_tracker::AdvertisementParserType get_advertisement_parser_type() override;

  esp_err_t read_characteristic(uint16_t handle);
  esp_err_t write_characteristic(uint16_t handle, const std::string &data, bool response);
  esp_err_t read_descriptor(uint16_t handle);
  esp_err_t write_descriptor(uint16_t handle, const std::string &data, bool response);

  esp_err_t notify_characteristic(uint16_t handle, bool enable);

 protected:
  friend class BluetoothProxy;
  bool seen_mtu_or_services_{false};

  int16_t send_service_{-2};
  BluetoothProxy *proxy_;
};

}  // namespace bluetooth_proxy
}  // namespace esphome

#endif  // USE_ESP32
