#include "ble_esp32.h"

#include "esphome/core/log.h"

namespace esphome {
namespace bthome {

#ifdef USE_ESP32

static const char *const TAG = "bthome";

bool ESP32BLEListener::parse_device(const esp32_ble_tracker::ESPBTDevice &device) {
  bool matched = false;
  for (auto &service_data : device.get_service_datas()) {
    if (!service_data.uuid.contains(BTHOME_SVC_UUID_LOW, BTHOME_SVC_UUID_HIGH)) {
      ESP_LOGD(TAG, "not bthome service data from %s", MacAddressPtr(device.address()).c_str());
      continue;
    }

    const uint8_t *data = service_data.data.data();
    size_t data_size = service_data.data.size();

    if (data_size < sizeof(BTHomeHeader)) {
      ESP_LOGVV(TAG, "BTHome data too short: %zu", data_size);
      continue;
    }

    const BTHomeHeader &header = *reinterpret_cast<const BTHomeHeader *>(data);
    if (header.version != BTHOME_VERSION_2) {
      ESP_LOGVV(TAG, "Unsupported BTHome version %u", header.version);
      continue;
    }

    if (this->listener_->on_bthome_data(device.address(), data, data_size))
      matched = true;
  }
  return matched;
}

#endif  // USE_ESP32

#if defined(USE_ESP32) && defined(USE_BTHOME_SERVER)
static constexpr esp_ble_adv_params_t BLE_ADV_PARAMS{
    .adv_int_min = 0x20,  // 20ms
    .adv_int_max = 0x40,  // 40ms
    .adv_type = ADV_TYPE_NONCONN_IND,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .channel_map = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

MacAddressPtr ESP32BLEAdvertiser::get_local_mac() { return esp_bt_dev_get_address(); }

void ESP32BLEAdvertiser::setup(IBLEAdvHandler *adv_handler) {
  // Register raw advertisement callback for cycling
  esp32_ble::global_ble->advertising_register_raw_advertisement_callback([this, adv_handler](bool active) {
    this->advertising_ = active;
    adv_handler->on_advertise(active);
  });
}

void ESP32BLEAdvertiser::config_adv_data_raw(const uint8_t *data, size_t len) {
  esp_ble_gap_config_adv_data_raw(const_cast<uint8_t *>(data), len);
}

void ESP32BLEAdvertiser::gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
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
