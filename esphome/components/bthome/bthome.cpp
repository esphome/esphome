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

// Maximum BLE advertisement payload size
// Total advertisement is 31 bytes max, minus overhead (flags + service data header)
static const size_t MAX_BLE_ADVERTISEMENT_SIZE = 31;
// Overhead: Flags (3 bytes) + Service Data length (1) + Service Data type (1) +
// Service UUID (2) + Device Info (1) = 8 bytes
// For encrypted: + Counter (4) + MIC (4) = 16 bytes total overhead
static const size_t UNENCRYPTED_OVERHEAD = 8;
static const size_t ENCRYPTED_OVERHEAD = 16;

void BTHome::dump_config() {
  ESP_LOGCONFIG(TAG,
                "BTHome:\n"
                "  Min Interval: %ums\n"
                "  Max Interval: %ums\n"
                "  TX Power: %ddBm\n"
                "  Encryption: %s\n"
                "  Sensors: %d\n"
                "  Binary Sensors: %d",
                this->min_interval_, this->max_interval_, (this->tx_power_ * 3) - 12,
                this->encryption_enabled_ ? "enabled" : "disabled", this->measurements_.size(),
                this->binary_measurements_.size());
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
  for (size_t i = 0; i < this->measurements_.size(); i++) {
    auto &measurement = this->measurements_[i];
    measurement.sensor->add_on_state_callback([this, i](float) {
      if (this->measurements_[i].advertise_immediately) {
        this->trigger_immediate_advertising_(i, false);
      } else {
        this->data_changed_ = true;
      }
    });
  }

  for (size_t i = 0; i < this->binary_measurements_.size(); i++) {
    auto &measurement = this->binary_measurements_[i];
    measurement.sensor->add_on_state_callback([this, i](bool) {
      if (this->binary_measurements_[i].advertise_immediately) {
        this->trigger_immediate_advertising_(i, true);
      } else {
        this->data_changed_ = true;
      }
    });
  }

  // Start with loop disabled - only enable when immediate advertising is needed
  this->disable_loop();
}

void BTHome::loop() {
  // Handle immediate advertising requests only
  // Regular data changes wait for the next advertising callback
  if (this->immediate_advertising_pending_ && this->advertising_) {
    this->immediate_advertising_pending_ = false;
    // Rebuild with only the immediate measurement
    this->build_advertisement_packets_();
    this->current_packet_index_ = 0;
    this->on_advertise_();
    // Disable loop again until next immediate advertising request
    this->disable_loop();
  }
}

void BTHome::set_encryption_key(const std::array<uint8_t, 16> &key) {
  this->encryption_enabled_ = true;
  this->encryption_key_ = key;
}

void BTHome::add_measurement(sensor::Sensor *sensor, uint8_t object_id, bool advertise_immediately) {
  this->measurements_.push_back({sensor, object_id, advertise_immediately});
}

void BTHome::add_binary_measurement(binary_sensor::BinarySensor *sensor, uint8_t object_id,
                                    bool advertise_immediately) {
  this->binary_measurements_.push_back({sensor, object_id, advertise_immediately});
}

void BTHome::trigger_immediate_advertising_(uint8_t measurement_index, bool is_binary) {
  this->immediate_advertising_pending_ = true;
  this->immediate_adv_measurement_index_ = measurement_index;
  this->immediate_adv_is_binary_ = is_binary;
  // Enable loop to process immediate advertising
  this->enable_loop();
}

