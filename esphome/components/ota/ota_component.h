#pragma once

#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include <WiFiServer.h>
#include <WiFiClient.h>

namespace esphome {
namespace ota {

enum OTAResponseTypes {
  OTA_RESPONSE_OK = 0,
  OTA_RESPONSE_REQUEST_AUTH = 1,

  OTA_RESPONSE_HEADER_OK = 64,
  OTA_RESPONSE_AUTH_OK = 65,
  OTA_RESPONSE_UPDATE_PREPARE_OK = 66,
  OTA_RESPONSE_BIN_MD5_OK = 67,
  OTA_RESPONSE_RECEIVE_OK = 68,
  OTA_RESPONSE_UPDATE_END_OK = 69,

  OTA_RESPONSE_ERROR_MAGIC = 128,
  OTA_RESPONSE_ERROR_UPDATE_PREPARE = 129,
  OTA_RESPONSE_ERROR_AUTH_INVALID = 130,
  OTA_RESPONSE_ERROR_WRITING_FLASH = 131,
  OTA_RESPONSE_ERROR_UPDATE_END = 132,
  OTA_RESPONSE_ERROR_INVALID_BOOTSTRAPPING = 133,
  OTA_RESPONSE_ERROR_WRONG_CURRENT_FLASH_CONFIG = 134,
  OTA_RESPONSE_ERROR_WRONG_NEW_FLASH_CONFIG = 135,
  OTA_RESPONSE_ERROR_ESP8266_NOT_ENOUGH_SPACE = 136,
  OTA_RESPONSE_ERROR_ESP32_NOT_ENOUGH_SPACE = 137,
  OTA_RESPONSE_ERROR_UNKNOWN = 255,
};

/// OTAComponent provides a simple way to integrate Over-the-Air updates into your app using ArduinoOTA.
class OTAComponent : public Component {
 public:
  /** Set a plaintext password that OTA will use for authentication.
   *
   * Warning: This password will be stored in plaintext in the ROM and can be read
   * by intruders.
   *
   * @param password The plaintext password.
   */
  void set_auth_password(const std::string &password);

  /// Manually set the port OTA should listen on.
  void set_port(uint16_t port);

  void start_safe_mode(uint8_t num_attempts, uint32_t enable_time);

  // ========== INTERNAL METHODS ==========
  // (In most use cases you won't need these)
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void loop() override;

  uint16_t get_port() const;

  void clean_rtc();

  void on_safe_shutdown() override;

 protected:
  void write_rtc_(uint32_t val);
  uint32_t read_rtc_();

  void handle_();
  size_t wait_receive_(uint8_t *buf, size_t bytes, bool check_disconnected = true);

  std::string password_;

  uint16_t port_;

  WiFiServer *server_{nullptr};
  WiFiClient client_{};

  bool has_safe_mode_{false};              ///< stores whether safe mode can be enabled.
  uint32_t safe_mode_start_time_;          ///< stores when safe mode was enabled.
  uint32_t safe_mode_enable_time_{60000};  ///< The time safe mode should be on for.
  uint32_t safe_mode_rtc_value_;
  uint8_t safe_mode_num_attempts_;
  ESPPreferenceObject rtc_;
};

}  // namespace ota
}  // namespace esphome
