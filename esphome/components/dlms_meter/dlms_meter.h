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

#if defined(ESP_IDF_VERSION) && defined(ESP_IDF_VERSION_VAL)
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
#include <dlms_parser/decryption/aes_128_gcm_decryptor_tfpsa.h>
using Aes128GcmDecryptorImpl = dlms_parser::Aes128GcmDecryptorTfPsa;
#else
#include <mbedtls/esp_config.h>
#include <dlms_parser/decryption/aes_128_gcm_decryptor_mbedtls.h>
using Aes128GcmDecryptorImpl = dlms_parser::Aes128GcmDecryptorMbedTls;
#endif
#elif __has_include(<mbedtls/gcm.h>)
#include <mbedtls/esp_config.h>
#include <dlms_parser/decryption/aes_128_gcm_decryptor_mbedtls.h>
using Aes128GcmDecryptorImpl = dlms_parser::Aes128GcmDecryptorMbedTls;
#elif __has_include(<bearssl.h>)
#include <dlms_parser/decryption/aes_128_gcm_decryptor_bearssl.h>
using Aes128GcmDecryptorImpl = dlms_parser::Aes128GcmDecryptorBearSsl;
#else
#error "The platform doesn't provide a compatible encryption library for dlms_meter"
#endif

#include <vector>
#include <string>
#include <array>

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

class DlmsMeterComponent : public Component, public uart::UARTDevice {
 public:
  DlmsMeterComponent() : parser_(&decryptor_) {}

  void setup() override;
  void dump_config() override;
  void loop() override;

  void set_decryption_key(const std::array<uint8_t, 16> &key);
  void set_authentication_key(const std::array<uint8_t, 16> &key);
  void add_custom_pattern(const std::string &pattern) { this->custom_patterns_.push_back(pattern); }
  void set_skip_crc_check(bool skip) { this->skip_crc_check_ = skip; }

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
  void process_frame_();
  void on_data_(const char *obis_code, float float_val, const char *str_val, bool is_numeric);

  std::array<uint8_t, 2048> rx_buffer_;
  size_t bytes_accumulated_{0};
  uint32_t last_rx_char_time_{0};
  bool receiving_{false};

  uint32_t receive_timeout_ms_{500};
  bool skip_crc_check_{false};

  std::vector<std::string> custom_patterns_;

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