void BTHome::build_advertisement_packets_() {
  // Clear existing packets
  this->adv_packets_.clear();
  this->adv_packet_sizes_.clear();

  const size_t overhead = this->encryption_enabled_ ? ENCRYPTED_OVERHEAD : UNENCRYPTED_OVERHEAD;
  const size_t max_payload = MAX_BLE_ADVERTISEMENT_SIZE - overhead;

  // Handle immediate advertising - single sensor only
  if (this->immediate_advertising_pending_) {
    auto packet = std::make_unique<uint8_t[]>(MAX_BLE_ADVERTISEMENT_SIZE);
    uint8_t *data = packet.get();
    size_t pos = 0;

    // Flags
    data[pos++] = 0x02;
    data[pos++] = 0x01;
    data[pos++] = 0x06;

    // Service UUID
    size_t service_data_start = pos;
    pos++;  // Length placeholder
    data[pos++] = 0x16;  // Service Data type
    data[pos++] = BTHOME_SERVICE_UUID & 0xFF;
    data[pos++] = (BTHOME_SERVICE_UUID >> 8) & 0xFF;

    uint8_t device_info = this->encryption_enabled_ ? BTHOME_DEVICE_INFO_ENCRYPTED : BTHOME_DEVICE_INFO_UNENCRYPTED;
    data[pos++] = device_info;

    size_t measurement_start = pos;

    // Encode the single measurement
    if (this->immediate_adv_is_binary_) {
      auto &measurement = this->binary_measurements_[this->immediate_adv_measurement_index_];
      if (measurement.sensor->has_state()) {
        pos += this->encode_binary_measurement_(data + pos, max_payload - (pos - measurement_start),
                                                measurement.object_id, measurement.sensor->state);
      }
    } else {
      auto &measurement = this->measurements_[this->immediate_adv_measurement_index_];
      if (measurement.sensor->has_state() && !std::isnan(measurement.sensor->state)) {
        pos += this->encode_measurement_(data + pos, max_payload - (pos - measurement_start), measurement.object_id,
                                        measurement.sensor->state);
      }
    }

    size_t measurement_len = pos - measurement_start;

    if (this->encryption_enabled_ && measurement_len > 0) {
      uint8_t ciphertext[MAX_BLE_ADVERTISEMENT_SIZE];
      size_t ciphertext_len = 0;
      if (this->encrypt_payload_(data + measurement_start, measurement_len, ciphertext, &ciphertext_len)) {
        memcpy(data + measurement_start, ciphertext, ciphertext_len);
        pos = measurement_start + ciphertext_len;

        // Add counter
        data[pos++] = this->counter_ & 0xFF;
        data[pos++] = (this->counter_ >> 8) & 0xFF;
        data[pos++] = (this->counter_ >> 16) & 0xFF;
        data[pos++] = (this->counter_ >> 24) & 0xFF;
        this->counter_++;
      }
    }

    // Set service data length
    data[service_data_start] = (pos - service_data_start - 1);

    this->adv_packets_.push_back(std::move(packet));
    this->adv_packet_sizes_.push_back(pos);
    return;
  }

  // Normal cycling: Build packets that fit within max size
  auto packet = std::make_unique<uint8_t[]>(MAX_BLE_ADVERTISEMENT_SIZE);
  uint8_t *data = packet.get();
  size_t pos = 0;
  size_t measurement_start = 0;
  bool packet_started = false;

  auto start_new_packet = [&]() {
    pos = 0;
    // Flags
    data[pos++] = 0x02;
    data[pos++] = 0x01;
    data[pos++] = 0x06;

    // Service UUID
    measurement_start = pos;
    pos++;  // Length placeholder
    data[pos++] = 0x16;  // Service Data type
    data[pos++] = BTHOME_SERVICE_UUID & 0xFF;
    data[pos++] = (BTHOME_SERVICE_UUID >> 8) & 0xFF;

    uint8_t device_info = this->encryption_enabled_ ? BTHOME_DEVICE_INFO_ENCRYPTED : BTHOME_DEVICE_INFO_UNENCRYPTED;
    data[pos++] = device_info;
    measurement_start = pos;
    packet_started = true;
  };

  auto finish_packet = [&]() {
    if (!packet_started)
      return;

    size_t measurement_len = pos - measurement_start;
    if (measurement_len == 0) {
      packet_started = false;
      return;
    }

    if (this->encryption_enabled_) {
      uint8_t ciphertext[MAX_BLE_ADVERTISEMENT_SIZE];
      size_t ciphertext_len = 0;
      if (this->encrypt_payload_(data + measurement_start, measurement_len, ciphertext, &ciphertext_len)) {
        memcpy(data + measurement_start, ciphertext, ciphertext_len);
        pos = measurement_start + ciphertext_len;

        // Add counter
        data[pos++] = this->counter_ & 0xFF;
        data[pos++] = (this->counter_ >> 8) & 0xFF;
        data[pos++] = (this->counter_ >> 16) & 0xFF;
        data[pos++] = (this->counter_ >> 24) & 0xFF;
        this->counter_++;
      }
    }

    // Set service data length
    data[measurement_start - 5] = (pos - measurement_start + 4);

    this->adv_packets_.push_back(std::move(packet));
    this->adv_packet_sizes_.push_back(pos);
    packet = std::make_unique<uint8_t[]>(MAX_BLE_ADVERTISEMENT_SIZE);
    data = packet.get();
    packet_started = false;
  };

  start_new_packet();

  // Add all sensor measurements
  for (const auto &measurement : this->measurements_) {
    if (!measurement.sensor->has_state() || std::isnan(measurement.sensor->state))
      continue;

    size_t encoded_size = this->encode_measurement_(nullptr, 0, measurement.object_id, measurement.sensor->state);

    // Check if adding this measurement would exceed packet size
    if (pos - measurement_start + encoded_size > max_payload) {
      finish_packet();
      start_new_packet();
    }

    pos += this->encode_measurement_(data + pos, max_payload - (pos - measurement_start), measurement.object_id,
                                    measurement.sensor->state);
  }

  // Add all binary sensor measurements
  for (const auto &measurement : this->binary_measurements_) {
    if (!measurement.sensor->has_state())
      continue;

    size_t encoded_size = 2;  // Binary sensors are always 2 bytes (object_id + value)

    // Check if adding this measurement would exceed packet size
    if (pos - measurement_start + encoded_size > max_payload) {
      finish_packet();
      start_new_packet();
    }

    pos += this->encode_binary_measurement_(data + pos, max_payload - (pos - measurement_start),
                                           measurement.object_id, measurement.sensor->state);
  }

  finish_packet();

  ESP_LOGD(TAG, "Built %d advertisement packet(s)", this->adv_packets_.size());
}

