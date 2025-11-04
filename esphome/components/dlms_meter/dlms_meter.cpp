#include "dlms_meter.h"

#if defined(USE_ESP8266_FRAMEWORK_ARDUINO)
#include <bearssl/bearssl.h>
#elif defined(USE_ESP32)
#include "mbedtls/esp_config.h"
#include "mbedtls/gcm.h"
#endif

namespace esphome {
namespace dlms_meter {

void DlmsMeterComponent::setup() { ESP_LOGI(TAG, "DLMS smart meter component v%s started", DLMS_METER_VERSION); }

void DlmsMeterComponent::dump_config() {
  ESP_LOGCONFIG(TAG,
                "DLMS Meter:\n"
                "Meter Provider: %d\n"
                "read_timeout: %u\n"
                "decryption_key_length: %d\n"
                "Decryption key: (actual key only logged when log-level is VERY_VERBOSE)",
                this->provider_, this->read_timeout_, this->decryption_key_length_);

  // Verbose level prints decryption key!
  ESP_LOGVV(TAG, "decryption_key: %s",
            format_hex_pretty(&this->decryption_key_[0], this->decryption_key_length_).c_str());

#define DLMS_METER_LOG_SENSOR(s) LOG_SENSOR("  ", #s, this->s##_sensor_);
  DLMS_METER_SENSOR_LIST(DLMS_METER_LOG_SENSOR, )

#define DLMS_METER_LOG_TEXT_SENSOR(s) LOG_TEXT_SENSOR("  ", #s, this->s##_text_sensor_);
  DLMS_METER_TEXT_SENSOR_LIST(DLMS_METER_LOG_TEXT_SENSOR, )

  this->check_uart_settings(2400);
}

void DlmsMeterComponent::loop() {
  while (this->available()) {  // Read while data is available
    uint8_t c;
    this->read_byte(&c);
    this->receive_buffer_.push_back(c);

    this->last_read_ = millis();
  }

  if (!this->receive_buffer_.empty() && millis() - this->last_read_ > this->read_timeout_) {
    this->log_packet_(this->receive_buffer_);

    // Verify and parse M-Bus frames

    ESP_LOGV(TAG, "Parsing M-Bus frames");

    uint16_t frame_offset = 0;          // Offset is used if the M-Bus message is split into multiple frames
    std::vector<uint8_t> mbus_payload;  // Contains the data of the payload

    while (frame_offset < this->receive_buffer_.size()) {
      ESP_LOGV(TAG, "MBUS: Parsing frame");

      // Ensure enough bytes remain for the minimal intro header before accessing indices
      if (this->receive_buffer_.size() - frame_offset < MBUS_HEADER_INTRO_LENGTH) {
        ESP_LOGE(TAG, "MBUS: Not enough data for frame header (need %d, have %d)", MBUS_HEADER_INTRO_LENGTH, (this->receive_buffer_.size() - frame_offset));
        this->abort_();
        return;
      }

      // Check start bytes
      if (this->receive_buffer_[frame_offset + MBUS_START1_OFFSET] != START_BYTE_LONG_FRAME ||
          this->receive_buffer_[frame_offset + MBUS_START2_OFFSET] != START_BYTE_LONG_FRAME) {
        ESP_LOGE(TAG, "MBUS: Start bytes do not match");
        this->abort_();
        return;
      }

      // Both length bytes must be identical
      if (this->receive_buffer_[frame_offset + MBUS_LENGTH1_OFFSET] !=
          this->receive_buffer_[frame_offset + MBUS_LENGTH2_OFFSET]) {
        ESP_LOGE(TAG, "MBUS: Length bytes do not match");
        this->abort_();
        return;
      }

      uint8_t frame_length = this->receive_buffer_[frame_offset + MBUS_LENGTH1_OFFSET];  // Get length of this frame

      // Check if received data is enough for the given frame length
      if (this->receive_buffer_.size() - frame_offset < frame_length + 3) { // length field inside packet does not account for second start- + checksum- + stop- byte
        ESP_LOGE(TAG, "MBUS: Frame too big for received data");
        this->abort_();
        return;
      }

      if (this->receive_buffer_[frame_offset + frame_length + MBUS_HEADER_INTRO_LENGTH + MBUS_FOOTER_LENGTH - 1] !=
          STOP_BYTE) {
        ESP_LOGE(TAG, "MBUS: Invalid stop byte");
        this->abort_();
        return;
      }

      /*
       * verify checksum:
       * sum of all bytes starting with c (so start at MBUS_HEADER_INTRO_LENGTH) until the last byte before the checksum
       * only take last byte of the sum
       */
      uint8_t checksum = 0;  // use uint8_t so only the 8 least significant bits are stored
      for (uint8_t i = 0; i < frame_length; i++) {
        checksum += this->receive_buffer_[frame_offset + MBUS_HEADER_INTRO_LENGTH + i];
      }
      if (checksum != this->receive_buffer_[frame_offset + frame_length + MBUS_HEADER_INTRO_LENGTH]) {
        ESP_LOGE(TAG, "MBUS: Invalid checksum: %x != %x", checksum,
                 this->receive_buffer_[frame_offset + frame_length + MBUS_HEADER_INTRO_LENGTH]);
        this->abort_();
        return;
      }

      mbus_payload.insert(mbus_payload.end(), &this->receive_buffer_[frame_offset + MBUS_FULL_HEADER_LENGTH],
                          &this->receive_buffer_[frame_offset + MBUS_HEADER_INTRO_LENGTH + frame_length]);

      frame_offset += MBUS_HEADER_INTRO_LENGTH + frame_length + MBUS_FOOTER_LENGTH;
    }

    // Verify and parse DLMS header

    ESP_LOGV(TAG, "Parsing DLMS header");

    if (mbus_payload.size() < 20) {  // If the payload is too short we need to abort
      ESP_LOGE(TAG, "DLMS: Payload too short");
      this->abort_();
      return;
    }

    if (mbus_payload[DLMS_CIPHER_OFFSET] != GLO_CIPHERING) {  // Only general-glo-ciphering is supported (0xDB)
      ESP_LOGE(TAG, "DLMS: Unsupported cipher");
      this->abort_();
      return;
    }

    uint8_t systitle_length = mbus_payload[DLMS_SYST_OFFSET];

    if (systitle_length != 0x08) {  // Only system titles with length of 8 are supported
      ESP_LOGE(TAG, "DLMS: Unsupported system title length");
      this->abort_();
      return;
    }

    uint16_t message_length = mbus_payload[DLMS_LENGTH_OFFSET];
    uint16_t header_offset = 0;

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
        ESP_LOGV(TAG, "DLMS: Message length > 127");

        memcpy(&message_length, &mbus_payload[DLMS_LENGTH_OFFSET + 1], 2);
        message_length = swap_uint16_(message_length);

        header_offset = DLMS_HEADER_EXT_OFFSET;  // Header is now 2 bytes longer due to length > 127
      } else {
        ESP_LOGV(TAG, "DLMS: Message length <= 127");
      }
    }

