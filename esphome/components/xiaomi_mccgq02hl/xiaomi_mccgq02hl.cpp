#include "xiaomi_mccgq02hl.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::xiaomi_mccgq02hl {

static const char *const TAG = "xiaomi_mccgq02hl";

static constexpr uint16_t PRODUCT_ID = 0x098b;

// MiBeacon frame control, low byte (raw[0])
static constexpr uint8_t FC_ENCRYPTED = 0x08;
static constexpr uint8_t FC_MAC_INCLUDED = 0x10;
static constexpr uint8_t FC_CAPABILITY = 0x20;
static constexpr uint8_t FC_OBJECT = 0x40;

// Encrypted frames end in a 3-byte extended counter and a 4-byte MIC.
static constexpr size_t ENCRYPTED_TRAILER = 7;

// Object ids. The door and light objects come in both the 0x00xx and the
// 0x10xx flavour depending on firmware; the semantics are identical.
static constexpr uint16_t OBJ_LIGHT = 0x0018;
static constexpr uint16_t OBJ_LIGHT_ALT = 0x1018;
static constexpr uint16_t OBJ_DOOR = 0x0019;
static constexpr uint16_t OBJ_DOOR_ALT = 0x1019;
static constexpr uint16_t OBJ_BATTERY = 0x100A;
static constexpr uint16_t OBJ_BATTERY_ALT = 0x4803;

void XiaomiMCCGQ02HL::dump_config() {
  ESP_LOGCONFIG(TAG, "Xiaomi MCCGQ02HL");
  LOG_BINARY_SENSOR("  ", "Opening", this);
  LOG_BINARY_SENSOR("  ", "Open", this->open_);
  LOG_BINARY_SENSOR("  ", "Light", this->light_);
  LOG_SENSOR("  ", "Battery Level", this->battery_level_);
}

void XiaomiMCCGQ02HL::set_bindkey(const char *bindkey) { parse_hex(bindkey, this->bindkey_, sizeof(this->bindkey_)); }

bool XiaomiMCCGQ02HL::parse_device(const ble_device_base::ESPBTDevice &device) {
  if (device.address_uint64() != this->address_)
    return false;

  bool success = false;
  for (auto &service_data : device.get_service_datas()) {
    if (!service_data.uuid.contains(0x95, 0xFE))
      continue;

    Reading reading;
    // Pass a copy: decryption rewrites the buffer in place, and the tracker's
    // service data is shared with every other listener.
    if (!this->parse_service_data_(service_data.data, reading))
      continue;

    ESP_LOGD(TAG, "%s: open=%s light=%s battery=%s", this->get_name().c_str(),
             reading.open.has_value() ? (*reading.open ? "yes" : "no") : "-",
             reading.light.has_value() ? (*reading.light ? "yes" : "no") : "-",
             reading.battery_level.has_value() ? "updated" : "-");

    if (reading.open.has_value()) {
      this->publish_state(*reading.open);
      if (this->open_ != nullptr)
        this->open_->publish_state(*reading.open);
    }
    if (reading.light.has_value() && this->light_ != nullptr)
      this->light_->publish_state(*reading.light);
    if (reading.battery_level.has_value() && this->battery_level_ != nullptr)
      this->battery_level_->publish_state(*reading.battery_level);
    success = true;
  }
  return success;
}

bool XiaomiMCCGQ02HL::parse_service_data_(std::vector<uint8_t> raw, Reading &reading) {
  if (raw.size() < 5)
    return false;

  const uint8_t fc = raw[0];
  if (!(fc & FC_OBJECT))
    return false;
  if (encode_uint16(raw[3], raw[2]) != PRODUCT_ID) {
    ESP_LOGVV(TAG, "Not an MCCGQ02HL frame (product id %02X%02X).", raw[3], raw[2]);
    return false;
  }

  const uint8_t frame_count = raw[4];
  if (this->last_frame_count_.has_value() && *this->last_frame_count_ == frame_count) {
    ESP_LOGVV(TAG, "Duplicate frame %u.", frame_count);
    return false;
  }
  this->last_frame_count_ = frame_count;

  size_t offset = 5 + ((fc & FC_MAC_INCLUDED) ? 6 : 0) + ((fc & FC_CAPABILITY) ? 1 : 0);
  size_t end = raw.size();

  if (fc & FC_ENCRYPTED) {
    // decrypt_xiaomi_payload() assumes the ciphertext starts at byte 5 for a
    // 19-byte frame and at byte 11 otherwise (MAC included, no capability).
    const size_t cipher_pos = (raw.size() == 19) ? 5 : 11;
    if (offset != cipher_pos) {
      ESP_LOGW(TAG, "Unsupported encrypted frame layout (fc=0x%02X, %u bytes).", fc, (unsigned) raw.size());
      return false;
    }
    if (!xiaomi_ble::decrypt_xiaomi_payload(raw, this->bindkey_, this->address_)) {
      ESP_LOGW(TAG, "Decryption failed (%u-byte frame) -- check the bindkey.", (unsigned) raw.size());
      return false;
    }
    end = raw.size() - ENCRYPTED_TRAILER;
  }

  if (offset >= end)
    return false;
  return parse_objects_(raw.data() + offset, end - offset, reading);
}

bool XiaomiMCCGQ02HL::parse_objects_(const uint8_t *payload, size_t length, Reading &reading) {
  bool found = false;
  // Each object: id (uint16 LE), length (uint8), value.
  while (length >= 3) {
    const uint16_t id = encode_uint16(payload[1], payload[0]);
    const uint8_t len = payload[2];
    if (len < 1 || length < 3u + len)
      break;
    const uint8_t *value = payload + 3;

    if ((id == OBJ_DOOR || id == OBJ_DOOR_ALT) && len == 1) {
      // 0 = open, 1 = closed, 2 = left open past timeout, 3 = device reset
      if (value[0] <= 2) {
        reading.open = value[0] != 1;
        found = true;
      }
    } else if ((id == OBJ_LIGHT || id == OBJ_LIGHT_ALT) && len == 1) {
      reading.light = value[0] != 0;
      found = true;
    } else if ((id == OBJ_BATTERY || id == OBJ_BATTERY_ALT) && len == 1) {
      reading.battery_level = value[0];
      found = true;
    } else {
      ESP_LOGVV(TAG, "Ignoring object 0x%04X (%u bytes).", id, len);
    }

    payload += 3 + len;
    length -= 3 + len;
  }
  return found;
}

}  // namespace esphome::xiaomi_mccgq02hl