size_t BTHome::encode_measurement_(uint8_t *data, size_t max_len, uint8_t object_id, float value) {
  // If data is nullptr, just calculate the size
  if (data == nullptr) {
    switch (object_id) {
      case 0x01:  // battery (uint8)
        return 2;
      case 0x02:  // temperature (sint16)
      case 0x03:  // humidity (uint16)
      case 0x06:  // mass (uint16)
      case 0x08:  // dewpoint (sint16)
      case 0x0C:  // voltage (uint16)
      case 0x0D:  // PM2.5 (uint16)
      case 0x0E:  // PM10 (uint16)
      case 0x12:  // CO2 (uint16)
      case 0x13:  // TVOC (uint16)
      case 0x14:  // moisture (uint16)
      case 0x43:  // current (uint16)
      case 0x44:  // speed (uint16)
        return 3;
      case 0x04:  // pressure (uint24)
      case 0x05:  // illuminance (uint24)
      case 0x0A:  // energy (uint24)
      case 0x0B:  // power (uint24)
        return 4;
      case 0x50:  // timestamp (uint32)
        return 5;
      default:
        return 0;
    }
  }

  size_t pos = 0;
  data[pos++] = object_id;

  // Encode based on object ID
  switch (object_id) {
    case 0x01:  // battery (uint8, 1%)
    {
      if (max_len < 2)
        return 0;
      data[pos++] = static_cast<uint8_t>(std::round(value));
      break;
    }
    case 0x02:  // temperature (sint16, 0.01°C)
    case 0x08:  // dewpoint (sint16, 0.01°C)
    {
      if (max_len < 3)
        return 0;
      int16_t encoded = static_cast<int16_t>(std::round(value * 100.0f));
      data[pos++] = encoded & 0xFF;
      data[pos++] = (encoded >> 8) & 0xFF;
      break;
    }
    case 0x03:  // humidity (uint16, 0.01%)
    case 0x14:  // moisture (uint16, 0.01%)
    {
      if (max_len < 3)
        return 0;
      uint16_t encoded = static_cast<uint16_t>(std::round(value * 100.0f));
      data[pos++] = encoded & 0xFF;
      data[pos++] = (encoded >> 8) & 0xFF;
      break;
    }
    case 0x04:  // pressure (uint24, 0.01 hPa)
    case 0x05:  // illuminance (uint24, 0.01 lux)
    case 0x0A:  // energy (uint24, 0.001 kWh)
    case 0x0B:  // power (uint24, 0.01 W)
    {
      if (max_len < 4)
        return 0;
      float factor = (object_id == 0x0A) ? 1000.0f : 100.0f;
      uint32_t encoded = static_cast<uint32_t>(std::round(value * factor));
      data[pos++] = encoded & 0xFF;
      data[pos++] = (encoded >> 8) & 0xFF;
      data[pos++] = (encoded >> 16) & 0xFF;
      break;
    }
    case 0x06:  // mass (uint16, 0.01 kg)
    case 0x43:  // current (uint16, 0.001 A)
    case 0x44:  // speed (uint16, 0.01 m/s)
    {
      if (max_len < 3)
        return 0;
      float factor = (object_id == 0x43) ? 1000.0f : 100.0f;
      uint16_t encoded = static_cast<uint16_t>(std::round(value * factor));
      data[pos++] = encoded & 0xFF;
      data[pos++] = (encoded >> 8) & 0xFF;
      break;
    }
    case 0x0C:  // voltage (uint16, 0.001 V)
    {
      if (max_len < 3)
        return 0;
      uint16_t encoded = static_cast<uint16_t>(std::round(value * 1000.0f));
      data[pos++] = encoded & 0xFF;
      data[pos++] = (encoded >> 8) & 0xFF;
      break;
    }
    case 0x0D:  // PM2.5 (uint16, 1 µg/m³)
    case 0x0E:  // PM10 (uint16, 1 µg/m³)
    case 0x12:  // CO2 (uint16, 1 ppm)
    case 0x13:  // TVOC (uint16, 1 µg/m³)
    {
      if (max_len < 3)
        return 0;
      uint16_t encoded = static_cast<uint16_t>(std::round(value));
      data[pos++] = encoded & 0xFF;
      data[pos++] = (encoded >> 8) & 0xFF;
      break;
    }
    case 0x50:  // timestamp (uint32, seconds)
    {
      if (max_len < 5)
        return 0;
      uint32_t encoded = static_cast<uint32_t>(value);
      data[pos++] = encoded & 0xFF;
      data[pos++] = (encoded >> 8) & 0xFF;
      data[pos++] = (encoded >> 16) & 0xFF;
      data[pos++] = (encoded >> 24) & 0xFF;
      break;
    }
    default:
      ESP_LOGW(TAG, "Unsupported sensor object ID: 0x%02X", object_id);
      return 0;
  }

  return pos;
}

