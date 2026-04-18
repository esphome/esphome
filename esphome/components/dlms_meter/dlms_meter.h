#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/log.h"
#include "esphome/components/uart/uart.h"

#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif

#include <dlms_parser/dlms_parser.h>

#if __has_include(<psa/crypto.h>)
#include <dlms_parser/decryption/aes_128_gcm_decryptor_tfpsa.h>
using Aes128GcmDecryptorImpl = dlms_parser::Aes128GcmDecryptorTfPsa;
#elif __has_include(<mbedtls/gcm.h>)
#if __has_include(<mbedtls/esp_config.h>)
#include <mbedtls/esp_config.h>
#endif
#include <dlms_parser/decryption/aes_128_gcm_decryptor_mbedtls.h>
using Aes128GcmDecryptorImpl = dlms_parser::Aes128GcmDecryptorMbedTls;
#elif __has_include(<bearssl/bearssl.h>)
#include <dlms_parser/decryption/aes_128_gcm_decryptor_bearssl.h>
using Aes128GcmDecryptorImpl = dlms_parser::Aes128GcmDecryptorBearSsl;
#else
#define DLMS_METER_NO_CRYPTO
#include <span>
// Fallback dummy decryptor for platforms without supported crypto (e.g., Zephyr during clang-tidy)
class Aes128GcmDecryptorDummy : public dlms_parser::Aes128GcmDecryptor {
 public:
  void set_decryption_key(const dlms_parser::Aes128GcmDecryptionKey &key) override {}
  bool decrypt_in_place(std::span<const uint8_t> iv, std::span<uint8_t> ciphertext_and_plaintext,
                        std::span<const uint8_t> aad, std::span<const uint8_t> tag) override {
    return false;
  }
};
using Aes128GcmDecryptorImpl = Aes128GcmDecryptorDummy;
#endif

#include <vector>
#include <string>
#include <array>
#include <optional>

namespace esphome::dlms_meter {

#ifdef USE_SENSOR
struct SensorItem {
  std::string obis_code;
  sensor::Sensor *sensor;
};
#endif
#ifdef USE_TEXT_SENSOR
struct TextSensorItem {
  std::string obis_code;
  text_sensor::TextSensor *sensor;
};
#endif
#ifdef USE_BINARY_SENSOR
struct BinarySensorItem {
  std::string obis_code;
  binary_sensor::BinarySensor *sensor;
};
#endif

struct CustomPattern {
  std::string pattern;
  std::optional<std::string> name;
  int priority{0};
  std::optional<std::array<uint8_t, 6>> default_obis;
};

class DlmsMeterComponent : public Component, public uart::UARTDevice {
 public:
  DlmsMeterComponent(bool skip_crc_check, std::optional<std::array<uint8_t, 16>> decryption_key,
                     std::optional<std::array<uint8_t, 16>> authentication_key,
                     std::vector<CustomPattern> custom_patterns)
      : skip_crc_check_(skip_crc_check),
        custom_patterns_(std::move(custom_patterns)),
        decryption_key_(decryption_key),
        authentication_key_(authentication_key),
        parser_(&decryptor_) {}

  void setup() override;
  void dump_config() override;
  void loop() override;

#ifdef USE_SENSOR
  void register_sensor(const std::string &obis_code, sensor::Sensor *sensor);
#endif
#ifdef USE_TEXT_SENSOR
  void register_text_sensor(const std::string &obis_code, text_sensor::TextSensor *sensor);
#endif
#ifdef USE_BINARY_SENSOR
  void register_binary_sensor(const std::string &obis_code, binary_sensor::BinarySensor *sensor);
#endif

 protected:
  void read_rx_buffer_();
  void flush_rx_buffer_();
  void process_frame_();
  void on_data_(const char *obis_code, float float_val, const char *str_val, bool is_numeric);

  std::array<uint8_t, 2048> rx_buffer_;
  size_t bytes_accumulated_{0};
  uint32_t last_rx_char_time_{0};

  uint32_t receive_timeout_ms_{500};
  bool skip_crc_check_{false};

  std::vector<CustomPattern> custom_patterns_;
  std::optional<std::array<uint8_t, 16>> decryption_key_;
  std::optional<std::array<uint8_t, 16>> authentication_key_;

  Aes128GcmDecryptorImpl decryptor_;
  dlms_parser::DlmsParser parser_;

#ifdef USE_SENSOR
  std::vector<SensorItem> sensors_;
#endif
#ifdef USE_TEXT_SENSOR
  std::vector<TextSensorItem> text_sensors_;
#endif
#ifdef USE_BINARY_SENSOR
  std::vector<BinarySensorItem> binary_sensors_;
#endif
};

}  // namespace esphome::dlms_meter
