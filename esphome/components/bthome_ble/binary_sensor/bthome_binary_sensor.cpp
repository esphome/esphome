#include "bthome_binary_sensor.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

#include <mbedtls/ccm.h>

namespace esphome {
namespace bthome_ble {

static const char *const TAG = "bthome_ble.binary_sensor";

// BTHome v2 service UUID
static const uint16_t BTHOME_SERVICE_UUID = 0xFCD2;

// Device info byte masks
static const uint8_t BTHOME_ENCRYPTION_FLAG = 0x01;

void BTHomeBinarySensor::set_bindkey(const std::string &bindkey) {
  if (bindkey.length() != 32) {
    ESP_LOGE(TAG, "Bindkey must be 32 characters (16 bytes)");
    return;
  }
  this->bindkey_ = bindkey;
}

bool BTHomeBinarySensor::parse_device(const esp32_ble_tracker::ESPBTDevice &device) {
  // Look for BTHome service data
  const auto &service_datas = device.get_service_datas();

  for (const auto &service_data : service_datas) {
    if (service_data.uuid.get_uuid().len != ESP_UUID_LEN_16) {
      continue;
    }

    if (service_data.uuid.get_uuid().uuid.uuid16 != BTHOME_SERVICE_UUID) {
      continue;
    }

    ESP_LOGVV(TAG, "Found BTHome device: %s for binary sensor object_id 0x%02X", device.address_str().c_str(),
              this->object_id_);

    if (service_data.data.empty()) {
      ESP_LOGW(TAG, "Empty service data");
      return false;
    }

    // Copy service data to mutable vector
    std::vector<uint8_t> payload(service_data.data.begin(), service_data.data.end());

    // Parse device info byte
    uint8_t device_info = payload[0];
    bool is_encrypted = (device_info & BTHOME_ENCRYPTION_FLAG) != 0;

    // Handle encryption
    if (is_encrypted) {
      if (!this->bindkey_.has_value()) {
        ESP_LOGW(TAG, "Encrypted payload but no bindkey configured");
        return false;
      }

      // Convert bindkey from hex string to bytes
      uint8_t key[16];
      for (int i = 0; i < 16; i++) {
        key[i] = (uint8_t) strtol(this->bindkey_.value().substr(i * 2, 2).c_str(), nullptr, 16);
      }

      // Build nonce: address (6 bytes) + UUID (2 bytes) + device info (1 byte) + counter (4 bytes)
      uint8_t nonce[13];
      uint64_t address = device.address_uint64();
      nonce[0] = (address >> 40) & 0xFF;
      nonce[1] = (address >> 32) & 0xFF;
      nonce[2] = (address >> 24) & 0xFF;
      nonce[3] = (address >> 16) & 0xFF;
      nonce[4] = (address >> 8) & 0xFF;
      nonce[5] = address & 0xFF;
      nonce[6] = 0xD2;  // UUID LSB
      nonce[7] = 0xFC;  // UUID MSB
      nonce[8] = device_info;

      // Extract packet ID as counter if present
      uint32_t count = 0;
      if (payload.size() > 6 && payload[1] == 0x00) {  // Packet ID present
        count = payload[2];
      }
      nonce[9] = (count >> 24) & 0xFF;
      nonce[10] = (count >> 16) & 0xFF;
      nonce[11] = (count >> 8) & 0xFF;
      nonce[12] = count & 0xFF;

      // Tag is last 4 bytes of payload
      if (payload.size() < 5) {
        ESP_LOGW(TAG, "Encrypted payload too short");
        return false;
      }

      uint8_t tag[4];
      size_t ciphertext_len = payload.size() - 5;
      memcpy(tag, &payload[payload.size() - 4], 4);

      // Decrypt using AES-CCM
      mbedtls_ccm_context ctx;
      mbedtls_ccm_init(&ctx);

      int ret = mbedtls_ccm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, key, 128);
      if (ret != 0) {
        ESP_LOGW(TAG, "Failed to set decryption key: %d", ret);
        mbedtls_ccm_free(&ctx);
        return false;
      }

      std::vector<uint8_t> plaintext(ciphertext_len);
      ret = mbedtls_ccm_auth_decrypt(&ctx, ciphertext_len, nonce, 13, nullptr, 0, &payload[1], plaintext.data(), tag,
                                      4);

      mbedtls_ccm_free(&ctx);

      if (ret != 0) {
        ESP_LOGVV(TAG, "Failed to decrypt payload: %d", ret);
        return false;
      }

      // Replace encrypted data with decrypted data
      payload.resize(1 + ciphertext_len);
      memcpy(&payload[1], plaintext.data(), ciphertext_len);
    }

    // Parse measurement data looking for our object ID
    BTHomeParseResult result;
    if (payload.size() > 1) {
      if (!parse_bthome_data_byte(&payload[1], payload.size() - 1, result)) {
        ESP_LOGW(TAG, "Failed to parse BTHome data");
        return false;
      }

      // Check if our binary sensor object ID is present
      auto it = result.binary_sensors.find(this->object_id_);
      if (it != result.binary_sensors.end()) {
        bool value = it->second;
        ESP_LOGD(TAG, "Binary sensor 0x%02X: %s", this->object_id_, value ? "ON" : "OFF");
        this->publish_state(value);
        return true;
      }
    }
  }

  return false;
}

}  // namespace bthome_ble
}  // namespace esphome

#endif