size_t BTHome::encode_binary_measurement_(uint8_t *data, size_t max_len, uint8_t object_id, bool value) {
  if (data == nullptr)
    return 2;  // Binary sensors are always 2 bytes

  if (max_len < 2)
    return 0;

  data[0] = object_id;
  data[1] = value ? 0x01 : 0x00;
  return 2;
}

bool BTHome::encrypt_payload_(const uint8_t *plaintext, size_t plaintext_len, uint8_t *ciphertext,
                              size_t *ciphertext_len) {
  if (!this->encryption_enabled_) {
    return false;
  }

  // Get MAC address
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_BT);

  // Build nonce according to BTHome spec:
  // MAC (6 bytes) + UUID (2 bytes) + device info (1 byte) + counter (4 bytes) = 13 bytes
  uint8_t nonce[13];
  memcpy(nonce, mac, 6);
  nonce[6] = BTHOME_SERVICE_UUID & 0xFF;          // UUID byte 1
  nonce[7] = (BTHOME_SERVICE_UUID >> 8) & 0xFF;   // UUID byte 2
  nonce[8] = BTHOME_DEVICE_INFO_ENCRYPTED;        // Device info byte
  nonce[9] = this->counter_ & 0xFF;               // Counter byte 0
  nonce[10] = (this->counter_ >> 8) & 0xFF;       // Counter byte 1
  nonce[11] = (this->counter_ >> 16) & 0xFF;      // Counter byte 2
  nonce[12] = (this->counter_ >> 24) & 0xFF;      // Counter byte 3

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

  // Encrypt and generate tag (4-byte MIC)
  ret = mbedtls_ccm_encrypt_and_tag(&ctx, plaintext_len, nonce, sizeof(nonce), nullptr, 0, plaintext, ciphertext,
                                     ciphertext + plaintext_len, 4);

  mbedtls_ccm_free(&ctx);

  if (ret != 0) {
    ESP_LOGE(TAG, "mbedtls_ccm_encrypt_and_tag failed: %d", ret);
    return false;
  }

  *ciphertext_len = plaintext_len + 4;  // Ciphertext + 4-byte MIC
  return true;
}

void BTHome::on_advertise_() {
  // Build advertisement packets if needed
  if (this->data_changed_ || this->adv_packets_.empty()) {
    this->build_advertisement_packets_();
    this->data_changed_ = false;
  }

  if (this->adv_packets_.empty()) {
    ESP_LOGW(TAG, "No advertisement packets to send");
    return;
  }

  // Send current packet
  uint8_t *packet = this->adv_packets_[this->current_packet_index_].get();
  uint16_t size = this->adv_packet_sizes_[this->current_packet_index_];

  ESP_LOGD(TAG, "Setting BLE TX power");
  esp_err_t err = esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, this->tx_power_);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "esp_ble_tx_power_set failed: %s", esp_err_to_name(err));
  }

  ESP_LOGD(TAG, "Starting BTHome advertisement packet %d/%d (%d bytes)", this->current_packet_index_ + 1,
           this->adv_packets_.size(), size);
  err = esp_ble_gap_config_adv_data_raw(packet, size);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_ble_gap_config_adv_data_raw failed: %s", esp_err_to_name(err));
    return;
  }

  // Cycle to next packet for next time
  this->current_packet_index_ = (this->current_packet_index_ + 1) % this->adv_packets_.size();
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
