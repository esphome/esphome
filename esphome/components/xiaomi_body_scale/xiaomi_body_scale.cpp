#include "xiaomi_body_scale.h"
#include "esphome/components/ble_device_base/ble_aes_ccm.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <algorithm>
#include <cstring>
#include <iterator>

namespace esphome::xiaomi_body_scale {

static const char *const TAG = "xiaomi_body_scale";

// Encrypted MiBeacon frame without a MAC: frame control, device id, frame count, cipher, counter, tag
static constexpr size_t DEVICE_ID_POS = 2;
static constexpr size_t FRAME_COUNT_POS = 4;
static constexpr size_t CIPHER_POS = 5;
static constexpr size_t CIPHER_SIZE = 12;
static constexpr size_t COUNTER_POS = CIPHER_POS + CIPHER_SIZE;
static constexpr size_t TAG_POS = COUNTER_POS + 3;
static constexpr size_t TAG_SIZE = 4;
static constexpr size_t FRAME_SIZE = TAG_POS + TAG_SIZE;
static constexpr uint8_t FRAME_HAS_DATA = 0x40;
static constexpr uint8_t OBJECT_SIZE = 9;
static constexpr uint16_t OBJECT_S200_MEASUREMENT = 0x4E16;
static constexpr uint16_t OBJECT_S400_MEASUREMENT = 0x6E16;
static constexpr uint16_t DEVICE_IDS[] = {
    0x45C9, 0x4C04, 0x4DCB,          // S200 (MJTZC02YM)
    0x30D9, 0x3BD5, 0x48CF, 0x4B05,  // S400 (MJTZC01YM, MJTZC03YM)
};
static constexpr uint8_t FRAME_ENCRYPTED = 0x08;
static constexpr uint32_t STABILIZED_RESET_ID = 0;
static constexpr uint32_t STABILIZED_RESET_MS = 1000;

XiaomiBodyScale::XiaomiBodyScale(uint64_t address, const char *bindkey) : address_(address) {
  parse_hex(bindkey, this->bindkey_, sizeof(this->bindkey_));
}

void XiaomiBodyScale::dump_config() {
  uint8_t mac[MAC_ADDRESS_SIZE];
  ble_device_base::uint64_to_mac_msb_first(this->address_, mac);
  char mac_buf[MAC_ADDRESS_PRETTY_BUFFER_SIZE];
  ESP_LOGCONFIG(TAG,
                "Xiaomi Body Scale\n"
                "  MAC Address: %s",
                format_mac_addr_upper(mac, mac_buf));
  LOG_SENSOR("  ", "Weight", this->weight_);
  LOG_SENSOR("  ", "Impedance Low (50 kHz)", this->impedance_low_);
  LOG_SENSOR("  ", "Impedance High (250 kHz)", this->impedance_high_);
  LOG_SENSOR("  ", "Heart Rate", this->heart_rate_);
  LOG_SENSOR("  ", "Profile ID", this->profile_id_);
  LOG_BINARY_SENSOR("  ", "Stabilized", this->stabilized_);
}

bool XiaomiBodyScale::decrypt_(const uint8_t *frame, uint8_t *plaintext) const {
  uint8_t nonce[MAC_ADDRESS_SIZE + 6];
  for (size_t i = 0; i < MAC_ADDRESS_SIZE; i++)
    nonce[i] = static_cast<uint8_t>(this->address_ >> (i * 8));  // MAC, reversed
  memcpy(nonce + MAC_ADDRESS_SIZE, frame + DEVICE_ID_POS, 3);    // device id + frame count
  memcpy(nonce + MAC_ADDRESS_SIZE + 3, frame + COUNTER_POS, 3);
  static constexpr uint8_t AUTH_DATA[1] = {0x11};
  return ble_device_base::aes_ccm_auth_decrypt(this->bindkey_, nonce, sizeof(nonce), AUTH_DATA, sizeof(AUTH_DATA),
                                               frame + CIPHER_POS, CIPHER_SIZE, plaintext, frame + TAG_POS, TAG_SIZE);
}

void XiaomiBodyScale::publish_stabilized_(bool stabilized) {
  if (this->stabilized_ == nullptr)
    return;
  this->stabilized_->publish_state(stabilized);
  // Clear it again so the next measurement is seen as a new one
  if (stabilized)
    this->set_timeout(STABILIZED_RESET_ID, STABILIZED_RESET_MS, [this]() { this->stabilized_->publish_state(false); });
}

void XiaomiBodyScale::publish_s400_(uint32_t packed) {
  // Weight x10 (11 bits), heart rate - 50 (7 bits), impedance x10 (14 bits). A measurement sends weight, heart
  // rate and the 50 kHz impedance (the larger value), then a packet with only the 250 kHz impedance.
  const uint16_t weight = packed & 0x7FF;
  const uint8_t heart_rate = (packed >> 11) & 0x7F;
  const uint16_t impedance = packed >> 18;
  ESP_LOGD(TAG, "weight=%u heart_rate=%u impedance=%u", weight, heart_rate, impedance);

  if (weight != 0 && this->weight_ != nullptr)
    this->weight_->publish_state(weight / 10.0f);
  if (heart_rate > 0 && heart_rate < 127 && this->heart_rate_ != nullptr)
    this->heart_rate_->publish_state(heart_rate + 50.0f);

  if (weight == 0 && heart_rate == 0) {
    // All zero: stepped off the scale. Otherwise the final 250 kHz packet (bare feet).
    if (impedance != 0 && this->impedance_high_ != nullptr)
      this->impedance_high_->publish_state(impedance / 10.0f);
    this->publish_stabilized_(impedance != 0);
  } else if (impedance != 0) {
    if (this->impedance_low_ != nullptr)
      this->impedance_low_->publish_state(impedance / 10.0f);
    this->publish_stabilized_(false);
  } else {
    // Weight without impedance: measurement complete (with socks)
    this->publish_stabilized_(true);
  }
}

bool XiaomiBodyScale::parse_device(const ble_device_base::ESPBTDevice &device) {
  if (device.address_uint64() != this->address_)
    return false;

  for (const auto &service_data : device.get_service_datas()) {
    if (service_data.data.size() != FRAME_SIZE || !service_data.uuid.contains(0x95, 0xFE))
      continue;
    const uint8_t *frame = service_data.data.data();
    // The bindkey is required, so plaintext frames are never trusted
    if ((frame[0] & (FRAME_HAS_DATA | FRAME_ENCRYPTED)) != (FRAME_HAS_DATA | FRAME_ENCRYPTED))
      continue;

    const uint16_t device_id = encode_uint16(frame[DEVICE_ID_POS + 1], frame[DEVICE_ID_POS]);
    if (std::find(std::begin(DEVICE_IDS), std::end(DEVICE_IDS), device_id) == std::end(DEVICE_IDS))
      continue;
    if (frame[FRAME_COUNT_POS] == this->last_frame_count_)
      continue;
    uint8_t object[CIPHER_SIZE];
    if (!this->decrypt_(frame, object)) {
      ESP_LOGW(TAG, "Decryption failed, wrong bindkey?");
      continue;
    }
    // Only an authenticated frame may advance the duplicate filter
    this->last_frame_count_ = frame[FRAME_COUNT_POS];

    const uint16_t value_type = encode_uint16(object[1], object[0]);
    if (object[2] != OBJECT_SIZE || (value_type != OBJECT_S400_MEASUREMENT && value_type != OBJECT_S200_MEASUREMENT)) {
      ESP_LOGVV(TAG, "Unknown object 0x%04X, length %u", value_type, object[2]);
      continue;
    }
    // Both objects start with the profile ID and a packed uint32 (LE); a timestamp follows (not published)
    const uint8_t *data = object + 3;
    const uint32_t packed = encode_uint32(data[4], data[3], data[2], data[1]);
    if (this->profile_id_ != nullptr)
      this->profile_id_->publish_state(data[0]);
    if (value_type == OBJECT_S400_MEASUREMENT) {
      this->publish_s400_(packed);
    } else if (packed != 0 && this->weight_ != nullptr) {
      this->weight_->publish_state(packed / 100.0f);  // S200: weight x100 only
    }
    return true;
  }
  return false;
}

}  // namespace esphome::xiaomi_body_scale