    message_length -= DLMS_LENGTH_CORRECTION;  // Correct message length due to part of header being included in length

    if (mbus_payload.size() - DLMS_HEADER_LENGTH - header_offset != message_length) {
      ESP_LOGV(TAG, "lengths: %d, %d, %d, %d", mbus_payload.size(), DLMS_HEADER_LENGTH, header_offset, message_length);
      ESP_LOGE(TAG, "DLMS: Message has invalid length");
      this->abort_();
      return;
    }

    if (mbus_payload[header_offset + DLMS_SECBYTE_OFFSET] != 0x21 &&
        mbus_payload[header_offset + DLMS_SECBYTE_OFFSET] !=
            0x20) {  // Only certain security suite is supported (0x21 || 0x20)
      ESP_LOGE(TAG, "DLMS: Unsupported security control byte");
      this->abort_();
      return;
    }

    // Decryption

    ESP_LOGV(TAG, "Decrypting payload");

    uint8_t iv[12];  // Reserve space for the IV, always 12 bytes
    // Copy system title to IV (System title is before length; no header offset needed!)
    // Add 1 to the offset in order to skip the system title length byte
    memcpy(&iv[0], &mbus_payload[DLMS_SYST_OFFSET + 1], systitle_length);
    memcpy(&iv[8], &mbus_payload[header_offset + DLMS_FRAMECOUNTER_OFFSET],
           DLMS_FRAMECOUNTER_LENGTH);  // Copy frame counter to IV

