#pragma once

// Ignore Zephyr. It doesn't have any encryption library.
#if defined(USE_ESP32) || defined(USE_ARDUINO) || defined(USE_HOST)

#include <array>
#include <vector>

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
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
#elif !defined(USE_ESP8266) && __has_include(<mbedtls/gcm.h>)
#if __has_include(<mbedtls/esp_config.h>)
#include <mbedtls/esp_config.h>
#endif
#include <dlms_parser/decryption/aes_128_gcm_decryptor_mbedtls.h>
#elif __has_include(<bearssl/bearssl.h>)
#include <dlms_parser/decryption/aes_128_gcm_decryptor_bearssl.h>
#else
#error "The platform doesn't provide a compatible encryption library for dlms_parser"
#endif

namespace esphome::dlms_meter {

// Entity storage is sized by codegen; a platform with no entities emits no define
#ifndef DLMS_MAX_SENSORS
static constexpr uint8_t DLMS_MAX_SENSORS = 0;
#endif
#ifndef DLMS_MAX_TEXT_SENSORS
static constexpr uint8_t DLMS_MAX_TEXT_SENSORS = 0;
#endif
#ifndef DLMS_MAX_BINARY_SENSORS
static constexpr uint8_t DLMS_MAX_BINARY_SENSORS = 0;
#endif

#ifdef USE_SENSOR
struct SensorItem {
  dlms_parser::ObisId obis;
  sensor::Sensor *sensor;
};
#endif
#ifdef USE_TEXT_SENSOR
struct TextSensorItem {
  dlms_parser::ObisId obis;
  text_sensor::TextSensor *sensor;
};
#endif
#ifdef USE_BINARY_SENSOR
struct BinarySensorItem {
  dlms_parser::ObisId obis;
  binary_sensor::BinarySensor *sensor;
};
#endif

struct CustomPattern {
  const char *pattern;
  const char *name;
  int priority;
  dlms_parser::ObisId default_obis;
};

#if __has_include(<psa/crypto.h>)
using Aes128GcmDecryptorImpl = dlms_parser::Aes128GcmDecryptorTfPsa;
#elif !defined(USE_ESP8266) && __has_include(<mbedtls/gcm.h>)
using Aes128GcmDecryptorImpl = dlms_parser::Aes128GcmDecryptorMbedTls;
#else
using Aes128GcmDecryptorImpl = dlms_parser::Aes128GcmDecryptorBearSsl;
#endif

class DlmsMeterComponent final : public Component, public uart::UARTDevice {
 public:
  DlmsMeterComponent(uint32_t receive_timeout_ms, bool skip_crc_check, const char *decryption_key,
                     const char *authentication_key, std::vector<CustomPattern> custom_patterns);

  void setup() override;
  void dump_config() override;
  void loop() override;

#ifdef USE_SENSOR
  void register_sensor(const dlms_parser::ObisId &obis, sensor::Sensor *sensor);
#endif
#ifdef USE_TEXT_SENSOR
  void register_text_sensor(const dlms_parser::ObisId &obis, text_sensor::TextSensor *sensor);
#endif
#ifdef USE_BINARY_SENSOR
  void register_binary_sensor(const dlms_parser::ObisId &obis, binary_sensor::BinarySensor *sensor);
#endif

 protected:
  void read_rx_buffer_();
  void flush_rx_buffer_();
  void process_frame_();
  void on_data_(const dlms_parser::AxdrCapture &capture);

  std::array<uint8_t, 2048> rx_buffer_;
  size_t bytes_accumulated_{0};
  uint32_t last_rx_char_time_{0};

  uint32_t receive_timeout_ms_{1000};
  bool skip_crc_check_{false};

  std::vector<CustomPattern> custom_patterns_;

  Aes128GcmDecryptorImpl decryptor_;
  dlms_parser::DlmsParser parser_;

#ifdef USE_SENSOR
  StaticVector<SensorItem, DLMS_MAX_SENSORS> sensors_;
#endif
#ifdef USE_TEXT_SENSOR
  StaticVector<TextSensorItem, DLMS_MAX_TEXT_SENSORS> text_sensors_;
#endif
#ifdef USE_BINARY_SENSOR
  StaticVector<BinarySensorItem, DLMS_MAX_BINARY_SENSORS> binary_sensors_;
#endif
};

}  // namespace esphome::dlms_meter

#endif
