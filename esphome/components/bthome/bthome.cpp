#include "bthome.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

#ifndef CONFIG_ESP_HOSTED_ENABLE_BT_BLUEDROID
#include <esp_bt.h>
#endif
#include <esp_bt_main.h>
#include <esp_gap_ble_api.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <nvs_flash.h>
#include <cstring>
#include <cmath>

#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "mbedtls/ccm.h"

namespace esphome {
namespace bthome {

static const char *const TAG = "bthome";

// BTHome v2 service UUID (0xFCD2)
static const uint16_t BTHOME_SERVICE_UUID = 0xFCD2;

// Device info byte for BTHome v2 (bit 5-7: version 010 = v2, bit 0: encryption)
static const uint8_t BTHOME_DEVICE_INFO_UNENCRYPTED = 0x40;  // 01000000
static const uint8_t BTHOME_DEVICE_INFO_ENCRYPTED = 0x41;    // 01000001

void BTHome::dump_config() {
  ESP_LOGCONFIG(TAG, "BTHome:");
  ESP_LOGCONFIG(TAG, "  Min Interval: %ums", this->min_interval_);
  ESP_LOGCONFIG(TAG, "  Max Interval: %ums", this->max_interval_);
  ESP_LOGCONFIG(TAG, "  TX Power: %ddBm", (this->tx_power_ * 3) - 12);
  ESP_LOGCONFIG(TAG, "  Encryption: %s", this->encryption_enabled_ ? "enabled" : "disabled");
  ESP_LOGCONFIG(TAG, "  Sensors: %d", this->measurements_.size());
  ESP_LOGCONFIG(TAG, "  Binary Sensors: %d", this->binary_measurements_.size());
}

float BTHome::get_setup_priority() const { return setup_priority::AFTER_BLUETOOTH; }

void BTHome::setup() {
  this->ble_adv_params_ = {
      .adv_int_min = static_cast<uint16_t>(this->min_interval_ / 0.625f),
      .adv_int_max = static_cast<uint16_t>(this->max_interval_ / 0.625f),
      .adv_type = ADV_TYPE_NONCONN_IND,
      .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
      .peer_addr = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
      .peer_addr_type = BLE_ADDR_TYPE_PUBLIC,
      .channel_map = ADV_CHNL_ALL,
      .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
  };

  global_ble->advertising_register_raw_advertisement_callback([this](bool advertise) {
    this->advertising_ = advertise;
    if (advertise) {
      this->on_advertise_();
    }
  });

  // Register callbacks for sensor state changes
  for (auto &measurement : this->measurements_) {
    measurement.sensor->add_on_state_callback([this](float) { this->data_changed_ = true; });
  }

  for (auto &measurement : this->binary_measurements_) {
    measurement.sensor->add_on_state_callback([this](bool) { this->data_changed_ = true; });
  }
}

void BTHome::loop() {
  // Rebuild advertisement if data has changed
  if (this->data_changed_ && this->advertising_) {
    this->build_advertisement_data_();
    this->on_advertise_();
    this->data_changed_ = false;
  }
}

void BTHome::set_encryption_key(const std::vector<uint8_t> &key) {
  if (key.size() != 16) {
    ESP_LOGE(TAG, "Encryption key must be 16 bytes");
    return;
  }
  this->encryption_enabled_ = true;
  std::copy(key.begin(), key.end(), this->encryption_key_.begin());
}

void BTHome::add_measurement(sensor::Sensor *sensor, uint8_t object_id) {
  this->measurements_.push_back({sensor, object_id});
}

void BTHome::add_binary_measurement(binary_sensor::BinarySensor *sensor, uint8_t object_id) {
  this->binary_measurements_.push_back({sensor, object_id});
}

void BTHome::build_advertisement_data_() {
  std::vector<uint8_t> service_data;

  // Add BTHome service UUID (little-endian)
  service_data.push_back(BTHOME_SERVICE_UUID & 0xFF);
  service_data.push_back((BTHOME_SERVICE_UUID >> 8) & 0xFF);

  // Add device info byte
  uint8_t device_info = this->encryption_enabled_ ? BTHOME_DEVICE_INFO_ENCRYPTED : BTHOME_DEVICE_INFO_UNENCRYPTED;
  service_data.push_back(device_info);

  // Build measurement data
  std::vector<uint8_t> measurement_data;

  // Add all sensor measurements
  for (const auto &measurement : this->measurements_) {
    if (measurement.sensor->has_state() && !std::isnan(measurement.sensor->state)) {
      this->encode_measurement_(measurement_data, measurement.object_id, measurement.sensor->state);
    }
  }

  // Add all binary sensor measurements
  for (const auto &measurement : this->binary_measurements_) {
    if (measurement.sensor->has_state()) {
      this->encode_binary_measurement_(measurement_data, measurement.object_id, measurement.sensor->state);
    }
  }

  if (this->encryption_enabled_) {
    // Encrypt the measurement data
    std::vector<uint8_t> ciphertext;
    if (this->encrypt_payload_(measurement_data, ciphertext)) {
      // Add ciphertext to service data
      service_data.insert(service_data.end(), ciphertext.begin(), ciphertext.end());

      // Add counter (little-endian)
      service_data.push_back(this->counter_ & 0xFF);
      service_data.push_back((this->counter_ >> 8) & 0xFF);
      service_data.push_back((this->counter_ >> 16) & 0xFF);
      service_data.push_back((this->counter_ >> 24) & 0xFF);

      // Increment counter for next advertisement
      this->counter_++;
    } else {
      ESP_LOGE(TAG, "Encryption failed");
      return;
    }
  } else {
    // Add unencrypted measurement data
    service_data.insert(service_data.end(), measurement_data.begin(), measurement_data.end());
  }

  // Build the complete advertisement data
  this->adv_data_.clear();

  // Flags AD element (required): 0x020106
  this->adv_data_.push_back(0x02);  // Length
  this->adv_data_.push_back(0x01);  // Type: Flags
  this->adv_data_.push_back(0x06);  // LE General Discoverable, BR/EDR not supported

  // Service Data AD element
  this->adv_data_.push_back(service_data.size() + 1);  // Length (data + type byte)
  this->adv_data_.push_back(0x16);                     // Type: Service Data
  this->adv_data_.insert(this->adv_data_.end(), service_data.begin(), service_data.end());
}

void BTHome::encode_measurement_(std::vector<uint8_t> &data, uint8_t object_id, float value) {
  data.push_back(object_id);

  // Encode based on object ID
  switch (object_id) {
    case 0x01:  // battery (uint8, 1%)
    {
      uint8_t encoded = static_cast<uint8_t>(std::round(value));
      data.push_back(encoded);
      break;
    }
    case 0x02:  // temperature (sint16, 0.01°C)
    case 0x08:  // dewpoint (sint16, 0.01°C)
    {
      int16_t encoded = static_cast<int16_t>(std::round(value * 100.0f));
      data.push_back(encoded & 0xFF);
      data.push_back((encoded >> 8) & 0xFF);
      break;
    }
    case 0x03:  // humidity (uint16, 0.01%)
    case 0x14:  // moisture (uint16, 0.01%)
    {
      uint16_t encoded = static_cast<uint16_t>(std::round(value * 100.0f));
      data.push_back(encoded & 0xFF);
      data.push_back((encoded >> 8) & 0xFF);
      break;
    }
    case 0x04:  // pressure (uint24, 0.01 hPa)
    case 0x05:  // illuminance (uint24, 0.01 lux)
    case 0x0A:  // energy (uint24, 0.001 kWh)
    case 0x0B:  // power (uint24, 0.01 W)
    {
      float factor = (object_id == 0x0A) ? 1000.0f : 100.0f;
      uint32_t encoded = static_cast<uint32_t>(std::round(value * factor));
      data.push_back(encoded & 0xFF);
      data.push_back((encoded >> 8) & 0xFF);
      data.push_back((encoded >> 16) & 0xFF);
      break;
    }
    case 0x06:  // mass (uint16, 0.01 kg)
    case 0x43:  // current (uint16, 0.001 A)
    case 0x44:  // speed (uint16, 0.01 m/s)
    {
      float factor = (object_id == 0x43) ? 1000.0f : 100.0f;
      uint16_t encoded = static_cast<uint16_t>(std::round(value * factor));
      data.push_back(encoded & 0xFF);
      data.push_back((encoded >> 8) & 0xFF);
      break;
    }
    case 0x0C:  // voltage (uint16, 0.001 V)
    {
      uint16_t encoded = static_cast<uint16_t>(std::round(value * 1000.0f));
      data.push_back(encoded & 0xFF);
      data.push_back((encoded >> 8) & 0xFF);
      break;
    }
    case 0x0D:  // PM2.5 (uint16, 1 µg/m³)
    case 0x0E:  // PM10 (uint16, 1 µg/m³)
    case 0x12:  // CO2 (uint16, 1 ppm)
    case 0x13:  // TVOC (uint16, 1 µg/m³)
    {
      uint16_t encoded = static_cast<uint16_t>(std::round(value));
      data.push_back(encoded & 0xFF);
      data.push_back((encoded >> 8) & 0xFF);
      break;
    }
    case 0x50:  // timestamp (uint32, seconds)
    {
      uint32_t encoded = static_cast<uint32_t>(value);
      data.push_back(encoded & 0xFF);
      data.push_back((encoded >> 8) & 0xFF);
      data.push_back((encoded >> 16) & 0xFF);
      data.push_back((encoded >> 24) & 0xFF);
      break;
    }
    default:
      ESP_LOGW(TAG, "Unsupported sensor object ID: 0x%02X", object_id);
      // Remove the object ID we just added
      data.pop_back();
      break;
  }
}

void BTHome::encode_binary_measurement_(std::vector<uint8_t> &data, uint8_t object_id, bool value) {
  data.push_back(object_id);
  data.push_back(value ? 0x01 : 0x00);
}

bool BTHome::encrypt_payload_(const std::vector<uint8_t> &plaintext, std::vector<uint8_t> &ciphertext) {
  if (!this->encryption_enabled_) {
    return false;
  }

  // Get MAC address
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_BT);

