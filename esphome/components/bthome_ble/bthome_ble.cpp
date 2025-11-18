#include "bthome_ble.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

#include <mbedtls/ccm.h>

namespace esphome {
namespace bthome_ble {

static const char *const TAG = "bthome_ble";

// BTHome v2 service UUID
static const uint16_t BTHOME_SERVICE_UUID = 0xFCD2;

// Device info byte masks
static const uint8_t BTHOME_ENCRYPTION_FLAG = 0x01;
static const uint8_t BTHOME_TRIGGER_FLAG = 0x04;
static const uint8_t BTHOME_VERSION_MASK = 0xE0;

struct BTHomeObjectSpec {
  uint8_t size;        // Size in bytes (0 for variable length)
  float factor;        // Multiplication factor for decoding
  bool is_signed;      // Whether the value is signed
  bool is_binary;      // Whether this is a binary sensor
};

// BTHome object specifications
static const std::map<uint8_t, BTHomeObjectSpec> BTHOME_OBJECTS = {
    {PACKET_ID, {1, 1, false, false}},
    {BATTERY, {1, 1, false, false}},
    {TEMPERATURE, {2, 0.01, true, false}},
    {HUMIDITY, {2, 0.01, false, false}},
    {PRESSURE, {3, 0.01, false, false}},
    {ILLUMINANCE, {3, 0.01, false, false}},
    {MASS_KG, {2, 0.01, false, false}},
    {MASS_LB, {2, 0.01, false, false}},
    {DEWPOINT, {2, 0.01, true, false}},
    {COUNT, {1, 1, false, false}},
    {ENERGY, {3, 0.001, false, false}},
    {POWER, {3, 0.01, false, false}},
    {VOLTAGE, {2, 0.001, false, false}},
    {PM25, {2, 1, false, false}},
    {PM10, {2, 1, false, false}},
    {CO2, {2, 1, false, false}},
    {VOC, {2, 1, false, false}},
    {MOISTURE, {2, 0.01, false, false}},
    {BATTERY_LOW, {1, 1, false, true}},
    {BATTERY_CHARGING, {1, 1, false, true}},
    {CARBON_MONOXIDE, {1, 1, false, true}},
    {COLD, {1, 1, false, true}},
    {CONNECTIVITY, {1, 1, false, true}},
    {DOOR, {1, 1, false, true}},
    {GARAGE_DOOR, {1, 1, false, true}},
    {GAS, {1, 1, false, true}},
    {HEAT, {1, 1, false, true}},
    {LIGHT, {1, 1, false, true}},
    {LOCK, {1, 1, false, true}},
    {MOISTURE_BOOL, {1, 1, false, true}},
    {MOTION, {1, 1, false, true}},
    {MOVING, {1, 1, false, true}},
    {OCCUPANCY, {1, 1, false, true}},
    {OPENING, {1, 1, false, true}},
    {PLUG, {1, 1, false, true}},
    {POWER_ON, {1, 1, false, true}},
    {PRESENCE, {1, 1, false, true}},
    {PROBLEM, {1, 1, false, true}},
    {RUNNING, {1, 1, false, true}},
    {SAFETY, {1, 1, false, true}},
    {SMOKE, {1, 1, false, true}},
    {SOUND, {1, 1, false, true}},
    {TAMPER, {1, 1, false, true}},
    {VIBRATION, {1, 1, false, true}},
    {WINDOW, {1, 1, false, true}},
    {COUNT_UINT16, {2, 1, false, false}},
    {COUNT_UINT32, {4, 1, false, false}},
    {ROTATION, {2, 0.1, true, false}},
    {DISTANCE_MM, {2, 1, false, false}},
    {DISTANCE_M, {2, 0.1, false, false}},
    {DURATION, {3, 0.001, false, false}},
    {CURRENT, {2, 0.001, false, false}},
    {SPEED, {2, 0.01, false, false}},
    {TEMPERATURE_PRECISE, {2, 0.1, true, false}},
    {UV_INDEX, {1, 0.1, false, false}},
    {VOLUME_L, {2, 0.1, false, false}},
    {VOLUME_ML, {2, 1, false, false}},
    {VOLUME_FLOW, {2, 0.001, false, false}},
    {VOLTAGE_PRECISE, {2, 0.1, false, false}},
    {GAS_VOLUME, {3, 0.001, false, false}},
    {GAS_VOLUME_L, {3, 0.001, false, false}},
    {ENERGY_PRECISE, {4, 0.001, false, false}},
    {VOLUME_PRECISE, {4, 0.001, false, false}},
    {WATER, {3, 0.001, false, false}},
    {TIMESTAMP, {4, 1, false, false}},
    {ACCELERATION, {2, 0.001, true, false}},
    {GYROSCOPE, {2, 0.001, true, false}},
};

void BTHomeListener::set_bindkey(const std::string &bindkey) {
  if (bindkey.length() != 32) {
    ESP_LOGE(TAG, "Bindkey must be 32 characters (16 bytes)");
    return;
  }
  this->bindkey_ = bindkey;
}

bool BTHomeListener::decrypt_payload(std::vector<uint8_t> &payload, const uint64_t &address,
                                      const uint32_t &count) {
  if (!this->bindkey_.has_value()) {
    ESP_LOGE(TAG, "Bindkey required for encrypted payload");
    return false;
  }

  // Convert bindkey from hex string to bytes
  uint8_t key[16];
  for (int i = 0; i < 16; i++) {
    key[i] = (uint8_t) strtol(this->bindkey_.value().substr(i * 2, 2).c_str(), nullptr, 16);
  }

  // Build nonce: address (6 bytes) + UUID (2 bytes) + device info (1 byte)
  uint8_t nonce[13];
  nonce[0] = (address >> 40) & 0xFF;
  nonce[1] = (address >> 32) & 0xFF;
  nonce[2] = (address >> 24) & 0xFF;
  nonce[3] = (address >> 16) & 0xFF;
  nonce[4] = (address >> 8) & 0xFF;
  nonce[5] = address & 0xFF;
  nonce[6] = 0xD2;  // UUID LSB
  nonce[7] = 0xFC;  // UUID MSB
  nonce[8] = payload[0];  // Device info byte
  // Counter (4 bytes) - use packet ID if available, otherwise count
  nonce[9] = (count >> 24) & 0xFF;
  nonce[10] = (count >> 16) & 0xFF;
  nonce[11] = (count >> 8) & 0xFF;
  nonce[12] = count & 0xFF;

  // Tag is last 4 bytes of payload
  if (payload.size() < 5) {  // Device info + at least 1 data byte + 4 byte tag
    ESP_LOGE(TAG, "Encrypted payload too short");
    return false;
  }

  uint8_t tag[4];
  size_t ciphertext_len = payload.size() - 5;  // Exclude device info and tag
  memcpy(tag, &payload[payload.size() - 4], 4);

  // Decrypt using AES-CCM
  mbedtls_ccm_context ctx;
  mbedtls_ccm_init(&ctx);

  int ret = mbedtls_ccm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, key, 128);
  if (ret != 0) {
    ESP_LOGE(TAG, "Failed to set decryption key: %d", ret);
    mbedtls_ccm_free(&ctx);
    return false;
  }

