#include "bthome_server.h"
#include "ble_esp32.h"

namespace esphome {
namespace bthome {
namespace server {

// ---------------------------------------------------------------------------
// BTHomeServerBase
// ---------------------------------------------------------------------------

BTHomeServerBase::BTHomeServerBase(IBLEAdapter *ble_adapter) : ble_adapter_(ble_adapter) {}

void BTHomeServerBase::setup() {
  this->ble_adapter_->setup(this);

  // Read local BLE MAC address
  MacAddressPtr mac = this->ble_adapter_->get_local_mac();
  if (mac != nullptr) {
    this->local_mac_ = mac;
  }

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

void BTHomeServerBase::on_advertise(bool active) {
  if (!active) {
    return;
  }
  auto sensors = this->get_local_sensors();
  this->encoder_.reset();

  // Pack sensors starting from next_sensor_index_
  // All sensors of the same type must fit in the same frame
  size_t start = this->next_sensor_index_;

  for (size_t i = 0; i < sensors.size();) {
    size_t idx = (start + i) % sensors.size();
    BTHomeLocalBase *sensor = sensors[idx];
    BTHomeObjectType current_type = sensor->get_object_type();

    // Find the complete group of consecutive sensors with the same type
    size_t group_size = 1;
    for (size_t j = 1; j < sensors.size() && i + j < sensors.size(); j++) {
      size_t next_idx = (start + i + j) % sensors.size();
      if (sensors[next_idx]->get_object_type() != current_type) {
        break;
      }
      group_size++;
    }

    // Calculate total size needed for this group
    size_t group_encoded_size = 0;
    for (size_t j = 0; j < group_size; j++) {
      size_t member_idx = (start + i + j) % sensors.size();
      size_t encoded_size = sensors[member_idx]->get_encoded_size();
      if (encoded_size == 0) {
        group_encoded_size = 0;
        break;  // Skip entire group if any sensor doesn't have valid state
      }
      group_encoded_size += encoded_size;
    }

    // Check if entire group can be sent
    if (group_encoded_size == 0 || this->encoder_.get_remaining() < group_encoded_size) {
      // Group doesn't fit or invalid sensor value — skip this group and come back to it next time
      this->next_sensor_index_ = idx;
      break;
    }

    // Add entire group to frame
    for (size_t j = 0; j < group_size; j++) {
      size_t member_idx = (start + i + j) % sensors.size();
      sensors[member_idx]->write(this->encoder_);
    }

    i += group_size;

    // If we've processed all sensors, wrap around
    if (i >= sensors.size()) {
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
  const uint8_t *payload = this->encoder_.get_buffer();
  size_t payload_size = this->encoder_.get_size();
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

  ESP_LOGD("bthome.server", "Advertising frame with payload size %zu (total %zu): content: %s", payload_size, total,
           format_hex(payload, payload_size).c_str());

  this->ble_adapter_->config_adv_data_raw(this->adv_buffer_, pos);
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

}  // namespace server
}  // namespace bthome
}  // namespace esphome
