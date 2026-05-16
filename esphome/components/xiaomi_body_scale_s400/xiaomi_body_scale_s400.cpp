#include "xiaomi_body_scale_s400.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "mbedtls/ccm.h"

#ifdef USE_ESP32

namespace esphome {
namespace xiaomi_body_scale_s400 {

static const char *const TAG = "xiaomi_body_scale_s400";

// Inline AES-CCM decryption for S400 24-byte payload
// cipher_pos=5, datasize=12 (matches xiaomi_miscale component logic)
static bool decrypt_s400(std::vector<uint8_t> &raw, const uint8_t *bindkey, const uint64_t address) {
  if (raw.size() != 24)
    return false;

  uint8_t mac[6];
  mac[5] = (uint8_t) (address >> 40);
  mac[4] = (uint8_t) (address >> 32);
  mac[3] = (uint8_t) (address >> 24);
  mac[2] = (uint8_t) (address >> 16);
  mac[1] = (uint8_t) (address >> 8);
  mac[0] = (uint8_t) (address >> 0);

  const uint8_t *v = raw.data();
  const int cipher_pos = 5;
  const int datasize = 12;

  uint8_t iv[12];
  memcpy(iv, mac, 6);
  memcpy(iv + 6, v + 2, 3);
  memcpy(iv + 9, v + 17, 3);

  uint8_t authdata[1] = {0x11};
  uint8_t ciphertext[12] = {0};
  uint8_t plaintext[12] = {0};
  uint8_t tag[4] = {0};

  memcpy(ciphertext, v + cipher_pos, datasize);
  memcpy(tag, v + 20, 4);

  mbedtls_ccm_context ctx;
  mbedtls_ccm_init(&ctx);

  int ret = mbedtls_ccm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, bindkey, 128);
  if (ret) {
    mbedtls_ccm_free(&ctx);
    return false;
  }

  ret = mbedtls_ccm_auth_decrypt(&ctx, datasize, iv, sizeof(iv), authdata, sizeof(authdata), ciphertext, plaintext, tag,
                                 sizeof(tag));
  mbedtls_ccm_free(&ctx);
  if (ret)
    return false;

  for (int i = 0; i < datasize; i++)
    raw[cipher_pos + i] = plaintext[i];

  raw[0] &= ~0x08;
  return true;
}

void XiaomiBodyScaleS400::set_bindkey(const std::string &bindkey) {
  memset(this->bindkey_, 0, 16);
  if (bindkey.size() != 32)
    return;
  char temp[3] = {0};
  for (int i = 0; i < 16; i++) {
    strncpy(temp, &(bindkey.c_str()[i * 2]), 2);
    this->bindkey_[i] = std::strtoul(temp, nullptr, 16);
  }
}

void XiaomiBodyScaleS400::dump_config() {
  ESP_LOGCONFIG(TAG, "Xiaomi Body Composition Scale S400");
  LOG_SENSOR("  ", "Weight", this->weight_);
  LOG_SENSOR("  ", "Impedance", this->impedance_);
  LOG_SENSOR("  ", "Impedance Low (50 kHz)", this->impedance_low_);
  LOG_SENSOR("  ", "Impedance High (250 kHz)", this->impedance_high_);
  LOG_SENSOR("  ", "Heart Rate", this->heart_rate_);
  LOG_SENSOR("  ", "Profile ID", this->profile_id_);
}

bool XiaomiBodyScaleS400::parse_device(const esp32_ble_tracker::ESPBTDevice &device) {
  if (device.address_uint64() != this->address_)
    return false;

  for (auto &service_data : device.get_service_datas()) {
    std::vector<uint8_t> raw = service_data.data;

    if (raw.size() != 24)
      continue;
    if (!service_data.uuid.contains(0x95, 0xFE))
      continue;
    if (!(raw[0] & 0x40))
      continue;

    const uint16_t device_id = encode_uint16(raw[3], raw[2]);
    if (device_id != 0x3BD5 && device_id != 0x4B05 && device_id != 0x30D9 && device_id != 0x48CF)
      continue;

    static uint8_t last_frame_count = 0xFF;
    if (last_frame_count == raw[4])
      continue;
    last_frame_count = raw[4];

    if (raw[0] & 0x08) {
      if (!decrypt_s400(raw, this->bindkey_, this->address_)) {
        ESP_LOGW(TAG, "Decryption failed — wrong bindkey?");
        continue;
      }
    }

    const uint8_t raw_offset = 5;
    const uint16_t value_type = encode_uint16(raw[raw_offset + 1], raw[raw_offset + 0]);
    if (value_type != 0x6E16) {
      ESP_LOGVV(TAG, "Unknown object ID: 0x%04X", value_type);
      continue;
    }

    const uint8_t value_length = raw[raw_offset + 2];
    if (value_length != 9) {
      ESP_LOGVV(TAG, "Wrong payload length: %d (expected 9)", value_length);
      continue;
    }

    const uint8_t *data = raw.data() + raw_offset + 3;

    // data[0]   : profile ID (uint8, 1-5)
    // data[1-4] : compressed metrics (uint32 LE)
    //   bits  0-10 : weight × 10      (0.1 kg)
    //   bits 11-17 : heart_rate − 50  (1 bpm)
    //   bits 18-31 : impedance × 10   (0.1 Ω)
    //
    // Impedance frequency convention (aligned with bodymiscale / BIA standard):
    //   impedance_low  = low frequency  50 kHz  → numerically LARGER  value (~558 Ω)
    //                    packet WITH weight and heart_rate
    //   impedance_high = high frequency 250 kHz → numerically SMALLER value (~503 Ω)
    //                    packet WITHOUT weight and heart_rate
    //
    // data[5-8] : UNIX timestamp (not published — requires Xiaomi Home app)

    const uint32_t data_int = encode_uint32(data[4], data[3], data[2], data[1]);
    const uint16_t weight = data_int & 0x7FF;
    const uint8_t heart_rate = (data_int >> 11) & 0x7F;
    const uint16_t impedance = data_int >> 18;

    ESP_LOGD(TAG, "profile=%d weight=%u heart_rate=%u impedance=%u", data[0], weight, heart_rate, impedance);

    if (this->profile_id_ != nullptr)
      this->profile_id_->publish_state((float) data[0]);

    if (weight != 0 && this->weight_ != nullptr)
      this->weight_->publish_state(weight / 10.0f);

    if (heart_rate > 0 && heart_rate < 127 && this->heart_rate_ != nullptr)
      this->heart_rate_->publish_state((float) heart_rate + 50.0f);

    if (impedance != 0) {
      if (weight == 0 && heart_rate == 0) {
        // Packet without weight/heart_rate → high frequency 250 kHz → impedance_high
        if (this->impedance_high_ != nullptr)
          this->impedance_high_->publish_state(impedance / 10.0f);
      } else {
        // Packet with weight/heart_rate → low frequency 50 kHz → impedance_low
        if (this->impedance_low_ != nullptr)
          this->impedance_low_->publish_state(impedance / 10.0f);
        if (this->impedance_ != nullptr)
          this->impedance_->publish_state(impedance / 10.0f);
      }
    }

    return true;
  }
  return false;
}

}  // namespace xiaomi_body_scale_s400
}  // namespace esphome

#endif
