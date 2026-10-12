#include "xiaomi_mccgq02hl.h"
#include "esphome/components/ble_device_base/ble_aes_ccm.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <cstring>

namespace esphome::xiaomi_mccgq02hl {

ESPHOME_LOG_TAG(TAG, "xiaomi_mccgq02hl");

static constexpr uint16_t PRODUCT_ID = 0x098b;

// MiBeacon frame control, low byte (raw[0])
static constexpr uint8_t FC_ENCRYPTED = 0x08;
static constexpr uint8_t FC_MAC_INCLUDED = 0x10;
static constexpr uint8_t FC_CAPABILITY = 0x20;
static constexpr uint8_t FC_OBJECT = 0x40;

// Encrypted frames end in a 3-byte extended counter and a 4-byte MIC.
static constexpr size_t MIC_SIZE = 4;
static constexpr size_t ENCRYPTED_TRAILER = 3 + MIC_SIZE;
// Largest encrypted payload accepted; MCCGQ02HL objects are 4 bytes.
static constexpr size_t MAX_PAYLOAD_SIZE = 16;

// Object ids. The door and light objects come in both the 0x00xx and the
// 0x10xx flavour depending on firmware; the semantics are identical.
static constexpr uint16_t OBJ_LIGHT = 0x0018;
static constexpr uint16_t OBJ_LIGHT_ALT = 0x1018;
static constexpr uint16_t OBJ_DOOR = 0x0019;
static constexpr uint16_t OBJ_DOOR_ALT = 0x1019;
static constexpr uint16_t OBJ_BATTERY = 0x100A;
static constexpr uint16_t OBJ_BATTERY_ALT = 0x4803;

void XiaomiMCCGQ02HL::dump_config() {
  uint8_t mac[MAC_ADDRESS_SIZE];
  ble_device_base::uint64_to_mac_msb_first(this->address_, mac);
  char mac_buf[MAC_ADDRESS_PRETTY_BUFFER_SIZE];
  ESP_LOGCONFIG(TAG,
                "Xiaomi MCCGQ02HL\n"
                "  MAC Address: %s",
                format_mac_addr_upper(mac, mac_buf));
  LOG_BINARY_SENSOR("  ", "Opening", this);
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
    if (!this->parse_service_data_(service_data.data, reading))
      continue;

    ESP_LOGD(TAG, "%s: open=%s light=%s battery=%s", this->get_name().c_str(),
             reading.open.has_value() ? (*reading.open ? LOG_STR_LITERAL("yes") : LOG_STR_LITERAL("no"))
                                      : LOG_STR_LITERAL("-"),
             reading.light.has_value() ? (*reading.light ? LOG_STR_LITERAL("yes") : LOG_STR_LITERAL("no"))
                                       : LOG_STR_LITERAL("-"),
             reading.battery_level.has_value() ? LOG_STR_LITERAL("updated") : LOG_STR_LITERAL("-"));

    if (reading.open.has_value())
      this->publish_state(*reading.open);
    if (reading.light.has_value() && this->light_ != nullptr)
      this->light_->publish_state(*reading.light);
    if (reading.battery_level.has_value() && this->battery_level_ != nullptr)
      this->battery_level_->publish_state(*reading.battery_level);
    success = true;
  }
  return success;
}

bool XiaomiMCCGQ02HL::decrypt_(const uint8_t *frame, size_t size, size_t offset, uint8_t *plaintext) const {
  uint8_t nonce[MAC_ADDRESS_SIZE + 6];
  for (size_t i = 0; i < MAC_ADDRESS_SIZE; i++)
    nonce[i] = static_cast<uint8_t>(this->address_ >> (i * 8));               // MAC, reversed
  memcpy(nonce + MAC_ADDRESS_SIZE, frame + 2, 3);                             // product id + frame count
  memcpy(nonce + MAC_ADDRESS_SIZE + 3, frame + size - ENCRYPTED_TRAILER, 3);  // extended counter
  static constexpr uint8_t AUTH_DATA[1] = {0x11};
  return ble_device_base::aes_ccm_auth_decrypt(this->bindkey_, nonce, sizeof(nonce), AUTH_DATA, sizeof(AUTH_DATA),
                                               frame + offset, size - offset - ENCRYPTED_TRAILER, plaintext,
                                               frame + size - MIC_SIZE, MIC_SIZE);
}

bool XiaomiMCCGQ02HL::parse_service_data_(const std::vector<uint8_t> &data, Reading &reading) {
  const size_t size = data.size();
  if (size < 5)
    return false;
  const uint8_t *raw = data.data();

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

  // The bindkey is required, so plaintext frames are never trusted
  if (!(fc & FC_ENCRYPTED)) {
    ESP_LOGVV(TAG, "Ignoring unencrypted frame %u.", frame_count);
    return false;
  }

  const size_t offset = 5 + ((fc & FC_MAC_INCLUDED) ? 6 : 0) + ((fc & FC_CAPABILITY) ? 1 : 0);
  if (size <= offset + ENCRYPTED_TRAILER || size - offset - ENCRYPTED_TRAILER > MAX_PAYLOAD_SIZE) {
    ESP_LOGW(TAG, "Unsupported encrypted frame layout (fc=0x%02X, %u bytes).", fc, (unsigned) size);
    return false;
  }
  uint8_t plaintext[MAX_PAYLOAD_SIZE];
  if (!this->decrypt_(raw, size, offset, plaintext)) {
    ESP_LOGW(TAG, "Decryption failed (%u-byte frame) -- check the bindkey.", (unsigned) size);
    return false;
  }
  // Only an authenticated frame may advance the duplicate filter. There is no replay protection: the
  // counter is not persisted and restarts on a battery change, so a strictly increasing check could lock
  // the sensor out until the next reboot.
  this->last_frame_count_ = frame_count;
  return this->parse_objects_(plaintext, size - offset - ENCRYPTED_TRAILER, reading);
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