    uint8_t plaintext[message_length];

#if defined(USE_ESP8266_FRAMEWORK_ARDUINO)
    memcpy(plaintext, &mbus_payload[header_offset + DLMS_PAYLOAD_OFFSET], message_length);
    br_gcm_context gcm_ctx;
    br_aes_ct_ctr_keys bc;
    br_aes_ct_ctr_init(&bc, this->decryption_key_, this->decryption_key_length_);
    br_gcm_init(&gcm_ctx, &bc.vtable, br_ghash_ctmul32);
    br_gcm_reset(&gcm_ctx, iv, sizeof(iv));
    br_gcm_flip(&gcm_ctx);
    br_gcm_run(&gcm_ctx, 0, plaintext, message_length);
#elif defined(USE_ESP32)
    mbedtls_gcm_context gcm_ctx;
    mbedtls_gcm_init(&gcm_ctx);
    mbedtls_gcm_setkey(&gcm_ctx, MBEDTLS_CIPHER_ID_AES, this->decryption_key_, this->decryption_key_length_ * 8);

    mbedtls_gcm_auth_decrypt(&gcm_ctx, message_length, iv, sizeof(iv), NULL, 0, NULL, 0,
                             &mbus_payload[header_offset + DLMS_PAYLOAD_OFFSET], plaintext);

    mbedtls_gcm_free(&gcm_ctx);
#else
#error "Invalid Platform"
#endif

    ESP_LOGV(TAG, "Decrypted payload: %d", message_length);
    ESP_LOGV(TAG, "%s", format_hex_pretty(&plaintext[0], message_length).c_str());

    if (plaintext[0] != DATA_NOTIFICATION || plaintext[5] != TIMESTAMP_DATETIME) {
      ESP_LOGE(TAG, "OBIS: Packet was decrypted but data is invalid");
      this->abort_();
      return;
    }

    // Decoding

    ESP_LOGV(TAG, "Decoding payload");

    MeterData data{};
    uint16_t current_position = DECODER_START_OFFSET;

