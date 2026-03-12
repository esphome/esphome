#include "dlms_meter.h"

#if defined(USE_ESP8266_FRAMEWORK_ARDUINO)
#include <bearssl/bearssl.h>
#elif defined(USE_ESP32)
#include "mbedtls/esp_config.h"
#include "mbedtls/gcm.h"
#endif

namespace esphome::dlms_meter {

static constexpr const char *TAG = "dlms_meter";

void DlmsMeterComponent::dump_config() {
  const char *provider_name = "Generic";
  if (this->provider_ == PROVIDER_NETZNOE) {
    provider_name = "Netz NOE";
  } else if (this->provider_ == PROVIDER_KAMSTRUP_OMNIPOWER) {
    provider_name = "Kamstrup";
  }
  ESP_LOGCONFIG(TAG,
                "DLMS Meter:\n"
                "  Provider: %s\n"
                "  Read Timeout: %u ms\n"
                "  Authentication Key: %s",
                provider_name, this->read_timeout_, this->has_authentication_key_ ? "configured" : "not configured");
#define DLMS_METER_LOG_SENSOR(s) LOG_SENSOR("  ", #s, this->s##_sensor_);
  DLMS_METER_SENSOR_LIST(DLMS_METER_LOG_SENSOR, )
#define DLMS_METER_LOG_TEXT_SENSOR(s) LOG_TEXT_SENSOR("  ", #s, this->s##_text_sensor_);
  DLMS_METER_TEXT_SENSOR_LIST(DLMS_METER_LOG_TEXT_SENSOR, )
}

void DlmsMeterComponent::publish_parsed_data_(MeterData &data) {
  this->receive_buffer_.clear();
  ESP_LOGI(TAG, "Received valid data");
  this->publish_sensors(data);
  this->status_clear_warning();
}

void DlmsMeterComponent::loop() {
  size_t avail = this->available();
  if (avail > 0) {
    size_t remaining = this->max_receive_length_() - this->receive_buffer_.size();
    if (remaining == 0) {
      ESP_LOGW(TAG, "Receive buffer full, dropping remaining bytes");
    } else {
      if (avail > remaining) {
        avail = remaining;
      }
      uint8_t buf[64];
      while (avail > 0) {
        size_t to_read = std::min(avail, sizeof(buf));
        if (!this->read_array(buf, to_read)) {
          break;
        }
        avail -= to_read;
        this->receive_buffer_.insert(this->receive_buffer_.end(), buf, buf + to_read);
        this->last_read_ = millis();
      }
    }
  }

  if (!this->receive_buffer_.empty() && millis() - this->last_read_ > this->read_timeout_) {
    const TransportType transport = this->detect_transport_();
    if (transport == TransportType::UNKNOWN) {
      ESP_LOGE(TAG, "Unable to determine transport type from received data");
      this->receive_buffer_.clear();
      return;
    }

    this->dlms_payload_.clear();
    const bool frame_ok = transport == TransportType::HDLC ? this->parse_hdlc_(this->dlms_payload_)
                                                           : this->parse_mbus_(this->dlms_payload_);
    if (!frame_ok) {
      return;
    }

    uint16_t message_length;
    uint8_t systitle_length;
    uint16_t header_offset;
    if (!this->parse_dlms_(this->dlms_payload_, message_length, systitle_length, header_offset)) {
      return;
    }

    if (message_length < DECODER_START_OFFSET || message_length > MAX_MESSAGE_LENGTH) {
      ESP_LOGE(TAG, "DLMS: Message length invalid: %u", message_length);
      this->receive_buffer_.clear();
      return;
    }

    if (!this->decrypt_(this->dlms_payload_, message_length, systitle_length, header_offset)) {
      return;
    }
    this->decode_obis_(&this->dlms_payload_[header_offset + DLMS_PAYLOAD_OFFSET], message_length);
  }
}

bool DlmsMeterComponent::parse_dlms_(const std::vector<uint8_t> &dlms_payload, uint16_t &message_length,
                                     uint8_t &systitle_length, uint16_t &header_offset) {
  ESP_LOGV(TAG, "Parsing DLMS header");
  if (dlms_payload.size() < DLMS_HEADER_LENGTH + DLMS_HEADER_EXT_OFFSET) {
    ESP_LOGE(TAG, "DLMS: Payload too short");
    this->receive_buffer_.clear();
    return false;
  }

  if (dlms_payload[DLMS_CIPHER_OFFSET] != GLO_CIPHERING) {
    ESP_LOGE(TAG, "DLMS: Unsupported cipher");
    this->receive_buffer_.clear();
    return false;
  }

  systitle_length = dlms_payload[DLMS_SYST_OFFSET];
  if (systitle_length != 0x08) {
    ESP_LOGE(TAG, "DLMS: Unsupported system title length");
    this->receive_buffer_.clear();
    return false;
  }

  message_length = dlms_payload[DLMS_LENGTH_OFFSET];
  header_offset = 0;

  if (this->provider_ == PROVIDER_NETZNOE) {
    if (message_length == NETZ_NOE_MAGIC_BYTE &&
        dlms_payload[DLMS_LENGTH_OFFSET + 1] == NETZ_NOE_EXPECTED_MESSAGE_LENGTH &&
        dlms_payload[DLMS_LENGTH_OFFSET + 2] == NETZ_NOE_EXPECTED_SECURITY_CONTROL_BYTE) {
      message_length = dlms_payload[DLMS_LENGTH_OFFSET + 1];
      header_offset = 1;
    } else {
      ESP_LOGE(TAG, "Wrong Length - Security Control Byte sequence detected for provider EVN");
    }
  } else if (message_length == TWO_BYTE_LENGTH) {
    message_length = encode_uint16(dlms_payload[DLMS_LENGTH_OFFSET + 1], dlms_payload[DLMS_LENGTH_OFFSET + 2]);
    header_offset = DLMS_HEADER_EXT_OFFSET;
  }

  if (message_length < DLMS_LENGTH_CORRECTION) {
    ESP_LOGE(TAG, "DLMS: Message length too short: %u", message_length);
    this->receive_buffer_.clear();
    return false;
  }
  message_length -= DLMS_LENGTH_CORRECTION;

  if (dlms_payload.size() - DLMS_HEADER_LENGTH - header_offset != message_length) {
    ESP_LOGV(TAG, "DLMS: Length mismatch - payload=%d, header=%d, offset=%d, message=%d", dlms_payload.size(),
             DLMS_HEADER_LENGTH, header_offset, message_length);
    ESP_LOGE(TAG, "DLMS: Message has invalid length");
    this->receive_buffer_.clear();
    return false;
  }

  const uint8_t security_control = dlms_payload[header_offset + DLMS_SECBYTE_OFFSET];
  const uint8_t security_suite = security_control & DLMS_SECURITY_SUITE_MASK;
  if (this->provider_ == PROVIDER_KAMSTRUP_OMNIPOWER && security_suite != DLMS_SECURITY_SUPPORTED_SUITE_0) {
    ESP_LOGE(TAG, "DLMS: Kamstrup provider only supports security suite 0, got 0x%02X", security_control);
    this->receive_buffer_.clear();
    return false;
  }
  if ((security_control & DLMS_SECURITY_ENCRYPTION) == 0) {
    ESP_LOGE(TAG, "DLMS: Unsupported security control byte 0x%02X, encryption bit not set", security_control);
    this->receive_buffer_.clear();
    return false;
  }
  if (security_suite != DLMS_SECURITY_SUPPORTED_SUITE_0 && security_suite != DLMS_SECURITY_SUPPORTED_SUITE_1) {
    ESP_LOGE(TAG, "DLMS: Unsupported security suite in security control byte 0x%02X", security_control);
    this->receive_buffer_.clear();
    return false;
  }

  return true;
}

bool DlmsMeterComponent::decrypt_(std::vector<uint8_t> &dlms_payload, uint16_t message_length, uint8_t systitle_length,
                                  uint16_t header_offset) {
  ESP_LOGV(TAG, "Decrypting payload");
  const uint8_t security_control = dlms_payload[header_offset + DLMS_SECBYTE_OFFSET];
  const bool has_authentication = (security_control & DLMS_SECURITY_AUTHENTICATION) != 0;
  uint8_t iv[12];
  memcpy(&iv[0], &dlms_payload[DLMS_SYST_OFFSET + 1], systitle_length);
  memcpy(&iv[8], &dlms_payload[header_offset + DLMS_FRAMECOUNTER_OFFSET], DLMS_FRAMECOUNTER_LENGTH);

  uint8_t *payload_ptr = &dlms_payload[header_offset + DLMS_PAYLOAD_OFFSET];
  size_t ciphertext_length = message_length;
  const uint8_t *tag_ptr = nullptr;
  uint8_t aad[17];
  size_t aad_length = 0;

  if (has_authentication) {
    if (!this->has_authentication_key_) {
      ESP_LOGE(TAG, "DLMS: Authentication key required but not configured");
      this->receive_buffer_.clear();
      return false;
    }
    if (message_length < DLMS_AUTH_TAG_LENGTH) {
      ESP_LOGE(TAG, "DLMS: Authenticated payload too short: %u", message_length);
      this->receive_buffer_.clear();
      return false;
    }
    ciphertext_length -= DLMS_AUTH_TAG_LENGTH;
    tag_ptr = payload_ptr + ciphertext_length;
    aad[0] = security_control;
    memcpy(&aad[1], this->authentication_key_.data(), this->authentication_key_.size());
    aad_length = sizeof(aad);
  }

#if defined(USE_ESP8266_FRAMEWORK_ARDUINO)
  br_gcm_context gcm_ctx;
  br_aes_ct_ctr_keys bc;
  br_aes_ct_ctr_init(&bc, this->decryption_key_.data(), this->decryption_key_.size());
  br_gcm_init(&gcm_ctx, &bc.vtable, br_ghash_ctmul32);
  br_gcm_reset(&gcm_ctx, iv, sizeof(iv));
  if (has_authentication) {
    br_gcm_aad_inject(&gcm_ctx, aad, aad_length);
  }
  br_gcm_flip(&gcm_ctx);
  br_gcm_run(&gcm_ctx, 0, payload_ptr, ciphertext_length);
  if (has_authentication && br_gcm_check_tag_trunc(&gcm_ctx, tag_ptr, DLMS_AUTH_TAG_LENGTH) == 0) {
    ESP_LOGE(TAG, "Decryption failed: authentication tag mismatch");
    this->receive_buffer_.clear();
    return false;
  }
#elif defined(USE_ESP32)
  mbedtls_gcm_context gcm_ctx;
  mbedtls_gcm_init(&gcm_ctx);
  int ret = mbedtls_gcm_setkey(&gcm_ctx, MBEDTLS_CIPHER_ID_AES, this->decryption_key_.data(),
                               this->decryption_key_.size() * 8);
  if (ret != 0) {
    mbedtls_gcm_free(&gcm_ctx);
    ESP_LOGE(TAG, "DLMS: Failed to initialize GCM key: %d", ret);
    this->receive_buffer_.clear();
    return false;
  }

  if (has_authentication) {
    ret = mbedtls_gcm_auth_decrypt(&gcm_ctx, ciphertext_length, iv, sizeof(iv), aad, aad_length, tag_ptr,
                                   DLMS_AUTH_TAG_LENGTH, payload_ptr, payload_ptr);
  } else {
    size_t outlen = 0;
    ret = mbedtls_gcm_starts(&gcm_ctx, MBEDTLS_GCM_DECRYPT, iv, sizeof(iv));
    if (ret == 0) {
      ret = mbedtls_gcm_update_ad(&gcm_ctx, nullptr, 0);
    }
    if (ret == 0) {
      ret = mbedtls_gcm_update(&gcm_ctx, payload_ptr, ciphertext_length, payload_ptr, ciphertext_length, &outlen);
      if (ret == 0 && outlen != ciphertext_length) {
        ret = -1;
      }
    }
    if (ret == 0) {
      ret = mbedtls_gcm_finish(&gcm_ctx, nullptr, 0, &outlen, nullptr, 0);
      if (ret == 0 && outlen != 0) {
        ret = -1;
      }
    }
  }
  mbedtls_gcm_free(&gcm_ctx);
  if (ret != 0) {
    ESP_LOGE(TAG, "Decryption failed with error: %d", ret);
    this->receive_buffer_.clear();
    return false;
  }
#endif

  if (payload_ptr[0] != DATA_NOTIFICATION || payload_ptr[5] != TIMESTAMP_DATETIME) {
    ESP_LOGE(TAG, "OBIS: Packet was decrypted but data is invalid");
    this->receive_buffer_.clear();
    return false;
  }
  ESP_LOGV(TAG, "Decrypted payload: %d bytes", message_length);
  return true;
}

}  // namespace esphome::dlms_meter
