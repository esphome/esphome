#include "bthome_ble.h"

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <array>
#include <span>

#ifdef USE_ESP32

namespace esphome {
namespace bthome_mithermometer {

static const char *const TAG = "bthome_mithermometer";

static const char *format_mac_address(std::span<char, MAC_ADDRESS_PRETTY_BUFFER_SIZE> buffer, uint64_t address) {
  std::array<uint8_t, MAC_ADDRESS_SIZE> mac{};
  for (size_t i = 0; i < MAC_ADDRESS_SIZE; i++) {
    mac[i] = (address >> ((MAC_ADDRESS_SIZE - 1 - i) * 8)) & 0xFF;
  }

  format_mac_addr_upper(mac.data(), buffer.data());
  return buffer.data();
}

static bool get_bthome_value_length(uint8_t obj_type, size_t &value_length) {
  switch (obj_type) {
    case 0x00:  // packet id
    case 0x01:  // battery
    case 0x09:  // count (uint8)
    case 0x0F:  // generic boolean
    case 0x10:  // power (bool)
    case 0x11:  // opening
    case 0x15:  // battery low
    case 0x16:  // battery charging
    case 0x17:  // carbon monoxide
    case 0x18:  // cold
    case 0x19:  // connectivity
    case 0x1A:  // door
    case 0x1B:  // garage door
    case 0x1C:  // gas
    case 0x1D:  // heat
    case 0x1E:  // light
    case 0x1F:  // lock
    case 0x20:  // moisture
    case 0x21:  // motion
    case 0x22:  // moving
    case 0x23:  // occupancy
    case 0x24:  // plug
    case 0x25:  // presence
    case 0x26:  // problem
    case 0x27:  // running
    case 0x28:  // safety
    case 0x29:  // smoke
    case 0x2A:  // sound
    case 0x2B:  // tamper
    case 0x2C:  // vibration
    case 0x2D:  // water leak
    case 0x2E:  // humidity (uint8)
    case 0x2F:  // moisture (uint8)
    case 0x46:  // UV index
    case 0x57:  // temperature (sint8)
    case 0x58:  // temperature (0.35C step)
    case 0x59:  // count (sint8)
    case 0x60:  // channel
      value_length = 1;
      return true;
    case 0x02:  // temperature (0.01C)
    case 0x03:  // humidity
    case 0x06:  // mass (kg)
    case 0x07:  // mass (lb)
    case 0x08:  // dewpoint
    case 0x0C:  // voltage (mV)
    case 0x0D:  // pm2.5
    case 0x0E:  // pm10
    case 0x12:  // CO2
    case 0x13:  // TVOC
    case 0x14:  // moisture
    case 0x3D:  // count (uint16)
    case 0x3F:  // rotation
    case 0x40:  // distance (mm)
    case 0x41:  // distance (m)
    case 0x43:  // current (A)
    case 0x44:  // speed
    case 0x45:  // temperature (0.1C)
    case 0x47:  // volume (L)
    case 0x48:  // volume (mL)
    case 0x49:  // volume flow rate
    case 0x4A:  // voltage (0.1V)
    case 0x51:  // acceleration
    case 0x52:  // gyroscope
    case 0x56:  // conductivity
    case 0x5A:  // count (sint16)
    case 0x5D:  // current (sint16)
    case 0x5E:  // direction
    case 0x5F:  // precipitation
    case 0x61:  // rotational speed
    case 0xF0:  // button event
      value_length = 2;
      return true;
    case 0x04:  // pressure
    case 0x05:  // illuminance
    case 0x0A:  // energy
    case 0x0B:  // power
    case 0x42:  // duration
    case 0x4B:  // gas (uint24)
    case 0xF2:  // firmware version (uint24)
      value_length = 3;
      return true;
    case 0x3E:  // count (uint32)
    case 0x4C:  // gas (uint32)
    case 0x4D:  // energy (uint32)
    case 0x4E:  // volume (uint32)
    case 0x4F:  // water (uint32)
    case 0x50:  // timestamp
    case 0x55:  // volume storage
    case 0x5B:  // count (sint32)
    case 0x5C:  // power (sint32)
    case 0x62:  // speed (sint32)
    case 0x63:  // acceleration (sint32)
    case 0xF1:  // firmware version (uint32)
      value_length = 4;
      return true;
    default:
      return false;
  }
}

void BTHomeMiThermometer::dump_config() {
  char addr_buf[MAC_ADDRESS_PRETTY_BUFFER_SIZE];
  ESP_LOGCONFIG(TAG, "BTHome MiThermometer");
  ESP_LOGCONFIG(TAG, "  MAC Address: %s", format_mac_address(addr_buf, this->address_));
  LOG_SENSOR("  ", "Temperature", this->temperature_);
  LOG_SENSOR("  ", "Humidity", this->humidity_);
  LOG_SENSOR("  ", "Battery Level", this->battery_level_);
  LOG_SENSOR("  ", "Battery Voltage", this->battery_voltage_);
  LOG_SENSOR("  ", "Signal Strength", this->signal_strength_);
}

bool BTHomeMiThermometer::parse_device(const esp32_ble_tracker::ESPBTDevice &device) {
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

bool BTHomeMiThermometer::handle_service_data_(const esp32_ble_tracker::ServiceData &service_data,
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

  char addr_buf[MAC_ADDRESS_PRETTY_BUFFER_SIZE];
  if (is_encrypted) {
    ESP_LOGV(TAG, "Ignoring encrypted BTHome frame from %s", device.address_str_to(addr_buf));
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
    ESP_LOGVV(TAG, "BTHome frame from unexpected device %s", format_mac_address(addr_buf, source_address));
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
    bool has_length_byte = obj_type == 0x53;  // text objects include explicit length

    if (has_length_byte) {
      if (offset >= data.size()) {
        break;
      }
      value_length = data[offset++];
    } else {
      if (!get_bthome_value_length(obj_type, value_length)) {
        ESP_LOGVV(TAG, "Unknown BTHome object 0x%02X", obj_type);
        break;
      }
    }

    if (value_length == 0) {
      break;
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
      case 0x0C: {  // battery voltage (mV)
        if (this->battery_voltage_ != nullptr) {
          const uint16_t raw = encode_uint16(value[1], value[0]);
          this->battery_voltage_->publish_state(raw * 0.001f);
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
      default:
        break;
    }
  }

  if (reported) {
    ESP_LOGD(TAG, "BTHome data%sfrom %s", is_trigger_based ? " (triggered) " : " ", device.address_str_to(addr_buf));
  }

  return reported;
}

}  // namespace bthome_mithermometer
}  // namespace esphome

#endif