  // Build nonce according to BTHome spec:
  // MAC (6 bytes) + UUID reversed (2 bytes) + device info (1 byte) + counter (4 bytes) = 13 bytes
  uint8_t nonce[13];
  memcpy(nonce, mac, 6);
  nonce[6] = BTHOME_SERVICE_UUID & 0xFF;          // UUID byte 1
  nonce[7] = (BTHOME_SERVICE_UUID >> 8) & 0xFF;   // UUID byte 2 (already little-endian)
  nonce[8] = BTHOME_DEVICE_INFO_ENCRYPTED;        // Device info byte
  nonce[9] = this->counter_ & 0xFF;               // Counter byte 0
  nonce[10] = (this->counter_ >> 8) & 0xFF;       // Counter byte 1
  nonce[11] = (this->counter_ >> 16) & 0xFF;      // Counter byte 2
  nonce[12] = (this->counter_ >> 24) & 0xFF;      // Counter byte 3

  // Prepare output buffer (ciphertext + 4-byte MIC)
  ciphertext.resize(plaintext.size() + 4);

  // Initialize mbedtls CCM context
  mbedtls_ccm_context ctx;
  mbedtls_ccm_init(&ctx);

  // Set encryption key
  int ret = mbedtls_ccm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, this->encryption_key_.data(), 128);
  if (ret != 0) {
    ESP_LOGE(TAG, "mbedtls_ccm_setkey failed: %d", ret);
    mbedtls_ccm_free(&ctx);
    return false;
  }

