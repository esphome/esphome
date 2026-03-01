#include "ble_esp32.h"

namespace esphome {
namespace bthome {

#if defined(USE_ESP32) && defined(USE_BTHOME_SERVER)
static constexpr esp_ble_adv_params_t BLE_ADV_PARAMS{
    .adv_int_min = 0x20,  // 20ms
    .adv_int_max = 0x40,  // 40ms
    .adv_type = ADV_TYPE_NONCONN_IND,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .channel_map = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

MacAddressPtr ESP32BLEAdapter::get_local_mac() { return esp_bt_dev_get_address(); }

void ESP32BLEAdapter::setup(IBLEAdvHandler *adv_handler) {
  // Register raw advertisement callback for cycling
  esp32_ble::global_ble->advertising_register_raw_advertisement_callback([this, adv_handler](bool active) {
    this->advertising_ = active;
    adv_handler->on_advertise(active);
  });
}

void ESP32BLEAdapter::config_adv_data_raw(const uint8_t *data, size_t len) {
  esp_ble_gap_config_adv_data_raw(const_cast<uint8_t *>(data), len);
}

void ESP32BLEAdapter::gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
  if (!this->advertising_) {
    return;
  }
  switch (event) {
    case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT: {
      esp_err_t err = esp_ble_gap_start_advertising((esp_ble_adv_params_t *) &BLE_ADV_PARAMS);
      if (err != ESP_OK) {
        ESP_LOGW("bthome.server", "esp_ble_gap_start_advertising failed: %s", esp_err_to_name(err));
      }
      break;
    }
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT: {
      if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
        ESP_LOGW("bthome.server", "BLE adv start failed: %d", param->adv_start_cmpl.status);
      }
      break;
    }
    default:
      break;
  }
}
#endif  // USE_ESP32 && USE_BTHOME_SERVER

}  // namespace bthome
}  // namespace esphome
