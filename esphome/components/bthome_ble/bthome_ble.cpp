#include "bthome_ble.h"

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

namespace esphome {
namespace bthome_ble {

static const char *const TAG = "bthome_ble";

void BTHomeBLE::dump_config() {
  ESP_LOGCONFIG(TAG, "BTHome BLE");
  ESP_LOGCONFIG(TAG, "  MAC Address: %s", format_mac(this->address_).c_str());
  LOG_SENSOR("  ", "Temperature", this->temperature_);
  LOG_SENSOR("  ", "Humidity", this->humidity_);
  LOG_SENSOR("  ", "Battery Level", this->battery_level_);
  LOG_SENSOR("  ", "Signal Strength", this->signal_strength_);
  LOG_BINARY_SENSOR("  ", "Battery Low", this->battery_low_);
  LOG_TEXT_SENSOR("  ", "Firmware", this->firmware_);
}

bool BTHomeBLE::parse_device(const esp32_ble_tracker::ESPBTDevice &device) {
  bool matched = false;
  for (auto &service_data : device.get_service_datas()) {
    if (this->handle_service_data_(service_data, device)) {
      matched = true;
    }
  }
  if (matched && this->signal_strength_ != nullptr) {
    this->signal_strength_->publish_state(device.get_rssi());
  }
  return matched;
}

bool BTHomeBLE::handle_service_data_(const esp32_ble_tracker::ServiceData &service_data,
                                     const esp32_ble_tracker::ESPBTDevice &device) {
  if (!service_data.uuid.contains(0xD2, 0xFC)) {
    return false;
  }

  const auto &data = service_data.data;
  if (data.size() < 2) {
    ESP_LOGVV(TAG, "BTHome data too short: %zu", data.size());
    return false;
  }

  const uint8_t adv_info = data[0];
  const bool is_encrypted = adv_info & 0x01;
  const bool mac_included = adv_info & 0x02;
  const bool is_trigger_based = adv_info & 0x04;
  const uint8_t version = (adv_info >> 5) & 0x07;

  if (version != 0x02) {
    ESP_LOGVV(TAG, "Unsupported BTHome version %u", version);
    return false;
  }

  if (is_encrypted) {
    ESP_LOGV(TAG, "Ignoring encrypted BTHome frame from %s", device.address_str().c_str());
    return false;
  }

  size_t payload_index = 1;
  uint64_t source_address = device.address_uint64();

  if (mac_included) {
    if (data.size() < 7) {
      ESP_LOGVV(TAG, "BTHome payload missing MAC address");
      return false;
    }
    source_address = 0;
    for (int i = 5; i >= 0; i--) {
      source_address = (source_address << 8) | data[1 + i];
    }
    payload_index = 7;
  }

  if (source_address != this->address_) {
    ESP_LOGVV(TAG, "BTHome frame from unexpected device %s", format_mac(source_address).c_str());
    return false;
  }

  if (payload_index >= data.size()) {
    ESP_LOGVV(TAG, "BTHome payload empty after header");
    return false;
  }

  bool reported = false;
  size_t offset = payload_index;
  uint8_t last_type = 0;

  while (offset < data.size()) {
    const uint8_t obj_type = data[offset++];
    size_t value_length = 0;
    bool has_length_byte = false;

    if (obj_type == 0x53 || obj_type == 0xF1) {
      has_length_byte = true;
    }

    if (has_length_byte) {
      if (offset >= data.size()) {
        break;
      }
      value_length = data[offset++];
    } else {
      switch (obj_type) {
        case 0x00:
        case 0x01:
        case 0x15:
          value_length = 1;
          break;
        case 0x02:
        case 0x03:
          value_length = 2;
          break;
        default:
          // Unknown type, stop parsing to avoid misalignment
          ESP_LOGVV(TAG, "Unknown BTHome object 0x%02X", obj_type);
          return reported;
      }
    }

    if (offset + value_length > data.size()) {
      ESP_LOGVV(TAG, "BTHome object length exceeds payload");
      break;
    }

    const uint8_t *value = &data[offset];
    offset += value_length;

    if (obj_type < last_type) {
      ESP_LOGVV(TAG, "BTHome objects not in ascending order");
    }
    last_type = obj_type;

    switch (obj_type) {
      case 0x00: {  // packet id
        const uint8_t packet_id = value[0];
        if (this->last_packet_id_.has_value() && *this->last_packet_id_ == packet_id) {
          return reported;
        }
        this->last_packet_id_ = packet_id;
        break;
      }
      case 0x01: {  // battery percentage
        if (this->battery_level_ != nullptr) {
          this->battery_level_->publish_state(value[0]);
          reported = true;
        }
        break;
      }
      case 0x02: {  // temperature
        if (this->temperature_ != nullptr) {
          const int16_t raw = encode_uint16(value[1], value[0]);
          this->temperature_->publish_state(raw * 0.01f);
          reported = true;
        }
        break;
      }
      case 0x03: {  // humidity
        if (this->humidity_ != nullptr) {
          const uint16_t raw = encode_uint16(value[1], value[0]);
          this->humidity_->publish_state(raw * 0.01f);
          reported = true;
        }
        break;
      }
      case 0x15: {  // battery low binary sensor
        if (this->battery_low_ != nullptr) {
          this->battery_low_->publish_state(value[0] != 0);
          reported = true;
        }
        break;
      }
      case 0x53:  // text
      case 0xF1: {  // firmware version (custom)
        if (this->firmware_ != nullptr) {
          std::string text_value(reinterpret_cast<const char *>(value), value_length);
          this->firmware_->publish_state(text_value);
          reported = true;
        }
        break;
      }
      default:
        break;
    }
  }

  if (reported) {
    ESP_LOGD(TAG, "BTHome data%sfrom %s", is_trigger_based ? " (triggered) " : " ",
             device.address_str().c_str());
  }

  return reported;
}

}  // namespace bthome_ble
}  // namespace esphome

#endif