  // Encrypt and generate tag
  // BTHome uses no additional authenticated data (AAD)
  ret = mbedtls_ccm_encrypt_and_tag(&ctx, plaintext.size(), nonce, sizeof(nonce), nullptr, 0, plaintext.data(),
                                    ciphertext.data(), ciphertext.data() + plaintext.size(), 4);

  mbedtls_ccm_free(&ctx);

  if (ret != 0) {
    ESP_LOGE(TAG, "mbedtls_ccm_encrypt_and_tag failed: %d", ret);
    return false;
  }

  return true;
}

void BTHome::on_advertise_() {
  // Build advertisement data if needed
  if (this->data_changed_ || this->adv_data_.empty()) {
    this->build_advertisement_data_();
    this->data_changed_ = false;
  }

  ESP_LOGD(TAG, "Setting BLE TX power");
  esp_err_t err = esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, this->tx_power_);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "esp_ble_tx_power_set failed: %s", esp_err_to_name(err));
  }

  ESP_LOGD(TAG, "Starting BTHome advertisement (%d bytes)", this->adv_data_.size());
  err = esp_ble_gap_config_adv_data_raw(this->adv_data_.data(), this->adv_data_.size());
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_ble_gap_config_adv_data_raw failed: %s", esp_err_to_name(err));
    return;
  }
}

void BTHome::gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
  if (!this->advertising_)
    return;

  esp_err_t err;
  switch (event) {
    case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT: {
      err = esp_ble_gap_start_advertising(&this->ble_adv_params_);
      if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ble_gap_start_advertising failed: %s", esp_err_to_name(err));
      }
      break;
    }
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT: {
      err = param->adv_start_cmpl.status;
      if (err != ESP_BT_STATUS_SUCCESS) {
        ESP_LOGE(TAG, "BLE adv start failed: %s", esp_err_to_name(err));
      } else {
        ESP_LOGD(TAG, "BLE advertising started successfully");
      }
      break;
    }
    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT: {
      err = param->adv_stop_cmpl.status;
      if (err != ESP_BT_STATUS_SUCCESS) {
        ESP_LOGE(TAG, "BLE adv stop failed: %s", esp_err_to_name(err));
      } else {
        ESP_LOGD(TAG, "BLE stopped advertising successfully");
      }
      break;
    }
    default:
      break;
  }
}

}  // namespace bthome
}  // namespace esphome

#endif
