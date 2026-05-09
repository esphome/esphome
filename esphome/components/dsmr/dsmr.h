#pragma once

// Ignore Zephyr. It doesn't have any encryption library.
#if defined(USE_ESP32) || defined(USE_ARDUINO) || defined(USE_HOST)

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/log.h"
#include <dsmr_parser/dlms_packet_decryptor.h>
#include <dsmr_parser/fields.h>
#include <dsmr_parser/packet_accumulator.h>
#include <dsmr_parser/parser.h>
#include <array>
#include <span>
#include <vector>

#if __has_include(<psa/crypto.h>)
#include <dsmr_parser/decryption/aes128gcm_tfpsa.h>
#elif __has_include(<mbedtls/gcm.h>)
#if __has_include(<mbedtls/esp_config.h>)
#include <mbedtls/esp_config.h>
#endif
#include <dsmr_parser/decryption/aes128gcm_mbedtls.h>
#elif __has_include(<bearssl/bearssl.h>)
#include <dsmr_parser/decryption/aes128gcm_bearssl.h>
#else
#error "The platform doesn't provide a compatible encryption library for dsmr_parser"
#endif

namespace esphome::dsmr {

#if __has_include(<psa/crypto.h>)
using Aes128GcmDecryptorImpl = dsmr_parser::Aes128GcmTfPsa;
#elif __has_include(<mbedtls/gcm.h>)
using Aes128GcmDecryptorImpl = dsmr_parser::Aes128GcmMbedTls;
#else
using Aes128GcmDecryptorImpl = dsmr_parser::Aes128GcmBearSsl;
#endif

using namespace dsmr_parser::fields;

#ifndef DSMR_SENSOR_LIST
#define DSMR_SENSOR_LIST(F, SEP)
#endif

#ifndef DSMR_TEXT_SENSOR_LIST
#define DSMR_TEXT_SENSOR_LIST(F, SEP)
#endif

#define DSMR_IDENTITY(s) s
#define DSMR_COMMA ,
#define DSMR_PREPEND_COMMA(...) __VA_OPT__(, ) __VA_ARGS__

#ifdef DSMR_TEXT_SENSOR_LIST_DEFINED
using MyData = dsmr_parser::ParsedData<DSMR_TEXT_SENSOR_LIST(DSMR_IDENTITY, DSMR_COMMA)
                                           DSMR_PREPEND_COMMA(DSMR_SENSOR_LIST(DSMR_IDENTITY, DSMR_COMMA))>;
#else
using MyData = dsmr_parser::ParsedData<DSMR_SENSOR_LIST(DSMR_IDENTITY, DSMR_COMMA)>;
#endif

class Dsmr : public Component, public uart::UARTDevice {
 public:
  Dsmr(uart::UARTComponent *uart, bool crc_check, size_t max_telegram_length, uint32_t request_interval,
       uint32_t receive_timeout, GPIOPin *request_pin, const char *decryption_key)
      : uart::UARTDevice(uart),
        request_interval_(request_interval),
        receive_timeout_(receive_timeout),
        request_pin_(request_pin),
        buffer_(max_telegram_length),
        packet_accumulator_(buffer_, crc_check) {
    this->set_decryption_key_(decryption_key);
  }

  void setup() override;
  void loop() override;

  void publish_sensors(MyData &data) {
#define DSMR_PUBLISH_SENSOR(s) \
  if (data.s##_present && this->s_##s##_ != nullptr) \
    s_##s##_->publish_state(data.s);
    DSMR_SENSOR_LIST(DSMR_PUBLISH_SENSOR, )

#define DSMR_PUBLISH_TEXT_SENSOR(s) \
  if (data.s##_present && this->s_##s##_ != nullptr) \
    s_##s##_->publish_state(data.s.data(), data.s.size());
    DSMR_TEXT_SENSOR_LIST(DSMR_PUBLISH_TEXT_SENSOR, )
  };

  void dump_config() override;

  // Remove before 2026.8.0
  ESPDEPRECATED("Use 'decryption_key' configuration parameter. This method will be removed in 2026.8.0", "2026.2.0")
  void set_decryption_key(const std::string &decryption_key) { this->set_decryption_key_(decryption_key.c_str()); }

// Sensor setters
#define DSMR_SET_SENSOR(s) \
  void set_##s(sensor::Sensor *sensor) { s_##s##_ = sensor; }
  DSMR_SENSOR_LIST(DSMR_SET_SENSOR, )

#define DSMR_SET_TEXT_SENSOR(s) \
  void set_##s(text_sensor::TextSensor *sensor) { s_##s##_ = sensor; }
  DSMR_TEXT_SENSOR_LIST(DSMR_SET_TEXT_SENSOR, )

  // handled outside dsmr
  void set_telegram(text_sensor::TextSensor *sensor) { s_telegram_ = sensor; }

 protected:
  void set_decryption_key_(const char *decryption_key);
  void receive_telegram_();
  void receive_encrypted_telegram_();
  void flush_rx_buffer_();

  bool parse_telegram_(const dsmr_parser::DsmrUnencryptedTelegram &telegram);
  bool request_interval_reached_() const;
  bool ready_to_request_data_();
  void start_requesting_data_();
  void stop_requesting_data_();
  std::span<uint8_t> uart_read_chunk_();

  // Config
  uint32_t request_interval_;
  uint32_t receive_timeout_;
  GPIOPin *request_pin_{nullptr};
  text_sensor::TextSensor *s_telegram_{nullptr};
#define DSMR_DECLARE_SENSOR(s) sensor::Sensor *s_##s##_{nullptr};
  DSMR_SENSOR_LIST(DSMR_DECLARE_SENSOR, )
#define DSMR_DECLARE_TEXT_SENSOR(s) text_sensor::TextSensor *s_##s##_{nullptr};
  DSMR_TEXT_SENSOR_LIST(DSMR_DECLARE_TEXT_SENSOR, )

  // State
  uint32_t last_request_time_{0};
  uint32_t last_read_time_{0};
  bool requesting_data_{false};
  bool encryption_enabled_{false};
  size_t buffer_pos_{0};
  std::vector<uint8_t> buffer_;
  dsmr_parser::PacketAccumulator packet_accumulator_;
  Aes128GcmDecryptorImpl gcm_decryptor_;
  dsmr_parser::DlmsPacketDecryptor dlms_decryptor_{gcm_decryptor_};
  std::array<uint8_t, 256> uart_chunk_reading_buf_;
};
}  // namespace esphome::dsmr

#endif