    while (current_position < message_length) {
      if (plaintext[current_position + OBIS_TYPE_OFFSET] != DataType::OCTET_STRING) {
        ESP_LOGE(TAG, "OBIS: Unsupported OBIS header type: %x", plaintext[current_position + OBIS_TYPE_OFFSET]);
        this->abort_();
        return;
      }

      uint8_t obis_code_length = plaintext[current_position + OBIS_LENGTH_OFFSET];

      if (obis_code_length != 0x06 && obis_code_length != 0x0C) {
        ESP_LOGE(TAG, "OBIS: Unsupported OBIS header length: %x", obis_code_length);
        this->abort_();
        return;
      }

      uint8_t obis_code[obis_code_length];
      memcpy(&obis_code[0], &plaintext[current_position + OBIS_CODE_OFFSET],
             obis_code_length);  // Copy OBIS code to array

      bool timestamp_found = false;
      bool meter_number_found = false;
      if (this->provider_ == PROVIDER_NETZNOE) {
        // Do not advance Position when reading the Timestamp at DECODER_START_OFFSET
        if ((obis_code_length == 0x0C) && (current_position == DECODER_START_OFFSET)) {
          timestamp_found = true;
        } else if ((current_position != DECODER_START_OFFSET) && plaintext[current_position - 1] == 0xFF) {
          meter_number_found = true;
        } else {
          current_position += obis_code_length + 2;  // Advance past code and position
        }
      } else {
        current_position += obis_code_length + 2;  // Advance past code, position and type
      }

      uint8_t data_type = plaintext[current_position];
      current_position++;  // Advance past data type

      uint8_t data_length = 0x00;

      CodeType code_type = CodeType::UNKNOWN;

      ESP_LOGV(TAG, "obisCode (OBIS_A): %d", obis_code[OBIS_A]);
      ESP_LOGV(TAG, "currentPosition: %d", current_position);

      if (obis_code[OBIS_A] == Medium::ELECTRICITY) {
        // Compare C and D against code
        if (memcmp(&obis_code[OBIS_C], ESPDM_VOLTAGE_L1, 2) == 0) {
          code_type = CodeType::VOLTAGE_L1;
        } else if (memcmp(&obis_code[OBIS_C], ESPDM_VOLTAGE_L2, 2) == 0) {
          code_type = CodeType::VOLTAGE_L2;
        } else if (memcmp(&obis_code[OBIS_C], ESPDM_VOLTAGE_L3, 2) == 0) {
          code_type = CodeType::VOLTAGE_L3;
        }

        else if (memcmp(&obis_code[OBIS_C], ESPDM_CURRENT_L1, 2) == 0) {
          code_type = CodeType::CURRENT_L1;
        } else if (memcmp(&obis_code[OBIS_C], ESPDM_CURRENT_L2, 2) == 0) {
          code_type = CodeType::CURRENT_L2;
        } else if (memcmp(&obis_code[OBIS_C], ESPDM_CURRENT_L3, 2) == 0) {
          code_type = CodeType::CURRENT_L3;
        }

        else if (memcmp(&obis_code[OBIS_C], ESPDM_ACTIVE_POWER_PLUS, 2) == 0) {
          code_type = CodeType::ACTIVE_POWER_PLUS;
        } else if (memcmp(&obis_code[OBIS_C], ESPDM_ACTIVE_POWER_MINUS, 2) == 0) {
          code_type = CodeType::ACTIVE_POWER_MINUS;
        } else if (this->provider_ == PROVIDER_NETZNOE && memcmp(&obis_code[OBIS_C], ESPDM_POWER_FACTOR, 2) == 0) {
          code_type = CodeType::POWER_FACTOR;
        }

        else if (memcmp(&obis_code[OBIS_C], ESPDM_ACTIVE_ENERGY_PLUS, 2) == 0) {
          code_type = CodeType::ACTIVE_ENERGY_PLUS;
        } else if (memcmp(&obis_code[OBIS_C], ESPDM_ACTIVE_ENERGY_MINUS, 2) == 0) {
          code_type = CodeType::ACTIVE_ENERGY_MINUS;
        }

        else if (memcmp(&obis_code[OBIS_C], ESPDM_REACTIVE_ENERGY_PLUS, 2) == 0) {
          code_type = CodeType::REACTIVE_ENERGY_PLUS;
        } else if (memcmp(&obis_code[OBIS_C], ESPDM_REACTIVE_ENERGY_MINUS, 2) == 0) {
          code_type = CodeType::REACTIVE_ENERGY_MINUS;
        } else {
          ESP_LOGW(TAG, "OBIS: Unsupported OBIS code OBIS_C: %d, OBIS_D: %d", obis_code[OBIS_C], obis_code[OBIS_D]);
        }
      } else if (obis_code[OBIS_A] == Medium::ABSTRACT) {
        if (memcmp(&obis_code[OBIS_C], ESPDM_TIMESTAMP, 2) == 0) {
          code_type = CodeType::TIMESTAMP;
        } else if (memcmp(&obis_code[OBIS_C], ESPDM_SERIAL_NUMBER, 2) == 0) {
          code_type = CodeType::SERIAL_NUMBER;
        } else if (memcmp(&obis_code[OBIS_C], ESPDM_DEVICE_NAME, 2) == 0) {
          code_type = CodeType::DEVICE_NAME;
        }

        else {
          ESP_LOGW(TAG, "OBIS: Unsupported OBIS code OBIS_C: %d, OBIS_D: %d", obis_code[OBIS_C], obis_code[OBIS_D]);
        }
      } else {
        if (this->provider_ == PROVIDER_NETZNOE) {
          // Needed so the Timestamp at DECODER_START_OFFSET gets read correctly
          // as it doesn't have an obisMedium
          if (timestamp_found) {
            ESP_LOGV(TAG, "Found Timestamp without obisMedium");
            code_type = CodeType::TIMESTAMP;
          } else if (meter_number_found) {
            ESP_LOGV(TAG, "Found MeterNumber without obisMedium");
            code_type = CodeType::METER_NUMBER;
          } else {
            ESP_LOGE(TAG, "OBIS: Unsupported OBIS medium: %x", obis_code[OBIS_A]);
            this->abort_();
            return;
          }
        } else {
          ESP_LOGE(TAG, "OBIS: Unsupported OBIS medium: %x", obis_code[OBIS_A]);
          this->abort_();
          return;
        }
      }

      uint16_t uint16_value;
      uint32_t uint32_value;
      float float_value;

      switch (data_type) {
        case DataType::DOUBLE_LONG_UNSIGNED:
          data_length = 4;

          memcpy(&uint32_value, &plaintext[current_position], 4);  // Copy bytes to integer
          uint32_value = swap_uint32_(uint32_value);               // Swap bytes

          float_value = uint32_value;  // Ignore decimal digits for now

          switch (code_type) {
            case CodeType::ACTIVE_POWER_PLUS:
              data.active_power_plus = float_value;
              break;
            case CodeType::ACTIVE_POWER_MINUS:
              data.active_power_minus = float_value;
              break;
            case CodeType::ACTIVE_ENERGY_PLUS:
              data.active_energy_plus = float_value;
              break;
            case CodeType::ACTIVE_ENERGY_MINUS:
              data.active_energy_minus = float_value;
              break;
            case CodeType::REACTIVE_ENERGY_PLUS:
              data.reactive_energy_plus = float_value;
              break;
            case CodeType::REACTIVE_ENERGY_MINUS:
              data.reactive_energy_minus = float_value;
              break;
            default:
              ESP_LOGW(TAG, "Unsupported CodeType '%d' for DataType '%d'", code_type, data_type);
          }

          break;
        case DataType::LONG_UNSIGNED:
          data_length = 2;

          memcpy(&uint16_value, &plaintext[current_position], 2);  // Copy bytes to integer
          uint16_value = swap_uint16_(uint16_value);               // Swap bytes

          if (plaintext[current_position + 5] == Accuracy::SINGLE_DIGIT) {
            float_value = uint16_value / 10.0;  // Divide by 10 to get decimal places
          } else if (plaintext[current_position + 5] == Accuracy::DOUBLE_DIGIT) {
            float_value = uint16_value / 100.0;  // Divide by 100 to get decimal places
          } else {
            float_value = uint16_value;  // No decimal places
          }

          switch (code_type) {
            case CodeType::VOLTAGE_L1:
              data.voltage_l1 = float_value;
              break;
            case CodeType::VOLTAGE_L2:
              data.voltage_l2 = float_value;
              break;
            case CodeType::VOLTAGE_L3:
              data.voltage_l3 = float_value;
              break;
            case CodeType::CURRENT_L1:
              data.current_l1 = float_value;
              break;
            case CodeType::CURRENT_L2:
              data.current_l2 = float_value;
              break;
            case CodeType::CURRENT_L3:
              data.current_l3 = float_value;
              break;
            case CodeType::POWER_FACTOR:
              if (this->provider_ == PROVIDER_NETZNOE) {
                data.power_factor = float_value / 1000.0f;
              } else {
                ESP_LOGW(TAG, "Unsupported CodeType '%d' for DataType '%d'", code_type, data_type);
              }
              break;
            default:
              ESP_LOGW(TAG, "Unsupported CodeType '%d' for DataType '%d'", code_type, data_type);
          }
          break;
        case DataType::OCTET_STRING:
          ESP_LOGV(TAG, "Arrived on OctetString");
          ESP_LOGV(TAG, "currentPosition: %d, plaintext: %d", current_position, plaintext[current_position]);

          data_length = plaintext[current_position];
          current_position++;  // Advance past string length

          if (code_type == CodeType::TIMESTAMP) {  // Handle timestamp generation
            char timestamp[27];                    // 0000-00-00T00:00:00Z

            uint16_t year;
            uint8_t month;
            uint8_t day;

            uint8_t hour;
            uint8_t minute;
            uint8_t second;

            memcpy(&uint16_value, &plaintext[current_position], 2);
            year = swap_uint16_(uint16_value);

            memcpy(&month, &plaintext[current_position + 2], 1);
            memcpy(&day, &plaintext[current_position + 3], 1);

            memcpy(&hour, &plaintext[current_position + 5], 1);
            memcpy(&minute, &plaintext[current_position + 6], 1);
            memcpy(&second, &plaintext[current_position + 7], 1);
            if (year > 9999 || month > 12 || day > 31 || hour > 23 || minute > 59 || second > 59) {
              ESP_LOGE(TAG, "Invalid timestamp values: %04u-%02u-%02uT%02u:%02u:%02uZ", year, month, day, hour, minute,
                       second);
              this->abort_();
              return;
            }
            sprintf(timestamp, "%04u-%02u-%02uT%02u:%02u:%02uZ", year, month, day, hour, minute, second);

            data.timestamp = timestamp;
          } else if (this->provider_ == PROVIDER_NETZNOE && code_type == CodeType::METER_NUMBER) {
            ESP_LOGV(TAG, "Constructing MeterNumber...");
            char meter_number[13];  // 121110284568

            memcpy(meter_number, &plaintext[current_position], data_length);
            meter_number[12] = '\0';

            data.meternumber = meter_number;
          }
          break;
        default:
          ESP_LOGE(TAG, "OBIS: Unsupported OBIS data type: %x", data_type);
          this->abort_();
          return;
      }

      current_position += data_length;  // Skip data length

      if (this->provider_ == PROVIDER_NETZNOE) {
        // Don't skip the break on the first Timestamp, as there's none
        if (!timestamp_found) {
          current_position += 2;  // Skip break after data
        }
      } else {
        current_position += 2;  // Skip break after data
      }

      if (plaintext[current_position] == DATA_NOTIFICATION) {  // There is still additional data for this type, skip it
        // on EVN Meters the additional data (no additional Break) is only 3 Bytes + 1 Byte for the "there is data" Byte
        if (this->provider_ == PROVIDER_NETZNOE) {
          current_position +=
              4;  // Skip additional data and additional break; this will jump out of bounds on last frame
        } else {
          current_position +=
              6;  // Skip additional data and additional break; this will jump out of bounds on last frame
        }
      }
    }

    this->receive_buffer_.clear();  // Reset buffer

    ESP_LOGI(TAG, "Received valid data");
    this->publish_sensors(data);
    this->status_clear_warning();
  }
}

void DlmsMeterComponent::abort_() { this->receive_buffer_.clear(); }

uint16_t DlmsMeterComponent::swap_uint16_(uint16_t val) { return (val << 8) | (val >> 8); }

uint32_t DlmsMeterComponent::swap_uint32_(uint32_t val) {
  val = ((val << 8) & 0xFF00FF00) | ((val >> 8) & 0xFF00FF);
  return (val << 16) | (val >> 16);
}

void DlmsMeterComponent::set_decryption_key(const uint8_t decryption_key[], size_t decryption_key_length) {
  memcpy(&this->decryption_key_[0], &decryption_key[0], decryption_key_length);
  this->decryption_key_length_ = decryption_key_length;
}

void DlmsMeterComponent::set_provider(uint32_t provider) { this->provider_ = provider; }

void DlmsMeterComponent::log_packet_(const std::vector<uint8_t> &data) {
  ESP_LOGV(TAG, "%s", format_hex_pretty(data).c_str());
}

}  // namespace dlms_meter
}  // namespace esphome
