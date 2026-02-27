#pragma once
#include <array>
#include <cstddef>
#include <cstring>

#include "bthome_decoder.h"
#include "bthome_encoder.h"
#include "bthome_local_sensor.h"
#include "bthome_mac.h"
#include "esphome/components/esp32_ble/ble.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

#ifdef USE_BTHOME_SERVER

#ifndef CONFIG_ESP_HOSTED_ENABLE_BT_BLUEDROID
#include <esp_bt.h>
#endif
#include <esp_bt_main.h>
#include <esp_gap_ble_api.h>
#include "esp_bt_device.h"
#ifdef USE_BTHOME_DECRYPTION
#include "bthome_encryption.h"
#endif

namespace esphome {
namespace bthome {
namespace server {

static constexpr size_t BLE_FLAGS_SIZE = 3;       // [02 01 06]
static constexpr size_t BLE_SVC_HEADER_SIZE = 4;  // [LL 16 D2 FC]
static constexpr size_t BLE_ADV_MAX_SIZE = 31;
static constexpr uint8_t BTHOME_VERSION_2 = 0x02;

template<size_t N> class BTHomeServer : public Component, public esp32_ble::GAPEventHandler {
 public:
  void setup() override {
    // Read local BLE MAC address
    const uint8_t *mac = esp_bt_dev_get_address();
    if (mac != nullptr) {
      this->local_mac_ = MacAddress(mac);
    }

    // Configure advertising parameters (non-connectable, undirected)
    this->ble_adv_params_.adv_int_min = 0x20;  // 20ms
    this->ble_adv_params_.adv_int_max = 0x40;  // 40ms
    this->ble_adv_params_.adv_type = ADV_TYPE_NONCONN_IND;
    this->ble_adv_params_.own_addr_type = BLE_ADDR_TYPE_PUBLIC;
    this->ble_adv_params_.channel_map = ADV_CHNL_ALL;
    this->ble_adv_params_.adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY;

    // Register raw advertisement callback for cycling
    esp32_ble::global_ble->advertising_register_raw_advertisement_callback([this](bool advertise) {
      this->advertising_ = advertise;
      if (advertise) {
        this->on_advertise_();
      }
    });

    // Register immediate callbacks for sensors that need it
    for (size_t i = 0; i < N; i++) {
      if (this->local_sensors_[i]->get_advertise_immediately()) {
        BTHomeObjectType type = this->local_sensors_[i]->get_object_type();
        this->local_sensors_[i]->register_immediate_callback([this, type]() { this->advertise_immediate_(type); });
      }
    }
  }

  void dump_config() override {
    ESP_LOGCONFIG("bthome.server", "BTHome Server:");
    ESP_LOGCONFIG("bthome.server", "  Local sensors: %zu", N);
    ESP_LOGCONFIG("bthome.server", "  MAC address: %s", this->local_mac_.c_str());
#ifdef USE_BTHOME_DECRYPTION
    if (this->encryption_key_.has_value()) {
      ESP_LOGCONFIG("bthome.server", "  Encryption: enabled");
    }
#endif
  }

  float get_setup_priority() const override { return setup_priority::AFTER_BLUETOOTH; }

  void set_local_sensor(size_t index, BTHomeLocalBase *sensor) { this->local_sensors_[index] = sensor; }

#ifdef USE_BTHOME_DECRYPTION
  void set_encryption_key(std::initializer_list<uint8_t> key) {
    EncryptionKey k{};
    std::copy(key.begin(), key.end(), k.begin());
    this->encryption_key_ = k;
  }
#endif

  void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) override {
    switch (event) {
      case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT: {
        if (this->advertising_) {
          esp_err_t err = esp_ble_gap_start_advertising(&this->ble_adv_params_);
          if (err != ESP_OK) {
            ESP_LOGW("bthome.server", "esp_ble_gap_start_advertising failed: %s", esp_err_to_name(err));
          }
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

 protected:
  void on_advertise_() {
    this->encoder_.reset();

    // Pack sensors starting from next_sensor_index_, greedily filling the frame
    size_t start = this->next_sensor_index_;
    size_t packed = 0;

    for (size_t i = 0; i < N; i++) {
      size_t idx = (start + i) % N;
      BTHomeLocalBase *sensor = this->local_sensors_[idx];

      // Check if this sensor fits
      if (this->encoder_.get_remaining() < sensor->get_encoded_size()) {
        // Frame is full — stop here
        this->next_sensor_index_ = idx;
        break;
      }

      sensor->write(this->encoder_);
      packed++;

      // If we've packed all sensors, wrap around
      if (packed == N) {
        this->next_sensor_index_ = 0;
        break;
      }
    }

    if (this->encoder_.get_size() > 0) {
      this->send_frame_();
    }
  }

  void send_frame_() {
    // Build BTHome header byte
    const uint8_t *payload = this->encoder_.get_buffer();
    size_t payload_size = this->encoder_.get_size();
    BTHomeHeader header{};
    header.version = BTHOME_VERSION_2;

#ifdef USE_BTHOME_DECRYPTION
    header.encrypted = this->encryption_key_.has_value() ? 1 : 0;
    size_t encrypted_size = 0;
    const uint8_t *encrypted = nullptr;
    if (this->encryption_key_.has_value()) {
      encrypted = bthome_encrypt(payload, payload_size, MacAddressPtr(this->local_mac_), this->encryption_counter_++,
                                 header, *this->encryption_key_, encrypted_size);
      if (encrypted == nullptr) {
        ESP_LOGW("bthome.server", "Encryption failed");
        return;
      }
      payload = encrypted;
      payload_size = encrypted_size;
    }
#endif

    // Build raw advertisement: [flags][service data AD]
    size_t svc_data_len = 1 + 2 + 1 + payload_size;    // AD type + UUID(2) + header + payload
    size_t total = BLE_FLAGS_SIZE + 1 + svc_data_len;  // flags + length byte + service data

    if (total > BLE_ADV_MAX_SIZE) {
      ESP_LOGW("bthome.server", "Advertisement too large: %zu > %zu", total, BLE_ADV_MAX_SIZE);
      return;
    }

    size_t pos = 0;
    // BLE Flags AD structure
    this->adv_buffer_[pos++] = 0x02;  // Length
    this->adv_buffer_[pos++] = 0x01;  // AD Type: Flags
    this->adv_buffer_[pos++] = 0x06;  // General Discoverable + BR/EDR Not Supported

    // Service Data AD structure
    this->adv_buffer_[pos++] = static_cast<uint8_t>(svc_data_len);  // Length
    this->adv_buffer_[pos++] = 0x16;                                // AD Type: Service Data 16-bit UUID
    this->adv_buffer_[pos++] = 0xD2;                                // BTHome UUID low byte
    this->adv_buffer_[pos++] = 0xFC;                                // BTHome UUID high byte

    // BTHome header
    uint8_t header_byte;
    memcpy(&header_byte, &header, 1);
    this->adv_buffer_[pos++] = header_byte;

    // Payload
    memcpy(&this->adv_buffer_[pos], payload, payload_size);
    pos += payload_size;

    esp_ble_gap_config_adv_data_raw(this->adv_buffer_, pos);
  }

  void advertise_immediate_(BTHomeObjectType type) {
    this->encoder_.reset();

    // Find and encode all sensors matching this object type
    for (size_t i = 0; i < N; i++) {
      if (this->local_sensors_[i]->get_object_type() == type) {
        if (!this->local_sensors_[i]->write(this->encoder_)) {
          ESP_LOGW("bthome.server", "Immediate advertisement: sensors of same type don't fit in frame");
          return;
        }
      }
    }

    if (this->encoder_.get_size() > 0) {
      this->send_frame_();
    }
  }

  std::array<BTHomeLocalBase *, N> local_sensors_{};
  size_t next_sensor_index_{0};
  BTHomeEncoder encoder_;
  bool advertising_{false};
  esp_ble_adv_params_t ble_adv_params_{};
  MacAddress local_mac_;
  uint8_t adv_buffer_[BLE_ADV_MAX_SIZE]{};

#ifdef USE_BTHOME_DECRYPTION
  optional<EncryptionKey> encryption_key_;
  uint32_t encryption_counter_{0};
#endif
};

}  // namespace server
}  // namespace bthome
}  // namespace esphome

#endif  // USE_BTHOME_SERVER

#endif  // USE_ESP32