  std::vector<uint8_t> plaintext(ciphertext_len);
  ret = mbedtls_ccm_auth_decrypt(&ctx, ciphertext_len, nonce, 13, nullptr, 0, &payload[1], plaintext.data(), tag, 4);

  mbedtls_ccm_free(&ctx);

  if (ret != 0) {
    ESP_LOGE(TAG, "Failed to decrypt payload: %d", ret);
    return false;
  }

  // Replace encrypted data with decrypted data (keep device info byte)
  payload.resize(1 + ciphertext_len);
  memcpy(&payload[1], plaintext.data(), ciphertext_len);

  return true;
}

bool parse_bthome_data_byte(const uint8_t *data, uint8_t data_length, BTHomeParseResult &result) {
  uint8_t offset = 0;

  while (offset < data_length) {
    if (offset >= data_length) {
      ESP_LOGW(TAG, "Unexpected end of data");
      return false;
    }

    uint8_t obj_id = data[offset++];

    // Check if this is a known object type
    auto it = BTHOME_OBJECTS.find(obj_id);
    if (it == BTHOME_OBJECTS.end()) {
      ESP_LOGW(TAG, "Unknown object ID: 0x%02X", obj_id);
      return false;
    }

    const BTHomeObjectSpec &spec = it->second;
    uint8_t obj_size = spec.size;

    // Handle variable length objects
    if (obj_size == 0) {
      if (offset >= data_length) {
        ESP_LOGW(TAG, "Missing length byte for variable object");
        return false;
      }
      obj_size = data[offset++];
    }

    if (offset + obj_size > data_length) {
      ESP_LOGW(TAG, "Object data exceeds payload length");
      return false;
    }

    // Parse the value
    if (spec.is_binary) {
      bool value = data[offset] != 0;
      result.binary_sensors[obj_id] = value;
      offset += obj_size;
    } else if (obj_id == TEXT) {
      std::string value((char *) &data[offset], obj_size);
      result.text_sensors[obj_id] = value;
      offset += obj_size;
    } else if (obj_id == RAW) {
      // Skip raw data for now
      offset += obj_size;
    } else {
      // Parse numeric value
      int32_t raw_value = 0;

      if (spec.is_signed) {
        // Handle signed integers
        switch (obj_size) {
          case 1:
            raw_value = (int8_t) data[offset];
            break;
          case 2:
            raw_value = (int16_t) (data[offset] | (data[offset + 1] << 8));
            break;
          case 3:
            raw_value = (int32_t) (data[offset] | (data[offset + 1] << 8) | (data[offset + 2] << 16));
            // Sign extend 24-bit to 32-bit
            if (raw_value & 0x800000) {
              raw_value |= 0xFF000000;
            }
            break;
          case 4:
            raw_value = (int32_t) (data[offset] | (data[offset + 1] << 8) | (data[offset + 2] << 16) |
                                   (data[offset + 3] << 24));
            break;
        }
      } else {
        // Handle unsigned integers
        uint32_t unsigned_value = 0;
        for (int i = 0; i < obj_size; i++) {
          unsigned_value |= ((uint32_t) data[offset + i] << (i * 8));
        }
        raw_value = unsigned_value;
      }

      float value = raw_value * spec.factor;
      result.sensors[obj_id] = value;
      offset += obj_size;
    }
  }

  return true;
}

bool BTHomeListener::parse_device(const esp32_ble_tracker::ESPBTDevice &device) {
  // Look for BTHome service data
  const auto &service_datas = device.get_service_datas();

  for (const auto &service_data : service_datas) {
    if (service_data.uuid.get_uuid().len != ESP_UUID_LEN_16) {
      continue;
    }

    if (service_data.uuid.get_uuid().uuid.uuid16 != BTHOME_SERVICE_UUID) {
      continue;
    }

    ESP_LOGV(TAG, "Found BTHome device: %s", device.address_str().c_str());

    if (service_data.data.empty()) {
      ESP_LOGW(TAG, "Empty service data");
      return false;
    }

    // Copy service data to mutable vector
    std::vector<uint8_t> payload(service_data.data.begin(), service_data.data.end());

    // Parse device info byte
    uint8_t device_info = payload[0];
    bool is_encrypted = (device_info & BTHOME_ENCRYPTION_FLAG) != 0;
    bool is_trigger_based = (device_info & BTHOME_TRIGGER_FLAG) != 0;

    ESP_LOGD(TAG, "Device info: 0x%02X, encrypted: %d, trigger: %d", device_info, is_encrypted, is_trigger_based);

    // Handle encryption
    if (is_encrypted) {
      uint32_t count = 0;
      if (!decrypt_payload(payload, device.address_uint64(), count)) {
        ESP_LOGW(TAG, "Failed to decrypt payload");
        return false;
      }
    }

    // Parse measurement data
    BTHomeParseResult result;
    result.has_encryption = is_encrypted;
    result.is_trigger_based = is_trigger_based;
    result.device_info = device_info;

    if (payload.size() > 1) {
      if (!parse_bthome_data_byte(&payload[1], payload.size() - 1, result)) {
        ESP_LOGW(TAG, "Failed to parse BTHome data");
        return false;
      }
    }

    ESP_LOGD(TAG, "Parsed %d sensors and %d binary sensors", result.sensors.size(), result.binary_sensors.size());

    // Result will be processed by child sensor/binary_sensor components
    return true;
  }

  return false;
}

}  // namespace bthome_ble
}  // namespace esphome

#endif
