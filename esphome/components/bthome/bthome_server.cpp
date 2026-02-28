#include "bthome_server.h"

#ifdef USE_ESP32
#ifdef USE_BTHOME_SERVER

namespace esphome {
namespace bthome {
namespace server {

void BTHomeServerBase::setup() {
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
  auto sensors = this->get_local_sensors();
  for (size_t i = 0; i < sensors.size(); i++) {
    if (sensors[i]->get_advertise_immediately()) {
      BTHomeObjectType type = sensors[i]->get_object_type();
      sensors[i]->register_immediate_callback([this, type]() { this->advertise_immediate_(type); });
    }
  }
}

void BTHomeServerBase::dump_config() {
  auto sensors = this->get_local_sensors();
  ESP_LOGCONFIG("bthome.server", "BTHome Server:");
  ESP_LOGCONFIG("bthome.server", "  Local sensors: %zu", sensors.size());
  ESP_LOGCONFIG("bthome.server", "  MAC address: %s", this->local_mac_.c_str());
#ifdef USE_BTHOME_DECRYPTION
  if (this->encryption_key_.has_value()) {
    ESP_LOGCONFIG("bthome.server", "  Encryption: enabled");
  }
#endif
}

void BTHomeServerBase::on_advertise_() {
  auto sensors = this->get_local_sensors();
  this->encoder_.reset();

  // Pack sensors starting from next_sensor_index_, greedily filling the frame
  size_t start = this->next_sensor_index_;
  size_t packed = 0;

  for (size_t i = 0; i < sensors.size(); i++) {
    size_t idx = (start + i) % sensors.size();
    BTHomeLocalBase *sensor = sensors[idx];

    // Check if this sensor fits
    if (this->encoder_.get_remaining() < sensor->get_encoded_size()) {
      // Frame is full — stop here
      this->next_sensor_index_ = idx;
      break;
    }

    sensor->write(this->encoder_);
    packed++;

    // If we've packed all sensors, wrap around
    if (packed == sensors.size()) {
      this->next_sensor_index_ = 0;
      break;
    }
  }

  if (this->encoder_.get_size() > 0) {
    this->send_frame_();
  }
}

void BTHomeServerBase::send_frame_() {
  // Build BTHome header byte
  BTHomeHeader header{};
  header.version = BTHOME_VERSION_2;
#ifdef USE_BTHOME_DECRYPTION
  header.encrypted = this->encryption_key_.has_value() ? 1 : 0;
#endif

  const uint8_t *payload = this->encoder_.get_buffer();
  size_t payload_size = this->encoder_.get_size();

#ifdef USE_BTHOME_DECRYPTION
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

void BTHomeServerBase::advertise_immediate_(BTHomeObjectType type) {
  auto sensors = this->get_local_sensors();
  this->encoder_.reset();

  // Find and encode all sensors matching this object type
  for (size_t i = 0; i < sensors.size(); i++) {
    if (sensors[i]->get_object_type() == type) {
      if (!sensors[i]->write(this->encoder_)) {
        ESP_LOGW("bthome.server", "Immediate advertisement: sensors of same type don't fit in frame");
        return;
      }
    }
  }

  if (this->encoder_.get_size() > 0) {
    this->send_frame_();
  }
}

void BTHomeServerBase::gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
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

}  // namespace server
}  // namespace bthome
}  // namespace esphome

#endif  // USE_BTHOME_SERVER
#endif  // USE_ESP32
