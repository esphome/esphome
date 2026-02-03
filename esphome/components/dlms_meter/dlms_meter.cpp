#include "dlms_meter.h"

#include <cmath>

#if defined(USE_ESP8266_FRAMEWORK_ARDUINO)
#include <bearssl/bearssl.h>
#elif defined(USE_ESP32)
#include "mbedtls/esp_config.h"
#include "mbedtls/gcm.h"
#endif

namespace esphome::dlms_meter {

static constexpr const char *TAG = "dlms_meter";

void DlmsMeterComponent::dump_config() {
  const char *provider_name = this->provider_ == PROVIDER_NETZNOE ? "Netz NOE" : "Generic";
  ESP_LOGCONFIG(TAG,
                "DLMS Meter:\n"
                "  Provider: %s\n"
                "  Read Timeout: %u ms",
                provider_name, this->read_timeout_);
#define DLMS_METER_LOG_SENSOR(s) LOG_SENSOR("  ", #s, this->s##_sensor_);
  DLMS_METER_SENSOR_LIST(DLMS_METER_LOG_SENSOR, )
#define DLMS_METER_LOG_TEXT_SENSOR(s) LOG_TEXT_SENSOR("  ", #s, this->s##_text_sensor_);
  DLMS_METER_TEXT_SENSOR_LIST(DLMS_METER_LOG_TEXT_SENSOR, )
}

void DlmsMeterComponent::loop() {
  // Read while data is available, netznoe uses two frames so allow 2x max frame length
  while (this->available()) {
    if (this->receive_buffer_.size() >= MBUS_MAX_FRAME_LENGTH * 2) {
      ESP_LOGW(TAG, "Receive buffer full, dropping remaining bytes");
      break;
    }
    uint8_t c;
    this->read_byte(&c);
    this->receive_buffer_.push_back(c);
    this->last_read_ = millis();
  }

  if (!this->receive_buffer_.empty() && millis() - this->last_read_ > this->read_timeout_) {
    this->mbus_payload_.clear();
    if (!this->parse_mbus_(this->mbus_payload_))
      return;

    uint16_t message_length;
    uint8_t systitle_length;
    uint16_t header_offset;
    if (!this->parse_dlms_(this->mbus_payload_, message_length, systitle_length, header_offset))
      return;

    if (message_length < DECODER_START_OFFSET || message_length > MAX_MESSAGE_LENGTH) {
      ESP_LOGE(TAG, "DLMS: Message length invalid: %u", message_length);
      this->receive_buffer_.clear();
      return;
    }

    // Decrypt in place and then decode the OBIS codes
    if (!this->decrypt_(this->mbus_payload_, message_length, systitle_length, header_offset))
      return;
    this->decode_obis_(&this->mbus_payload_[header_offset + DLMS_PAYLOAD_OFFSET], message_length);
  }
}

bool DlmsMeterComponent::parse_mbus_(std::vector<uint8_t> &mbus_payload) {
  ESP_LOGV(TAG, "Parsing M-Bus frames");
  uint16_t frame_offset = 0;  // Offset is used if the M-Bus message is split into multiple frames

  while (frame_offset < this->receive_buffer_.size()) {
    // Ensure enough bytes remain for the minimal intro header before accessing indices
    if (this->receive_buffer_.size() - frame_offset < MBUS_HEADER_INTRO_LENGTH) {
      ESP_LOGE(TAG, "MBUS: Not enough data for frame header (need %d, have %d)", MBUS_HEADER_INTRO_LENGTH,
               (this->receive_buffer_.size() - frame_offset));
      this->receive_buffer_.clear();
      return false;
    }

    // Check start bytes
    if (this->receive_buffer_[frame_offset + MBUS_START1_OFFSET] != START_BYTE_LONG_FRAME ||
        this->receive_buffer_[frame_offset + MBUS_START2_OFFSET] != START_BYTE_LONG_FRAME) {
      ESP_LOGE(TAG, "MBUS: Start bytes do not match");
      this->receive_buffer_.clear();
      return false;
    }

    // Both length bytes must be identical
    if (this->receive_buffer_[frame_offset + MBUS_LENGTH1_OFFSET] !=
        this->receive_buffer_[frame_offset + MBUS_LENGTH2_OFFSET]) {
      ESP_LOGE(TAG, "MBUS: Length bytes do not match");
      this->receive_buffer_.clear();
      return false;
    }

    uint8_t frame_length = this->receive_buffer_[frame_offset + MBUS_LENGTH1_OFFSET];  // Get length of this frame

    // Check if received data is enough for the given frame length
    if (this->receive_buffer_.size() - frame_offset <
        frame_length + 3) {  // length field inside packet does not account for second start- + checksum- + stop- byte
      ESP_LOGE(TAG, "MBUS: Frame too big for received data");
      this->receive_buffer_.clear();
      return false;
    }

    // Ensure we have full frame (header + payload + checksum + stop byte) before accessing stop byte
    size_t required_total =
        frame_length + MBUS_HEADER_INTRO_LENGTH + MBUS_FOOTER_LENGTH;  // payload + header + 2 footer bytes
    if (this->receive_buffer_.size() - frame_offset < required_total) {
      ESP_LOGE(TAG, "MBUS: Incomplete frame (need %d, have %d)", (unsigned int) required_total,
               this->receive_buffer_.size() - frame_offset);
      this->receive_buffer_.clear();
      return false;
    }

    if (this->receive_buffer_[frame_offset + frame_length + MBUS_HEADER_INTRO_LENGTH + MBUS_FOOTER_LENGTH - 1] !=
        STOP_BYTE) {
      ESP_LOGE(TAG, "MBUS: Invalid stop byte");
      this->receive_buffer_.clear();
      return false;
    }

    // Verify checksum: sum of all bytes starting at MBUS_HEADER_INTRO_LENGTH, take last byte
    uint8_t checksum = 0;  // use uint8_t so only the 8 least significant bits are stored
    for (uint16_t i = 0; i < frame_length; i++) {
      checksum += this->receive_buffer_[frame_offset + MBUS_HEADER_INTRO_LENGTH + i];
    }
    if (checksum != this->receive_buffer_[frame_offset + frame_length + MBUS_HEADER_INTRO_LENGTH]) {
      ESP_LOGE(TAG, "MBUS: Invalid checksum: %x != %x", checksum,
               this->receive_buffer_[frame_offset + frame_length + MBUS_HEADER_INTRO_LENGTH]);
      this->receive_buffer_.clear();
      return false;
    }

    mbus_payload.insert(mbus_payload.end(), &this->receive_buffer_[frame_offset + MBUS_FULL_HEADER_LENGTH],
                        &this->receive_buffer_[frame_offset + MBUS_HEADER_INTRO_LENGTH + frame_length]);

    frame_offset += MBUS_HEADER_INTRO_LENGTH + frame_length + MBUS_FOOTER_LENGTH;
  }
  return true;
}

bool DlmsMeterComponent::parse_dlms_(const std::vector<uint8_t> &mbus_payload, uint16_t &message_length,
                                     uint8_t &systitle_length, uint16_t &header_offset) {
  ESP_LOGV(TAG, "Parsing DLMS header");
  if (mbus_payload.size() < DLMS_HEADER_LENGTH + DLMS_HEADER_EXT_OFFSET) {
    ESP_LOGE(TAG, "DLMS: Payload too short");
    this->receive_buffer_.clear();
    return false;
  }

  if (mbus_payload[DLMS_CIPHER_OFFSET] != GLO_CIPHERING) {  // Only general-glo-ciphering is supported (0xDB)
    ESP_LOGE(TAG, "DLMS: Unsupported cipher");
    this->receive_buffer_.clear();
    return false;
  }

  systitle_length = mbus_payload[DLMS_SYST_OFFSET];

  if (systitle_length != 0x08) {  // Only system titles with length of 8 are supported
    ESP_LOGE(TAG, "DLMS: Unsupported system title length");
    this->receive_buffer_.clear();
    return false;
  }

  message_length = mbus_payload[DLMS_LENGTH_OFFSET];
  header_offset = 0;

  if (this->provider_ == PROVIDER_NETZNOE) {
    // for some reason EVN seems to set the standard "length" field to 0x81 and then the actual length is in the next
    // byte. Check some bytes to see if received data still matches expectation
    if (message_length == NETZ_NOE_MAGIC_BYTE &&
        mbus_payload[DLMS_LENGTH_OFFSET + 1] == NETZ_NOE_EXPECTED_MESSAGE_LENGTH &&
        mbus_payload[DLMS_LENGTH_OFFSET + 2] == NETZ_NOE_EXPECTED_SECURITY_CONTROL_BYTE) {
      message_length = mbus_payload[DLMS_LENGTH_OFFSET + 1];
      header_offset = 1;
    } else {
      ESP_LOGE(TAG, "Wrong Length - Security Control Byte sequence detected for provider EVN");
    }
  } else {
    if (message_length == TWO_BYTE_LENGTH) {
      message_length = encode_uint16(mbus_payload[DLMS_LENGTH_OFFSET + 1], mbus_payload[DLMS_LENGTH_OFFSET + 2]);
      header_offset = DLMS_HEADER_EXT_OFFSET;
    }
  }
  if (message_length < DLMS_LENGTH_CORRECTION) {
    ESP_LOGE(TAG, "DLMS: Message length too short: %u", message_length);
    this->receive_buffer_.clear();
    return false;
  }
  message_length -= DLMS_LENGTH_CORRECTION;  // Correct message length due to part of header being included in length

  if (mbus_payload.size() - DLMS_HEADER_LENGTH - header_offset != message_length) {
    ESP_LOGV(TAG, "DLMS: Length mismatch - payload=%d, header=%d, offset=%d, message=%d", mbus_payload.size(),
             DLMS_HEADER_LENGTH, header_offset, message_length);
    ESP_LOGE(TAG, "DLMS: Message has invalid length");
    this->receive_buffer_.clear();
    return false;
  }

  if (mbus_payload[header_offset + DLMS_SECBYTE_OFFSET] != 0x21 &&
      mbus_payload[header_offset + DLMS_SECBYTE_OFFSET] !=
          0x20) {  // Only certain security suite is supported (0x21 || 0x20)
    ESP_LOGE(TAG, "DLMS: Unsupported security control byte");
    this->receive_buffer_.clear();
    return false;
  }

  return true;
}

bool DlmsMeterComponent::decrypt_(std::vector<uint8_t> &mbus_payload, uint16_t message_length, uint8_t systitle_length,
                                  uint16_t header_offset) {
  ESP_LOGV(TAG, "Decrypting payload");
  uint8_t iv[12];  // Reserve space for the IV, always 12 bytes
  // Copy system title to IV (System title is before length; no header offset needed!)
  // Add 1 to the offset in order to skip the system title length byte
  memcpy(&iv[0], &mbus_payload[DLMS_SYST_OFFSET + 1], systitle_length);
  memcpy(&iv[8], &mbus_payload[header_offset + DLMS_FRAMECOUNTER_OFFSET],
         DLMS_FRAMECOUNTER_LENGTH);  // Copy frame counter to IV

  uint8_t *payload_ptr = &mbus_payload[header_offset + DLMS_PAYLOAD_OFFSET];

#if defined(USE_ESP8266_FRAMEWORK_ARDUINO)
  br_gcm_context gcm_ctx;
  br_aes_ct_ctr_keys bc;
  br_aes_ct_ctr_init(&bc, this->decryption_key_.data(), this->decryption_key_.size());
  br_gcm_init(&gcm_ctx, &bc.vtable, br_ghash_ctmul32);
  br_gcm_reset(&gcm_ctx, iv, sizeof(iv));
  br_gcm_flip(&gcm_ctx);
  br_gcm_run(&gcm_ctx, 0, payload_ptr, message_length);
#elif defined(USE_ESP32)
  size_t outlen = 0;
  mbedtls_gcm_context gcm_ctx;
  mbedtls_gcm_init(&gcm_ctx);
  mbedtls_gcm_setkey(&gcm_ctx, MBEDTLS_CIPHER_ID_AES, this->decryption_key_.data(), this->decryption_key_.size() * 8);
  mbedtls_gcm_starts(&gcm_ctx, MBEDTLS_GCM_DECRYPT, iv, sizeof(iv));
  auto ret = mbedtls_gcm_update(&gcm_ctx, payload_ptr, message_length, payload_ptr, message_length, &outlen);
  mbedtls_gcm_free(&gcm_ctx);
  if (ret != 0) {
    ESP_LOGE(TAG, "Decryption failed with error: %d", ret);
    this->receive_buffer_.clear();
    return false;
  }
#else
#error "Invalid Platform"
#endif

  if (payload_ptr[0] != DATA_NOTIFICATION || payload_ptr[5] != TIMESTAMP_DATETIME) {
    ESP_LOGE(TAG, "OBIS: Packet was decrypted but data is invalid");
    this->receive_buffer_.clear();
    return false;
  }
  ESP_LOGV(TAG, "Decrypted payload: %d bytes", message_length);
  return true;
}

void DlmsMeterComponent::decode_obis_(uint8_t *plaintext, uint16_t message_length) {
  ESP_LOGV(TAG, "Decoding payload");
  MeterData data{};
  uint16_t current_position = DECODER_START_OFFSET;
  bool power_factor_found = false;

  while (current_position + OBIS_CODE_OFFSET <= message_length) {
    if (plaintext[current_position + OBIS_TYPE_OFFSET] != DataType::OCTET_STRING) {
      ESP_LOGE(TAG, "OBIS: Unsupported OBIS header type: %x", plaintext[current_position + OBIS_TYPE_OFFSET]);
      this->receive_buffer_.clear();
      return;
    }

    uint8_t obis_code_length = plaintext[current_position + OBIS_LENGTH_OFFSET];
    if (obis_code_length != OBIS_CODE_LENGTH_STANDARD && obis_code_length != OBIS_CODE_LENGTH_EXTENDED) {
      ESP_LOGE(TAG, "OBIS: Unsupported OBIS header length: %x", obis_code_length);
      this->receive_buffer_.clear();
      return;
    }
    if (current_position + OBIS_CODE_OFFSET + obis_code_length > message_length) {
      ESP_LOGE(TAG, "OBIS: Buffer too short for OBIS code");
      this->receive_buffer_.clear();
      return;
    }

    uint8_t *obis_code = &plaintext[current_position + OBIS_CODE_OFFSET];
    uint8_t obis_medium = obis_code[OBIS_A];
    uint16_t obis_cd = encode_uint16(obis_code[OBIS_C], obis_code[OBIS_D]);

    bool timestamp_found = false;
    bool meter_number_found = false;
    if (this->provider_ == PROVIDER_NETZNOE) {
      // Do not advance Position when reading the Timestamp at DECODER_START_OFFSET
      if ((obis_code_length == OBIS_CODE_LENGTH_EXTENDED) && (current_position == DECODER_START_OFFSET)) {
        timestamp_found = true;
      } else if (power_factor_found) {
        meter_number_found = true;
        power_factor_found = false;
      } else {
        current_position += obis_code_length + OBIS_CODE_OFFSET;  // Advance past code and position
      }
    } else {
      current_position += obis_code_length + OBIS_CODE_OFFSET;  // Advance past code, position and type
    }
    if (!timestamp_found && !meter_number_found && obis_medium != Medium::ELECTRICITY &&
        obis_medium != Medium::ABSTRACT) {
      ESP_LOGE(TAG, "OBIS: Unsupported OBIS medium: %x", obis_medium);
      this->receive_buffer_.clear();
      return;
    }

    if (current_position >= message_length) {
      ESP_LOGE(TAG, "OBIS: Buffer too short for data type");
      this->receive_buffer_.clear();
      return;
    }

    float value = 0.0f;
    uint8_t value_size = 0;
    uint8_t data_type = plaintext[current_position];
    current_position++;

    switch (data_type) {
      case DataType::DOUBLE_LONG_UNSIGNED: {
        value_size = 4;
        if (current_position + value_size > message_length) {
          ESP_LOGE(TAG, "OBIS: Buffer too short for DOUBLE_LONG_UNSIGNED");
          this->receive_buffer_.clear();
          return;
        }
        value = encode_uint32(plaintext[current_position + 0], plaintext[current_position + 1],
                              plaintext[current_position + 2], plaintext[current_position + 3]);
        current_position += value_size;
        break;
      }
      case DataType::LONG_UNSIGNED: {
        value_size = 2;
        if (current_position + value_size > message_length) {
          ESP_LOGE(TAG, "OBIS: Buffer too short for LONG_UNSIGNED");
          this->receive_buffer_.clear();
          return;
        }
        value = encode_uint16(plaintext[current_position + 0], plaintext[current_position + 1]);
        current_position += value_size;
        break;
      }
      case DataType::OCTET_STRING: {
        uint8_t data_length = plaintext[current_position];
        current_position++;  // Advance past string length
        if (current_position + data_length > message_length) {
          ESP_LOGE(TAG, "OBIS: Buffer too short for OCTET_STRING");
          this->receive_buffer_.clear();
          return;
        }
        // Handle timestamp (normal OBIS code or NETZNOE special case)
        if (obis_cd == OBIS_TIMESTAMP || timestamp_found) {
          if (data_length < 8) {
            ESP_LOGE(TAG, "OBIS: Timestamp data too short: %u", data_length);
            this->receive_buffer_.clear();
            return;
          }
          uint16_t year = encode_uint16(plaintext[current_position + 0], plaintext[current_position + 1]);
          uint8_t month = plaintext[current_position + 2];
          uint8_t day = plaintext[current_position + 3];
          uint8_t hour = plaintext[current_position + 5];
          uint8_t minute = plaintext[current_position + 6];
          uint8_t second = plaintext[current_position + 7];
          if (year > 9999 || month > 12 || day > 31 || hour > 23 || minute > 59 || second > 59) {
            ESP_LOGE(TAG, "Invalid timestamp values: %04u-%02u-%02uT%02u:%02u:%02uZ", year, month, day, hour, minute,
                     second);
            this->receive_buffer_.clear();
            return;
          }
          snprintf(data.timestamp, sizeof(data.timestamp), "%04u-%02u-%02uT%02u:%02u:%02uZ", year, month, day, hour,
                   minute, second);
        } else if (meter_number_found) {
          snprintf(data.meternumber, sizeof(data.meternumber), "%.*s", data_length, &plaintext[current_position]);
        }
        current_position += data_length;
        break;
      }
      default:
        ESP_LOGE(TAG, "OBIS: Unsupported OBIS data type: %x", data_type);
        this->receive_buffer_.clear();
        return;
    }

    // Skip break after data
    if (this->provider_ == PROVIDER_NETZNOE) {
      // Don't skip the break on the first timestamp, as there's none
      if (!timestamp_found) {
        current_position += 2;
      }
    } else {
      current_position += 2;
    }

    // Check for additional data (scaler-unit structure)
    if (current_position < message_length && plaintext[current_position] == DataType::INTEGER) {
      // Apply scaler: real_value = raw_value × 10^scaler
      if (current_position + 1 < message_length) {
        int8_t scaler = static_cast<int8_t>(plaintext[current_position + 1]);
        if (scaler != 0) {
          value *= powf(10.0f, scaler);
        }
      }

      // on EVN Meters there is no additional break
      if (this->provider_ == PROVIDER_NETZNOE) {
        current_position += 4;
      } else {
        current_position += 6;
      }
    }

    // Handle numeric values (LONG_UNSIGNED and DOUBLE_LONG_UNSIGNED)
    if (value_size > 0) {
      switch (obis_cd) {
        case OBIS_VOLTAGE_L1:
          data.voltage_l1 = value;
          break;
        case OBIS_VOLTAGE_L2:
          data.voltage_l2 = value;
          break;
        case OBIS_VOLTAGE_L3:
          data.voltage_l3 = value;
          break;
        case OBIS_CURRENT_L1:
          data.current_l1 = value;
          break;
        case OBIS_CURRENT_L2:
          data.current_l2 = value;
          break;
        case OBIS_CURRENT_L3:
          data.current_l3 = value;
          break;
        case OBIS_ACTIVE_POWER_PLUS:
          data.active_power_plus = value;
          break;
        case OBIS_ACTIVE_POWER_MINUS:
          data.active_power_minus = value;
          break;
        case OBIS_ACTIVE_ENERGY_PLUS:
          data.active_energy_plus = value;
          break;
        case OBIS_ACTIVE_ENERGY_MINUS:
          data.active_energy_minus = value;
          break;
        case OBIS_REACTIVE_ENERGY_PLUS:
          data.reactive_energy_plus = value;
          break;
        case OBIS_REACTIVE_ENERGY_MINUS:
          data.reactive_energy_minus = value;
          break;
        case OBIS_POWER_FACTOR:
          data.power_factor = value;
          power_factor_found = true;
          break;
        default:
          ESP_LOGW(TAG, "Unsupported OBIS code 0x%04X", obis_cd);
      }
    }
  }

  this->receive_buffer_.clear();

  ESP_LOGI(TAG, "Received valid data");
  this->publish_sensors(data);
  this->status_clear_warning();
}

}  // namespace esphome::dlms_meter
